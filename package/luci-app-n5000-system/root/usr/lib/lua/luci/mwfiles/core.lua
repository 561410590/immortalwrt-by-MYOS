-- Mwrt 文件管理 · 核心: 授权根目录 / 路径规范化与边界检查 / 列表 / 搜索 / 增删改 / 后台任务
-- 所有对文件系统的读写都必须经过 resolve*(): 规范化 + 符号链接 + 授权根目录边界检查。
-- 不把用户输入拼进 shell: 外部命令一律 argv 形式(luci.sys.process.exec), 不经 /bin/sh。
local nixio = require "nixio"
local fs = require "nixio.fs"
local sys = require "luci.sys"

local M = {}

M.RUN = "/tmp/mwfiles"
M.JOBS = "/tmp/mwfiles/jobs"
M.HELPER = "/usr/libexec/mwfiles-job"
M.CRED_DIR = "/etc/mwfiles/cred"
M.SMB_BASE = "/tmp/mwfiles-smb"

-- 即便在授权目录之内也绝不允许经文件管理读写的位置(凭据、影子口令)
local PROTECTED = { "/etc/mwfiles", "/etc/shadow", "/etc/shadow-", M.RUN .. "/cred" }
-- 不允许被授权为根目录的位置
local DENY_ROOT = { "/", "/proc", "/sys", "/dev", "/etc/mwfiles", "/tmp/mwfiles" }

local REMOTE_FS = { cifs = true, smb3 = true, smbfs = true, nfs = true, nfs4 = true, ["fuse.sshfs"] = true, ["fuse.rclone"] = true, davfs = true }
local LOCAL_FS = { ext2 = true, ext3 = true, ext4 = true, vfat = true, exfat = true, ntfs = true, ntfs3 = true,
	["fuseblk"] = true, ["fuse.ntfs-3g"] = true, f2fs = true, btrfs = true, xfs = true, ubifs = true, jffs2 = true,
	squashfs = true, overlay = true, iso9660 = true, udf = true, hfsplus = true }
-- 自动发现存储卷时只认这些挂载点前缀(系统目录如 /overlay、/rom 不自动出现)
local AUTO_PREFIX = { "/mnt/", "/media/", "/tmp/mnt/", M.SMB_BASE .. "/" }

-- ------------------------------------------------------------ 小工具
local function uci() return require("uci").cursor() end

function M.err(code, msg, extra)
	local t = { ok = false, err = code, msg = msg }
	if extra then for k, v in pairs(extra) do t[k] = v end end
	return t
end

local function has_prefix(path, dir)
	if dir == "/" then return true end
	return path == dir or path:sub(1, #dir + 1) == dir .. "/"
end
M.has_prefix = has_prefix

local function run(argv, timeout)
	if timeout then
		local a = { "/usr/bin/timeout", tostring(timeout) }
		for _, v in ipairs(argv) do a[#a + 1] = v end
		argv = a
	end
	local r = sys.process.exec(argv, true, true)
	return r.code or -1, r.stdout or "", r.stderr or ""
end
M.run = run

-- JSON 里只能放合法 UTF-8: 非法字节替换成 U+FFFD 并标记
local function utf8_clean(s)
	if s == nil then return "", false end
	local bad = false
	local out, i, n = {}, 1, #s
	while i <= n do
		local c = s:byte(i)
		local len
		if c < 0x80 then len = 1
		elseif c >= 0xC2 and c <= 0xDF then len = 2
		elseif c >= 0xE0 and c <= 0xEF then len = 3
		elseif c >= 0xF0 and c <= 0xF4 then len = 4 end
		local ok = len ~= nil and i + len - 1 <= n
		if ok and len > 1 then
			for j = i + 1, i + len - 1 do
				local b = s:byte(j)
				if b < 0x80 or b > 0xBF then ok = false break end
			end
			if ok and len == 3 then
				local c2 = s:byte(i + 1)
				if (c == 0xE0 and c2 < 0xA0) or (c == 0xED and c2 > 0x9F) then ok = false end
			elseif ok and len == 4 then
				local c2 = s:byte(i + 1)
				if (c == 0xF0 and c2 < 0x90) or (c == 0xF4 and c2 > 0x8F) then ok = false end
			end
		end
		if ok then
			out[#out + 1] = s:sub(i, i + len - 1)
			i = i + len
		else
			out[#out + 1] = "\239\191\189"
			bad = true
			i = i + 1
		end
	end
	return table.concat(out), bad
end
M.utf8_clean = utf8_clean

-- ------------------------------------------------------------ 文件类型(服务端唯一来源)
local EXT = {}
local function reg(ty, list) for w in list:gmatch("%S+") do EXT[w] = ty end end
reg("image", "jpg jpeg png gif webp bmp ico avif svg tif tiff heic")
reg("video", "mp4 m4v mkv mov webm avi ogv ts flv wmv 3gp")
reg("audio", "mp3 m4a aac wav ogg oga opus flac wma ape")
reg("doc", "pdf doc docx xls xlsx ppt pptx odt ods odp rtf txt md markdown log csv epub")
reg("archive", "zip tar gz tgz bz2 tbz2 xz txz 7z rar zst lz4 apk ipk img")
reg("code", "sh lua js ts json yaml yml toml ini conf cfg xml html htm css c h cpp hpp py go rs java php sql pl rb uc")

function M.ftype(name)
	local ext = name:match("%.([^./]+)$")
	return ext and EXT[ext:lower()] or "other"
end

-- ------------------------------------------------------------ 挂载信息
local function read_mounts()
	local list = {}
	local f = io.open("/proc/mounts", "r")
	if not f then return list end
	for l in f:lines() do
		local dev, mp, ft, opts = l:match("^(%S+) (%S+) (%S+) (%S+)")
		if dev then
			-- /proc/mounts 把空格写成 \040
			mp = mp:gsub("\\(%d%d%d)", function(o) return string.char(tonumber(o, 8)) end)
			list[#list + 1] = { dev = dev, mp = mp, fstype = ft, opts = opts }
		end
	end
	f:close()
	return list
end
M.read_mounts = read_mounts

local function mount_of(path, mounts)
	local best
	for _, m in ipairs(mounts) do
		if has_prefix(path, m.mp) and (not best or #m.mp > #best.mp) then best = m end
	end
	return best
end
M.mount_of = mount_of

local function opt_has(opts, key)
	for o in (opts or ""):gmatch("[^,]+") do if o == key then return true end end
	return false
end

-- 带超时的容量探测: 远程卷掉线时 statvfs 可能挂死, 所以远程卷走 timeout 子进程
local function vol_stat(path, remote)
	if remote then
		local code, out = run({ "/bin/stat", "-f", "-c", "%S %b %a", path }, 3)
		if code ~= 0 then return nil end
		local bs, blocks, avail = out:match("(%d+) (%d+) (%d+)")
		if not bs then return nil end
		return { total = tonumber(bs) * tonumber(blocks), free = tonumber(bs) * tonumber(avail) }
	end
	local v = fs.statvfs(path)
	if not v then return nil end
	local fr = v.frsize or v.bsize or 0
	return { total = fr * (v.blocks or 0), free = fr * (v.bavail or 0) }
end
M.vol_stat = vol_stat

-- ------------------------------------------------------------ 授权根目录
function M.settings()
	local c = uci()
	local s = {
		enabled = c:get("mwfiles", "main", "enabled") ~= "0",
		hidden = c:get("mwfiles", "main", "hidden") == "1",
		max_upload_mb = tonumber(c:get("mwfiles", "main", "max_upload_mb")) or 0,
		sysroot = c:get("mwfiles", "main", "sysroot") ~= "0",
		roots = {},
	}
	local r = c:get("mwfiles", "main", "root")
	if type(r) == "table" then s.roots = r elseif type(r) == "string" then s.roots = { r } end
	return s
end

local function protected(path)
	for _, p in ipairs(PROTECTED) do if has_prefix(path, p) then return true end end
	return false
end

-- 内核虚拟目录: 内容是实时生成的、可能极大或有副作用, 不允许经文件管理访问
local VIRTUAL = { "/proc", "/sys", "/dev" }
local function virtual(path)
	for _, p in ipairs(VIRTUAL) do if has_prefix(path, p) then return true end end
	return false
end
M.virtual = virtual

-- 路径本身是挂载点, 或里面挂着别的存储(删除/移动会连带抹掉那块盘的内容, 一律拒绝); 返回那个挂载点
local function mount_inside(abs, mounts)
	abs = abs:gsub("/+", "/")
	for _, m in ipairs(mounts) do
		if m.mp ~= "/" and has_prefix(m.mp, abs) then return m.mp end
	end
end

-- root 下的相对路径(root 可能是 "/")
local function rel_of(root, abs)
	if root == "/" then return (abs:gsub("^/+", "")) end
	return (abs:sub(#root + 2))
end

-- 存储接口: 按块设备在 sysfs 里的真实路径判断(usb / sd / nvme); USB 再取协商速率(Mbps: 480=USB2.0, 5000=USB3.0)
local function bus_of(dev)
	local name = type(dev) == "string" and dev:match("^/dev/([%w_%-]+)$")
	if not name then return nil end
	local real = fs.realpath("/sys/class/block/" .. name)
	if not real then return nil end
	if real:find("/usb%d+/") then
		local d, speed = real, nil
		for _ = 1, 14 do
			d = d:match("^(.*)/[^/]+$")
			if not d or d == "" or d == "/sys" then break end
			local sp = tonumber((fs.readfile(d .. "/speed") or ""):match("^%s*(%d+)"))
			if sp then speed = sp; break end
		end
		return "usb", speed
	end
	if real:find("/mmc_host/", 1, true) then return require("luci.mwfiles.guard").dev_emmc(dev) and "emmc" or "sd" end
	if real:find("/nvme", 1, true) then return "nvme" end
end

-- 列出当前所有授权根: 配置里明确授权的目录 + /mnt /media 下已挂载的真实存储卷 + 本插件挂载的 SMB 共享
function M.roots(with_stat)
	local mounts = read_mounts()
	local st = M.settings()
	local out, seen = {}, {}
	local G = require "luci.mwfiles.guard"
	local sset = G.system_set(mounts)

	local function add(path, kind, label, m, cfg)
		local real = fs.realpath(path)
		if not real or seen[real] then return end
		local ds = fs.stat(real)
		if not ds or ds.type ~= "dir" then return end
		seen[real] = true
		local mt = m or mount_of(real, mounts)
		local remote = (mt and REMOTE_FS[mt.fstype]) or false
		local r = {
			id = real, path = real, label = label or real:match("([^/]+)$") or real,
			kind = remote and "remote" or kind, cfg = cfg and true or false,
			fstype = mt and mt.fstype or "", dev = mt and mt.dev or "",
			ro = mt and opt_has(mt.opts, "ro") or false,
			mounted = mt and mt.mp == real or false,
		}
		if mt and remote then r.share = mt.dev end
		-- internal: 固件所在闪存/内存盘(改动要 root 密码); sysdisk: 与系统同盘的存储卷(不可格式化)
		r.internal = (not remote) and G.mount_internal(mt) or false
		r.sysdisk = (not remote) and (not r.internal) and G.dev_is_system(r.dev, sset) or false
		if not remote and not r.internal then r.bus, r.speed = bus_of(r.dev) end
		if with_stat then
			local v = vol_stat(real, remote)
			if v then r.total, r.free = v.total, v.free; r.state = r.ro and "ro" or "ok"
			else r.state = "error" end
		end
		out[#out + 1] = r
	end

	-- 内部存储(固件所在的整个文件系统, 含 /etc /usr /www 等系统目录): 内置位置, 可直接浏览;
	-- 任何改动都要 root 密码(见 guard)。/proc /sys /dev 是内核虚拟目录, 永远不开放。
	if st.sysroot then add("/", "system", "内部存储", nil, false) end
	for _, p in ipairs(st.roots) do
		if type(p) == "string" and p:sub(1, 1) == "/" then add(p, "local", nil, nil, true) end
	end
	for _, m in ipairs(mounts) do
		local auto = false
		for _, pre in ipairs(AUTO_PREFIX) do if m.mp:sub(1, #pre) == pre then auto = true end end
		if auto and (LOCAL_FS[m.fstype] or REMOTE_FS[m.fstype]) then
			local lbl = m.mp:match("([^/]+)$")
			if REMOTE_FS[m.fstype] then lbl = (m.dev or ""):gsub("^//", ""):gsub("/", " / ") end
			add(m.mp, "local", lbl, m, false)
		end
	end
	return out
end

local function find_root(id)
	if type(id) ~= "string" or id == "" then return nil end
	for _, r in ipairs(M.roots(false)) do
		if r.id == id then return r end
	end
end
M.find_root = find_root

-- ------------------------------------------------------------ 路径解析(安全核心)
local function split_rel(rel)
	rel = rel or ""
	if type(rel) ~= "string" or rel:find("%z") then return nil end
	local comps = {}
	for c in rel:gmatch("[^/]+") do
		if c == "." then
			-- 忽略
		elseif c == ".." then
			return nil
		else
			if #c > 255 then return nil end
			comps[#comps + 1] = c
		end
	end
	return comps
end

function M.valid_name(name)
	if type(name) ~= "string" or name == "" or #name > 255 then return false end
	if name == "." or name == ".." or name:find("/", 1, true) or name:find("%c") then return false end
	return true
end

-- follow=true: 叶子符号链接也解析(读取/列目录/下载); false: 只解析父目录, 叶子保持原样(删除/重命名/复制源)
-- 返回 { root=, rel=, abs=, real=(follow 时) , parent= }
function M.resolve(rootid, rel, follow, allow_missing)
	local root = find_root(rootid)
	if not root then return nil, M.err("no_root", "该位置不在授权范围内或已不可用") end
	local comps = split_rel(rel)
	if not comps then return nil, M.err("bad_path", "路径不合法") end
	local base = (root.path == "/") and "" or root.path   -- 内部存储根是 "/", 拼接时不能出现 "//"
	local abs = root.path
	if #comps > 0 then abs = base .. "/" .. table.concat(comps, "/") end
	local res = { root = root, comps = comps, rel = table.concat(comps, "/") }

	if #comps == 0 then
		res.abs, res.real, res.parent = root.path, root.path, nil
		return res
	end
	if virtual(abs) then return nil, M.err("virtual", "这是系统内核的虚拟目录, 不允许经文件管理访问") end

	local parent = root.path
	if #comps > 1 then parent = base .. "/" .. table.concat(comps, "/", 1, #comps - 1) end
	local rparent = fs.realpath(parent)
	if not rparent or not has_prefix(rparent, root.path) then
		return nil, M.err("not_found", "目录不存在或不在授权范围内")
	end
	local leaf = comps[#comps]
	res.abs = (rparent == "/" and "" or rparent) .. "/" .. leaf
	res.parent = rparent
	if virtual(res.abs) then return nil, M.err("virtual", "这是系统内核的虚拟目录, 不允许经文件管理访问") end
	res.name = leaf

	if follow then
		local real = fs.realpath(res.abs)
		if not real then
			if allow_missing then res.real = nil; return res end
			return nil, M.err("not_found", "文件或目录不存在")
		end
		if not has_prefix(real, root.path) then
			return nil, M.err("outside", "链接指向授权目录之外, 已拒绝访问")
		end
		if virtual(real) then return nil, M.err("virtual", "这是系统内核的虚拟目录, 不允许经文件管理访问") end
		res.real = real
	else
		if not allow_missing and not fs.lstat(res.abs) then
			return nil, M.err("not_found", "文件或目录不存在")
		end
	end
	if protected(res.real or res.abs) or protected(res.abs) then
		return nil, M.err("protected", "这是受保护的系统位置, 不允许经文件管理访问")
	end
	return res
end

-- 内部存储写入守卫(需要 root 密码): 通过返回 true, 否则 nil + 错误
local function gcheck(op, paths)
	return require("luci.mwfiles.guard").check(op, paths)
end
M.gcheck = gcheck

local function writable_root(root)
	if root.ro then return nil, M.err("readonly", "该存储卷是只读的") end
	return true
end
M.writable_root = writable_root

-- 系统根 "/" 这一层(/etc /usr /mnt ...)是固件的骨架: 不能在这里新增内容, 也不能删除/重命名/移动这一层的条目(进入子目录后才可操作)
local function top_ok(r, as_dir)
	if r.root.path == "/" and ((as_dir and not r.name) or (not as_dir and r.parent == "/")) then
		return nil, M.err("toplevel", as_dir and "这里是系统根目录, 不能直接新增内容, 请先进入某个文件夹" or "「" .. tostring(r.name) .. "」是系统根目录下的顶层条目, 不允许删除、重命名或移动")
	end
	return true
end
M.top_ok = top_ok

-- ------------------------------------------------------------ 列表
local function natkey(s)
	return (s:lower():gsub("%d+", function(d)
		if #d >= 12 then return d end
		return ("0"):rep(12 - #d) .. d
	end))
end

local function entry_of(dir, name, with_ty)
	local p = dir .. "/" .. name
	local st = fs.lstat(p)
	if not st then return nil end
	local nm, bad = utf8_clean(name)
	local e = { n = nm, s = st.size or 0, m = st.mtime or 0 }
	if bad then e.bad = 1 end
	local ty = st.type
	if ty == "lnk" then
		e.l = 1
		local ts = fs.stat(p)
		if not ts then e.b = 1; ty = "reg"
		else ty = ts.type; e.s = ts.size or 0; e.m = ts.mtime or e.m end
	end
	if ty == "dir" then e.d = 1
	elseif ty ~= "reg" then e.o = 1 end
	e.ty = e.d and "dir" or M.ftype(name)
	return e
end

local function match_filter(e, flt)
	if not flt or flt == "" or flt == "all" then return true end
	if flt == "dir" then return e.d == 1 end
	return e.ty == flt
end

local SORTERS = {
	name = function(a, b) return a._k < b._k end,
	size = function(a, b) if a.s ~= b.s then return a.s < b.s end return a._k < b._k end,
	time = function(a, b) if a.m ~= b.m then return a.m < b.m end return a._k < b._k end,
	type = function(a, b) if a.ty ~= b.ty then return a.ty < b.ty end return a._k < b._k end,
}

function M.list(rootid, rel, opt)
	opt = opt or {}
	local r, err = M.resolve(rootid, rel, true)
	if not r then return err end
	local st = fs.stat(r.real)
	if not st or st.type ~= "dir" then return M.err("not_dir", "这不是一个目录") end

	if r.root.kind == "remote" then
		local code = run({ "/bin/stat", "-f", "-c", "%b", r.root.path }, 3)
		if code ~= 0 then return M.err("share_down", "网络共享无响应或已断开, 请检查服务器与网络") end
	end

	local items, dirs, files, scanned, capped = {}, 0, 0, 0, false
	local it = fs.dir(r.real)
	if not it then return M.err("denied", "没有权限读取该目录") end
	local q = opt.q and opt.q ~= "" and opt.q:lower() or nil
	for name in it do
		scanned = scanned + 1
		if scanned > 200000 then capped = true break end
		if (opt.hidden or name:sub(1, 1) ~= ".") and not (q and not name:lower():find(q, 1, true))
			and not (r.real == "/" and virtual("/" .. name)) then
			local e = entry_of(r.real, name)
			if e and match_filter(e, opt.type) then
				e._k = natkey(e.n)
				items[#items + 1] = e
				if e.d then dirs = dirs + 1 else files = files + 1 end
			end
		end
	end

	local sk = SORTERS[opt.sort] and opt.sort or "name"
	local cmp = SORTERS[sk]
	local desc = opt.dir == "desc"
	table.sort(items, function(a, b)
		if (a.d or 0) ~= (b.d or 0) then return (a.d or 0) > (b.d or 0) end   -- 目录始终在前
		if desc then return cmp(b, a) end
		return cmp(a, b)
	end)

	local total = #items
	local off = math.max(0, tonumber(opt.offset) or 0)
	local lim = math.min(5000, math.max(1, tonumber(opt.limit) or 1000))
	local page = {}
	for i = off + 1, math.min(total, off + lim) do
		local e = items[i]
		e._k = nil
		page[#page + 1] = e
	end

	local v = vol_stat(r.root.path, r.root.kind == "remote")
	local G = require "luci.mwfiles.guard"
	local internal = r.root.kind ~= "remote" and G.path_internal(r.real) or false
	local owner
	if internal then
		G.annotate(r.real, page)
		owner = G.dir_owner(r.real)
	end
	return {
		ok = true, root = r.root.id, path = r.rel, items = page, total = total, dirs = dirs, files = files,
		offset = off, capped = capped, ro = r.root.ro, free = v and v.free, size = v and v.total,
		kind = r.root.kind, fstype = r.root.fstype, internal = internal, owner = owner,
	}
end

-- ------------------------------------------------------------ 搜索(递归, 不跟随符号链接)
local function glob2pat(q)
	if not q:find("[%*%?]") then return nil end
	local p = q:lower():gsub("[%^%$%(%)%%%.%[%]%+%-]", "%%%0"):gsub("%*", ".*"):gsub("%?", ".")
	return "^" .. p .. "$"
end

function M.search(rootid, rel, q, opt)
	opt = opt or {}
	if type(q) ~= "string" or q == "" or #q > 100 then return M.err("bad_query", "请输入要搜索的文件名") end
	local r, err = M.resolve(rootid, rel, true)
	if not r then return err end
	if r.root.kind == "remote" then
		-- 远程卷递归遍历太慢且可能挂死, 只搜当前目录
		opt.shallow = true
	end
	local ql = q:lower()
	local pat = glob2pat(q)
	local results, visited, deadline = {}, 0, os.time() + 8
	local stack = { { r.real, r.rel } }
	local truncated = false
	local maxres = 500
	while #stack > 0 do
		local cur = table.remove(stack)
		local it = fs.dir(cur[1])
		if it then
			for name in it do
				visited = visited + 1
				if visited > 300000 or os.time() > deadline then truncated = true stack = {} break end
				if opt.hidden or name:sub(1, 1) ~= "." then
					local lname = name:lower()
					local hit
					if pat then hit = lname:find(pat) else hit = lname:find(ql, 1, true) end
					local e
					if hit then
						e = entry_of(cur[1], name)
						if e and match_filter(e, opt.type) then
							e.p = cur[2]
							results[#results + 1] = e
							if #results >= maxres then truncated = true stack = {} break end
						end
					end
					if not opt.shallow then
						local sub = (cur[1] == "/" and "" or cur[1]) .. "/" .. name
						local st = not virtual(sub) and fs.lstat(sub)
						if st and st.type == "dir" then
							stack[#stack + 1] = { sub, (cur[2] ~= "" and (cur[2] .. "/") or "") .. name }
						end
					end
				end
			end
		end
	end
	return { ok = true, items = results, total = #results, truncated = truncated, visited = visited, shallow = opt.shallow and true or false }
end

-- ------------------------------------------------------------ 属性
function M.stat(rootid, rel)
	local r, err = M.resolve(rootid, rel, false)
	if not r then return err end
	local st = fs.lstat(r.abs) or fs.stat(r.root.path)
	if not st then return M.err("not_found", "文件或目录不存在") end
	local mounts = read_mounts()
	local mt = mount_of(r.real or r.abs, mounts)
	local info = {
		ok = true, name = r.name or (r.root.label), rel = r.rel, type = st.type, size = st.size or 0,
		mtime = st.mtime, atime = st.atime, ctime = st.ctime, mode = st.modestr, uid = st.uid, gid = st.gid,
		volume = r.root.label, volume_path = r.root.path, kind = r.root.kind, fstype = mt and mt.fstype or "",
		dev = mt and mt.dev or "", ro = r.root.ro, ty = (st.type == "dir") and "dir" or M.ftype(r.name or ""),
	}
	if st.type == "lnk" then
		local tgt = fs.readlink(r.abs)
		info.link = tgt and (select(1, utf8_clean(tgt)))
		local real = fs.realpath(r.abs)
		info.link_ok = real ~= nil and has_prefix(real, r.root.path)
	end
	local sh = require("luci.mwfiles.net").share_for(r.real or r.abs)
	if sh then info.shared = sh end
	local G = require "luci.mwfiles.guard"
	if r.root.kind ~= "remote" and G.path_internal(r.real or r.abs, mounts) then
		info.internal = true
		info.owner = G.dir_owner(r.real or r.abs)
	end
	return info
end

-- 目录大小(有上限的 du, 超时就如实说算不完)
function M.dirsize(rootid, rel)
	local r, err = M.resolve(rootid, rel, true)
	if not r then return err end
	local st = fs.stat(r.real)
	if not st or st.type ~= "dir" then return { ok = true, bytes = st and st.size or 0 } end
	-- 内部存储根按单一文件系统统计(-x/-xdev): 不钻进 /proc /sys /dev, 也不把 /mnt 上的外接盘算进来
	local sysroot = r.root.kind == "system"
	local code, out = run(sysroot and { "/usr/bin/du", "-skx", r.real } or { "/usr/bin/du", "-sk", r.real }, 15)
	if code == 124 then return { ok = true, timeout = true } end
	local kb = out:match("^(%d+)")
	if not kb then return M.err("failed", "无法计算目录大小") end
	local c2, o2 = run(sysroot and { "/usr/bin/find", r.real, "-xdev", "-type", "f" } or { "/usr/bin/find", r.real, "-type", "f" }, 15)
	local nf
	if c2 == 0 then
		nf = 0
		for _ in o2:gmatch("\n") do nf = nf + 1 end
	end
	return { ok = true, bytes = tonumber(kb) * 1024, files = nf }
end

-- ------------------------------------------------------------ 简单同步操作
local function unique_name(dir, name)
	if not fs.lstat(dir .. "/" .. name) then return name end
	local base, ext = name:match("^(.*)(%.[^./]+)$")
	if not base or base == "" then base, ext = name, "" end
	-- tar.gz 这类双扩展名
	local b2, e2 = name:match("^(.*)(%.tar%.[%w]+)$")
	if b2 and b2 ~= "" then base, ext = b2, e2 end
	for i = 1, 9999 do
		local cand = string.format("%s (%d)%s", base, i, ext)
		if #cand <= 255 and not fs.lstat(dir .. "/" .. cand) then return cand end
	end
	return name .. "." .. tostring(os.time())
end
M.unique_name = unique_name

function M.mkdir(rootid, rel, name)
	if not M.valid_name(name) then return M.err("bad_name", "名称不合法(不能为空、不能含 / 、不能是 . 或 ..)") end
	local r, err = M.resolve(rootid, rel, true)
	if not r then return err end
	local ok, e2 = writable_root(r.root)
	if not ok then return e2 end
	local tk, te = top_ok(r, true)
	if not tk then return te end
	if fs.lstat(r.real .. "/" .. name) then return M.err("exists", "已存在同名文件或文件夹") end
	if protected(r.real .. "/" .. name) then return M.err("protected", "受保护的系统位置") end
	local gok, ge = gcheck("write", { r.real .. "/" .. name })
	if not gok then return ge end
	if not fs.mkdir(r.real .. "/" .. name, "755") then return M.err("denied", "创建失败(没有权限或空间不足)") end
	return { ok = true, name = name }
end

function M.newfile(rootid, rel, name)
	if not M.valid_name(name) then return M.err("bad_name", "名称不合法") end
	local r, err = M.resolve(rootid, rel, true)
	if not r then return err end
	local ok, e2 = writable_root(r.root)
	if not ok then return e2 end
	local tk, te = top_ok(r, true)
	if not tk then return te end
	local p = r.real .. "/" .. name
	if fs.lstat(p) then return M.err("exists", "已存在同名文件或文件夹") end
	if protected(p) then return M.err("protected", "受保护的系统位置") end
	local gok, ge = gcheck("write", { p })
	if not gok then return ge end
	local fd = nixio.open(p, nixio.open_flags("wronly", "creat", "excl"), "644")
	if not fd then return M.err("denied", "创建失败(没有权限或空间不足)") end
	fd:close()
	return { ok = true, name = name }
end

function M.rename(rootid, rel, newname)
	if not M.valid_name(newname) then return M.err("bad_name", "名称不合法(不能为空、不能含 / 、不能是 . 或 ..)") end
	local r, err = M.resolve(rootid, rel, false)
	if not r then return err end
	if not r.name then return M.err("bad_path", "不能重命名根目录") end
	local ok, e2 = writable_root(r.root)
	if not ok then return e2 end
	local tk, te = top_ok(r, false)
	if not tk then return te end
	if newname == r.name then return { ok = true, name = newname } end
	local dst = r.parent .. "/" .. newname
	if fs.lstat(dst) then return M.err("exists", "已存在同名文件或文件夹") end
	if protected(dst) then return M.err("protected", "受保护的系统位置") end
	local gok, ge = gcheck("rename", { r.abs, dst })
	if not gok then return ge end
	if not fs.rename(r.abs, dst) then return M.err("denied", "重命名失败(没有权限或目标不可写)") end
	return { ok = true, name = newname }
end

-- ------------------------------------------------------------ 后台任务
local function rnd(n)
	local f = io.open("/dev/urandom", "rb")
	local s = f and f:read(n) or tostring(os.time())
	if f then f:close() end
	local t = {}
	for i = 1, #s do t[i] = string.format("%02x", s:byte(i)) end
	return table.concat(t)
end
M.rnd = rnd

local function job_state(id)
	if type(id) ~= "string" or not id:match("^%x+$") or #id > 32 then return nil end
	local f = io.open(M.JOBS .. "/" .. id .. ".st", "r")
	if not f then return nil end
	local t = { id = id }
	for l in f:lines() do
		local k, v = l:match("^(%w+)=(.*)$")
		if k then t[k] = v end
	end
	f:close()
	return t
end

function M.job_status(id)
	local t = job_state(id)
	if not t then return M.err("no_job", "任务不存在或已过期") end
	-- 进程不在了但状态仍是 running => 被杀/崩溃
	if t.state == "running" and t.pid and not fs.access("/proc/" .. t.pid) then
		t.state = "failed"; t.code = "killed"; t.msg = "任务被中断"
	end
	local now = os.time()
	local r = {
		ok = true, id = id, state = t.state, op = t.op, msg = (select(1, utf8_clean(t.msg or ""))), code = t.code,
		items = tonumber(t.items), done = tonumber(t.done), fails = tonumber(t.fails),
		elapsed = (tonumber(t.started) and (now - tonumber(t.started))) or 0,
		dest = t.dest and (select(1, utf8_clean(t.dest))),
	}
	-- 真实字节进度: 只在知道总量(单文件复制/移动)或在写临时文件(压缩)时给出, 数值取自目标文件当前大小
	if t.state == "running" and t.cur and t.cur ~= "" and has_prefix(t.cur, "/") then
		local cs = fs.lstat(t.cur)
		if cs and cs.type == "reg" then
			r.bytes = cs.size
			r.total = tonumber(t.total)
		end
	end
	return r
end

function M.job_cancel(id)
	local t = job_state(id)
	if not t then return M.err("no_job", "任务不存在或已过期") end
	if t.state ~= "running" or not t.pid or not t.pid:match("^%d+$") then return { ok = true } end
	local cm = io.open("/proc/" .. t.pid .. "/cmdline", "rb")
	local cmd = cm and cm:read("*a") or ""
	if cm then cm:close() end
	if cmd:find("mwfiles-job", 1, true) then
		-- 杀整个进程组(helper 以 setsid 起, 组长就是它自己)
		run({ "/bin/kill", "-TERM", "-" .. t.pid })
		run({ "/bin/kill", "-TERM", t.pid })
	end
	return { ok = true }
end

-- 启动 helper; 参数全部走 argv。返回 job id
local function start_job(op, args)
	fs.mkdirr(M.JOBS)
	-- 清理 1 天前的任务状态
	local now = os.time()
	for n in (fs.dir(M.JOBS) or function() end) do
		local st = fs.stat(M.JOBS .. "/" .. n)
		if st and st.mtime and now - st.mtime > 86400 then fs.unlink(M.JOBS .. "/" .. n) end
	end
	local id = rnd(6)
	-- 先落一个初始状态, 避免 helper 还没来得及写时前端轮询到"任务不存在"
	local sf = io.open(M.JOBS .. "/" .. id .. ".st", "w")
	if sf then
		sf:write("op=" .. op .. "\nstate=running\nstarted=" .. now .. "\n")
		sf:close()
	end
	local argv = { "/usr/bin/setsid", M.HELPER, id, op }
	for _, a in ipairs(args) do argv[#argv + 1] = a end
	sys.process.exec(argv, false, false, true)
	return id
end

-- 起任务后最多等 wait 秒: 很快做完的直接返回结果, 否则交给前端轮询
local function job_and_wait(op, args, wait)
	local id = start_job(op, args)
	local deadline = os.time() + (wait or 2)
	local tries = 0
	while true do
		nixio.nanosleep(0, 120000000)
		tries = tries + 1
		local s = M.job_status(id)
		if s.ok and s.state ~= "running" then
			s.job = id
			return s
		end
		if tries > 40 or os.time() > deadline then break end
	end
	local s = M.job_status(id)
	if s.ok then s.job = id end
	return s.ok and s or { ok = true, id = id, job = id, state = "running" }
end

M.job_and_wait = job_and_wait

-- ------------------------------------------------------------ 删除
function M.delete(rootid, rels)
	if type(rels) ~= "table" or #rels == 0 then return M.err("bad_path", "没有选择项目") end
	local paths = {}
	local root
	local mounts = read_mounts()
	for _, rel in ipairs(rels) do
		local r, err = M.resolve(rootid, rel, false)
		if not r then return err end
		if not r.name then return M.err("bad_path", "不能删除根目录") end
		local ok, e2 = writable_root(r.root)
		if not ok then return e2 end
		local tk, te = top_ok(r, false)
		if not tk then return te end
		local mi = mount_inside(r.abs, mounts)
		if mi then return M.err("mountpoint", "「" .. r.name .. "」是存储挂载点或里面挂载着其他存储(" .. mi .. "), 删除会连带清空那块存储, 已拒绝; 请先卸载它") end
		root = r.root
		paths[#paths + 1] = r.abs
	end
	local gok, ge = gcheck("delete", paths)
	if not gok then return ge end
	local res = job_and_wait("rm", paths, 2)
	res.count = #paths
	return res
end

-- ------------------------------------------------------------ 复制 / 移动
-- conflict: ask(只检查并返回冲突清单) | overwrite | rename | skip
function M.transfer(op, src_root, rels, dst_root, dst_rel, conflict)
	if op ~= "cp" and op ~= "mv" then return M.err("bad_op", "未知操作") end
	if type(rels) ~= "table" or #rels == 0 then return M.err("bad_path", "没有选择项目") end
	local d, err = M.resolve(dst_root, dst_rel, true)
	if not d then return err end
	local dst_st = fs.stat(d.real)
	if not dst_st or dst_st.type ~= "dir" then return M.err("not_dir", "目标不是目录") end
	local ok, e2 = writable_root(d.root)
	if not ok then return e2 end
	local tk, te = top_ok(d, true)
	if not tk then return te end
	if protected(d.real) then return M.err("protected", "受保护的系统位置") end
	conflict = conflict or "ask"

	local plan, conflicts, skipped = {}, {}, 0
	local used = {}
	for _, rel in ipairs(rels) do
		local s, e = M.resolve(src_root, rel, false)
		if not s then return e end
		if not s.name then return M.err("bad_path", "不能复制或移动根目录") end
		if op == "mv" then
			local ok3, e3 = writable_root(s.root)
			if not ok3 then return e3 end
			local tk3, te3 = top_ok(s, false)
			if not tk3 then return te3 end
			local mi = mount_inside(s.abs, read_mounts())
			if mi then return M.err("mountpoint", "「" .. s.name .. "」是存储挂载点或里面挂载着其他存储(" .. mi .. "), 不能移动; 请先卸载它") end
		end
		local sst = fs.lstat(s.abs)
		local is_dir = sst and sst.type == "dir"
		-- 目录不能放进它自己里面
		if is_dir then
			local rs = fs.realpath(s.abs)
			if rs and has_prefix(d.real, rs) then
				return M.err("into_self", "不能把文件夹「" .. s.name .. "」复制或移动到它自己里面")
			end
		end
		local same_dir = (s.parent == d.real)
		local target = d.real .. "/" .. s.name
		local exist = fs.lstat(target)
		local clash = exist or used[s.name]
		local action = "go"     -- go | skip | ask | rename | overwrite
		if op == "mv" and same_dir then
			action = "skip"; skipped = skipped + 1
		elseif clash then
			if op == "cp" and same_dir then
				action = "rename"           -- 在原目录复制 = 生成副本, 不需要询问
			elseif conflict == "ask" then
				action = "ask"
				conflicts[#conflicts + 1] = { n = (select(1, utf8_clean(s.name))), d = is_dir and 1 or 0 }
			elseif conflict == "skip" then
				action = "skip"; skipped = skipped + 1
			elseif conflict == "rename" then
				action = "rename"
			elseif conflict == "overwrite" then
				if exist and (is_dir or exist.type == "dir") then
					return M.err("dir_conflict", "「" .. s.name .. "」是文件夹或与目标中的文件夹同名, 不支持直接覆盖; 请选择「保留两者」或先删除目标")
				end
				action = "overwrite"
			end
		end
		if action == "rename" then
			-- 在已存在 + 本批已占用的名字里找唯一名
			local nn = s.name
			local b, ext = nn:match("^(.*)(%.[^./]+)$")
			if not b or b == "" then b, ext = nn, "" end
			local b2, e3 = nn:match("^(.*)(%.tar%.[%w]+)$")
			if b2 and b2 ~= "" then b, ext = b2, e3 end
			local i, cand = 0, nn
			while fs.lstat(d.real .. "/" .. cand) or used[cand] do
				i = i + 1
				cand = string.format("%s (%d)%s", b, i, ext)
			end
			target = d.real .. "/" .. cand
		end
		if conflict ~= "ask" and action ~= "skip" then
			used[target:match("([^/]+)$")] = true
			plan[#plan + 1] = { s.abs, target, is_dir and "d" or "f", (action == "overwrite") and "o" or "n" }
		end
	end

	if conflict == "ask" then
		if #conflicts > 0 then return { ok = true, conflicts = conflicts } end
		-- 无冲突, 直接按 rename(等价无冲突)执行
		return M.transfer(op, src_root, rels, dst_root, dst_rel, "rename")
	end
	if #plan == 0 then return { ok = true, state = "done", skipped = skipped, msg = "没有需要处理的项目" } end

	-- 内部存储: 目标在内部存储, 或移动(会从源处删除)的源在内部存储, 都要 root 密码
	local gp = {}
	for _, p in ipairs(plan) do
		gp[#gp + 1] = p[2]
		if op == "mv" then gp[#gp + 1] = p[1] end
	end
	local gok, ge = gcheck(op == "mv" and "move" or "write", gp)
	if not gok then return ge end
	local args = {}
	for _, p in ipairs(plan) do
		args[#args + 1] = p[1]; args[#args + 1] = p[2]; args[#args + 1] = p[3]; args[#args + 1] = p[4]
	end
	local res = job_and_wait(op, args, 2)
	res.skipped = skipped
	return res
end

-- ------------------------------------------------------------ 压缩 / 解压
function M.caps_archive()
	local function have(p) return fs.access(p, "x") and true or false end
	return {
		tar = have("/bin/tar") or have("/usr/bin/tar"),
		gzip = have("/bin/gzip") or have("/usr/bin/gzip"),
		bzip2 = have("/usr/bin/bzip2"), xz = have("/usr/bin/xz"),
		unzip = have("/usr/bin/unzip"), zip = have("/usr/bin/zip"),
	}
end

local function archive_kind(name)
	local l = name:lower()
	if l:match("%.tar%.gz$") or l:match("%.tgz$") then return "tgz" end
	if l:match("%.tar%.bz2$") or l:match("%.tbz2?$") then return "tbz" end
	if l:match("%.tar%.xz$") or l:match("%.txz$") then return "txz" end
	if l:match("%.tar$") then return "tar" end
	if l:match("%.zip$") then return "zip" end
	if l:match("%.gz$") then return "gz" end
	return nil
end
M.archive_kind = archive_kind

function M.compress(rootid, rels, dst_rel, name, fmt)
	local caps = M.caps_archive()
	fmt = fmt == "zip" and "zip" or "tgz"
	if fmt == "tgz" and not (caps.tar and caps.gzip) then return M.err("no_dep", "缺少 tar/gzip, 无法压缩为 .tar.gz") end
	if fmt == "zip" and not caps.zip then return M.err("no_dep", "固件未安装 zip 工具(软件包 zip), 无法压缩为 .zip; 可改用 .tar.gz") end
	if type(rels) ~= "table" or #rels == 0 then return M.err("bad_path", "没有选择项目") end
	local d, err = M.resolve(rootid, dst_rel, true)
	if not d then return err end
	local ok, e2 = writable_root(d.root)
	if not ok then return e2 end
	local tk, te = top_ok(d, true)
	if not tk then return te end
	local ext = fmt == "zip" and ".zip" or ".tar.gz"
	name = tostring(name or "")
	if not M.valid_name(name) then return M.err("bad_name", "压缩包名称不合法") end
	if not name:lower():match(ext:gsub("%.", "%%.") .. "$") then name = name .. ext end
	local dest = d.real .. "/" .. unique_name(d.real, name)
	local parent, names = nil, {}
	for _, rel in ipairs(rels) do
		local s, e = M.resolve(rootid, rel, false)
		if not s then return e end
		if not s.name then return M.err("bad_path", "不能压缩根目录") end
		if parent and s.parent ~= parent then return M.err("bad_path", "只能压缩同一目录下的项目") end
		parent = s.parent
		names[#names + 1] = s.name
	end
	local gok, ge = gcheck("write", { dest })
	if not gok then return ge end
	local args = { fmt, dest, parent }
	for _, n in ipairs(names) do args[#args + 1] = n end
	local res = job_and_wait("pack", args, 2)
	res.dest = dest:match("([^/]+)$")
	return res
end

function M.extract(rootid, rel, dst_rel)
	local s, err = M.resolve(rootid, rel, true)
	if not s then return err end
	local st = fs.stat(s.real)
	if not st or st.type ~= "reg" then return M.err("not_file", "这不是文件") end
	local kind = archive_kind(s.name or "")
	if not kind then return M.err("bad_type", "不支持的压缩格式(支持 zip / tar / tar.gz / tar.bz2 / tar.xz / gz)") end
	local caps = M.caps_archive()
	if kind == "zip" and not caps.unzip then return M.err("no_dep", "固件未安装 unzip, 无法解压 .zip") end
	if kind ~= "zip" and kind ~= "gz" and not caps.tar then return M.err("no_dep", "固件未安装 tar, 无法解压") end
	if (kind == "tgz" or kind == "gz") and not caps.gzip then return M.err("no_dep", "固件未安装 gzip, 无法解压") end
	if kind == "tbz" and not caps.bzip2 then return M.err("no_dep", "固件未安装 bzip2, 无法解压 .tar.bz2") end
	if kind == "txz" and not caps.xz then return M.err("no_dep", "固件未安装 xz, 无法解压 .tar.xz") end
	local d, e2 = M.resolve(rootid, dst_rel ~= nil and dst_rel or rel_of(s.root.path, s.parent), true)
	if not d then return e2 end
	local ok, e3 = writable_root(d.root)
	if not ok then return e3 end
	local tk, te = top_ok(d, true)
	if not tk then return te end
	-- 解到以压缩包名命名的新文件夹里, 不把内容散在当前目录
	local base = (s.name:gsub("%.tar%.%w+$", ""):gsub("%.[^.]+$", ""))
	if base == "" then base = "解压结果" end
	local outdir = d.real .. "/" .. unique_name(d.real, base)
	local gok, ge = gcheck("write", { outdir })
	if not gok then return ge end
	local res = job_and_wait("unpack", { kind, s.real, outdir }, 3)
	res.dest = outdir:match("([^/]+)$")
	return res
end

return M
