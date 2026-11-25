module("luci.controller.mwpbr", package.seeall)

-- 策略路由的薄控制器: UCI 里只有规则/出口组/全局三类配置由这里写,
-- 其余(校验、编译、应用、出口发现与别名/顺序/模式)都交给 mwrt-pbr 与 mwrt-wans 两个 CLI。
-- 接口形状见 SPEC §7 与 §13.6。

local fs = require "nixio.fs"
local jsonc = require "luci.jsonc"
local NULL = jsonc.null   -- 旧版 luci.jsonc 没有它, 此时对应字段缺省不输出

local CFG = "mwrt_pbr"
local CFG_FILE = "/etc/config/mwrt_pbr"
local PBR = "/usr/libexec/mwrt-pbr"
local WANS = "/usr/libexec/mwrt-wans"
local CLIENTS = "/usr/libexec/mwrt-clients"
local INIT = "/etc/init.d/mwrt-pbr"
local HNAT = "/sys/kernel/debug/hnat/hnat_entry"
local RUN = "/tmp/mwpbr"
local ICON_BASE = "/luci-static/resources/mwbehav/"

local MAX_RULES, MAX_GROUPS, MAX_JSON = 128, 16, 65536
local COPY_SUFFIX = "(副本)"
local SCALARS = { "name", "note", "match", "target", "on_fail", "proto" }
local LISTS = { "dport", "sport", "app", "src_dev", "src_ip", "dst_ip", "domain" }

function index()
	if not fs.access("/usr/libexec/mwrt-pbr") then return end
	local base = { "admin", "services", "oaf", "policy_routing" }
	entry(base, alias("admin", "services", "oaf", "policy_routing", "overview"), _("策略路由"), 32).dependent = false
	for _, page in ipairs({ "overview", "rules", "wans", "diag" }) do
		local node = entry({ "admin", "services", "oaf", "policy_routing", page }, template("mwpbr/page"))
		node.dependent = false
		node.leaf = true
	end
	entry({ "admin", "services", "mwpbr" }, call("action_ping")).dependent = false
	-- 循环变量不能叫 _, 否则会盖住上面用的翻译函数
	for i, fn in ipairs({ "state", "wans", "rule_get", "rule_check", "rule_save", "rule_del", "rule_toggle",
		"rule_copy", "rule_order", "group_save", "group_del", "wan_save", "wan_order", "wan_mode", "global_save",
		"apply", "counters", "counters_reset", "diag", "preview", "trace", "accel_flush", "apps", "clients" }) do
		entry({ "admin", "services", "mwpbr", fn }, post("api_" .. fn)).leaf = true
	end
end

function action_ping()
	luci.http.prepare_content("text/plain")
	luci.http.write("mwpbr ok")
end

-- ------------------------------------------------------------ 输出与基础工具
local function out(t)
	luci.http.prepare_content("application/json")
	luci.http.write(jsonc.stringify(t))
end

local function out_raw(s)
	luci.http.prepare_content("application/json")
	luci.http.write(s)
end

local function fail(msg, extra)
	local t = extra or {}
	t.ok = false
	t.error = msg
	return t
end

local function fv(k)
	local v = luci.http.formvalue(k)
	if type(v) ~= "string" then return nil end
	return v
end

local function cursor() return require("uci").cursor() end

local function has(list, v)
	for i, x in ipairs(list) do
		if x == v then return true end
	end
	return false
end

-- 外部命令的参数一律单引号包住; 控制字符(含换行与 \0)先换成空格
local function sq(s)
	s = tostring(s):gsub("%c", " ")
	return "'" .. s:gsub("'", "'\\''") .. "'"
end

local function sh(cmd)
	local p = io.popen(cmd .. " 2>/dev/null")
	if not p then return "" end
	local s = p:read("*a") or ""
	p:close()
	return s
end

-- 两个 CLI 的 stdout 都是单个 JSON 对象; 返回 解析结果, 原始文本
local function run_json(cmd)
	local s = sh(cmd)
	local j = jsonc.parse(s)
	if type(j) ~= "table" then return nil, s end
	return j, s
end

local function pbr(args) return run_json(PBR .. " " .. args) end
local function wans_cli(args) return run_json(WANS .. " " .. args) end

-- 引擎输出原样回给前端(经 Lua 表中转会把空对象变成 []); 解析不了才换成统一的错误
local function relay(cmd)
	local j, raw = run_json(cmd)
	if not j then return fail("后端没有返回有效结果，请确认策略路由组件已正常安装") end
	return nil, raw
end

local function rand_hex(n)
	local f = io.open("/dev/urandom", "rb")
	local s = f and f:read(n)
	if f then f:close() end
	if not s or #s < n then return nil end
	return (s:gsub(".", function(ch) return string.format("%02x", ch:byte()) end))
end

local run_ready
local function ensure_run()
	if run_ready then return end
	os.execute("umask 077; mkdir -p " .. RUN)
	run_ready = true
end

-- Lua 的 io.popen 只能单向, 校验用的 JSON 经临时文件重定向给 stdin, 不进命令行
local function pbr_stdin(args, text)
	ensure_run()
	local hex = rand_hex(8)
	if not hex then return nil end
	local path = RUN .. "/in." .. hex
	if not fs.writefile(path, text) then return nil end
	local j = run_json(PBR .. " " .. args .. " <" .. sq(path))
	fs.remove(path)
	return j
end

local function guard()
	local ctx = require("luci.dispatcher").context
	if luci.http.getenv("REQUEST_METHOD") ~= "POST" then
		out(fail("请求方式不对，需要 POST"))
		return false
	end
	if not ctx or not ctx.authtoken or luci.http.formvalue("token") ~= ctx.authtoken then
		out(fail("会话已过期，请刷新页面后重试"))
		return false
	end
	return true
end

-- 每个接口的统一外壳: 校验 token → 执行 → 输出; 处理函数返回 表 或 (nil, 原始 JSON 文本)
local function ep(fn)
	return function()
		if not guard() then return end
		local ok, res, raw = pcall(fn)
		if not ok then
			local msg = (tostring(res):gsub("[^\32-\126]", "?")):sub(1, 200)
			return out(fail("内部错误：" .. msg))
		end
		if raw then return out_raw(raw) end
		return out(res)
	end
end

local function jarg(name)
	local s = fv(name)
	if not s or s == "" or #s > MAX_JSON then return nil end
	local j = jsonc.parse(s)
	if type(j) ~= "table" then return nil end
	return j
end

-- 字符串数组参数(ids): 元素必须全是字符串
local function jlist(name)
	local t = jarg(name)
	if not t then return nil end
	local n = 0
	for k, v in pairs(t) do
		if type(k) ~= "number" or type(v) ~= "string" then return nil end
		n = n + 1
	end
	if n ~= #t then return nil end
	return t
end

local function valid_sec(id) return type(id) == "string" and #id <= 64 and id:match("^[%w_]+$") ~= nil end
local function valid_wan_id(id) return type(id) == "string" and #id <= 128 and id:match("^[%w._:-]+$") ~= nil end

local function truthy(v, dflt)
	if v == nil then return dflt end
	if type(v) == "boolean" then return v end
	if type(v) == "number" then return v ~= 0 end
	local s = tostring(v):lower()
	return not (s == "" or s == "0" or s == "false" or s == "off" or s == "no")
end

-- 严格的布尔解析: 不认识的取值返回 nil, 供 global_save 拒绝
local function parse_bool(v)
	if type(v) == "boolean" then return v end
	if v == 1 or v == "1" or v == "true" then return true end
	if v == 0 or v == "0" or v == "false" then return false end
	return nil
end

local function tolist(v)
	local r = {}
	if type(v) == "table" then
		for i, x in ipairs(v) do r[#r + 1] = tostring(x) end
	elseif v ~= nil then
		for w in tostring(v):gmatch("%S+") do r[#r + 1] = w end
	end
	return r
end

local function normmac(m)
	m = tostring(m or ""):lower():gsub("%-", ":")
	if m:match("^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$") then return m end
end

-- 按字符(不是字节)取 UTF-8 前 n 个字符
local function usub(s, n)
	local i, cnt = 1, 0
	while i <= #s do
		if cnt >= n then return s:sub(1, i - 1) end
		local b = s:byte(i)
		i = i + ((b >= 240) and 4 or (b >= 224) and 3 or (b >= 192) and 2 or 1)
		cnt = cnt + 1
	end
	return s
end

local function ulen(s) return #(s:gsub("[\128-\191]", "")) end

local function lines(path)
	local f = io.open(path, "r")
	if not f then return function() end end
	return function()
		local l = f:read("*l")
		if not l then f:close() end
		return l
	end
end

-- ------------------------------------------------------------ UCI 读写
local function read_global(c)
	local s = c:get_all(CFG, "global") or {}
	local zones = tolist(s.src_zones)
	if #zones == 0 then zones = { "lan" } end
	return {
		enabled = truthy(s.enabled, false),
		ipv6 = truthy(s.ipv6, true),
		dns_sets = truthy(s.dns_sets, true),
		dns_ttl = (type(s.dns_ttl) == "string" and s.dns_ttl ~= "") and s.dns_ttl or "1d",
		learn_ttl = (type(s.learn_ttl) == "string" and s.learn_ttl ~= "") and s.learn_ttl or "30m",
		src_zones = zones,
		flush_accel = truthy(s.flush_accel, false),
	}
end

-- on_fail 的历史值 backup 读作 default
local function read_rule(s)
	local r = {
		id = s[".name"],
		name = s.name or "",
		enabled = truthy(s.enabled, true),
		note = s.note or "",
		match = (s.match == "any") and "any" or "all",
		target = s.target or "",
		on_fail = (s.on_fail == "block") and "block" or "default",
		proto = (type(s.proto) == "string" and s.proto ~= "") and s.proto or "any",
	}
	for i, k in ipairs(LISTS) do r[k] = tolist(s[k]) end
	return r
end

local function read_group(s)
	return {
		id = s[".name"],
		name = s.name or "",
		enabled = truthy(s.enabled, true),
		members = tolist(s.member),
		note = s.note or "",
	}
end

local function read_rules(c)
	local rules = {}
	c:foreach(CFG, "rule", function(s) rules[#rules + 1] = read_rule(s) end)
	return rules
end

local function read_groups(c)
	local groups = {}
	c:foreach(CFG, "group", function(s) groups[#groups + 1] = read_group(s) end)
	return groups
end

local function rule_ids(c)
	local ids = {}
	c:foreach(CFG, "rule", function(s) ids[#ids + 1] = s[".name"] end)
	return ids
end

local function put(c, sec, opt, val)
	if val == nil or val == "" then
		c:delete(CFG, sec, opt)
	else
		c:set(CFG, sec, opt, tostring(val))
	end
end

-- 列表先删后写; 空列表只删
local function setlist(c, sec, opt, list)
	c:delete(CFG, sec, opt)
	if type(list) == "string" then list = { list } end
	local vals = {}
	if type(list) == "table" then
		for i, v in ipairs(list) do vals[#vals + 1] = tostring(v) end
	end
	if #vals > 0 then c:set(CFG, sec, opt, vals) end
end

local function new_id(c, prefix)
	for i = 1, 20 do
		local h = rand_hex(4)
		if not h then return nil end
		local id = prefix .. h
		if not c:get(CFG, id) then return id end
	end
end

-- global/wan/group 段留在原位, 规则按给定顺序重新填回规则原来占的位置
local function reorder_rules(c, ids)
	local all, slots = {}, {}
	c:foreach(CFG, nil, function(s)
		all[#all + 1] = s[".name"]
		if s[".type"] == "rule" then slots[#slots + 1] = #all end
	end)
	for i, pos in ipairs(slots) do all[pos] = ids[i] end
	for i, name in ipairs(all) do c:reorder(CFG, name, i - 1) end
end

local function ensure_cfg()
	if not fs.access(CFG_FILE) then
		fs.writefile(CFG_FILE, "config global 'global'\n\toption enabled '0'\n")
	end
end

-- 配置事务: 控制器之间串行(锁文件与引擎的 /var/lock/mwrt-pbr.lock 不是同一把, 避免嵌套死锁);
-- 开头 revert 丢掉别处遗留的暂存改动, 失败也 revert, 不让半截改动留在 /tmp/.uci
local function tx(fn)
	ensure_run()
	local nixio = require "nixio"
	local lk = nixio.open(RUN .. "/cfg.lock", "w")
	if lk then lk:lock("lock") end
	local c = cursor()
	local ok, res = pcall(function()
		ensure_cfg()
		c:revert(CFG)
		return fn(c)
	end)
	if not ok or type(res) ~= "table" or res.ok == false then pcall(c.revert, c, CFG) end
	if lk then lk:lock("ulock"); lk:close() end
	if not ok then
		local msg = (tostring(res):gsub("[^\32-\126]", "?")):sub(1, 200)
		return fail("内部错误：" .. msg)
	end
	if type(res) ~= "table" then return fail("内部错误：写入流程没有返回结果") end
	return res
end

local function commit(c)
	if c:commit(CFG) then return true end
	return false
end

-- ------------------------------------------------------------ 校验与应用
-- kind: "rule" | "group"; 返回引擎的校验结果表, 引擎不可用返回 nil
local function validate(kind, obj, id)
	local payload = { id = id or NULL }
	payload[kind] = obj
	return pbr_stdin("validate", jsonc.stringify(payload))
end

-- 校验结果 → 是否通过, 错误列表
local function verdict(res)
	local errs = (type(res.errors) == "table") and res.errors or {}
	return (res.ok == true and #errs == 0), errs
end

local function invalid_reply(what, errs)
	local first = errs[1] and errs[1].msg or "未知原因"
	return fail(what .. "校验未通过（共 " .. #errs .. " 项）：" .. first, { errors = errs })
end

local function run_apply()
	local j = pbr("apply")
	if not j then return fail("应用失败：引擎没有返回有效结果") end
	return j
end

-- 保存成功后默认立即应用; 调用方传 apply=0 表示只暂存。
-- 保存本身已成功, 所以应用结果放在 applied 里, 不改顶层 ok
local function with_apply(res)
	if res.ok and fv("apply") ~= "0" then res.applied = run_apply() end
	return res
end

-- 总开关/运行模式变化要让服务层重新维护 wanmon 与 cron, 单纯 apply 不做这些
local function service_reload()
	if fs.access(INIT, "x") then
		os.execute(INIT .. " reload >/dev/null 2>&1 </dev/null")
		return true
	end
	return false
end

-- reload 内部会静默执行 apply, 这里读状态文件把结果带回给前端, 失败不至于被当成成功
local function applied_from_status()
	local st = pbr("status")
	if not st then return nil end
	return {
		ok = (st.ok ~= false),
		error = st.error or "",
		failed_open = (st.failed_open == true),
		warnings = (type(st.warnings) == "table") and st.warnings or {},
	}
end

-- ------------------------------------------------------------ 能力探测
local function app_lib()
	if not fs.access("/proc/net/af_visit") then return nil end
	local ok, m = pcall(require, "mwadv")
	if not ok or type(m) ~= "table" or type(m.app_lib) ~= "function" then return nil end
	local ok2, lib = pcall(m.app_lib)
	if not ok2 or type(lib) ~= "table" or not lib.ok then return nil end
	return lib
end

local function feature_version()
	local path = "/etc/mwrt_behav/feature.cfg"
	if not fs.access(path) then path = "/usr/share/mwrt-behav/feature.cfg" end
	for l in lines(path) do
		local v = l:match("^#version%s+(%S+)")
		if v then return v end
	end
	return ""
end

-- dnsmasq 是否编进 nftset: 看 --version 的编译选项, 结果按二进制的 mtime:size 缓存
local function dnsmasq_nftset()
	local bin = "/usr/sbin/dnsmasq"
	local st = fs.stat(bin)
	if not st then return false end
	local key = st.mtime .. ":" .. st.size
	local cf = RUN .. "/dnsmasq.cap"
	local k, v = (fs.readfile(cf) or ""):match("^(%S+) (%d)")
	if k == key then return v == "1" end
	local on = (" " .. sh(bin .. " --version"):gsub("%s+", " ") .. " "):find(" nftset ", 1, true) ~= nil
	ensure_run()
	fs.writefile(cf, key .. " " .. (on and "1" or "0") .. "\n")
	return on
end

-- ------------------------------------------------------------ state
local function build_state()
	local c = cursor()
	local warnings = {}
	local g = read_global(c)
	local rules, groups = read_rules(c), read_groups(c)

	local st = pbr("status")
	if not st then
		st = {}
		warnings[#warnings + 1] = "读取引擎状态失败"
	end
	local live = (type(st.live) == "table") and st.live or {}

	local snap = wans_cli("snapshot --json")
	if not snap then
		snap = {}
		warnings[#warnings + 1] = "读取出口列表失败"
	end
	local wans = (type(snap.wans) == "table") and snap.wans or {}
	if type(snap.errors) == "table" then
		for i, e in ipairs(snap.errors) do warnings[#warnings + 1] = tostring(e) end
	end

	local mode = (tostring(c:get("Mwrt-Assurance", "wan_failover", "mode") or ""):lower() == "multi") and "multi" or "pair"

	-- status 已带实时命中数; counters 只为 since 并顺带落盘内核计数, 每 30 秒取一次, since 缓存在 RUN 下
	local hits, since = {}, nil
	if #rules > 0 and live.table == true then
		local cf = RUN .. "/since.cache"
		local ts, sv = (fs.readfile(cf) or ""):match("^(%d+) (%d+)")
		local now = os.time()
		ts, sv = tonumber(ts), tonumber(sv)
		if ts and sv and now >= ts and now - ts < 30 then
			since = sv
		else
			local cj = pbr("counters")
			if cj and type(cj.rules) == "table" then
				hits = cj.rules
				since = cj.since
				ensure_run()
				fs.writefile(cf, now .. " " .. (tonumber(since) or 0) .. "\n")
			end
		end
	end

	local srules = {}
	if type(st.rules) == "table" then
		for i, r in ipairs(st.rules) do
			if r.id then srules[r.id] = r end
		end
	end

	local in_use = {}
	local n_active, n_error = 0, 0
	for i, r in ipairs(rules) do
		local sr = srules[r.id]
		r.index = i
		r.exit = NULL
		r.hits = { pkts = 0, bytes = 0 }
		if sr then
			r.status = sr.status or "disabled"
			r.reason = sr.reason or ""
			if sr.exit ~= nil then r.exit = sr.exit end
			local h = hits[r.id] or sr.hits
			if type(h) == "table" then r.hits = { pkts = tonumber(h.pkts) or 0, bytes = tonumber(h.bytes) or 0 } end
		elseif r.enabled then
			-- state.json 里还没有这条规则: 尚未应用过
			r.status, r.reason = "pending", "尚未应用"
		else
			r.status, r.reason = "disabled", ""
		end
		if r.status == "active" then n_active = n_active + 1 end
		if r.status == "error" then n_error = n_error + 1 end
		local gid = r.target:match("^group:(.+)$")
		if gid then
			in_use[gid] = in_use[gid] or {}
			in_use[gid][#in_use[gid] + 1] = r.id
		end
	end
	for i, gp in ipairs(groups) do gp.in_use = in_use[gp.id] or {} end

	-- 配置文件比上次应用新, 或总开关与运行状态不一致 → 有未应用的改动(提示用途, 取决于板上时钟)
	local applied_ts = tonumber(st.applied_ts) or 0
	local fst = fs.stat(CFG_FILE)
	local pending = false
	if (st.enabled == true) ~= g.enabled then
		pending = true
	elseif g.enabled and fst and fst.mtime > applied_ts then
		pending = true
	end

	local v6 = false
	if snap.lan_prefix6 then
		for i, w in ipairs(wans) do
			if type(w.caps) == "table" and w.caps.v6 == true then v6 = true; break end
		end
	end

	return {
		ok = true,
		ts = os.time(),
		installed = true,
		global = g,
		engine = {
			running = (st.enabled == true and live.table == true),
			applied_ts = applied_ts,
			ok = (st.ok ~= false),
			error = st.error or "",
			failed_open = (st.failed_open == true),
			health_source = st.health_source or "none",
			mode = mode,
			wanmon = (live.wanmon == true),
			rules_total = #rules,
			rules_active = n_active,
			rules_error = n_error,
			pending = pending,
			counters_since = since,
		},
		caps = {
			apps = app_lib() ~= nil,
			dns_sets = dnsmasq_nftset(),
			v6 = v6,
			hnat = fs.access(HNAT) and true or false,
		},
		default_dev = snap.default_dev or "",
		lan_prefix6 = snap.lan_prefix6 or NULL,
		wans = wans,
		groups = groups,
		rules = rules,
		exits = (type(st.exits) == "table") and st.exits or {},
		warnings = (#warnings > 0) and warnings or nil,
	}
end

api_state = ep(function() return build_state() end)

api_wans = ep(function() return relay(WANS .. " snapshot --json") end)

-- ------------------------------------------------------------ 规则
api_rule_get = ep(function()
	local id = fv("id") or ""
	if not valid_sec(id) then return fail("规则编号不合法") end
	local c = cursor()
	local s = c:get_all(CFG, id)
	if not s or s[".type"] ~= "rule" then return fail("规则不存在或已被删除，请刷新") end
	return { ok = true, rule = read_rule(s) }
end)

api_rule_check = ep(function()
	local obj = jarg("json")
	if not obj then return fail("规则内容为空或格式不对") end
	local id = fv("id")
	if id == "" then id = nil end
	if id and not valid_sec(id) then return fail("规则编号不合法") end
	obj.id = nil
	local res = validate("rule", obj, id)
	if not res then return fail("校验失败：引擎没有返回有效结果") end
	if res.ok == false and type(res.errors) ~= "table" then return fail(res.error or "校验失败") end
	local valid, errs = verdict(res)
	return {
		ok = true,
		valid = valid,
		errors = errs,
		warnings = res.warnings or {},
		normalized = res.normalized,
		shadow = res.shadow or {},
	}
end)

local function write_rule(c, id, n)
	c:set(CFG, id, "enabled", truthy(n.enabled, true) and "1" or "0")
	for i, k in ipairs(SCALARS) do put(c, id, k, n[k]) end
	for i, k in ipairs(LISTS) do setlist(c, id, k, n[k]) end
end

api_rule_save = ep(function()
	local obj = jarg("json")
	if not obj then return fail("规则内容为空或格式不对") end
	local id = fv("id")
	if id == "" then id = nil end
	if id and not valid_sec(id) then return fail("规则编号不合法") end
	obj.id = nil
	local r = tx(function(c)
		local cur_enabled = true
		if id then
			local s = c:get_all(CFG, id)
			if not s or s[".type"] ~= "rule" then return fail("规则不存在或已被删除，请刷新") end
			cur_enabled = truthy(s.enabled, true)
		elseif #rule_ids(c) >= MAX_RULES then
			return fail("规则数量已达上限（" .. MAX_RULES .. " 条）")
		end
		-- 引擎规范化会把缺省 enabled 补成 true, 所以要在校验前补上原值, 否则已停用的规则会被悄悄启用
		if id and obj.enabled == nil then obj.enabled = cur_enabled end
		-- 校验与写入在同一把锁里, 引擎读到的就是将要被改的那份配置
		local res = validate("rule", obj, id)
		if not res then return fail("校验失败：引擎没有返回有效结果") end
		if res.ok == false and type(res.errors) ~= "table" then return fail(res.error or "校验失败") end
		local valid, errs = verdict(res)
		if not valid then return invalid_reply("规则", errs) end
		local n = type(res.normalized) == "table" and res.normalized or obj
		local sid = id
		if not sid then
			sid = new_id(c, "r_")
			if not sid then return fail("无法生成规则编号") end
			c:set(CFG, sid, "rule")
		end
		write_rule(c, sid, n)
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true, id = sid, warnings = res.warnings or {} }
	end)
	return with_apply(r)
end)

api_rule_del = ep(function()
	local id = fv("id") or ""
	if not valid_sec(id) then return fail("规则编号不合法") end
	local r = tx(function(c)
		if c:get(CFG, id) ~= "rule" then return fail("规则不存在或已被删除，请刷新") end
		c:delete(CFG, id)
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true, id = id }
	end)
	return with_apply(r)
end)

api_rule_toggle = ep(function()
	local id = fv("id") or ""
	if not valid_sec(id) then return fail("规则编号不合法") end
	local want = fv("enabled")
	local r = tx(function(c)
		local s = c:get_all(CFG, id)
		if not s or s[".type"] ~= "rule" then return fail("规则不存在或已被删除，请刷新") end
		local en
		if want ~= nil then en = truthy(want, true) else en = not truthy(s.enabled, true) end
		c:set(CFG, id, "enabled", en and "1" or "0")
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true, id = id, enabled = en }
	end)
	return with_apply(r)
end)

-- 复制出的规则插在原规则之后, 默认停用, 所以不触发应用
api_rule_copy = ep(function()
	local id = fv("id") or ""
	if not valid_sec(id) then return fail("规则编号不合法") end
	return tx(function(c)
		local s = c:get_all(CFG, id)
		if not s or s[".type"] ~= "rule" then return fail("规则不存在或已被删除，请刷新") end
		local ids = rule_ids(c)
		if #ids >= MAX_RULES then return fail("规则数量已达上限（" .. MAX_RULES .. " 条）") end
		local nid = new_id(c, "r_")
		if not nid then return fail("无法生成规则编号") end
		local src = read_rule(s)
		local nm = src.name
		if ulen(nm) + ulen(COPY_SUFFIX) > 64 then nm = usub(nm, 64 - ulen(COPY_SUFFIX)) end
		src.name = nm .. COPY_SUFFIX
		src.enabled = false
		c:set(CFG, nid, "rule")
		write_rule(c, nid, src, false)
		local order = {}
		for i, rid in ipairs(ids) do
			order[#order + 1] = rid
			if rid == id then order[#order + 1] = nid end
		end
		reorder_rules(c, order)
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true, id = nid }
	end)
end)

api_rule_order = ep(function()
	local want = jlist("ids")
	if not want then return fail("参数无效") end
	local stale = "规则列表已变化,请刷新后重试"
	local r = tx(function(c)
		local ids = rule_ids(c)
		if #want ~= #ids then return fail(stale) end
		local have, seen = {}, {}
		for i, rid in ipairs(ids) do have[rid] = true end
		for i, rid in ipairs(want) do
			if not have[rid] or seen[rid] then return fail(stale) end
			seen[rid] = true
		end
		local same = true
		for i, rid in ipairs(ids) do
			if want[i] ~= rid then same = false; break end
		end
		if same then return { ok = true, unchanged = true } end
		reorder_rules(c, want)
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true }
	end)
	if r.unchanged then return r end
	return with_apply(r)
end)

-- ------------------------------------------------------------ 出口组
api_group_save = ep(function()
	local obj = jarg("json")
	if not obj then return fail("出口组内容为空或格式不对") end
	local id = fv("id")
	if id == nil or id == "" then id = obj.id end
	if id == "" then id = nil end
	if id ~= nil and not valid_sec(id) then return fail("出口组编号不合法") end
	obj.id = nil
	local r = tx(function(c)
		if id then
			if c:get(CFG, id) ~= "group" then return fail("出口组不存在或已被删除，请刷新") end
		else
			local n = 0
			c:foreach(CFG, "group", function() n = n + 1 end)
			if n >= MAX_GROUPS then return fail("出口组数量已达上限（" .. MAX_GROUPS .. " 个）") end
		end
		local res = validate("group", obj, id)
		if not res then return fail("校验失败：引擎没有返回有效结果") end
		if res.ok == false and type(res.errors) ~= "table" then return fail(res.error or "校验失败") end
		local valid, errs = verdict(res)
		if not valid then return invalid_reply("出口组", errs) end
		local n = type(res.normalized) == "table" and res.normalized or obj
		local sid = id
		if not sid then
			sid = new_id(c, "g_")
			if not sid then return fail("无法生成出口组编号") end
			c:set(CFG, sid, "group")
		end
		put(c, sid, "name", n.name)
		put(c, sid, "note", n.note)
		c:set(CFG, sid, "enabled", truthy(n.enabled, true) and "1" or "0")
		setlist(c, sid, "member", n.members)
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true, id = sid, warnings = res.warnings or {} }
	end)
	return with_apply(r)
end)

api_group_del = ep(function()
	local id = fv("id") or ""
	if not valid_sec(id) then return fail("出口组编号不合法") end
	local r = tx(function(c)
		if c:get(CFG, id) ~= "group" then return fail("出口组不存在或已被删除，请刷新") end
		local used = {}
		for i, rule in ipairs(read_rules(c)) do
			if rule.target == "group:" .. id then used[#used + 1] = { id = rule.id, name = rule.name } end
		end
		if #used > 0 then return fail("出口组仍被 " .. #used .. " 条规则使用", { in_use = used }) end
		c:delete(CFG, id)
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true, id = id }
	end)
	return with_apply(r)
end)

-- ------------------------------------------------------------ WAN 出口(别名/开关/顺序/模式只经 mwrt-wans)
local function cli_ok(j, what)
	if not j then return fail(what .. "失败：发现程序没有返回有效结果") end
	if j.ok == false then return fail(j.error or (what .. "失败")) end
	return nil
end

api_wan_save = ep(function()
	local id = fv("wan_id") or ""
	if not valid_wan_id(id) then return fail("出口标识不合法") end
	local alias, en = fv("alias"), fv("enabled")
	if alias == nil and en == nil then return fail("没有要保存的内容") end
	local r = tx(function()
		if alias ~= nil then
			local e = cli_ok(wans_cli("alias set " .. sq(id) .. " " .. sq(alias)), "保存别名")
			if e then return e end
		end
		if en ~= nil then
			local e = cli_ok(wans_cli("enable set " .. sq(id) .. " " .. (truthy(en, true) and "1" or "0")), "保存启用开关")
			if e then return e end
		end
		return { ok = true, wan_id = id }
	end)
	-- 只有启停会改变可选出口; 改别名不必重新应用
	if r.ok and en ~= nil then return with_apply(r) end
	return r
end)

api_wan_order = ep(function()
	local ids = jlist("ids")
	if not ids or #ids > 64 then return fail("参数无效") end
	local parts = {}
	for i, id in ipairs(ids) do
		if not valid_wan_id(id) then return fail("出口标识不合法：" .. usub(id, 40)) end
		parts[#parts + 1] = sq(id)
	end
	return tx(function()
		local j = wans_cli("order set " .. table.concat(parts, " "))
		local e = cli_ok(j, "保存出口顺序")
		if e then return e end
		return { ok = true, order = j.order or {} }
	end)
end)

api_wan_mode = ep(function()
	local mode = fv("mode")
	if mode ~= "pair" and mode ~= "multi" then return fail("运行模式只能是 pair 或 multi") end
	local r = tx(function()
		local e = cli_ok(wans_cli("mode set " .. mode), "保存运行模式")
		if e then return e end
		return { ok = true, mode = mode }
	end)
	if r.ok and service_reload() then
		local a = applied_from_status()
		if a then r.applied = a end
	end
	return r
end)

-- ------------------------------------------------------------ 全局设置
local TTL_UNIT = { s = 1, m = 60, h = 3600, d = 86400 }

local function ttl_seconds(v)
	if type(v) ~= "string" then return nil end
	local n, u = v:lower():match("^(%d+)([smhd])$")
	if not n or #n > 7 then return nil end
	return tonumber(n) * TTL_UNIT[u]
end

api_global_save = ep(function()
	local obj = jarg("json")
	if not obj then return fail("参数无效") end
	local upd = {}
	for i, k in ipairs({ "enabled", "ipv6", "dns_sets", "flush_accel" }) do
		if obj[k] ~= nil then
			local b = parse_bool(obj[k])
			if b == nil then return fail("设置项 " .. k .. " 的取值不对") end
			upd[k] = b and "1" or "0"
		end
	end
	if obj.dns_ttl ~= nil then
		local sec = ttl_seconds(obj.dns_ttl)
		if not sec or sec < 3600 or sec > 604800 then return fail("DNS 集合寿命要在 1 小时到 7 天之间，例如 1d、12h") end
		upd.dns_ttl = obj.dns_ttl:lower()
	end
	if obj.learn_ttl ~= nil then
		local sec = ttl_seconds(obj.learn_ttl)
		if not sec or sec < 60 or sec > 604800 then return fail("应用识别地址寿命要在 1 分钟到 7 天之间，例如 30m、2h") end
		upd.learn_ttl = obj.learn_ttl:lower()
	end
	local zones
	if obj.src_zones ~= nil then
		zones = tolist(obj.src_zones)
		if #zones == 0 or #zones > 16 then return fail("至少要选择一个参与策略路由的防火墙区域") end
		for i, z in ipairs(zones) do
			if #z > 64 or not z:match("^[%w_-]+$") then return fail("防火墙区域名不合法：" .. usub(z, 40)) end
		end
	end
	if next(upd) == nil and not zones then return fail("没有要保存的内容") end

	local flipped = false
	local r = tx(function(c)
		if zones then
			local known, any = {}, false
			c:foreach("firewall", "zone", function(s)
				if type(s.name) == "string" then known[s.name] = true; any = true end
			end)
			if any then
				for i, z in ipairs(zones) do
					if not known[z] then return fail("防火墙里没有名为 " .. z .. " 的区域") end
				end
			end
		end
		local was = read_global(c).enabled
		if not c:get(CFG, "global") then c:set(CFG, "global", "global") end
		for k, v in pairs(upd) do c:set(CFG, "global", k, v) end
		if zones then setlist(c, "global", "src_zones", zones) end
		if upd.enabled ~= nil then flipped = (upd.enabled == "1") ~= was end
		if not commit(c) then return fail("写入配置失败") end
		return { ok = true }
	end)
	if r.ok and fv("apply") ~= "0" then
		r.applied = run_apply()
		if flipped then service_reload() end
	end
	return r
end)

-- ------------------------------------------------------------ 应用 / 计数 / 诊断 / 试算
api_apply = ep(function() return relay(PBR .. " apply") end)
api_counters = ep(function() return relay(PBR .. " counters") end)
api_counters_reset = ep(function()
	fs.remove(RUN .. "/since.cache")
	return relay(PBR .. " counters-reset")
end)
api_diag = ep(function() return relay(PBR .. " diag") end)
api_preview = ep(function() return relay(PBR .. " preview") end)

api_trace = ep(function()
	local args = "trace"
	local function add(flag, v) args = args .. " " .. flag .. " " .. sq(v) end
	local function str(k)
		local v = fv(k)
		if v == nil then return nil end
		v = v:gsub("^%s+", ""):gsub("%s+$", "")
		if v == "" then return nil end
		return v
	end
	for i, k in ipairs({ "src", "dst" }) do
		local v = str(k)
		if v then
			if #v > 45 or not v:match("^[%x:.]+$") then return fail("地址 " .. usub(v, 40) .. " 格式不对") end
			add("--" .. k, v)
		end
	end
	local mac = str("mac")
	if mac then
		mac = normmac(mac)
		if not mac then return fail("MAC 地址格式不对，应为 aa:bb:cc:dd:ee:ff") end
		add("--mac", mac)
	end
	local proto = str("proto")
	if proto then
		proto = proto:lower()
		if proto ~= "tcp" and proto ~= "udp" and proto ~= "icmp" then return fail("协议只能是 tcp、udp 或 icmp") end
		add("--proto", proto)
	end
	for i, k in ipairs({ "dport", "sport" }) do
		local v = str(k)
		if v then
			local n = v:match("^%d+$") and tonumber(v)
			if not n or n < 1 or n > 65535 then return fail("端口 " .. usub(v, 20) .. " 不在 1-65535 之间") end
			add("--" .. k, tostring(n))
		end
	end
	local domain = str("domain")
	if domain then
		domain = domain:lower()
		if #domain > 253 or not domain:match("^[%w._-]+$") then return fail("域名格式不对，请使用 punycode（xn--）写法") end
		add("--domain", domain)
	end
	return relay(PBR .. " " .. args)
end)

-- 清空 MTK HNAT 绑定表(命令照 mwadv-apply; MT7987 的 PPE2 是否存在未验证)
api_accel_flush = ep(function()
	if not fs.access(HNAT) then return fail("本机没有硬件加速（HNAT）表，无需清除") end
	local n = 0
	for i, cmd in ipairs({ "3 -1", "5 -1", "7 -1" }) do
		local h = io.open(HNAT, "w")
		if h then
			h:write(cmd)
			h:close()
			n = n + 1
		end
	end
	if n == 0 then return fail("无法写入加速表，请确认 debugfs 已挂载") end
	return { ok = true, written = n }
end)

-- ------------------------------------------------------------ 应用库 / 设备
api_apps = ep(function()
	local lib = app_lib()
	local r = { ok = true, available = lib ~= nil, version = "", icon_base = ICON_BASE, classes = {}, apps = {} }
	if not lib then return r end
	r.version = feature_version()
	for i, cid in ipairs(lib.corder or {}) do
		local cl = lib.classes and lib.classes[cid]
		if cl then r.classes[#r.classes + 1] = { id = cl.id, key = cl.key, name = cl.name } end
	end
	for id, a in pairs(lib.ids or {}) do
		r.apps[#r.apps + 1] = { id = a.id, name = a.name, cls = a.cls }
	end
	table.sort(r.apps, function(x, y) return x.id < y.id end)
	return r
end)

-- 在线设备 ∪ DHCP 租约 ∪ 静态主机 ∪ 行为管理里的设备别名, 以 MAC 为键
api_clients = ep(function()
	local c = cursor()
	local RANK = { online = 1, dhcp = 2, static = 3, alias = 4 }
	local t, order = {}, {}
	local function ent(mac, src)
		local e = t[mac]
		if not e then
			e = { mac = mac, name = "", ip = "", ip6 = {}, online = false, alias = "", source = src }
			t[mac] = e
			order[#order + 1] = e
		elseif RANK[src] < RANK[e.source] then
			e.source = src
		end
		return e
	end
	local function fill(e, name, ip)
		if type(name) == "string" then
			name = name:gsub("%c", "")
			if e.name == "" and name ~= "" and name ~= "*" then e.name = name end
		end
		if type(ip) == "string" and e.ip == "" then e.ip = ip end
	end

	local lan_dev = "br-lan"
	local j = run_json(CLIENTS)
	if j then
		if type(j.lan_dev) == "string" and j.lan_dev:match("^[%w._-]+$") then lan_dev = j.lan_dev end
		for i, d in ipairs(type(j.clients) == "table" and j.clients or {}) do
			local mac = normmac(d.mac)
			if mac then
				local e = ent(mac, "online")
				e.online = true
				fill(e, d.name, d.ip)
			end
		end
	end

	for l in lines("/tmp/dhcp.leases") do
		local x, m, ip, nm = l:match("^(%d+) (%S+) (%S+) (%S+)")
		local mac = normmac(m)
		if mac then fill(ent(mac, "dhcp"), nm, ip) end
	end

	c:foreach("dhcp", "host", function(s)
		for i, m in ipairs(tolist(s.mac)) do
			local mac = normmac(m)
			if mac then fill(ent(mac, "static"), s.name, s.ip) end
		end
	end)

	c:foreach("mwrt_behav", "dev", function(s)
		local mac = normmac(s.mac)
		if mac and type(s.name) == "string" and s.name ~= "" then ent(mac, "alias").alias = s.name end
	end)

	-- 只给已知设备补 IPv6: 排除链路本地地址与失效邻居
	for l in sh("ip -6 neigh show dev " .. sq(lan_dev)):gmatch("[^\n]+") do
		local addr = l:match("^(%S+)")
		local mac = normmac(l:match("lladdr (%S+)"))
		local state = l:match("(%u+)%s*$") or ""
		if addr and mac and t[mac] and addr:find(":", 1, true) and not addr:lower():match("^fe80:")
			and state ~= "FAILED" and state ~= "INCOMPLETE" then
			local e = t[mac]
			addr = addr:lower()
			if #e.ip6 < 8 and not has(e.ip6, addr) then e.ip6[#e.ip6 + 1] = addr end
		end
	end

	table.sort(order, function(a, b)
		if a.online ~= b.online then return a.online end
		local an = (a.alias ~= "" and a.alias or a.name):lower()
		local bn = (b.alias ~= "" and b.alias or b.name):lower()
		if an ~= bn then return an < bn end
		return a.mac < b.mac
	end)
	for i = #order, 513, -1 do order[i] = nil end
	return { ok = true, clients = order }
end)
