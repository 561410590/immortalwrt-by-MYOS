-- Mwrt 文件管理 · 守卫: 内部存储识别 / 软件包归属 / root 密码校验 / 磁盘格式化
-- 内部存储(固件所在的闪存: /、/overlay、/rom 与内存盘)的任何改动必须: 用户确认后果 + 输入 root 密码, 由服务端强制校验。
-- root 密码只放在请求头 Authorization: Mwfiles <base64> 里(不进 URL、不进命令行、不写日志、不回显)。
-- 格式化只针对用户手动选中的外接/辅助存储分区, 绝不自动执行, 绝不动系统盘; 所有外部命令都是 argv, 不经 shell。
local nixio = require "nixio"
local fs = require "nixio.fs"
local http = require "luci.http"
local core = require "luci.mwfiles.core"

local M = {}

M.OPKG_INFO = "/usr/lib/opkg/info"
-- 允许出现在「格式化」候选里的整盘设备名(其余如 loop/zram/mtd/ubi/dm 一律不可能出现)
M.DISK_PATTERNS = { "^sd%a+$", "^mmcblk%d+$", "^nvme%d+n%d+$", "^vd%a+$" }

-- ------------------------------------------------------------ root 密码
local FAIL_FILE = core.RUN .. "/pwfail"
local MAX_FAIL, LOCK_SECS = 5, 300

local function read_fail()
	local f = io.open(FAIL_FILE, "r")
	if not f then return 0, 0 end
	local c, t = f:read("*a"):match("^(%d+) (%d+)")
	f:close()
	return tonumber(c) or 0, tonumber(t) or 0
end

local function write_fail(c, t)
	fs.mkdirr(core.RUN)
	local f = io.open(FAIL_FILE, "w")
	if f then f:write(c .. " " .. t); f:close() end
end

-- 返回 ok(true) 或 nil + 错误表
function M.verify_pw(pw)
	local sysu = require("luci.sys").user
	local hash = sysu.getpasswd and sysu.getpasswd("root") or nil
	if not hash or hash == "" or hash == "!" or hash == "*" then
		return nil, core.err("no_root_pw", "root 账户还没有设置密码, 请先在「系统 → 管理权」里设置后再操作")
	end
	local now = os.time()
	local c, t = read_fail()
	if c >= MAX_FAIL and now - t < LOCK_SECS then
		return nil, core.err("pw_locked", string.format("密码连续输错 %d 次, 已暂时锁定, 请 %d 分钟后再试", MAX_FAIL, math.ceil((LOCK_SECS - (now - t)) / 60)))
	end
	if c >= MAX_FAIL then c = 0 end
	if type(pw) ~= "string" or pw == "" or #pw > 256 or not sysu.checkpasswd("root", pw) then
		c = c + 1
		write_fail(c, now)
		local left = MAX_FAIL - c
		return nil, core.err("bad_pw", left > 0 and ("root 密码不正确(还可尝试 " .. left .. " 次)") or "root 密码不正确, 已暂时锁定 5 分钟")
	end
	fs.unlink(FAIL_FILE)
	return true
end

-- 密码放在 Authorization: Mwfiles <base64(密码)> 里: uhttpd 的 CGI 只转发白名单请求头(自定义头会被丢弃),
-- Authorization 在白名单内; base64 让含中文等非 ASCII 的密码也能放进请求头。
function M.req_pw()
	local h = http.getenv("HTTP_AUTHORIZATION")
	local b = type(h) == "string" and h:match("^Mwfiles%s+([%w%+/=]+)$")
	if not b or #b > 400 then return nil end
	local ok, pw = pcall(nixio.bin.b64decode, b)
	if ok and type(pw) == "string" and pw ~= "" then return pw end
	return nil
end
local req_pw = M.req_pw

-- ------------------------------------------------------------ 软件包归属(opkg 文件清单)
local sys_prefix = { "/etc", "/usr", "/lib", "/lib64", "/bin", "/sbin", "/www", "/rom", "/overlay", "/boot", "/opt", "/root", "/srv", "/var" }

-- 把 overlay 的 upper 层路径映射回它在系统里的位置
local function sys_path(p)
	p = p:gsub("/+", "/")   -- 内部存储根是 "/", 上层拼出来的路径可能带 "//"
	local q = p:match("^/overlay/upper(/.*)$") or p:match("^/rom(/.*)$")
	return q or p
end

local function under_system(p)
	p = sys_path(p)
	for _, s in ipairs(sys_prefix) do if core.has_prefix(p, s) then return true end end
	return false
end
M.under_system = under_system

local idx_cache
local function build_index()
	if idx_cache then return idx_cache end
	local exact, dirs = {}, {}
	for n in (fs.dir(M.OPKG_INFO) or function() end) do
		local pkg = n:match("^(.+)%.list$")
		if pkg then
			local f = io.open(M.OPKG_INFO .. "/" .. n, "r")
			if f then
				for l in f:lines() do
					if l:sub(1, 1) == "/" then
						if not exact[l] then exact[l] = pkg end
						local d = l
						while true do
							d = d:match("^(.*)/[^/]*$")
							if not d or d == "" then break end
							local cur = dirs[d]
							if cur == pkg or cur == "*" then break end
							dirs[d] = cur and "*" or pkg
						end
					end
				end
				f:close()
			end
		end
	end
	idx_cache = { exact = exact, dirs = dirs }
	return idx_cache
end

local zh_cache
local function zh_name(pkg)
	if zh_cache == nil then
		zh_cache = false
		local f = io.open("/www/luci-static/resources/pkg-zh.json", "r")
		if f then
			local d = require("luci.jsonc").parse(f:read("*a"))
			f:close()
			if type(d) == "table" and type(d.p) == "table" then zh_cache = d.p end
		end
	end
	local e = zh_cache and zh_cache[pkg]
	return type(e) == "table" and type(e[1]) == "string" and e[1] or nil
end

local pkg_cache = {}
local function pkg_meta(pkg)
	if pkg_cache[pkg] then return pkg_cache[pkg] end
	local m = { name = pkg }
	local f = io.open(M.OPKG_INFO .. "/" .. pkg .. ".control", "r")
	if f then
		for l in f:lines() do
			local k, v = l:match("^(%a[%w%-]*):%s*(.*)$")
			if k == "Version" then m.version = v
			elseif k == "Essential" then m.essential = (v == "yes")
			elseif k == "Description" and v ~= "" then m.desc = v:gsub("^%s+", "") end
		end
		f:close()
	end
	m.title = zh_name(pkg)
	if m.desc then m.desc = select(1, core.utf8_clean(m.desc:sub(1, 120))) end
	-- 内核/基础库/启动与网络核心: 即使没有标 Essential 也按核心组件对待
	local base = pkg:match("^kernel$") or pkg:match("^kmod%-") or pkg:match("^libc$") or pkg:match("^busybox$") or pkg:match("^base%-files$")
	local core_set = { procd = 1, ubus = 1, ubox = 1, uci = 1, netifd = 1, uhttpd = 1, dropbear = 1, firewall4 = 1, fstools = 1, ["luci-base"] = 1, ["luci-compat"] = 1, rpcd = 1, dnsmasq = 1, odhcpd = 1, ["luci-theme-Mwrt"] = 1, Airpifanctrl = 1, WTModem = 1, ["mwrt-nros"] = 1 }
	m.core = (m.essential or base or core_set[pkg] or pkg:match("^libubox") or pkg:match("^libuci")) and true or false
	pkg_cache[pkg] = m
	return m
end

-- 返回 { pkg=名|nil, multi=bool, kind="pkg"|"conf"|"sys"|"data" }
local function classify(abs)
	local p = sys_path(abs)
	if p:match("/bdinfo$") then return { kind = "bdinfo" } end
	if not under_system(abs) then return { kind = "data" } end
	-- 系统顶层目录本身(/etc、/usr、/bin ...): 动它等于动整个系统
	if p:match("^/[%w_]+$") then return { kind = "top" } end
	local ix = build_index()
	local pkg = ix.exact[p]
	local multi = false
	if not pkg then
		local d = ix.dirs[p]
		if d == "*" then multi = true elseif d then pkg = d end
	end
	local conf = core.has_prefix(p, "/etc/config") or (p:match("^/etc/[^/]+%.conf$") ~= nil)
	if pkg then return { kind = conf and "conf" or "pkg", pkg = pkg } end
	if multi then return { kind = "pkg", multi = true } end
	return { kind = conf and "conf" or "sys" }
end
M.classify = classify

-- 列表里给每一项附上归属(只在系统位置计算; 返回的是 e.ow=包名 / e.ow="*" 表示多个组件)
function M.annotate(dir_real, items)
	if not under_system(dir_real) then return end
	for _, e in ipairs(items) do
		local c = classify(dir_real .. "/" .. e.n)
		if c.pkg then e.ow = c.pkg elseif c.multi then e.ow = "*" else e.ow = "-" end
	end
end

function M.dir_owner(abs)
	if not under_system(abs) then return nil end
	local c = classify(abs)
	if c.pkg then return { pkg = c.pkg, meta = pkg_meta(c.pkg) } end
	return { multi = c.multi or nil }
end

-- ------------------------------------------------------------ 内部存储判定
local INTERNAL_FS = { overlay = true, squashfs = true, tmpfs = true, ramfs = true, ubifs = true, jffs2 = true, rootfs = true }
local INTERNAL_MP = { ["/"] = true, ["/overlay"] = true, ["/rom"] = true }

-- 板载 eMMC(/sys 里整盘 device/type 为 MMC; SD 卡是 SD): 里面是固件分区和出厂信息(bdinfo)等, 按内部存储对待
local emmc_cache = {}
function M.dev_emmc(dev)
	local disk = type(dev) == "string" and dev:match("^/dev/(mmcblk%d+)")
	if not disk then return false end
	-- N5000 系统运行在 TF 卡，sysfs 将它标为 SD。
	if (fs.readfile("/tmp/sysinfo/board_name") or ""):match("^nradio,n5000%s*$") then return true end
	if emmc_cache[disk] == nil then
		local f = io.open("/sys/class/block/" .. disk .. "/device/type", "r")
		local t = f and f:read("*l") or ""
		if f then f:close() end
		emmc_cache[disk] = (t == "MMC")
	end
	return emmc_cache[disk]
end

function M.mount_internal(mt)
	if not mt then return false end
	return INTERNAL_FS[mt.fstype] or INTERNAL_MP[mt.mp] or M.dev_emmc(mt.dev) or false
end

function M.path_internal(abs, mounts)
	abs = abs:gsub("/+", "/")
	return M.mount_internal(core.mount_of(abs, mounts or core.read_mounts())) and true or false
end

local OPLABEL = { delete = "删除", rename = "重命名或移动", move = "移动", write = "新增或写入", overwrite = "覆盖", share = "共享" }

local function consequence(op, infos)
	-- infos: classify 结果的列表; 汇总出最严重的一档 + 涉及的软件包
	local level, pk, seen, kinds = "info", {}, {}, {}
	local function bump(l) local o = { info = 1, warn = 2, crit = 3 }; if o[l] > o[level] then level = l end end
	for _, c in ipairs(infos) do
		kinds[c.kind] = true
		if c.pkg and not seen[c.pkg] then
			seen[c.pkg] = true
			local m = pkg_meta(c.pkg)
			pk[#pk + 1] = m
			bump(m.core and "crit" or "warn")
		elseif c.multi then kinds.multi = true; bump("warn")
		elseif c.kind == "top" or c.kind == "bdinfo" then bump("crit")
		elseif c.kind == "sys" or c.kind == "conf" then bump("warn") end
	end
	local verb = OPLABEL[op] or "修改"
	local t = {}
	if op == "share" then
		t[#t + 1] = "共享内部存储里的文件夹后, 局域网内的设备可以读取其中的文件(允许匿名或可写时还能直接修改), 存在泄露和被误改的风险。"
		for _, m in ipairs(pk) do t[#t + 1] = "该位置属于软件包「" .. ((m.title and (m.title .. "(" .. m.name .. ")")) or m.name) .. "」, 共享出去的是软件自身的文件。" end
	elseif op == "write" then
		t[#t + 1] = "在内部存储新增内容会占用固件的可用空间; 空间用尽时系统与各插件可能运行异常、设置无法保存。"
		if #pk > 0 or kinds.multi then t[#t + 1] = "目标位置属于已安装的软件, 新增的文件可能被软件误读或在升级时被清理。" end
	else
		for _, m in ipairs(pk) do
			local nm = (m.title and (m.title .. "(" .. m.name .. ")")) or m.name
			if m.core then
				t[#t + 1] = "「" .. nm .. "」是系统核心组件。" .. verb .. "其文件可能导致路由器无法启动、无法登录网页或失去网络, 需要重新刷机才能恢复。"
			else
				t[#t + 1] = "属于软件包「" .. nm .. "」。" .. verb .. "后该软件可能无法启动或功能异常(页面打不开、服务失效); 一般需要重新安装该软件包才能恢复, 固件内置的软件包可能需要重新刷机。"
			end
		end
		if kinds.top then t[#t + 1] = "这是系统的顶层目录, " .. verb .. "它相当于破坏整个系统: 路由器很可能无法再启动或登录, 需要重新刷机才能恢复。" end
		if kinds.multi then t[#t + 1] = "这里包含多个软件包的文件, " .. verb .. "会同时影响它们。" end
		if kinds.conf then t[#t + 1] = "其中有配置文件: 相关设置会丢失或被还原为默认值, 对应服务可能无法工作。" end
		if kinds.sys and #pk == 0 and not kinds.multi then t[#t + 1] = "这是系统目录里不属于任何软件包的文件(可能是系统运行时生成或手动放置的), 误改可能影响系统运行。" end
		if kinds.bdinfo then t[#t + 1] = "「bdinfo」是出厂信息(本机的 MAC 地址、序列号等), 存放在板载 eMMC 上。" .. verb .. "它会让网卡 MAC、云控绑定等信息丢失或错乱, 并且无法找回(没有回收站); 如确需操作, 请先把它下载备份。" end
		if kinds.data then t[#t + 1] = "这是内部存储(闪存)上的用户数据, " .. verb .. "后无法找回(没有回收站)。" end
		if op == "delete" or op == "overwrite" then t[#t + 1] = "此操作不可撤销。" end
	end
	return { level = level, owners = pk, text = t }
end

-- 对一批绝对路径给出内部存储说明; 全部不在内部存储时返回 nil
function M.describe(op, paths)
	local mounts = core.read_mounts()
	local infos, n = {}, 0
	for _, p in ipairs(paths) do
		p = p:gsub("/+", "/")
		if M.path_internal(p, mounts) then
			n = n + 1
			if n <= 400 then infos[#infos + 1] = classify(p) end
		end
	end
	if n == 0 then return nil end
	local d = consequence(op, infos)
	d.internal = true
	d.count = n
	-- 精简给前端的归属信息
	local ow = {}
	for _, m in ipairs(d.owners) do
		ow[#ow + 1] = { pkg = m.name, title = m.title, version = m.version, desc = m.desc, core = m.core }
	end
	d.owners = ow
	return d
end

-- 写操作守卫: paths 是将被改动的绝对路径(新建/上传时是目标目录)
-- 通过返回 true; 否则返回 nil + 错误(含 need_root_pw / bad_pw / pw_locked / no_root_pw)
function M.check(op, paths)
	local d = M.describe(op, paths)
	if not d then return true end
	local pw = req_pw()
	if not pw or pw == "" then
		return nil, core.err("need_root_pw", "这是内部存储, 需要输入 root 密码确认", { op = op, info = d })
	end
	local ok, e = M.verify_pw(pw)
	if not ok then e.info = d; return nil, e end
	return true
end

-- ------------------------------------------------------------ 磁盘 / 分区枚举与格式化
local function rd(path)
	local f = io.open(path, "r")
	if not f then return nil end
	local s = f:read("*l")
	f:close()
	return s and (s:gsub("^%s+", ""):gsub("%s+$", "")) or nil
end

local function find_tool(names)
	for _, n in ipairs(names) do
		for _, d in ipairs({ "/usr/sbin/", "/sbin/", "/usr/bin/", "/bin/" }) do
			if fs.access(d .. n, "x") then return d .. n end
		end
	end
end

local function kernel_fs()
	local set = {}
	local f = io.open("/proc/filesystems", "r")
	if f then
		for l in f:lines() do local n = l:match("(%S+)$"); if n then set[n] = true end end
		f:close()
	end
	return set
end

-- 能格式化成什么: 只由固件里真实存在的 mkfs 工具决定
M.FS = {
	{ id = "ext4", name = "ext4", note = "Linux 专用, 稳定可靠(路由器自用首选)", tools = { "mkfs.ext4", "mke2fs" }, kfs = "ext4", maxlabel = 16 },
	{ id = "f2fs", name = "F2FS", note = "为闪存(SD 卡/U 盘)优化, 仅 Linux 可读", tools = { "mkfs.f2fs" }, kfs = "f2fs", maxlabel = 16 },
	{ id = "vfat", name = "FAT32", note = "Windows/macOS/相机通用, 单文件最大 4 GB", tools = { "mkfs.vfat", "mkfs.fat" }, kfs = "vfat", maxlabel = 11, strict = true },
	{ id = "exfat", name = "exFAT", note = "Windows/macOS 通用, 无 4 GB 限制", tools = { "mkfs.exfat", "mkexfatfs", "exfatprogs" }, kfs = "exfat", maxlabel = 15 },
	{ id = "ntfs", name = "NTFS", note = "Windows 专用(Linux 读写较慢)", tools = { "mkntfs" }, kfs = "ntfs3", maxlabel = 32 },
}

function M.fs_options()
	local kf = kernel_fs()
	local out = {}
	for _, f in ipairs(M.FS) do
		local tool = find_tool(f.tools)
		local o = { id = f.id, name = f.name, note = f.note, maxlabel = f.maxlabel, ok = tool ~= nil and kf[f.kfs] ~= nil }
		if not tool then o.why = "固件没有 " .. f.tools[1] .. " 工具"
		elseif not kf[f.kfs] then o.why = "内核没有 " .. f.kfs .. " 文件系统支持" end
		out[#out + 1] = o
	end
	return out
end

-- 设备在 /sys 里的父磁盘
local function blk_parent(name)
	local p = fs.realpath("/sys/class/block/" .. name)
	if not p then return nil end
	if fs.access(p .. "/partition") then return p:match("([^/]+)/[^/]+$") end
	return name
end

local function mark_dev(set, dev, mounts, depth)
	depth = depth or 0
	if depth > 4 or type(dev) ~= "string" then return end
	local name = dev:match("^/dev/(.+)$")
	if not name then return end
	name = name:gsub("/", "!")
	if name:match("^loop%d+$") then
		set[name] = true
		local bf = rd("/sys/block/" .. name .. "/loop/backing_file")
		if bf then
			local mt = core.mount_of(bf, mounts)
			if mt and mt.dev and mt.dev:sub(1, 5) == "/dev/" then mark_dev(set, mt.dev, mounts, depth + 1) end
			-- 启动阶段的设备节点(如 /mmcblk0p4)直接按名字认
			local base = bf:match("([^/]+)$")
			if base and blk_parent(base) then mark_dev(set, "/dev/" .. base, mounts, depth + 1) end
		end
		return
	end
	set[name] = true
	local d = blk_parent(name)
	if d then set[d] = true end
end

-- 系统占用的磁盘集合: 承载 /、/rom、/overlay 的磁盘(含 loop 回溯)、正在用作交换的设备
function M.system_set(mounts)
	mounts = mounts or core.read_mounts()
	local set = {}
	for _, m in ipairs(mounts) do
		if INTERNAL_MP[m.mp] and m.dev:sub(1, 5) == "/dev/" then
			if m.dev == "/dev/root" then
				local root = (rd("/proc/cmdline") or ""):match("root=/dev/(%S+)")
				if root then mark_dev(set, "/dev/" .. root, mounts) end
				set["root"] = true
			else
				mark_dev(set, m.dev, mounts)
			end
		end
	end
	local f = io.open("/proc/swaps", "r")
	if f then
		f:read("*l")
		for l in f:lines() do
			local sw = l:match("^(%S+)")
			if sw and sw:sub(1, 5) == "/dev/" then mark_dev(set, sw, mounts) end
		end
		f:close()
	end
	return set
end

-- dev("/dev/xxx") 是否落在系统占用的磁盘上
function M.dev_is_system(dev, sset)
	local name = type(dev) == "string" and dev:match("^/dev/(.+)$")
	if not name then return false end
	name = name:gsub("/", "!")
	if sset[name] then return true end
	local d = blk_parent(name)
	if M.dev_emmc(dev) then return true end
	return (d and sset[d]) and true or false
end

local function blkid_info(devs)
	local info = {}
	if #devs == 0 then return info end
	local tool = find_tool({ "blkid" })
	if not tool then return info end
	local argv = { tool }
	for _, d in ipairs(devs) do argv[#argv + 1] = d end
	local _, out = core.run(argv, 8)
	for l in out:gmatch("[^\n]+") do
		local dev = l:match("^(/dev/[^:]+):")
		if dev then
			local t = {}
			t.type = l:match('TYPE="([^"]*)"')
			t.label = l:match(' LABEL="([^"]*)"') or l:match(':%s*LABEL="([^"]*)"')
			t.uuid = l:match('UUID="([^"]*)"')
			info[dev] = t
		end
	end
	return info
end

local function disk_names()
	local out = {}
	for n in (fs.dir("/sys/block") or function() end) do
		for _, pat in ipairs(M.DISK_PATTERNS) do
			if n:match(pat) then out[#out + 1] = n; break end
		end
	end
	table.sort(out)
	return out
end

-- 枚举可作为「格式化」目标的块设备: 有分区的盘只列分区, 没分区的整盘才列整盘(不创建分区表)
function M.disks()
	local mounts = core.read_mounts()
	local sset = M.system_set(mounts)
	local list, devs = {}, {}
	for _, dn in ipairs(disk_names()) do
		local base = "/sys/block/" .. dn
		local sectors = tonumber(rd(base .. "/size")) or 0
		local model = ((rd(base .. "/device/vendor") or "") .. " " .. (rd(base .. "/device/model") or "")):gsub("%s+", " "):gsub("^ ", ""):gsub(" $", "")
		local kind = dn:match("^mmcblk") and "sd" or (dn:match("^nvme") and "nvme" or "usb")
		local removable = rd(base .. "/removable") == "1"
		local parts = {}
		for n in (fs.dir(base) or function() end) do
			if n:sub(1, #dn) == dn and fs.access(base .. "/" .. n .. "/partition") then parts[#parts + 1] = n end
		end
		table.sort(parts, function(a, b)
			local na, nb = tonumber(a:match("(%d+)$")) or 0, tonumber(b:match("(%d+)$")) or 0
			return na < nb
		end)
		local function add(name, sec, isdisk)
			local dev = "/dev/" .. name
			local mps = {}
			for _, m in ipairs(mounts) do if m.dev == dev then mps[#mps + 1] = m.mp end end
			list[#list + 1] = {
				dev = dev, name = name, disk = dn, whole = isdisk and true or false, size = sec * 512,
				model = select(1, core.utf8_clean(model)), kind = kind, removable = removable, mounts = mps,
				system = M.dev_is_system(dev, sset),
			}
			devs[#devs + 1] = dev
		end
		if #parts == 0 then
			if sectors > 0 then add(dn, sectors, true) end
		else
			for _, pn in ipairs(parts) do
				local sec = tonumber(rd(base .. "/" .. pn .. "/size")) or 0
				if sec >= 2048 then add(pn, sec, false) end   -- 小于 1 MiB 的是引导/保留分区, 不列出
			end
		end
	end
	local bi = blkid_info(devs)
	for _, d in ipairs(list) do
		local b = bi[d.dev]
		if b then
			d.fstype = b.type or ""
			d.label = b.label and (select(1, core.utf8_clean(b.label))) or ""
			d.uuid = b.uuid
		else
			d.fstype, d.label = "", ""
		end
		if d.system then
			d.reason = "此磁盘承载着系统(固件或 /overlay), 不能格式化"
		end
		for _, mp in ipairs(d.mounts) do
			if INTERNAL_MP[mp] then d.system = true; d.reason = "这是系统挂载点, 不能格式化" end
		end
	end
	return list
end

function M.format_info()
	return { ok = true, disks = M.disks(), fs = M.fs_options() }
end

local function find_fs(id)
	for _, f in ipairs(M.FS) do if f.id == id then return f end end
end

local function clean_label(f, label)
	label = tostring(label or "")
	if label:find("%c") or label:find('[\\"/]') then return nil, "卷标含有不允许的字符" end
	if f.strict then
		label = label:upper()
		if not label:match("^[A-Z0-9_ %-]*$") then return nil, "FAT32 卷标只能包含英文字母、数字、空格、下划线和减号" end
	end
	if #label > f.maxlabel then return nil, string.format("卷标过长(%s 最多 %d 个字节, 中文占 3 个)", f.name, f.maxlabel) end
	return label
end

local function running_format()
	for n in (fs.dir(core.JOBS) or function() end) do
		if n:match("%.st$") then
			local f = io.open(core.JOBS .. "/" .. n, "r")
			if f then
				local s = f:read("*a")
				f:close()
				if s:find("op=mkfs", 1, true) and s:find("state=running", 1, true) then
					local pid = s:match("pid=(%d+)")
					if pid and fs.access("/proc/" .. pid) then return true end
				end
			end
		end
	end
	return false
end

-- p: { dev, fs, label, confirm }  密码走请求头
function M.format(p)
	local pw = req_pw()
	local fo = find_fs(p.fs)
	if not fo then return core.err("bad_fs", "不支持的文件系统") end
	local want
	for _, d in ipairs(M.disks()) do
		if d.dev == p.dev then want = d break end
	end
	if not want then return core.err("bad_dev", "没有找到这个存储设备(可能已被拔出)") end
	if want.system then return core.err("system_disk", want.reason or "系统盘不能格式化") end
	if p.confirm ~= want.name then return core.err("bad_confirm", "确认文字不匹配: 请输入设备名「" .. want.name .. "」") end
	local opt
	for _, o in ipairs(M.fs_options()) do if o.id == fo.id then opt = o end end
	if not opt or not opt.ok then return core.err("no_dep", "无法格式化为 " .. fo.name .. ": " .. ((opt and opt.why) or "缺少工具")) end
	local label, lerr = clean_label(fo, p.label)
	if not label then return core.err("bad_label", lerr) end
	local ok, e = M.verify_pw(pw)
	if not ok then return e end
	if running_format() then return core.err("busy", "已有一个格式化任务正在进行, 请等它结束") end

	-- 先卸载(只卸这个设备自己的挂载点); 卸不掉说明正被占用, 不强制
	for _, mp in ipairs(want.mounts) do
		local code = core.run({ find_tool({ "umount" }) or "/bin/umount", mp }, 20)
		if code ~= 0 then return core.err("busy", "卸载失败: 存储正被使用(有传输任务或打开的文件), 请先结束后再试") end
	end
	-- 卸载后重新确认一遍
	for _, m in ipairs(core.read_mounts()) do
		if m.dev == p.dev then return core.err("busy", "设备仍处于挂载状态, 已取消") end
	end
	-- 清掉旧挂载点留下的空目录(只清空目录, 不递归)
	for _, mp in ipairs(want.mounts) do fs.rmdir(mp) end

	local res = core.job_and_wait("mkfs", { fo.id, p.dev, label, fo.kfs }, 3)
	res.dev = p.dev
	return res
end

return M
