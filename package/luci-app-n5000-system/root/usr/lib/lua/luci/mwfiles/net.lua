-- Mwrt 文件管理 · 网络位置: SMB/CIFS 挂载 / 卸载 / 诊断 / "共享此文件夹"(写现有 ksmbd 或 samba4 配置)
-- 凭据只写 0600 文件传给 mount.cifs(credentials=), 绝不出现在命令行、日志、状态接口和错误信息里。
-- 所有外部命令都是 argv 形式, 不经 shell。
local nixio = require "nixio"
local fs = require "nixio.fs"
local core = require "luci.mwfiles.core"

local M = {}

local MOUNT_CIFS = { "/sbin/mount.cifs", "/usr/sbin/mount.cifs", "/bin/mount.cifs", "/usr/bin/mount.cifs" }
local VERS = { [""] = true, ["3.1.1"] = true, ["3.0"] = true, ["2.1"] = true, ["2.0"] = true, ["1.0"] = true }

local function uci() return require("uci").cursor() end

local function find_mount_cifs()
	for _, p in ipairs(MOUNT_CIFS) do if fs.access(p, "x") then return p end end
end

local function kernel_has_cifs()
	local f = io.open("/proc/filesystems", "r")
	if not f then return false end
	local s = f:read("*a")
	f:close()
	return s:find("%scifs\n") ~= nil
end

-- 能力探测: 给界面决定显示哪些入口, 并给出缺什么
function M.caps()
	local c = uci()
	local cifs_bin = find_mount_cifs()
	local ks = fs.access("/etc/config/ksmbd") and fs.access("/usr/sbin/ksmbd.mountd")
	local sm = fs.access("/etc/config/samba4") and (fs.access("/usr/sbin/smbd") or fs.access("/usr/bin/smbd"))
	local share_srv = ks and "ksmbd" or (sm and "samba4" or nil)
	local need = {}
	if not kernel_has_cifs() then need[#need + 1] = "kmod-fs-cifs" end
	if not cifs_bin then need[#need + 1] = "cifsmount" end
	return {
		mount = (#need == 0),
		mount_missing = need,
		kernel_cifs = kernel_has_cifs(),
		mount_cifs = cifs_bin and true or false,
		share = share_srv ~= nil,
		share_server = share_srv,
		share_missing = (share_srv == nil) and { "ksmbd-server 或 samba4-server" } or {},
	}
end

-- ------------------------------------------------------------ 输入校验
local function nocontrol(s) return not s:find("[%c]") end

local function valid_host(h)
	if type(h) ~= "string" or #h == 0 or #h > 253 then return false end
	if h:find("^[%w%.%-]+$") and not h:find("^%-") and not h:find("%.%.") then return true end
	return false
end

local function valid_share(s)
	if type(s) ~= "string" or #s == 0 or #s > 80 then return false end
	if not nocontrol(s) then return false end
	if s:find('[/\\:%*%?"<>|,]') then return false end
	if s == "." or s == ".." then return false end
	return true
end

local function valid_cred_field(s, maxlen)
	if s == nil or s == "" then return true end
	return type(s) == "string" and #s <= maxlen and nocontrol(s)
end

-- ------------------------------------------------------------ 已保存的共享(不含用户名/密码)
local function saved_list()
	local out = {}
	uci():foreach("mwfiles", "smb", function(s)
		out[#out + 1] = {
			id = s[".name"], name = s.name or s.share, host = s.host, share = s.share, vers = s.vers or "",
			auto = s.auto == "1", remember = s.remember == "1", guest = s.guest == "1",
		}
	end)
	return out
end

local function mp_of(id) return core.SMB_BASE .. "/" .. id end

local function is_mounted(mp)
	for _, m in ipairs(core.read_mounts()) do
		if m.mp == mp then return m end
	end
end

function M.list()
	local out = {}
	for _, s in ipairs(saved_list()) do
		local m = is_mounted(mp_of(s.id))
		s.mounted = m ~= nil
		s.root = m and fs.realpath(mp_of(s.id)) or nil   -- 已挂载时给出授权根 id, 前端据此直接进入
		out[#out + 1] = s
	end
	return { ok = true, items = out, caps = M.caps() }
end

-- ------------------------------------------------------------ 凭据文件
local function write_cred(path, user, pass, domain)
	fs.mkdirr(path:match("^(.*)/[^/]+$"))
	fs.chmod(path:match("^(.*)/[^/]+$"), "700")
	local fd = nixio.open(path, nixio.open_flags("wronly", "creat", "trunc"), "600")
	if not fd then return false end
	fs.chmod(path, "600")   -- nixio 文件对象没有 chmod 方法
	local body = "username=" .. user .. "\npassword=" .. (pass or "") .. "\n"
	if domain and domain ~= "" then body = body .. "domain=" .. domain .. "\n" end
	local ok = fd:write(body) == #body
	fd:close()
	return ok
end

-- mount.cifs 失败原因 -> 人话(不回显原始输出, 防止泄露)
local function explain_mount(code, err)
	local e = (err or ""):lower()
	local n = tonumber(e:match("mount error%((%d+)%)"))
	if code == 124 then return "timeout", "连接超时, 服务器没有响应" end
	if n == 13 or e:find("permission denied") or e:find("logon failure") then return "auth", "用户名或密码错误, 或该账户无权访问此共享" end
	if n == 113 or n == 101 then return "unreach", "无法到达服务器, 请检查地址和网络" end
	if n == 111 then return "refused", "服务器拒绝连接(没有开启 SMB 服务, 或 445 端口被防火墙拦截)" end
	if n == 110 then return "timeout", "连接超时, 服务器没有响应" end
	if n == 2 or n == 6 or e:find("no such file") then return "noshare", "找不到这个共享名。共享名是对方共享出来的名字(不是文件夹名或电脑名), 例如 D、share; 在 Windows 资源管理器地址栏输入 \\\\对方地址 回车, 里面列出的文件夹名就是共享名" end
	if n == 112 or n == 95 or e:find("protocol") then return "proto", "SMB 协议版本不匹配, 请在高级选项里换一个协议版本" end
	if n == 22 then return "badopt", "挂载参数被内核拒绝(协议版本或选项不支持)" end
	if n == 115 then return "unreach", "服务器暂不可用, 请稍后再试" end
	return "failed", "挂载失败(错误码 " .. tostring(n or code) .. ")"
end

-- ------------------------------------------------------------ 挂载 / 卸载
-- p: { host, share, user, pass, domain, vers, name, remember, auto, guest, ro }
function M.mount(p)
	local caps = M.caps()
	if not caps.mount then
		return core.err("no_dep", "缺少 SMB 挂载组件: " .. table.concat(caps.mount_missing, "、") .. "。请在软件包里安装后重试")
	end
	if not valid_host(p.host) then return core.err("bad_host", "服务器地址不合法(只支持主机名或 IPv4 地址)") end
	if not valid_share(p.share) then return core.err("bad_share", "共享名不合法") end
	if not VERS[p.vers or ""] then return core.err("bad_vers", "协议版本不合法") end
	local guest = p.guest and true or false
	if not guest then
		if not p.user or p.user == "" then return core.err("bad_user", "请填写用户名, 或勾选「匿名访问」") end
		if not valid_cred_field(p.user, 64) then return core.err("bad_user", "用户名不合法") end
		if not valid_cred_field(p.pass, 128) then return core.err("bad_pass", "密码不合法(含控制字符或过长)") end
		if not valid_cred_field(p.domain, 64) or (p.domain and p.domain:find("[,=]")) then return core.err("bad_domain", "域名不合法") end
	end
	local label = p.name and p.name ~= "" and p.name or (p.host .. " / " .. p.share)
	if #label > 60 or not nocontrol(label) then return core.err("bad_name", "显示名称不合法") end

	-- 同一个 host+share 已挂着就直接报
	for _, s in ipairs(saved_list()) do
		if s.host == p.host and s.share == p.share and is_mounted(mp_of(s.id)) then
			return core.err("exists", "该共享已经挂载")
		end
	end

	local id = core.rnd(4)
	local mp = mp_of(id)
	fs.mkdirr(mp)
	if not fs.stat(mp) then return core.err("failed", "无法创建挂载点") end

	local cred = core.RUN .. "/cred/" .. id
	local opts = { "iocharset=utf8", "uid=0", "gid=0", "file_mode=0664", "dir_mode=0775", "nosuid", "nodev", "noexec", "soft" }
	if p.vers and p.vers ~= "" then opts[#opts + 1] = "vers=" .. p.vers end
	if p.ro then opts[#opts + 1] = "ro" end
	if guest then
		opts[#opts + 1] = "guest"
	else
		if not write_cred(cred, p.user, p.pass or "", p.domain) then
			fs.rmdir(mp)
			return core.err("failed", "无法写入临时凭据文件")
		end
		opts[#opts + 1] = "credentials=" .. cred
	end

	local code, _, errout = core.run({ find_mount_cifs(), "//" .. p.host .. "/" .. p.share, mp, "-o", table.concat(opts, ",") }, 30)
	fs.unlink(cred)             -- 用完即删
	if code ~= 0 then
		fs.rmdir(mp)
		local k, msg = explain_mount(code, errout)
		return core.err("mount_" .. k, msg)
	end

	-- 记录(不含用户名/密码)。remember=1 时才把凭据写进 0600 文件用于开机自动挂载
	local c = uci()
	-- 原生 uci cursor 没有 section(), 用 set 建节再逐项写
	c:set("mwfiles", id, "smb")
	for k, v in pairs({
		name = label, host = p.host, share = p.share, vers = p.vers or "",
		auto = (p.auto and not guest) and "1" or "0", remember = (p.remember and not guest) and "1" or "0", guest = guest and "1" or "0",
	}) do
		if v ~= "" then c:set("mwfiles", id, k, v) end
	end
	c:save("mwfiles")
	c:commit("mwfiles")
	if p.remember and not guest then
		write_cred(core.CRED_DIR .. "/" .. id, p.user, p.pass or "", p.domain)
	end
	return { ok = true, id = id, root = mp, label = label }
end

function M.umount(id, lazy)
	if type(id) ~= "string" or not id:match("^%x+$") then return core.err("bad_id", "参数不合法") end
	local mp = mp_of(id)
	local m = is_mounted(mp)
	if not m then return { ok = true, already = true } end
	if m.fstype ~= "cifs" and m.fstype ~= "smb3" then return core.err("not_ours", "这不是文件管理挂载的共享") end
	local argv = lazy and { "/bin/umount", "-l", mp } or { "/bin/umount", mp }
	local code = core.run(argv, 15)
	if code ~= 0 then
		return core.err("busy", "卸载失败: 共享正被使用(有传输任务或打开的文件); 可稍后重试, 或选择「强制卸载」")
	end
	fs.rmdir(mp)
	return { ok = true }
end

-- 忘记一个共享: 先卸载, 再删记录和凭据
function M.forget(id)
	if type(id) ~= "string" or not id:match("^%x+$") then return core.err("bad_id", "参数不合法") end
	local r = M.umount(id, true)
	if not r.ok then return r end
	local c = uci()
	c:delete("mwfiles", id)
	c:save("mwfiles")
	c:commit("mwfiles")
	fs.unlink(core.CRED_DIR .. "/" .. id)
	return { ok = true }
end

-- 用"记住"的凭据文件(或匿名)挂载一条已保存的共享; 返回 (code, errout)
local function mount_saved(s)
	local mp = mp_of(s.id)
	fs.mkdirr(mp)
	local opts = { "iocharset=utf8", "uid=0", "gid=0", "file_mode=0664", "dir_mode=0775", "nosuid", "nodev", "noexec", "soft" }
	if s.vers ~= "" then opts[#opts + 1] = "vers=" .. s.vers end
	if s.guest then
		opts[#opts + 1] = "guest"
	else
		opts[#opts + 1] = "credentials=" .. core.CRED_DIR .. "/" .. s.id
	end
	local code, _, errout = core.run({ find_mount_cifs(), "//" .. s.host .. "/" .. s.share, mp, "-o", table.concat(opts, ",") }, 30)
	if code ~= 0 then fs.rmdir(mp) end
	return code, errout
end

-- 开机自动挂载(init 脚本调用): 只处理 remember=1 且 auto=1 的记录
function M.automount()
	if not M.caps().mount then return 0 end
	local pending = 0
	for _, s in ipairs(saved_list()) do
		if s.auto and s.remember and not is_mounted(mp_of(s.id)) and fs.access(core.CRED_DIR .. "/" .. s.id) then
			if mount_saved(s) ~= 0 then pending = pending + 1 end
		end
	end
	return pending
end

-- 手动重新连接一条已保存的共享(只有选了"记住"或匿名的才有可用凭据)
function M.remount(id)
	if type(id) ~= "string" or not id:match("^%x+$") then return core.err("bad_id", "参数不合法") end
	local caps = M.caps()
	if not caps.mount then
		return core.err("no_dep", "缺少 SMB 挂载组件: " .. table.concat(caps.mount_missing, "、") .. "。请在软件包里安装后重试")
	end
	for _, s in ipairs(saved_list()) do
		if s.id == id then
			if is_mounted(mp_of(id)) then return { ok = true, already = true } end
			if not s.guest and not fs.access(core.CRED_DIR .. "/" .. id) then
				return core.err("no_cred", "这个共享没有保存凭据, 请重新输入用户名和密码连接")
			end
			local code, errout = mount_saved(s)
			if code ~= 0 then
				local k, msg = explain_mount(code, errout)
				return core.err("mount_" .. k, msg)
			end
			return { ok = true, id = id }
		end
	end
	return core.err("not_found", "没有这条已保存的共享")
end

-- ------------------------------------------------------------ 诊断
local function tcp_probe(host, port, timeout_ms)
	local t0 = os.clock()
	local ai = nixio.getaddrinfo(host, "inet", "stream", tostring(port))
	if not ai or not ai[1] then return false, "dns" end
	local sock = nixio.socket("inet", "stream")
	if not sock then return false, "socket" end
	sock:setblocking(false)
	local ok, errno = sock:connect(ai[1].address, port)
	if ok then sock:close() return true, nil, math.floor((os.clock() - t0) * 1000) end
	local pr = nixio.poll({ { fd = sock, events = nixio.poll_flags("out") } }, timeout_ms)
	local result, why = false, "timeout"
	if pr and pr > 0 then
		local e = sock:getsockopt("socket", "error")
		if e == 0 then result, why = true, nil
		elseif e == 111 then why = "refused"
		else why = "unreach" end
	end
	sock:close()
	return result, why
end

function M.diag(p)
	local caps = M.caps()
	local items = {}
	local function add(ok, title, hint) items[#items + 1] = { ok = ok, t = title, h = hint } end
	add(caps.kernel_cifs, "内核支持 CIFS", (not caps.kernel_cifs) and "缺少内核模块 kmod-fs-cifs, 无法挂载 SMB 共享" or nil)
	add(caps.mount_cifs, "挂载工具 mount.cifs", (not caps.mount_cifs) and "缺少软件包 cifsmount" or nil)
	if p and p.host and valid_host(p.host) then
		local ok, why = tcp_probe(p.host, 445, 4000)
		local hint
		if not ok then
			if why == "dns" then hint = "无法解析主机名, 请检查地址或 DNS"
			elseif why == "refused" then hint = "对方拒绝连接: 没有启动 SMB 服务, 或被防火墙拦截 445 端口"
			elseif why == "timeout" then hint = "连接超时: 地址不通、不在同一网络或被防火墙丢弃"
			else hint = "网络不可达" end
		end
		add(ok, "服务器 " .. p.host .. " 的 445 端口", hint)
	end
	local ms = {}
	for _, s in ipairs(saved_list()) do
		local m = is_mounted(mp_of(s.id))
		if m then
			local code = core.run({ "/bin/stat", "-f", "-c", "%b", mp_of(s.id) }, 3)
			ms[#ms + 1] = { name = s.name, ok = code == 0 }
		end
	end
	return { ok = true, items = items, mounts = ms, caps = caps }
end

-- ------------------------------------------------------------ 共享此文件夹(写现有 SMB 服务的配置)
local function sections(cfg, stype)
	local out = {}
	uci():foreach(cfg, stype, function(s) out[#out + 1] = s end)
	return out
end

-- 某个路径是否正被共享: 返回 { name=, server= } (path 或其上级目录被共享都算)
function M.share_for(path)
	if not path then return nil end
	for _, srv in ipairs({ { "ksmbd", "share" }, { "samba4", "sambashare" } }) do
		if fs.access("/etc/config/" .. srv[1]) then
			for _, s in ipairs(sections(srv[1], srv[2])) do
				if s.path and s.path ~= "" and core.has_prefix(path, s.path) then
					return { name = s.name, server = srv[1], exact = (s.path == path), ro = (s.read_only == "yes" or s.read_only == "1") }
				end
			end
		end
	end
	return nil
end

local function valid_sharename(n)
	return type(n) == "string" and #n > 0 and #n <= 80 and nocontrol(n) and not n:find('[/\\:%*%?"<>|,=]')
end

local function valid_user(u)
	return type(u) == "string" and #u > 0 and #u <= 32 and u:match("^[%w_%.%-]+$") ~= nil
end

local function yes(v) return v == "yes" or v == "1" or v == "true" end

local function find_tool(names)
	for _, n in ipairs(names) do
		for _, d in ipairs({ "/bin/", "/usr/bin/", "/sbin/", "/usr/sbin/" }) do
			if fs.access(d .. n, "x") then return d .. n end
		end
	end
end

local SRV = {
	ksmbd = { stype = "share", daemon = "ksmbd.mountd" },
	samba4 = { stype = "sambashare", daemon = "smbd" },
}

local function reload_srv(srv)
	core.run({ "/etc/init.d/" .. srv, "reload" }, 20)
end

-- 服务状态: 是否在运行 / 是否开机自启(只读事实, 不猜)
local function srv_state(srv)
	local pidof = find_tool({ "pidof" })
	local running = false
	if pidof then running = core.run({ pidof, SRV[srv].daemon }, 3) == 0 end
	local enabled = fs.access("/etc/init.d/" .. srv, "x") and core.run({ "/etc/init.d/" .. srv, "enabled" }, 5) == 0 or false
	return { running = running, enabled = enabled }
end

-- ksmbd 的账户库(只取用户名, 绝不读出口令散列)
local function smb_accounts(srv)
	local out = {}
	if srv == "ksmbd" then
		local f = io.open("/etc/ksmbd/ksmbdpwd.db", "r")
		if f then
			for l in f:lines() do
				local u = l:match("^([^:]+):")
				if u and valid_user(u) then out[#out + 1] = u end
			end
			f:close()
		end
	end
	return out
end

function M.share_list()
	local out = {}
	local caps = M.caps()
	local state
	for _, srv in ipairs({ "ksmbd", "samba4" }) do
		if fs.access("/etc/config/" .. srv) then
			if srv == caps.share_server then state = srv_state(srv) end
			for _, s in ipairs(sections(srv, SRV[srv].stype)) do
				local users = {}
				for u in (s.users or s.valid_users or ""):gmatch("[^%s,]+") do users[#users + 1] = u end
				out[#out + 1] = {
					id = s[".name"], server = srv, name = s.name, path = s.path, ro = yes(s.read_only),
					guest = yes(s.guest_ok), users = users, comment = s.comment and (select(1, core.utf8_clean(s.comment))) or nil,
					ours = s.mwfiles == "1", exists = s.path and fs.stat(s.path) ~= nil or false,
				}
			end
		end
	end
	return {
		ok = true, items = out, caps = caps, state = state,
		accounts = caps.share_server and smb_accounts(caps.share_server) or {},
	}
end

local function no_share_dep()
	return core.err("no_dep", "固件未安装 SMB 服务端(ksmbd-server 或 samba4-server), 无法共享文件夹。请先在软件包里安装其中一个")
end

local function clean_users(users)
	local list, seen = {}, {}
	if type(users) == "table" then
		for _, u in ipairs(users) do
			if not valid_user(u) then return nil, "用户名「" .. tostring(u):sub(1, 20) .. "」不合法" end
			if not seen[u] then seen[u] = true; list[#list + 1] = u end
		end
	end
	return list
end

-- s297e 新建/修改共享时顺带创建(或重置密码)SMB 账户, 只支持 ksmbd。口令通过管道写进 ksmbd.adduser 的标准输入,
-- 不出现在命令行、日志、uci 里; 密码库(/etc/ksmbd/ksmbdpwd.db)由 ksmbd 自己存散列。
local ADDUSER = "/usr/sbin/ksmbd.adduser"

local function valid_pass(p) return type(p) == "string" and #p >= 1 and #p <= 128 and not p:find("%c") end

local function run_stdin(argv, input, timeout)
	local pr, pw = nixio.pipe()
	if not pr then return -1 end
	local pid = nixio.fork()
	if not pid then pr:close(); pw:close(); return -1 end
	if pid == 0 then
		pw:close()
		nixio.dup(pr, nixio.stdin)
		local nul = nixio.open("/dev/null", "w")
		if nul then nixio.dup(nul, nixio.stdout); nixio.dup(nul, nixio.stderr) end
		local a = { "/usr/bin/timeout", tostring(timeout or 15) }
		for _, v in ipairs(argv) do a[#a + 1] = v end
		nixio.exec((unpack or table.unpack)(a))
		nixio._exit(127)
	end
	pr:close()
	pw:write(input)
	pw:close()
	local _, _, code = nixio.waitpid(pid)
	return code or -1
end

local function smb_set_account(user, pass)
	if not fs.access(ADDUSER, "x") then return false end
	if run_stdin({ ADDUSER, user }, pass .. "\n" .. pass .. "\n", 15) ~= 0 then return false end
	for _, u in ipairs(smb_accounts("ksmbd")) do if u == user then return true end end
	return false
end

-- 校验账户相关入参: 返回 users(已并入新账户), 错误, 以及「真正去建账户」的函数(其余校验全过之后才调用)
local function plan_account(p, srv, users)
	local nu = p.newuser
	if p.guest or nu == nil or nu == "" then
		if not p.guest and #users == 0 and srv == "ksmbd" and #smb_accounts(srv) == 0 then
			return nil, core.err("no_account", "还没有任何 SMB 账户, 电脑将无法登录。请填写用户名和密码创建一个账户, 或勾选「允许匿名访问」")
		end
		return users
	end
	if srv ~= "ksmbd" then return nil, core.err("no_account_dep", "当前 SMB 服务端不是 ksmbd, 不能在这里创建账户, 请到「SMB 服务」页面创建") end
	if not valid_user(nu) then return nil, core.err("bad_user", "用户名只能含字母、数字、下划线、点和短横线, 最长 32 个字符") end
	if not valid_pass(p.newpass) then return nil, core.err("bad_pass", "密码不合法(不能为空、不能含控制字符、最长 128 字节)") end
	local seen
	for _, u in ipairs(users) do if u == nu then seen = true end end
	if not seen then users[#users + 1] = nu end
	return users, nil, function() return smb_set_account(nu, p.newpass) end
end

-- p: { root, path, name, ro, guest, users(table), newuser, newpass, comment }  创建共享(newuser 非空时顺带建账户)
function M.share_add(p)
	local caps = M.caps()
	if not caps.share then return no_share_dep() end
	if not valid_sharename(p.name) then return core.err("bad_name", "共享名不合法(不能含 / \\ : * ? \" < > | , =)") end
	local users, uerr = clean_users(p.users)
	if not users then return core.err("bad_user", uerr) end
	local comment = p.comment and tostring(p.comment) or ""
	if #comment > 120 or comment:find("%c") then return core.err("bad_comment", "说明不合法(过长或含控制字符)") end
	local r, err = core.resolve(p.root, p.path, true)
	if not r then return err end
	local st = fs.stat(r.real)
	if not st or st.type ~= "dir" then return core.err("not_dir", "只能共享文件夹") end
	if r.root.kind == "remote" then return core.err("remote", "不能把网络共享再次共享出去") end
	if r.real == "/" or r.real == "/etc" or r.real == "/root" then return core.err("deny_share", "不能共享整个系统或系统配置目录") end
	if not p.guest and #users == 0 then
		-- 不勾匿名又不指定账户: 由服务端的账户库决定, 允许, 但界面会提示
	end
	local gok, ge = core.gcheck("share", { r.real })
	if not gok then return ge end
	local srv = caps.share_server
	local c = uci()
	local exists
	c:foreach(srv, SRV[srv].stype, function(s) if s.name == p.name then exists = true end end)
	if exists then return core.err("exists", "已存在同名共享") end
	local users2, aerr, mkacct = plan_account(p, srv, users)
	if not users2 then return aerr end
	users = users2
	if mkacct and not mkacct() then return core.err("account_failed", "创建 SMB 账户失败(ksmbd.adduser 执行出错), 共享没有创建") end
	local sid = c:add(srv, SRV[srv].stype)
	local opt = { name = p.name, path = r.real, read_only = p.ro and "yes" or "no", guest_ok = p.guest and "yes" or "no",
		create_mask = "0666", dir_mask = "0777", mwfiles = "1" }
	if #users > 0 then opt.users = table.concat(users, " ") end
	if comment ~= "" then opt.comment = comment end
	for k, v in pairs(opt) do c:set(srv, sid, k, v) end
	c:save(srv)
	c:commit(srv)
	reload_srv(srv)
	return { ok = true, server = srv, name = p.name, id = sid }
end

local function our_section(id)
	if type(id) ~= "string" or not id:match("^[%w_%-]+$") then return nil, core.err("bad_id", "参数不合法") end
	for _, srv in ipairs({ "ksmbd", "samba4" }) do
		if fs.access("/etc/config/" .. srv) then
			local c = uci()
			local s = c:get_all(srv, id)
			if s and s[".type"] == SRV[srv].stype then
				if s.mwfiles ~= "1" then return nil, core.err("not_ours", "该共享不是由文件管理创建的, 请到「SMB 服务」页面修改") end
				return srv, nil, c, s
			end
		end
	end
	return nil, core.err("not_found", "共享不存在")
end

-- 修改(只允许改"文件管理创建的"共享; 路径不可改, 要换位置请删除重建)
-- p: { id, name, ro, guest, users, newuser, newpass, comment }
function M.share_edit(p)
	local srv, err, c, s = our_section(p.id)
	if not srv then return err end
	if not valid_sharename(p.name) then return core.err("bad_name", "共享名不合法") end
	local users, uerr = clean_users(p.users)
	if not users then return core.err("bad_user", uerr) end
	local comment = p.comment and tostring(p.comment) or ""
	if #comment > 120 or comment:find("%c") then return core.err("bad_comment", "说明不合法(过长或含控制字符)") end
	local dup
	c:foreach(srv, SRV[srv].stype, function(x) if x.name == p.name and x[".name"] ~= p.id then dup = true end end)
	if dup then return core.err("exists", "已存在同名共享") end
	local gok, ge = core.gcheck("share", { s.path })
	if not gok then return ge end
	local users2, aerr, mkacct = plan_account(p, srv, users)
	if not users2 then return aerr end
	users = users2
	if mkacct and not mkacct() then return core.err("account_failed", "创建 SMB 账户失败(ksmbd.adduser 执行出错), 共享没有修改") end
	c:set(srv, p.id, "name", p.name)
	c:set(srv, p.id, "read_only", p.ro and "yes" or "no")
	c:set(srv, p.id, "guest_ok", p.guest and "yes" or "no")
	if #users > 0 then c:set(srv, p.id, "users", table.concat(users, " ")) else c:delete(srv, p.id, "users") end
	if comment ~= "" then c:set(srv, p.id, "comment", comment) else c:delete(srv, p.id, "comment") end
	c:save(srv)
	c:commit(srv)
	reload_srv(srv)
	return { ok = true }
end

-- 只允许取消"文件管理创建的"共享, 不动用户自己写的
function M.share_remove(id)
	local srv, err, c = our_section(id)
	if not srv then return err end
	c:delete(srv, id)
	c:save(srv)
	c:commit(srv)
	reload_srv(srv)
	return { ok = true }
end

-- 服务控制: 白名单动作
function M.share_service(action)
	local caps = M.caps()
	local srv = caps.share_server
	if not srv then return no_share_dep() end
	local ok = { start = 1, stop = 1, restart = 1, enable = 1, disable = 1 }
	if not ok[action] then return core.err("bad_op", "未知操作") end
	local code = core.run({ "/etc/init.d/" .. srv, action }, 30)
	local st = srv_state(srv)
	if code ~= 0 and (action == "start" or action == "restart") and not st.running then
		return core.err("failed", "服务没能启动, 请到「系统 → 启动项」或日志里查看原因", { state = st })
	end
	return { ok = true, state = st }
end

return M
