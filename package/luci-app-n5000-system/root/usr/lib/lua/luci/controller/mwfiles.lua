module("luci.controller.mwfiles", package.seeall)

-- Mwrt 文件管理: 菜单 / 路由 / 权限
--   读取类接口: GET(会话 cookie 即可, 下载/预览链接要用)
--   写入类接口: post() 条目, 由 dispatcher 校验 token + POST
--   上传: 原始字节流 POST, 需手动校验 token(post() 条目会先把请求体当表单解析, 大文件会被 100KB 上限拦住)
-- 权限: luci-app-mwfiles(浏览/传输/改动授权目录内的文件), luci-app-mwfiles-net(SMB 挂载与共享配置),
--       luci-app-mwfiles-disk(格式化外接存储)
-- 入口: 不占菜单栏, 由首页「存储占用」卡片的「文件管理」按钮 / USB 存储通知打开(admin/mwfiles 页面仍可直接访问, 只是不进菜单)

local fs = require "nixio.fs"
local jsonc = require "luci.jsonc"

local ACL, ACL_NET, ACL_DISK = "luci-app-mwfiles", "luci-app-mwfiles-net", "luci-app-mwfiles-disk"

function index()
	if not fs.access("/etc/config/mwfiles") then return end

	-- 不给标题 = 不进菜单; 路由与权限照旧
	local page = entry({ "admin", "mwfiles" }, template("mwfiles/page"))
	page.acl_depends = { ACL }
	local menu = entry({ "admin", "system", "myos_nas" }, alias("admin", "mwfiles"), _("NAS 文件管理"), 73)
	menu.acl_depends = { ACL }

	local function api(fn, kind, acl)
		local target = (kind == "post") and post("api_" .. fn) or call("api_" .. fn)
		local e = entry({ "admin", "services", "mwfiles", fn }, target)
		e.leaf = true
		e.acl_depends = { acl or ACL }
	end

	for _, fn in ipairs({ "roots", "list", "search", "stat", "dirsize", "read", "dl", "upcheck", "job", "caps", "guardinfo" }) do
		api(fn, "get")
	end
	for _, fn in ipairs({ "mkdir", "newfile", "rename", "delete", "copy", "move", "compress", "extract", "jobcancel", "settings", "checkpw" }) do
		api(fn, "post")
	end
	-- 上传: 原始字节流
	api("upload", "get")
	-- 网络位置
	for _, fn in ipairs({ "netlist", "netdiag", "sharelist" }) do api(fn, "get", ACL_NET) end
	for _, fn in ipairs({ "smbmount", "smbremount", "smbumount", "smbforget", "shareadd", "shareedit", "shareremove", "sharesvc" }) do api(fn, "post", ACL_NET) end
	-- 格式化(只针对用户手动选中的外接存储)
	api("formatinfo", "get", ACL_DISK)
	api("format", "post", ACL_DISK)
end

-- ------------------------------------------------------------ 通用
local function core() return require "luci.mwfiles.core" end

local function out(t)
	luci.http.prepare_content("application/json")
	luci.http.header("X-Content-Type-Options", "nosniff")
	luci.http.header("Cache-Control", "no-store")
	luci.http.write(jsonc.stringify(t))
end

local function fv(k) return luci.http.formvalue(k) end

local function jfv(k)
	local s = fv(k)
	if not s or s == "" then return nil end
	return jsonc.parse(s)
end

local function flag(k) local v = fv(k); return v == "1" or v == "true" end

-- 极简 query 解析(上传时不能碰 formvalue: 一碰就会触发请求体解析)
local function parse_qs()
	local q = {}
	local s = luci.http.getenv("QUERY_STRING") or ""
	for pair in s:gmatch("[^&]+") do
		local k, v = pair:match("^([^=]*)=?(.*)$")
		if k then
			local function dec(x)
				x = x:gsub("%+", " "):gsub("%%(%x%x)", function(h) return string.char(tonumber(h, 16)) end)
				return x
			end
			q[dec(k)] = dec(v)
		end
	end
	return q
end

-- ------------------------------------------------------------ 读取
function api_roots()
	local c = core()
	local st = c.settings()
	local roots = c.roots(true)
	local net = require("luci.mwfiles.net")
	out({
		ok = true, roots = roots, hidden = st.hidden, max_upload_mb = st.max_upload_mb,
		archive = c.caps_archive(), net = net.caps(), authorized = st.roots,
	})
end

function api_caps()
	local c = core()
	out({ ok = true, archive = c.caps_archive(), net = require("luci.mwfiles.net").caps() })
end

function api_list()
	local st = core().settings()
	out(core().list(fv("root"), fv("path"), {
		sort = fv("sort"), dir = fv("dir"), type = fv("type"), q = fv("q"), offset = fv("offset"), limit = fv("limit"),
		hidden = st.hidden or flag("hidden"),
	}))
end

function api_search()
	local st = core().settings()
	out(core().search(fv("root"), fv("path"), fv("q"), { type = fv("type"), hidden = st.hidden or flag("hidden") }))
end

function api_stat() out(core().stat(fv("root"), fv("path"))) end
function api_dirsize() out(core().dirsize(fv("root"), fv("path"))) end
function api_read() out(require("luci.mwfiles.xfer").read(fv("root"), fv("path"), fv("offset"), fv("limit"))) end
function api_dl() require("luci.mwfiles.xfer").serve(fv("root"), fv("path"), fv("mode")) end
function api_job() out(core().job_status(fv("id"))) end

function api_upcheck()
	out(require("luci.mwfiles.xfer").upcheck(fv("root"), fv("path"), fv("name"), fv("size")))
end

-- ------------------------------------------------------------ 上传(原始字节流)
function api_upload()
	local http = luci.http
	if http.getenv("REQUEST_METHOD") ~= "POST" then
		http.status(405, "Method Not Allowed")
		return out(core().err("method", "只接受 POST"))
	end
	local q = parse_qs()
	local ctx = require("luci.dispatcher").context
	if not q.token or not ctx.authtoken or q.token ~= ctx.authtoken then
		http.status(403, "Forbidden")
		return out(core().err("token", "令牌无效, 请刷新页面后重试"))
	end
	out(require("luci.mwfiles.xfer").upload(q))
end

-- ------------------------------------------------------------ 写入
function api_mkdir() out(core().mkdir(fv("root"), fv("path"), fv("name"))) end
function api_newfile() out(core().newfile(fv("root"), fv("path"), fv("name"))) end
function api_rename() out(core().rename(fv("root"), fv("path"), fv("name"))) end
function api_delete() out(core().delete(fv("root"), jfv("paths"))) end

function api_copy()
	out(core().transfer("cp", fv("root"), jfv("paths"), fv("droot"), fv("dpath"), fv("conflict")))
end

function api_move()
	out(core().transfer("mv", fv("root"), jfv("paths"), fv("droot"), fv("dpath"), fv("conflict")))
end

function api_compress() out(core().compress(fv("root"), jfv("paths"), fv("dpath"), fv("name"), fv("fmt"))) end
function api_extract() out(core().extract(fv("root"), fv("path"), fv("dpath"))) end
function api_jobcancel() out(core().job_cancel(fv("id"))) end

-- 显示隐藏文件 / 上传上限: 只改 mwfiles 自己的 uci(授权目录列表只能在系统配置里改, 不开放给网页)
function api_settings()
	local c = require("uci").cursor()
	if fv("hidden") then c:set("mwfiles", "main", "hidden", flag("hidden") and "1" or "0") end
	local mb = tonumber(fv("max_upload_mb"))
	if mb then c:set("mwfiles", "main", "max_upload_mb", tostring(math.max(0, math.floor(mb)))) end
	c:save("mwfiles")
	c:commit("mwfiles")
	out({ ok = true })
end

-- ------------------------------------------------------------ 网络位置
function api_netlist() out(require("luci.mwfiles.net").list()) end
function api_netdiag() out(require("luci.mwfiles.net").diag({ host = fv("host") })) end
function api_sharelist() out(require("luci.mwfiles.net").share_list()) end

function api_smbmount()
	-- 用户名/密码只在这一次请求里经过内存, 不记录、不回显
	local r = require("luci.mwfiles.net").mount({
		host = fv("host"), share = fv("share"), user = fv("user"), pass = fv("pass"), domain = fv("domain"),
		vers = fv("vers") or "", name = fv("name"), remember = flag("remember"), auto = flag("auto"),
		guest = flag("guest"), ro = flag("ro"),
	})
	out(r)
end

function api_smbremount() out(require("luci.mwfiles.net").remount(fv("id"))) end
function api_smbumount() out(require("luci.mwfiles.net").umount(fv("id"), flag("lazy"))) end
function api_smbforget() out(require("luci.mwfiles.net").forget(fv("id"))) end
local function share_params()
	local users = jfv("users")
	return {
		id = fv("id"), root = fv("root"), path = fv("path"), name = fv("name"),
		ro = flag("ro"), guest = flag("guest"), comment = fv("comment"),
		newuser = fv("newuser"), newpass = fv("newpass"),
		users = type(users) == "table" and users or {},
	}
end
function api_shareadd() out(require("luci.mwfiles.net").share_add(share_params())) end
function api_shareedit() out(require("luci.mwfiles.net").share_edit(share_params())) end
function api_shareremove() out(require("luci.mwfiles.net").share_remove(fv("id"))) end
function api_sharesvc() out(require("luci.mwfiles.net").share_service(fv("action"))) end

-- ------------------------------------------------------------ 内部存储守卫 / 格式化
local function guard() return require "luci.mwfiles.guard" end

-- 前置查询: 这批路径动了会怎样(是否内部存储、属于哪个软件包、后果说明、是否要 root 密码)
-- op: delete|rename|move|write|overwrite|share; root+paths(JSON 数组, 相对授权根) 以及可选目的地 droot+dpath
function api_guardinfo()
	local c = core()
	local op = fv("op")
	local ops = { delete = 1, rename = 1, move = 1, write = 1, overwrite = 1, share = 1 }
	if not ops[op] then return out(c.err("bad_op", "不支持的操作")) end
	local abs = {}
	-- as_dir: 这条路径是「要往里面新增内容的目录」(否则是被删除/重命名/移动的条目本身)
	local function add(rootid, rel, as_dir)
		local r, e = c.resolve(rootid, rel, false, true)
		if not r then return e end
		-- 系统根顶层不可改: 在要密码之前就告诉用户, 不让人输完密码才被拒
		if r.root and (as_dir or op == "delete" or op == "rename" or op == "move") then
			local tk, te = c.top_ok(r, as_dir)
			if not tk then return te end
		end
		abs[#abs + 1] = r.real or r.abs
	end
	local paths = jfv("paths")
	if type(paths) == "table" then
		for _, p in ipairs(paths) do
			local e = add(fv("root"), p, op == "write")
			if e then return out(e) end
		end
	end
	if fv("dpath") then
		local e = add(fv("droot") or fv("root"), fv("dpath"), true)
		if e then return out(e) end
	end
	if #abs == 0 then return out(c.err("bad_path", "缺少路径")) end
	local d = guard().describe(op, abs)
	if not d then return out({ ok = true, internal = false, need_pw = false }) end
	d.ok = true
	d.need_pw = true
	out(d)
end

-- 只校验 root 密码(密码在请求头里), 供前端在弹窗里提前验证; 通过后前端仍要在真正的写请求里再带一次
function api_checkpw()
	local ok, e = guard().verify_pw(guard().req_pw())
	if not ok then return out(e) end
	out({ ok = true })
end

function api_formatinfo() out(guard().format_info()) end

function api_format()
	out(guard().format({ dev = fv("dev"), fs = fv("fs"), label = fv("label"), confirm = fv("confirm") }))
end
