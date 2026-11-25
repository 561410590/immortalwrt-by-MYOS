module("luci.controller.mwbehav", package.seeall)

local fs = require "nixio.fs"
local jsonc = require "luci.jsonc"

local RUN = "/tmp/mwbehav"
local FEATURE_BUILTIN = "/etc/appfilter/feature.cfg"
local FEATURE_USER = "/etc/mwrt_behav/feature.cfg"   -- 用户导入的特征库, 存在且可用就优先
local FEATURE = FEATURE_BUILTIN
local audit, who   -- 定义在文件后部, 这里先声明供前面的接口调用

function index()
	if not fs.access("/usr/libexec/mwadv-apply") then return end
	entry({ "admin", "services", "oaf", "behavior" }, template("mwbehav/page"), _("行为管理"), 31).dependent = false
	entry({ "admin", "services", "mwbehav" }, call("action_ping")).dependent = false
	for _, fn in ipairs({ "apps", "caps",
			"adv_get", "adv_diag", "adv_global_save", "adv_obj_save", "adv_obj_del", "adv_obj_copy", "adv_rule_save",
			"adv_rule_check", "adv_rule_del", "adv_rule_toggle", "adv_rule_copy", "adv_rule_move", "adv_rule_order",
			"adv_counters_reset", "adv_log", "adv_apply" }) do
		entry({ "admin", "services", "mwbehav", fn }, post("api_" .. fn)).leaf = true
	end
end

local function out(t)
	luci.http.prepare_content("application/json")
	luci.http.write(jsonc.stringify(t))
end

local function fv(k) return luci.http.formvalue(k) end
local function cursor() return require("uci").cursor() end
local function normmac(m)
	m = tostring(m or ""):lower()
	if m:match("^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$") then return m end
end

function action_ping() luci.http.prepare_content("text/plain"); luci.http.write("mwbehav ok") end

-- ------------------------------------------------------------ 数据读取
local function lines(path)
	local f = io.open(path, "r")
	if not f then return function() end end
	return function()
		local l = f:read("*l")
		if not l then f:close() end
		return l
	end
end

local function split(l)
	local r, n = {}, 0
	for x in (l .. "\t"):gmatch("(.-)\t") do n = n + 1; r[n] = x end
	return r
end

local feat_cache, feat_mt
local function parse_features(path)
	local classes, order, apps, seen = {}, {}, {}, {}
	for l in lines(path) do
		local id, nm = l:match("^(%d+)%s+([^:%[]+):")
		if id then
			id = tonumber(id)
			if not seen[id] then
				seen[id] = true
				apps[#apps + 1] = { id = id, name = (nm:gsub("%s+$", "")), cls = math.floor(id / 1000) }
			end
		else
			local key, num, label = l:match("^#class%s+(%S+)%s+(%d+)%s+(.+)$")
			if key then
				classes[tonumber(num)] = { key = key, name = (label:gsub("%s+$", "")) }
				order[#order + 1] = tonumber(num)
			end
		end
	end
	return { classes = classes, order = order, apps = apps }
end

local function features()
	local su, sb = fs.stat(FEATURE_USER), fs.stat(FEATURE_BUILTIN)
	local sig = (su and (su.mtime .. ":" .. su.size) or "-") .. "|" .. (sb and sb.mtime or 0)
	if feat_cache and feat_mt == sig then return feat_cache end
	local f, path = nil, FEATURE_BUILTIN
	if su then
		f = parse_features(FEATURE_USER)
		path = FEATURE_USER
		if #f.apps < 30 then f = nil; path = FEATURE_BUILTIN end   -- 导入文件不可用, 退回内置(守护进程同样处理)
	end
	f = f or parse_features(FEATURE_BUILTIN)
	f.path = path
	f.custom = path == FEATURE_USER
	feat_cache = f
	feat_mt = sig
	FEATURE = path
	return f
end

local function appname_map()
	local m = {}
	for _, a in ipairs(features().apps) do m[a.id] = a.name end
	-- 高级策略里的自定义域名特征(编号 60001+)按域名显示
	local f = io.open("/etc/mwrt_behav/adv_urlid.tsv", "r")
	if f then
		for l in f:lines() do
			local id, key = l:match("^(%d+)\t(.+)$")
			if id then m[tonumber(id)] = key end
		end
		f:close()
	end
	return m
end

local function lan_clients()
	local p = io.popen("/usr/libexec/mwrt-clients 2>/dev/null")
	if not p then return {} end
	local s = p:read("*a")
	p:close()
	local j = jsonc.parse(s or "")
	return (j and j.clients) or {}
end

local function dhcp_names()
	local m = {}
	for l in lines("/tmp/dhcp.leases") do
		local _, mac, ip, nm = l:match("^(%d+) (%S+) (%S+) (%S+)")
		if mac and nm and nm ~= "*" then m[mac:lower()] = nm end
	end
	return m
end

local function dev_table(include_offline_macs)
	local c = cursor()
	local names, icons = {}, {}
	c:foreach("mwrt_behav", "dev", function(s)
		local mac = normmac(s.mac)
		if mac then names[mac] = s.name; icons[mac] = s.icon end
	end)
	local dh = dhcp_names()
	local t, order = {}, {}
	for _, d in ipairs(lan_clients()) do
		d.online = true
		t[d.mac:lower()] = d
		order[#order + 1] = d.mac:lower()
	end
	for _, m in ipairs(include_offline_macs or {}) do
		if not t[m] then t[m] = { mac = m, ip = "", type = "", band = "", online = false }; order[#order + 1] = m end
	end
	for m, d in pairs(t) do
		d.mac = m
		d.cname = names[m]
		d.icon = icons[m]
		d.hostname = (d.name and d.name ~= "" and d.name) or dh[m] or ""
		d.name = nil
	end
	return t, order
end

local function live()
	local f = io.open(RUN .. "/live.json", "r")
	if not f then return nil end
	local j = jsonc.parse(f:read("*a") or "")
	f:close()
	return j
end

local function deny_state()
	local r, why = {}, {}
	for l in lines(RUN .. "/deny.tsv") do
		local a = split(l)
		if a[1] then r[a[1]] = tonumber(a[2]) or 0; why[a[1]] = a[3] ~= "" and a[3] or "manual" end
	end
	return r, why
end

local function range_since(rg)
	local now = os.time()
	if rg == "7d" then return now - 7 * 86400 end
	if rg == "24h" then return now - 86400 end
	local t = os.date("*t", now)
	t.hour, t.min, t.sec = 0, 0, 0
	return os.time(t)
end

local function acc(tbl, k, u, d)
	local x = tbl[k]
	if not x then x = { 0, 0 }; tbl[k] = x end
	x[1] = x[1] + u; x[2] = x[2] + d
end

local function sorted_hours(hours)
	local hs = {}
	for h, v in pairs(hours) do hs[#hs + 1] = { t = h, u = v[1], d = v[2] } end
	table.sort(hs, function(a, b) return a.t < b.t end)
	return hs
end

-- ------------------------------------------------------------ 概览
function api_overview()
	local rg = fv("range") or "today"
	local since = range_since(rg)
	local dev, app, cls, hours = {}, {}, {}, {}
	local tu, td = 0, 0
	for l in lines(RUN .. "/hist.tsv") do
		local a = split(l)
		local h = tonumber(a[1])
		if h and h >= since - 3599 then
			local u, d, ap = tonumber(a[4]) or 0, tonumber(a[5]) or 0, tonumber(a[3]) or 0
			acc(dev, a[2], u, d)
			acc(app, ap, u, d)
			acc(cls, math.floor(ap / 1000), u, d)
			acc(hours, h, u, d)
			tu = tu + u; td = td + d
		end
	end
	local macs = {}
	for m in pairs(dev) do macs[#macs + 1] = m end
	local dt = dev_table(macs)
	local an = appname_map()
	local function top(tbl, n, mk)
		local arr = {}
		for k, v in pairs(tbl) do arr[#arr + 1] = { k = k, u = v[1], d = v[2], t = v[1] + v[2] } end
		table.sort(arr, function(a, b) return a.t > b.t end)
		local r = {}
		for i = 1, math.min(n, #arr) do r[i] = mk(arr[i]) end
		return r
	end
	local topdev = top(dev, 5, function(x)
		local d = dt[x.k] or {}
		return { mac = x.k, up = x.u, down = x.d, name = d.cname or d.hostname or "", ip = d.ip, online = d.online and true or false,
			icon = d.icon, vendor = d.vendor, band = d.band }
	end)
	local topapp = top(app, 8, function(x)
		return { id = x.k, up = x.u, down = x.d, name = x.k == 0 and "其他/未识别" or (an[x.k] or tostring(x.k)) }
	end)
	local topcls = top(cls, 12, function(x)
		local c = features().classes[x.k]
		return { id = x.k, up = x.u, down = x.d, name = x.k == 0 and "其他/未识别" or (c and c.name or tostring(x.k)) }
	end)
	-- 近 2 小时 5 分钟曲线
	local ser, sl = {}, os.time() - 7200
	for l in lines(RUN .. "/series.tsv") do
		local a = split(l)
		local t = tonumber(a[1])
		if t and t >= sl then ser[#ser + 1] = { t = t, u = tonumber(a[2]) or 0, d = tonumber(a[3]) or 0 } end
	end
	local evs = {}
	for l in lines(RUN .. "/events.tsv") do
		local a = split(l)
		evs[#evs + 1] = { t = tonumber(a[1]), mac = a[2], app = tonumber(a[3]) or 0, host = a[4] or "" }
	end
	local ev2 = {}
	for i = #evs, math.max(1, #evs - 29), -1 do
		local e = evs[i]
		local d = dt[e.mac] or {}
		e.dev = d.cname or d.hostname or ""
		e.appname = e.app > 0 and (an[e.app] or "") or ""
		ev2[#ev2 + 1] = e
	end
	local lv = live() or {}
	local nonline = 0
	for _, d in pairs(dt) do if d.online then nonline = nonline + 1 end end
	local denied = 0
	for _ in pairs(deny_state()) do denied = denied + 1 end
	out({ ok = true, range = rg, since = since, now = os.time(), up = tu, down = td, top_dev = topdev, top_app = topapp,
		top_cls = topcls, hours = sorted_hours(hours), series = ser, events = ev2, rate_up = lv.up or 0,
		rate_down = lv.down or 0, flows = lv.flows or 0, clock = lv.clock,
		running = lv.t ~= nil and (os.time() - lv.t) < 30, online = nonline, denied = denied, enabled = lv.enabled })
end

-- ------------------------------------------------------------ 首页播报「活跃应用」
-- 只读 hist.tsv(按小时汇总的小文件)算今日应用流量 TOP5, 今日不足 3 个应用时放宽到近 24 小时; 结果缓存 60 秒,
-- 命中缓存时既不解析特征库也不扫 hist.tsv, 首页最多每分钟来问一次, 对 CPU 没有可感知的开销。
function api_topapps()
	local CACHE = RUN .. "/topapps.json"
	local function send(s)
		luci.http.prepare_content("application/json")
		luci.http.header("Cache-Control", "no-store")
		luci.http.write(s)
	end
	local st = fs.stat(CACHE)
	if st then
		local age = os.time() - st.mtime
		if age >= 0 and age < 60 then
			local s = fs.readfile(CACHE)
			if s and #s > 2 then return send(s) end
		end
	end
	local function scan(since)
		local app, n = {}, 0
		for l in lines(RUN .. "/hist.tsv") do
			local a = split(l)
			local h = tonumber(a[1])
			local ap = tonumber(a[3]) or 0
			if h and h >= since - 3599 and ap > 0 then
				if not app[ap] then app[ap] = 0; n = n + 1 end
				app[ap] = app[ap] + (tonumber(a[4]) or 0) + (tonumber(a[5]) or 0)
			end
		end
		return app, n
	end
	local rg = "today"
	local app, n = scan(range_since("today"))
	if n < 3 then app, n = scan(os.time() - 86400); rg = "24h" end
	local arr = {}
	for k, v in pairs(app) do arr[#arr + 1] = { id = k, bytes = v } end
	table.sort(arr, function(a, b) return a.bytes > b.bytes end)
	local res = {}
	if #arr > 0 then
		local an = appname_map()
		for i = 1, math.min(5, #arr) do
			res[i] = { id = arr[i].id, name = an[arr[i].id] or tostring(arr[i].id), bytes = arr[i].bytes }
		end
	end
	local s = jsonc.stringify({ ok = true, at = os.time(), range = rg, apps = res })
	local tmp = CACHE .. ".tmp"
	if fs.writefile(tmp, s) then os.rename(tmp, CACHE) end
	send(s)
end

-- ------------------------------------------------------------ 在线设备
local function policy_map()
	local c = cursor()
	local p, sc = {}, {}
	c:foreach("mwrt_behav", "policy", function(s)
		local m = normmac(s.mac)
		if m then p[m] = { name = s[".name"], blocked = s.blocked == "1", ["until"] = tonumber(s["until"]) or 0,
			apps = s.apps, classes = s.classes } end
	end)
	c:foreach("mwrt_behav", "sched", function(s)
		local m = normmac(s.mac)
		if m then
			sc[m] = sc[m] or {}
			table.insert(sc[m], { id = s[".name"], mode = s.mode or "block", start = s.start, stop = s.stop,
				days = s.days or "1 2 3 4 5 6 7", enabled = s.enabled ~= "0", label = s.label })
		end
	end)
	return p, sc
end

function api_devices()
	local lv = live() or {}
	local lm = {}
	for _, d in ipairs(lv.devs or {}) do lm[d.m] = d end
	local p, sc = policy_map()
	local deny, dwhy = deny_state()
	local extra = {}
	for m in pairs(p) do extra[#extra + 1] = m end
	for m in pairs(sc) do extra[#extra + 1] = m end
	local dt, order = dev_table(extra)
	local an = appname_map()
	local list = {}
	for _, m in ipairs(order) do
		local d = dt[m]
		local l = lm[m]
		local po = p[m]
		d.up = l and l.u or 0
		d.down = l and l.d or 0
		d.conns = l and l.c or 0
		d.app = l and l.a or 0
		d.appname = (d.app and d.app > 0) and an[d.app] or ""
		d.url = l and l.h or ""
		d.blocked = po and po.blocked or false
		d.block_until = po and po["until"] or 0
		d.denied_now = deny[m] ~= nil
		d.deny_until = deny[m]
		d.deny_why = dwhy[m]
		d.apps = po and po.apps or ""
		d.classes = po and po.classes or ""
		d.sched = sc[m] or {}
		list[#list + 1] = d
	end
	out({ ok = true, now = os.time(), devs = list, gl = (cursor():get_all("mwrt_behav", "global") or {}) })
end

local function find_policy(c, mac)
	local found
	c:foreach("mwrt_behav", "policy", function(s)
		if normmac(s.mac) == mac then found = s[".name"] end
	end)
	if not found then
		found = c:add("mwrt_behav", "policy")
		c:set("mwrt_behav", found, "mac", mac)
	end
	return found
end

local function apply()
	os.execute("/usr/libexec/mwbehav-apply >/dev/null 2>&1")
end

function api_dev_set()
	local mac = normmac(fv("mac"))
	if not mac then return out({ ok = false, err = "mac" }) end
	local name = ((fv("name") or ""):gsub("[%c]", "")):sub(1, 48)
	local icon = ((fv("icon") or ""):gsub("[^%w_%-]", "")):sub(1, 24)
	local c = cursor()
	local sec
	c:foreach("mwrt_behav", "dev", function(s) if normmac(s.mac) == mac then sec = s[".name"] end end)
	if name == "" and icon == "" then
		if sec then c:delete("mwrt_behav", sec) end
	else
		if not sec then sec = c:add("mwrt_behav", "dev"); c:set("mwrt_behav", sec, "mac", mac) end
		c:set("mwrt_behav", sec, "name", name)
		if icon ~= "" then c:set("mwrt_behav", sec, "icon", icon) else c:delete("mwrt_behav", sec, "icon") end
	end
	c:commit("mwrt_behav")
	audit("dev_set", who(mac) .. (name ~= "" and (" 命名为「" .. name .. "」") or " 更新图标"))
	out({ ok = true })
end

-- minutes: 0=一直断网, >0 断网 N 分钟后自动恢复; off=1 恢复上网
function api_block()
	local mac = normmac(fv("mac"))
	if not mac then return out({ ok = false, err = "mac" }) end
	local c = cursor()
	local sec = find_policy(c, mac)
	if fv("off") == "1" then
		c:set("mwrt_behav", sec, "blocked", "0")
		c:delete("mwrt_behav", sec, "until")
	else
		local mins = math.floor(tonumber(fv("minutes")) or 0)
		c:set("mwrt_behav", sec, "blocked", "1")
		c:set("mwrt_behav", sec, "until", mins > 0 and tostring(os.time() + mins * 60) or "0")
		audit("block", who(mac) .. (mins > 0 and (" 断网 " .. mins .. " 分钟") or " 一直断网"))
	end
	c:commit("mwrt_behav")
	if fv("off") == "1" then audit("allow", who(mac) .. " 恢复上网") end
	apply()
	out({ ok = true })
end

function api_sched_set()
	local mac = normmac(fv("mac"))
	local a, b = fv("start") or "", fv("stop") or ""
	if not mac or not a:match("^%d%d?:%d%d$") or not b:match("^%d%d?:%d%d$") then return out({ ok = false, err = "param" }) end
	local days = ((fv("days") or ""):gsub("[^1-7 ]", ""))
	if days == "" then days = "1 2 3 4 5 6 7" end
	local mode = fv("mode") == "allow" and "allow" or "block"
	local c = cursor()
	local id = fv("id")
	if not (id and c:get("mwrt_behav", id) == "sched") then id = c:add("mwrt_behav", "sched") end
	c:set("mwrt_behav", id, "mac", mac)
	c:set("mwrt_behav", id, "mode", mode)
	c:set("mwrt_behav", id, "start", a)
	c:set("mwrt_behav", id, "stop", b)
	c:set("mwrt_behav", id, "days", days)
	c:set("mwrt_behav", id, "enabled", fv("enabled") == "0" and "0" or "1")
	c:set("mwrt_behav", id, "label", (((fv("label") or ""):gsub("[%c]", "")):sub(1, 32)))
	c:commit("mwrt_behav")
	if not fv("id") then audit("sched_set", string.format("%s 添加定时规则: %s %s-%s", who(mac), mode == "allow" and "仅允许上网" or "禁止上网", a, b)) end
	apply()
	out({ ok = true, id = id })
end

function api_sched_del()
	local id = fv("id")
	local c = cursor()
	if id and c:get("mwrt_behav", id) == "sched" then
		audit("sched_del", who(normmac(c:get("mwrt_behav", id, "mac") or "") or "") .. " 删除定时规则")
		c:delete("mwrt_behav", id); c:commit("mwrt_behav")
	end
	apply()
	out({ ok = true })
end

-- mac="*" 全网封禁; apps / classes 用空格分隔的数字
function api_app_block()
	local mac = fv("mac")
	local c = cursor()
	local apps = ((fv("apps") or ""):gsub("[^%d ]", ""))
	local cls = ((fv("classes") or ""):gsub("[^%d ]", ""))
	local sec
	if mac == "*" then
		c:foreach("mwrt_behav", "policy", function(s) if s.mac == "*" then sec = s[".name"] end end)
		if not sec then sec = c:add("mwrt_behav", "policy"); c:set("mwrt_behav", sec, "mac", "*") end
	else
		mac = normmac(mac)
		if not mac then return out({ ok = false, err = "mac" }) end
		sec = find_policy(c, mac)
	end
	if apps ~= "" then c:set("mwrt_behav", sec, "apps", apps) else c:delete("mwrt_behav", sec, "apps") end
	if cls ~= "" then c:set("mwrt_behav", sec, "classes", cls) else c:delete("mwrt_behav", sec, "classes") end
	c:commit("mwrt_behav")
	audit("app_set", (mac == "*" and "全部设备" or who(mac)) .. " 应用封禁: " .. (apps ~= "" and ("应用 " .. select(2, apps:gsub("%d+", "")) .. " 个") or "") .. (cls ~= "" and (" 分类 " .. select(2, cls:gsub("%d+", "")) .. " 个") or "") .. (apps == "" and cls == "" and "已清空" or ""))
	apply()
	out({ ok = true })
end

-- ------------------------------------------------------------ 历史
function api_history()
	local mac, appid = normmac(fv("mac")), tonumber(fv("app"))
	local rg = fv("range") or "24h"
	local since = range_since(rg)
	local hours, other = {}, {}
	local tu, td = 0, 0
	for l in lines(RUN .. "/hist.tsv") do
		local a = split(l)
		local h = tonumber(a[1])
		if h and h >= since - 3599 then
			local ap = tonumber(a[3]) or 0
			if (not mac or a[2] == mac) and (not appid or ap == appid) then
				local u, d = tonumber(a[4]) or 0, tonumber(a[5]) or 0
				acc(hours, h, u, d)
				acc(other, mac and ap or a[2], u, d)
				tu = tu + u; td = td + d
			end
		end
	end
	local an = appname_map()
	local macs = {}
	if not mac then for k in pairs(other) do macs[#macs + 1] = k end end
	local dt = dev_table(macs)
	local br = {}
	for k, v in pairs(other) do
		local nm
		if mac then nm = k == 0 and "其他/未识别" or (an[k] or tostring(k))
		else nm = dt[k] and (dt[k].cname or dt[k].hostname) or "" end
		br[#br + 1] = { k = k, name = nm, u = v[1], d = v[2], t = v[1] + v[2] }
	end
	table.sort(br, function(a, b) return a.t > b.t end)
	while #br > 15 do br[#br] = nil end
	local evs = {}
	for l in lines(RUN .. "/events.tsv") do
		local a = split(l)
		local ap = tonumber(a[3]) or 0
		local t = tonumber(a[1]) or 0
		if t >= since and (not mac or a[2] == mac) and (not appid or ap == appid) then
			evs[#evs + 1] = { t = t, mac = a[2], app = ap, host = a[4] or "" }
		end
	end
	local ev2 = {}
	for i = #evs, math.max(1, #evs - 99), -1 do
		local e = evs[i]
		e.appname = e.app > 0 and (an[e.app] or "") or ""
		local d = dt[e.mac]
		e.dev = d and (d.cname or d.hostname) or ""
		ev2[#ev2 + 1] = e
	end
	out({ ok = true, range = rg, since = since, hours = sorted_hours(hours), up = tu, down = td, rank = br, events = ev2 })
end

-- ------------------------------------------------------------ 应用库
function api_apps()
	local f = features()
	local cl = {}
	for _, n in ipairs(f.order) do cl[#cl + 1] = { id = n, key = f.classes[n].key, name = f.classes[n].name } end
	local blocked = {}
	local c = cursor()
	c:foreach("mwrt_behav", "policy", function(s)
		if s.mac == "*" then blocked = { apps = s.apps or "", classes = s.classes or "" } end
	end)
	local ver = ""
	for l in lines(f.path) do
		local v = l:match("^#version%s+(%S+)")
		if v then ver = v; break end
	end
	local nico = 0
	if f.custom then
		for _ in fs.dir("/etc/mwrt_behav/icons") or function() end do nico = nico + 1 end
	end
	out({ ok = true, classes = cl, apps = f.apps, version = ver, global_block = blocked, custom = f.custom, icons = nico })
end

-- ------------------------------------------------------------ 设置
function api_settings()
	local c = cursor()
	if not c:get("mwrt_behav", "global") then c:set("mwrt_behav", "global", "global") end
	if fv("enabled") then c:set("mwrt_behav", "global", "enabled", fv("enabled") == "1" and "1" or "0") end
	if fv("rec_url") then c:set("mwrt_behav", "global", "rec_url", fv("rec_url") == "1" and "1" or "0") end
	c:commit("mwrt_behav")
	os.execute("/etc/init.d/mwbehavd reload >/dev/null 2>&1")
	out({ ok = true })
end

-- ------------------------------------------------------------ 特征库导入 / 恢复内置
local FEAT_TMP = RUN .. "/feat.upload"

function api_feature_import()
	local hp = require "luci.http"
	local fp, total, toobig = nil, 0, false
	fs.mkdirr(RUN)
	os.remove(FEAT_TMP)
	hp.setfilehandler(function(meta, chunk, eof)
		if toobig then return end
		if chunk and #chunk > 0 then
			total = total + #chunk
			if total > 8 * 1024 * 1024 then
				toobig = true
				if fp then fp:close(); fp = nil end
				os.remove(FEAT_TMP)
				return
			end
			if not fp then fp = io.open(FEAT_TMP, "wb") end
			if fp then fp:write(chunk) end
		end
		if eof and fp then fp:close(); fp = nil end
	end)
	local _ = hp.formvalue("file")   -- 触发 multipart 解析
	if toobig then return out({ ok = false, err = "too_big" }) end
	if total == 0 then os.remove(FEAT_TMP); return out({ ok = false, err = "no_file" }) end
	local p = io.popen("/usr/libexec/mwbehav-feat import " .. FEAT_TMP .. " 2>/dev/null")
	local r = p and p:read("*a") or ""
	if p then p:close() end
	os.remove(FEAT_TMP)
	local j = jsonc.parse(r) or { ok = false, err = "failed" }
	if j.ok then audit("feature_import", string.format("导入应用特征库 %s（%s 个应用）", tostring(j.version or ""), tostring(j.apps or 0))) end
	out(j)
end

function api_feature_reset()
	os.execute("/usr/libexec/mwbehav-feat reset >/dev/null 2>&1")
	audit("feature_reset", "恢复内置应用特征库")
	out({ ok = true })
end

function api_clear()
	os.execute("kill -USR1 $(pidof mwbehavd) 2>/dev/null")
	out({ ok = true })
end

-- ------------------------------------------------------------ 审计日志(策略中心「操作记录」)
local AUDIT = "/etc/mwrt_behav/audit.log"

function audit(kind, text)
	fs.mkdirr("/etc/mwrt_behav")
	local f = io.open(AUDIT, "a")
	if not f then return end
	f:write(os.time(), "\tadmin\t", kind, "\t", (tostring(text):gsub("[%c]", " ")), "\n")
	f:close()
	local st = fs.stat(AUDIT)
	if st and st.size > 120000 then               -- 只留最近 400 条
		local keep = {}
		for l in lines(AUDIT) do keep[#keep + 1] = l end
		local g = io.open(AUDIT, "w")
		if g then
			for i = math.max(1, #keep - 399), #keep do g:write(keep[i], "\n") end
			g:close()
		end
	end
end

function who(m)
	local names = {}
	cursor():foreach("mwrt_behav", "dev", function(s) if normmac(s.mac) == m then names[1] = s.name end end)
	if names[1] and names[1] ~= "" then return names[1] .. "(" .. m .. ")" end
	return m
end

local function reload_daemon()
	os.execute("/etc/init.d/mwbehavd reload >/dev/null 2>&1")
end

-- ------------------------------------------------------------ 设备流量配额
local function quota_usage()
	local r = {}
	for l in lines(RUN .. "/quota.tsv") do
		local a = split(l)
		if a[1] then
			r[a[1]] = { ps = tonumber(a[2]) or 0, pe = tonumber(a[3]) or 0, up = tonumber(a[4]) or 0, dn = tonumber(a[5]) or 0,
				hit = a[8] == "1" }
		end
	end
	return r
end

local function quota_list()
	local use, r = quota_usage(), {}
	cursor():foreach("mwrt_behav", "quota", function(s)
		local m = normmac(s.mac)
		if not m then return end
		local u = use[m] or {}
		r[m] = { id = s[".name"], mac = m, ul = tonumber(s.ul) or 0, dl = tonumber(s.dl) or 0, period = s.period or "month",
			rday = tonumber(s.rday) or 1, enabled = s.enabled ~= "0", up = u.up or 0, dn = u.dn or 0, ps = u.ps or 0,
			pe = u.pe or 0, hit = u.hit and s.enabled ~= "0" or false }
	end)
	return r
end

local function find_sec(c, typ, key, val)
	local found
	c:foreach("mwrt_behav", typ, function(s)
		if (key == "mac" and normmac(s.mac) == val) or (key ~= "mac" and s[key] == val) then found = s[".name"] end
	end)
	return found
end

local function period_arg(v)
	if v == "day" or v == "week" then return v end
	return "month"
end

-- ul/dl 单位字节, 0 = 不限; enabled=0 暂停(不计、不断)
function api_quota_set()
	local mac = normmac(fv("mac"))
	if not mac then return out({ ok = false, err = "mac" }) end
	local ul = math.floor(math.max(0, tonumber(fv("ul")) or 0))
	local dl = math.floor(math.max(0, tonumber(fv("dl")) or 0))
	local c = cursor()
	local sec = find_sec(c, "quota", "mac", mac)
	if not sec then sec = c:add("mwrt_behav", "quota"); c:set("mwrt_behav", sec, "mac", mac) end
	c:set("mwrt_behav", sec, "ul", string.format("%d", ul))
	c:set("mwrt_behav", sec, "dl", string.format("%d", dl))
	c:set("mwrt_behav", sec, "period", period_arg(fv("period")))
	c:set("mwrt_behav", sec, "rday", tostring(math.max(1, math.min(28, math.floor(tonumber(fv("rday")) or 1)))))
	c:set("mwrt_behav", sec, "enabled", fv("enabled") == "0" and "0" or "1")
	c:commit("mwrt_behav")
	audit("quota_set", string.format("%s 设置流量配额: 上行 %s / 下行 %s / 周期 %s", who(mac),
		ul > 0 and (math.floor(ul / 1048576) .. " MB") or "不限", dl > 0 and (math.floor(dl / 1048576) .. " MB") or "不限",
		({ day = "每日", week = "每周", month = "每月" })[period_arg(fv("period"))]))
	reload_daemon()
	out({ ok = true })
end

function api_quota_del()
	local mac = normmac(fv("mac"))
	local c = cursor()
	local sec = mac and find_sec(c, "quota", "mac", mac)
	if sec then c:delete("mwrt_behav", sec); c:commit("mwrt_behav"); audit("quota_del", who(mac) .. " 取消流量配额") end
	reload_daemon()
	out({ ok = true })
end

-- 本周期用量清零(并立即放行因超额断网的设备)
function api_quota_reset()
	local mac = normmac(fv("mac"))
	local c = cursor()
	local sec = mac and find_sec(c, "quota", "mac", mac)
	if not sec then return out({ ok = false, err = "none" }) end
	c:set("mwrt_behav", sec, "reset_ts", tostring(os.time()))
	c:commit("mwrt_behav")
	audit("quota_reset", who(mac) .. " 手动清零本周期流量")
	reload_daemon()
	out({ ok = true })
end

function api_quotas()
	local q = quota_list()
	local arr = {}
	for _, v in pairs(q) do arr[#arr + 1] = v end
	table.sort(arr, function(a, b) return (a.up + a.dn) > (b.up + b.dn) end)
	local macs = {}
	for _, v in ipairs(arr) do macs[#macs + 1] = v.mac end
	local dt = dev_table(macs)
	for _, v in ipairs(arr) do
		local d = dt[v.mac] or {}
		v.name = d.cname or d.hostname or ""
		v.ip = d.ip
		v.online = d.online and true or false
		v.icon = d.icon
		v.vendor = d.vendor
	end
	out({ ok = true, now = os.time(), list = arr })
end

-- ------------------------------------------------------------ 卡槽流量
local function read_file(p)
	local f = io.open(p, "r")
	if not f then return "" end
	local s = f:read("*a") or ""
	f:close()
	return s
end

local function slot_model()
	local mc = read_file("/tmp/modconf.conf")
	local model = (read_file("/tmp/sysinfo/model") .. " " .. read_file("/tmp/sysinfo/board_name")):lower()
	local c2000 = model:find("c2000", 1, true) ~= nil
	if mc:find("MT5700", 1, true) then
		if c2000 then
			return { kind = "c2000", slots = { { id = "0", name = "外置 SIM 1" }, { id = "6", name = "外置 SIM 2" },
				{ id = "1", name = "内置 SIM" } }, alias = { ["2"] = "1" } }
		end
		return { kind = "single", slots = {} }
	end
	if mc:find("FM350", 1, true) then
		return { kind = "fm350", slots = { { id = "1", name = "外置 SIM 1" }, { id = "2", name = "外置 SIM 2" } }, alias = {} }
	end
	return { kind = "single", slots = {}, alias = {} }
end

-- 机型能力: 纯路由器机型(没有蜂窝模组/SIM 卡槽)前端据此隐藏「卡槽流量」页签和卡槽配额
local function has_cell()
	local mc = read_file("/tmp/modconf.conf")
	if mc:find("MT5700", 1, true) or mc:find("FM350", 1, true) then return true end
	local model = (read_file("/tmp/sysinfo/model") .. " " .. read_file("/tmp/sysinfo/board_name")):lower()
	-- C2000 系列与 C5800/C8-668/C8-688 自带模组(后者经内部网口接入, 没有 qmodem 段)
	if model:find("c2000", 1, true) or model:find("c5800", 1, true) or model:find("c8-6", 1, true) then return true end
	local c = cursor()
	local g = c:get_all("mwrt_behav", "global") or {}
	if g.cell_if and g.cell_if ~= "" then return true end
	local found = false
	c:foreach("qmodem", "modem-device", function() found = true end)
	if found then return true end
	local sj = jsonc.parse(read_file(RUN .. "/slot.json")) or {}
	return type(sj.ifs) == "table" and #sj.ifs > 0
end

function api_caps()
	out({ ok = true, cell = false })
end

function api_slot_stat()
	local j = jsonc.parse(read_file(RUN .. "/slot.json")) or {}
	local sm = slot_model()
	local alias = sm.alias or {}
	local single = #sm.slots == 0
	local cur = tostring(j.cur or "0")
	if alias[cur] then cur = alias[cur] end
	local days = {}
	local order = {}
	for _, s in ipairs(sm.slots) do order[#order + 1] = s.id end
	local function norm_slot(s)
		s = tostring(s)
		if single then return "*" end
		return alias[s] or s
	end
	-- 同一天同卡槽的记录合并(2 号内置卡并入 1 号), 单卡槽机型统一记到「本卡」
	local agg = {}
	for _, r in ipairs(j.days or {}) do
		local k = r[1] .. "|" .. norm_slot(r[2])
		local x = agg[k]
		if not x then x = { r[1], norm_slot(r[2]), 0, 0 }; agg[k] = x; days[#days + 1] = x end
		x[3] = x[3] + (r[3] or 0); x[4] = x[4] + (r[4] or 0)
	end
	local known = {}
	for _, s in ipairs(sm.slots) do known[s.id] = true end
	local slots = {}
	for _, s in ipairs(sm.slots) do slots[#slots + 1] = s end
	if single then slots[1] = { id = "*", name = "本卡" } end
	for _, r in ipairs(days) do
		if not single and not known[r[2]] then known[r[2]] = true; slots[#slots + 1] = { id = r[2], name = "卡槽 " .. r[2] } end
	end
	local qs = {}
	for _, q in ipairs(j.q or {}) do
		if alias[q.slot] then q.slot = alias[q.slot] end
		qs[#qs + 1] = q
	end
	local c = cursor()
	local meta = {}
	c:foreach("mwrt_behav", "slotq", function(s)
		meta[#meta + 1] = { slot = s.slot, limit = tonumber(s.limit) or 0, period = s.period or "month",
			rday = tonumber(s.rday) or 1, enabled = s.enabled ~= "0" }
	end)
	local g = c:get_all("mwrt_behav", "global") or {}
	out({ ok = true, now = os.time(), running = j.t ~= nil and (os.time() - j.t) < 60, clock = j.clock, kind = sm.kind,
		single = single, cur = single and "*" or cur, cur_raw = tostring(j.cur or ""), ifs = j.ifs or {}, rx = j.rx or 0, tx = j.tx or 0,
		slots = slots, quotas = qs, cfg = meta, days = days, cell_if = g.cell_if })
end

local function slot_ok(s)
	return s and (s == "*" or s:match("^%d$")) and s
end

function api_slot_set()
	local slot = slot_ok(fv("slot"))
	if not slot then return out({ ok = false, err = "slot" }) end
	local limit = math.floor(math.max(0, tonumber(fv("limit")) or 0))
	local c = cursor()
	local sec = find_sec(c, "slotq", "slot", slot)
	if limit == 0 then
		if sec then c:delete("mwrt_behav", sec); c:commit("mwrt_behav") end
		audit("slot_del", "卡槽 " .. slot .. " 取消流量配额")
	else
		if not sec then sec = c:add("mwrt_behav", "slotq"); c:set("mwrt_behav", sec, "slot", slot) end
		c:set("mwrt_behav", sec, "limit", string.format("%d", limit))
		c:set("mwrt_behav", sec, "period", period_arg(fv("period")))
		c:set("mwrt_behav", sec, "rday", tostring(math.max(1, math.min(28, math.floor(tonumber(fv("rday")) or 1)))))
		c:set("mwrt_behav", sec, "enabled", fv("enabled") == "0" and "0" or "1")
		c:commit("mwrt_behav")
		audit("slot_set", string.format("卡槽 %s 设置流量配额 %d MB / %s", slot, math.floor(limit / 1048576),
			({ day = "每日", week = "每周", month = "每月" })[period_arg(fv("period"))]))
	end
	reload_daemon()
	out({ ok = true })
end

function api_slot_reset()
	local slot = slot_ok(fv("slot"))
	local c = cursor()
	local sec = slot and find_sec(c, "slotq", "slot", slot)
	if not sec then return out({ ok = false, err = "none" }) end
	c:set("mwrt_behav", sec, "reset_ts", tostring(os.time()))
	c:commit("mwrt_behav")
	audit("slot_reset", "卡槽 " .. slot .. " 手动清零本周期流量")
	reload_daemon()
	out({ ok = true })
end

-- ------------------------------------------------------------ 策略中心: 汇总所有已设策略
local function fmt_mb(b)
	if b <= 0 then return "不限" end
	if b >= 1073741824 then return string.format("%.1f GB", b / 1073741824) end
	return string.format("%d MB", math.floor(b / 1048576))
end

function api_policies()
	local now = os.time()
	local c = cursor()
	local p, sc = policy_map()
	local deny = {}
	for l in lines(RUN .. "/deny.tsv") do
		local a = split(l)
		if a[1] then deny[a[1]] = { ["until"] = tonumber(a[2]) or 0, why = a[3] or "manual" } end
	end
	local an = appname_map()
	local f = features()
	local macs = {}
	for m in pairs(p) do macs[#macs + 1] = m end
	for m in pairs(sc) do macs[#macs + 1] = m end
	local q = quota_list()
	for m in pairs(q) do macs[#macs + 1] = m end
	local dt = dev_table(macs)
	local function nm(m)
		local d = dt[m] or {}
		return d.cname or d.hostname or ""
	end
	local lt = os.date("*t", now)
	local wday = (lt.wday + 5) % 7 + 1
	local minute = lt.hour * 60 + lt.min
	local function hm(s)
		local h, m = tostring(s or ""):match("^(%d+):(%d+)$")
		return h and (tonumber(h) * 60 + tonumber(m))
	end
	local rules = {}
	local function R(t) rules[#rules + 1] = t end
	for m, po in pairs(p) do
		if po.blocked then
			local left = po["until"] > 0 and (po["until"] - now) or 0
			if po["until"] == 0 or left > 0 then
				R({ kind = "block", mac = m, name = nm(m), id = po.name, ["until"] = po["until"], active = true })
			end
		end
		local an_, cn_ = {}, {}
		for x in tostring(po.apps or ""):gmatch("%d+") do an_[#an_ + 1] = an[tonumber(x)] or x end
		for x in tostring(po.classes or ""):gmatch("%d+") do
			local cc = f.classes[tonumber(x)]
			cn_[#cn_ + 1] = cc and cc.name or x
		end
		if #an_ + #cn_ > 0 then
			R({ kind = "app", mac = m, name = nm(m), id = po.name, apps = an_, classes = cn_, active = true })
		end
	end
	c:foreach("mwrt_behav", "policy", function(s)
		if s.mac == "*" then
			local an_, cn_ = {}, {}
			for x in tostring(s.apps or ""):gmatch("%d+") do an_[#an_ + 1] = an[tonumber(x)] or x end
			for x in tostring(s.classes or ""):gmatch("%d+") do
				local cc = f.classes[tonumber(x)]
				cn_[#cn_ + 1] = cc and cc.name or x
			end
			if #an_ + #cn_ > 0 then R({ kind = "app", mac = "*", name = "全部设备", id = s[".name"], apps = an_, classes = cn_, active = true }) end
		end
	end)
	for m, list in pairs(sc) do
		for _, s in ipairs(list) do
			local a, b = hm(s.start), hm(s.stop)
			local inwin = false
			if a and b then
				local days = {}
				for d in tostring(s.days):gmatch("%d") do days[tonumber(d)] = true end
				if a == b then inwin = days[wday] and true or false
				elseif a < b then inwin = (days[wday] and minute >= a and minute < b) and true or false
				else
					local prev = (wday + 5) % 7 + 1
					inwin = ((days[wday] and minute >= a) or (days[prev] and minute < b)) and true or false
				end
			end
			local eff = s.enabled and ((s.mode == "allow" and not inwin) or (s.mode ~= "allow" and inwin))
			R({ kind = "sched", mac = m, name = nm(m), id = s.id, mode = s.mode, start = s.start, stop = s.stop, days = s.days,
				enabled = s.enabled, label = s.label, active = eff and true or false, inwin = inwin })
		end
	end
	for m, v in pairs(q) do
		R({ kind = "quota", mac = m, name = nm(m), id = v.id, ul = v.ul, dl = v.dl, up = v.up, dn = v.dn, period = v.period,
			rday = v.rday, enabled = v.enabled, hit = v.hit, active = v.hit, pe = v.pe, brief = "上行 " .. fmt_mb(v.ul) .. " / 下行 " .. fmt_mb(v.dl) })
	end
	local qs = {}
	c:foreach("mwrt_behav", "slotq", function(s)
		R({ kind = "slot", mac = "", name = s.slot == "*" and "本卡" or ("卡槽 " .. tostring(s.slot)), id = s[".name"], slot = s.slot,
			limit = tonumber(s.limit) or 0, period = s.period or "month", enabled = s.enabled ~= "0", active = false })
	end)
	local sj = jsonc.parse(read_file(RUN .. "/slot.json")) or {}
	for _, r in ipairs(rules) do
		if r.kind == "slot" then
			for _, x in ipairs(sj.q or {}) do
				if x.slot == r.slot then r.used = x.used; r.hit = x.hit == 1; r.cut = x.cut == 1; r.active = r.cut end
			end
		end
	end
	local g = {}
	for _, r in ipairs(rules) do g[r.kind] = (g[r.kind] or 0) + 1 end
	local au = {}
	for l in lines(AUDIT) do
		local a = split(l)
		if a[1] then au[#au + 1] = { t = tonumber(a[1]) or 0, who = a[2], kind = a[3], text = a[4] or "" } end
	end
	local au2 = {}
	for i = #au, math.max(1, #au - 149), -1 do au2[#au2 + 1] = au[i] end
	out({ ok = true, now = now, rules = rules, counts = g, audit = au2 })
end

-- kind=block|sched|app|quota|slot; id=配置段名; app 类要 mac(或 *) 才能清
function api_policy_del()
	local kind, id = fv("kind"), fv("id")
	local c = cursor()
	if not id or c:get("mwrt_behav", id) == nil then return out({ ok = false, err = "none" }) end
	local typ = c:get("mwrt_behav", id)
	local mac = normmac(c:get("mwrt_behav", id, "mac") or "") or c:get("mwrt_behav", id, "mac")
	if kind == "block" and typ == "policy" then
		c:set("mwrt_behav", id, "blocked", "0"); c:delete("mwrt_behav", id, "until")
		audit("allow", who(mac) .. " 解除断网")
	elseif kind == "sched" and typ == "sched" then
		c:delete("mwrt_behav", id)
		audit("sched_del", who(mac) .. " 删除定时规则")
	elseif kind == "app" and typ == "policy" then
		c:delete("mwrt_behav", id, "apps"); c:delete("mwrt_behav", id, "classes")
		audit("app_del", (mac == "*" and "全部设备" or who(mac)) .. " 清除应用封禁")
	elseif kind == "quota" and typ == "quota" then
		c:delete("mwrt_behav", id)
		audit("quota_del", who(mac) .. " 取消流量配额")
	elseif kind == "slot" and typ == "slotq" then
		audit("slot_del", "卡槽 " .. tostring(c:get("mwrt_behav", id, "slot")) .. " 取消流量配额")
		c:delete("mwrt_behav", id)
	else
		return out({ ok = false, err = "kind" })
	end
	c:commit("mwrt_behav")
	reload_daemon()
	out({ ok = true })
end

function api_policy_toggle()
	local kind, id = fv("kind"), fv("id")
	local c = cursor()
	local typ = id and c:get("mwrt_behav", id)
	if not typ or (typ ~= "sched" and typ ~= "quota" and typ ~= "slotq") then return out({ ok = false, err = "none" }) end
	local on = fv("enabled") == "1"
	c:set("mwrt_behav", id, "enabled", on and "1" or "0")
	c:commit("mwrt_behav")
	audit(on and "enable" or "disable", (kind or typ) .. " " .. id .. (on and " 已启用" or " 已暂停"))
	reload_daemon()
	out({ ok = true })
end

function api_audit_clear()
	fs.remove(AUDIT)
	out({ ok = true })
end

-- ------------------------------------------------------------ 高级策略(配置 /etc/config/mwrt_adv, 运行时由 mwadv-apply 下发)
-- 保存流程: 校验输入 -> 暂存到 UCI -> 整体试编译(只要出现"改动前没有的"新错误就整笔放弃) -> 提交 -> mwadv-apply 下发。
-- 下发失败(nft 拒绝等)且改动前是好的: 配置文件回滚到改动前, 运行中的规则始终没有被动过。
local ADV = "mwrt_adv"
local ADV_FILE = "/etc/config/mwrt_adv"
local adv_mod

local function A()
	if not adv_mod then adv_mod = require "mwadv" end
	return adv_mod
end

local function adv_ensure()
	if not fs.access(ADV_FILE) then
		fs.writefile(ADV_FILE, "config global 'global'\n\toption enabled '0'\n\toption default_action 'allow'\n\toption default_log '0'\n")
	end
end

-- 配置事务锁(控制器之间串行); 运行时锁 adv.lock 由 mwadv-apply 自己持有, 两把不同, 避免嵌套死锁
local function adv_tx(fn)
	local nixio = require "nixio"
	fs.mkdirr(RUN)
	local lk = nixio.open(RUN .. "/adv.cfg.lock", "w")
	if lk then lk:lock("lock") end
	adv_ensure()
	local ok, res = pcall(fn)
	if lk then lk:lock("ulock"); lk:close() end
	if not ok then return out({ ok = false, err = "内部错误：" .. tostring(res) }) end
	return out(res)
end

local function jarg()
	local d = jsonc.parse(fv("data") or "")
	return type(d) == "table" and d or nil
end

local function clean(s)
	return (tostring(s or ""):gsub("%c", " "):gsub("^%s+", ""):gsub("%s+$", ""))
end

-- 字符串数组: 去首尾空白、去空项; 超过数量/长度上限直接报错(不静默截断)
local function slist(v, what, maxn, maxlen, errs)
	local r = {}
	if v == nil then return r end
	if type(v) ~= "table" then errs[#errs + 1] = what .. "格式不对"; return r end
	for _, x in ipairs(v) do
		if type(x) ~= "string" then
			errs[#errs + 1] = what .. "里有无效的项"
		else
			x = clean(x)
			if #x > maxlen then
				errs[#errs + 1] = what .. "里有过长的条目"
			elseif x ~= "" then
				r[#r + 1] = x
			end
		end
	end
	if #r > maxn then errs[#errs + 1] = what .. "最多 " .. maxn .. " 项，当前 " .. #r .. " 项"; return {} end
	return r
end

local function ucut(s, n)   -- 按字节截断但不切断汉字
	if #s <= n then return s end
	local i = n
	while i > 0 do
		local b = s:byte(i + 1)
		if not b or b < 0x80 or b >= 0xC0 then break end
		i = i - 1
	end
	return s:sub(1, i)
end

local function norm_hm(s)
	s = clean(s)
	local h, mi = s:match("^(%d%d?):(%d%d)$")
	if h then return string.format("%02d:%s", tonumber(h), mi) end
	return s
end

local function norm_days(v)
	local set = {}
	local function eat(x) for d in tostring(x):gmatch("[1-7]") do set[tonumber(d)] = true end end
	if type(v) == "table" then for _, x in ipairs(v) do eat(x) end else eat(v or "") end
	local t = {}
	for i = 1, 7 do if set[i] then t[#t + 1] = tostring(i) end end
	return table.concat(t, " ")
end

local function truthy(v) return v == true or v == 1 or v == "1" end

local RULE_DIMS = { "src", "dst", "saddr", "daddr", "svc", "app", "url" }
local DIM_LABEL = { src = "源区域/接口", dst = "目的区域/接口", saddr = "源地址", daddr = "目的地址", svc = "服务", app = "应用", url = "URL/域名" }

local function norm_rule(d, id, errs)
	local r = {
		id = id, name = clean(d.name), note = clean(d.note),
		enabled = d.enabled == nil or truthy(d.enabled),
		action = d.action, log = truthy(d.log),
		tdays = norm_days(d.tdays), tstart = norm_hm(d.tstart), tstop = norm_hm(d.tstop),
	}
	for _, f in ipairs(RULE_DIMS) do r[f] = slist(d[f], DIM_LABEL[f], 200, 64, errs) end
	-- "任意"之外的重复项去重(保持顺序)
	for _, f in ipairs(RULE_DIMS) do
		local seen, nl = {}, {}
		for _, x in ipairs(r[f]) do if not seen[x] then seen[x] = true; nl[#nl + 1] = x end end
		r[f] = nl
	end
	return r
end

local function adv_setopt(c, id, opt, v)
	if v == nil or v == "" then c:delete(ADV, id, opt) else c:set(ADV, id, opt, v) end
end
local function adv_setlist(c, id, opt, list)
	c:delete(ADV, id, opt)
	if #list > 0 then c:set(ADV, id, opt, list) end
end

local function wr_rule(c, id, r)
	c:set(ADV, id, "rule")
	c:set(ADV, id, "name", r.name)
	adv_setopt(c, id, "note", r.note)
	c:set(ADV, id, "enabled", r.enabled and "1" or "0")
	for _, f in ipairs(RULE_DIMS) do adv_setlist(c, id, f, r[f]) end
	c:set(ADV, id, "action", r.action)
	c:set(ADV, id, "log", r.log and "1" or "0")
	adv_setopt(c, id, "tdays", r.tdays); adv_setopt(c, id, "tstart", r.tstart); adv_setopt(c, id, "tstop", r.tstop)
end

local function wr_obj(c, id, o)
	c:set(ADV, id, "obj")
	c:set(ADV, id, "kind", o.kind)
	c:set(ADV, id, "group", o.group and "1" or "0")
	c:set(ADV, id, "name", o.name)
	adv_setopt(c, id, "note", o.note)
	adv_setopt(c, id, "tag", o.tag)
	adv_setlist(c, id, "ent", o.ent)
	adv_setlist(c, id, "mem", o.mem)
end

local seeded
local function adv_newid(prefix, taken)
	if not seeded then math.randomseed(os.time() % 2147483647); seeded = true end
	for _ = 1, 50 do
		local id = prefix .. string.format("%x", os.time() % 0xfffffff) .. string.format("%03x", math.random(0, 4095))
		if not taken(id) then return id end
	end
	return prefix .. string.format("%x", math.random(0, 0xfffffff))
end

-- 把规则整体按给定顺序重排(其余段保持原有相对顺序, 排在规则之前)
local function adv_reorder(c, rule_ids)
	local seq = {}
	c:foreach(ADV, nil, function(s)
		if s[".type"] ~= "rule" then seq[#seq + 1] = s[".name"] end
	end)
	for _, id in ipairs(rule_ids) do seq[#seq + 1] = id end
	for i, id in ipairs(seq) do c:reorder(ADV, id, i - 1) end
end

local function rule_ids(m)
	local t = {}
	for _, r in ipairs(m.rules) do t[#t + 1] = r.id end
	return t
end

-- 暂存后整体试编译: 返回"改动前没有"的新错误(中文)
local function new_problems(c, plan0, env)
	local a = A()
	local plan1 = a.compile(a.load(c), env)
	local before = {}
	for _, e in ipairs(plan0.errors) do before[e.rule .. "|" .. tostring(e.msg)] = true end
	local res = {}
	for _, e in ipairs(plan1.errors) do
		if not before[e.rule .. "|" .. tostring(e.msg)] then res[#res + 1] = "这个改动会让策略「" .. e.name .. "」出错：" .. tostring(e.msg) end
	end
	return res
end

-- 把草稿策略放进模型副本里试编译(草稿即使是停用状态也要查出问题)
local function rule_problems(m, env, r)
	local a = A()
	local errs, warns = a.check_rule(m, r, env)
	if #errs == 0 then
		local rr = {}
		for k, v in pairs(r) do rr[k] = v end
		rr.enabled = true
		local m2 = { g = m.g, objs = m.objs, ord = m.ord, rules = {}, rmap = {} }
		local replaced = false
		for _, x in ipairs(m.rules) do
			if x.id == rr.id then m2.rules[#m2.rules + 1] = rr; replaced = true else m2.rules[#m2.rules + 1] = x end
		end
		if not replaced then m2.rules[#m2.rules + 1] = rr end
		for _, x in ipairs(m2.rules) do m2.rmap[x.id] = x end
		local plan = a.compile(m2, env)
		local pr = plan.rules[rr.id]
		if pr and pr.state == "error" then errs[#errs + 1] = tostring(pr.msg) end
		if pr and pr.state == "no_iface" then warns[#warns + 1] = tostring(pr.msg) end
		for _, w in ipairs(plan.warnings) do if w.rule == rr.id then warns[#warns + 1] = tostring(w.msg) end end
	end
	return errs, warns
end

-- 下发; 这次改动让下发失败而改动前是好的 -> 配置回滚
local function adv_done(snap, extra)
	local a = A()
	local was_ok = a.status_read().ok ~= false
	os.execute("/usr/libexec/mwadv-apply >/dev/null 2>&1")
	local st = a.status_read()
	local res = extra or {}
	if st.ok == false and was_ok and snap then
		fs.writefile(ADV_FILE, snap)
		os.execute("/usr/libexec/mwadv-apply >/dev/null 2>&1")
		res.ok = false
		res.err = "保存失败：规则下发未通过（" .. tostring(st.error) .. "），已恢复为修改前的配置，当前运行的规则没有变化"
		return res
	end
	res.ok = true
	res.applied = st.ok ~= false
	if st.ok == false then res.apply_error = st.error end
	return res
end

-- ------------------------------------------------------------ 读取
local function hit_info(live, h, id, now)
	local cn, x = live["c_" .. id], h[id]
	if not cn and not x then return { p = 0, b = 0, t = 0 } end
	x = x or { bp = 0, bb = 0, lp = 0, lt = 0 }
	local lt = x.lt
	if cn and cn.p > x.lp then lt = now end
	return { p = x.bp + (cn and cn.p or x.lp), b = x.bb + (cn and cn.b or 0), t = lt }
end

local function is_any(l) return #l == 1 and l[1] == "*" end

local function status_public(st)
	local t = {}
	for k, v in pairs(st) do if k ~= "rules" then t[k] = v end end
	return t
end

function api_adv_get()
	adv_ensure()
	local a = A()
	local c = cursor()
	local now = os.time()
	local m = a.load(c)
	local st = a.status_read()
	local live = a.counters_live()
	local h = a.hits_load()
	local srules = st.rules or {}
	local light = fv("live") == "1"

	local rules, shadow_by = {}, nil
	for i, r in ipairs(m.rules) do
		local pr = srules[r.id]
		local row = {
			id = r.id, pos = i, state = pr and pr.state or "pending", msg = pr and pr.msg, warn = pr and pr.warn, n = pr and pr.n or 0,
			sched = a.sched_state(r, now), hit = hit_info(live, h, r.id, now),
		}
		if not light then
			row.name, row.note, row.enabled, row.action, row.log = r.name, r.note, r.enabled, r.action, r.log
			row.tdays, row.tstart, row.tstop = r.tdays, r.tstart, r.tstop
			for _, f in ipairs(RULE_DIMS) do row[f] = r[f] end
		end
		-- 上面有一条"全部任意、无时间计划、启用"的策略: 下面的策略永远轮不到
		if shadow_by and r.enabled then row.shadow = shadow_by end
		if not shadow_by and r.enabled and r.tdays == "" and r.tstart == "" and r.tstop == "" then
			local all = true
			for _, f in ipairs(RULE_DIMS) do if not is_any(r[f]) then all = false end end
			if all then shadow_by = r.id end
		end
		rules[#rules + 1] = row
	end

	local res = {
		ok = true, now = now, status = status_public(st), rules = rules,
		g = m.g, default_hit = hit_info(live, h, "default", now), quic_hit = hit_info(live, h, "quic", now),
	}
	if light then return out(res) end

	local env = a.build_env(c, now)
	local objs = {}
	for _, id in ipairs(m.ord) do
		local o = m.objs[id]
		local n, bad = 0, nil
		if a.KINDS[o.kind] then
			local ex, err = a.expand(m, id, o.kind)
			if ex then n = #ex else bad = err end
		else
			bad = "对象类型无效"
		end
		local refs = a.refs(m, id)
		objs[#objs + 1] = {
			id = id, kind = o.kind, group = o.group, name = o.name, note = o.note, tag = o.tag,
			ent = o.ent, mem = o.mem, n = n, bad = bad, refs = refs,
		}
	end
	local lib = a.app_lib()
	local napps, ncls = 0, #lib.corder
	for _ in pairs(lib.ids) do napps = napps + 1 end
	res.objs = objs
	res.zones = env.zone_list
	res.ifaces = env.if_list
	res.caps = {
		engine = env.url_engine, engine_ver = env.oaf_version, dpi_running = a.daemon_pid() ~= nil,
		lib_ok = lib.ok, napps = napps, ncls = ncls, lib_custom = features().custom,
		max_url = a.MAX_URL, clock_ok = env.clock_ok,
	}
	res.behav_enabled = (cursor():get("mwrt_behav", "global", "enabled") ~= "0")
	out(res)
end

-- ------------------------------------------------------------ 诊断
function api_adv_diag()
	adv_ensure()
	local a = A()
	local c = cursor()
	local now = os.time()
	local m = a.load(c)
	local env = a.build_env(c, now)
	local st = a.status_read()
	local lib = a.app_lib()
	local napps = 0
	for _ in pairs(lib.ids) do napps = napps + 1 end
	local checks = {}
	local function add(level, title, detail) checks[#checks + 1] = { level = level, title = title, detail = detail } end

	local bg_on = (c:get("mwrt_behav", "global", "enabled") ~= "0")
	if not bg_on then add("bad", "行为管理总开关已关闭", "高级策略随总开关一起停用，所有策略都不会生效。到「设置」里打开行为管理。") end
	if not m.g.enabled then add("warn", "高级策略已停用", "策略保留在配置里，但不会下发。") end

	if env.url_engine == "full" then
		add("ok", "识别引擎正常", "内核识别模块 " .. env.oaf_version .. "，支持任意长度的域名规则与通配符，大小写不敏感，自动忽略端口与结尾的点。")
	elseif env.url_engine == "basic" then
		add("warn", "识别引擎较旧", "内核识别模块 " .. env.oaf_version .. " 只能下发很短的域名规则（匹配表达式不超过 31 个字符），较长的域名会被忽略并在策略上提示。升级固件后恢复完整能力。")
	else
		add("bad", "识别引擎未加载", "没有检测到内核识别模块（/proc/sys/oaf/version）。应用与 URL/域名条件无法工作；地址、服务、区域条件不受影响。")
	end
	if a.daemon_pid() then
		add("ok", "识别守护进程运行中", "mwbehavd 正在运行，自定义域名特征随策略自动加载。")
	else
		add("bad", "识别守护进程没有运行", "应用与 URL/域名条件不会命中。可到「设置」里重新开启行为管理，或在终端执行 /etc/init.d/mwbehavd restart。")
	end
	if lib.ok then
		add("ok", "应用识别库就绪", "共 " .. napps .. " 个应用、" .. #lib.corder .. " 个分类（" .. (features().custom and "用户导入" or "内置") .. "特征库）。")
	else
		add("bad", "应用识别库为空", "没有读到特征库，应用对象无法使用。")
	end

	local table_present = os.execute("nft list table inet mwrt_adv >/dev/null 2>&1") == 0
	if st.ok == false then
		add("bad", "最近一次下发失败，运行的是上一份有效配置", tostring(st.error))
	elseif m.g.enabled and bg_on then
		if table_present then
			add("ok", "规则已下发", "nft 表 inet mwrt_adv 已接入转发路径（优先级 -5，先于系统防火墙 fw4 的转发规则），共 " .. (st.nrules or 0) .. " 条规则。")
		elseif st.idle then
			add("ok", "没有需要下发的规则", "没有任何生效的策略且默认放行，所以没有接入转发路径，也不增加任何开销。")
		else
			add("warn", "规则尚未下发", "点击「重新下发」，或等待最长 1 分钟的自动同步。")
		end
	end
	local nerr, nwarn = #(st.errors or {}), #(st.warnings or {})
	if nerr > 0 then add("bad", nerr .. " 条策略有错误", "有错误的策略会让整份配置暂不下发，请到策略列表里修正或停用它们。") end
	if nwarn > 0 then add("warn", nwarn .. " 条提示", "见策略列表上的黄色提示。") end

	add(m.g.default_action == "deny" and "warn" or "ok", "没有命中任何策略时：" .. (m.g.default_action == "deny" and "禁止" or "放行"),
		m.g.default_action == "deny" and "转发的流量只有被某条「放行」策略命中才能通过（路由器自身的管理访问不受影响）。" or "转发的流量只有被某条「禁止」策略命中才会被拦截。")
	if not env.clock_ok then add("warn", "路由器时间尚未校准", "带时间计划的策略暂不生效，联网校时后自动恢复。") end

	local fo = c:get("firewall", "@defaults[0]", "flow_offloading") == "1"
	local fohw = c:get("firewall", "@defaults[0]", "flow_offloading_hw") == "1"
	if fo or fohw then
		add("warn", "系统开启了流量卸载", "已经被卸载的连接不再经过转发规则：新建的策略对这些旧连接不起作用，应用/域名识别也可能漏判。如需严格管控请在「网络 → 防火墙」关闭软件/硬件流量卸载。")
	end
	if fs.access("/sys/kernel/debug/hnat/hnat_entry") then
		add("ok", "硬件加速（HNAT）", "策略变化后会自动清空加速表，已绑定的连接回到软件路径并按新策略处理；「放行」策略的命中计数在加速期间可能偏少。")
	end
	if m.g.quic_block then
		add("ok", "已阻止 QUIC（UDP 443）", "浏览器会回落到 TCP+TLS，域名才能被识别。")
	else
		add("warn", "QUIC（UDP 443）未处理", "识别引擎不解析 QUIC：走 QUIC 的网站不会命中 URL/域名与大部分应用条件。可在「默认动作与高级选项」里开启「阻止 QUIC」。")
	end
	add("ok", "IPv6", "策略同时作用于 IPv4 与 IPv6：地址对象按各自的地址族生成规则；只写了 IPv4 地址的策略不会限制同一设备的 IPv6 流量。")
	local nurl = st.nurl or 0
	add(nurl > a.MAX_URL * 0.8 and "warn" or "ok", "域名条目 " .. nurl .. " / " .. a.MAX_URL, "域名条目越多，每条新连接的识别开销越大。")

	local zones = {}
	for _, z in ipairs(env.zone_list) do zones[#zones + 1] = { name = z.name, devs = z.devs, nets = z.nets } end
	out({
		ok = true, now = now, checks = checks, status = status_public(st), zones = zones, table_present = table_present,
		g = m.g, engine = env.url_engine, engine_ver = env.oaf_version,
	})
end

-- ------------------------------------------------------------ 全局设置
function api_adv_global_save()
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m0 = a.load(c)
		local env = a.build_env(c, os.time())
		local plan0 = a.compile(m0, env)
		local da = fv("default_action")
		if da ~= nil and da ~= "allow" and da ~= "deny" then return { ok = false, err = "默认动作只能是放行或禁止" } end
		if da == "deny" and m0.g.default_action ~= "deny" and fv("confirm") ~= "1" then
			return { ok = false, err = "confirm", msg = "默认禁止后，转发的流量只有被某条「放行」策略命中才能通过，其余全部断开。路由器自身的管理页面不受影响。确定要切换吗？" }
		end
		local snap = fs.readfile(ADV_FILE)
		if not c:get(ADV, "global") then c:set(ADV, "global", "global") end
		local ch = {}
		local function flag(k, label)
			local v = fv(k)
			if v ~= nil then
				c:set(ADV, "global", k, v == "1" and "1" or "0")
				ch[#ch + 1] = label .. (v == "1" and "开" or "关")
			end
		end
		flag("enabled", "高级策略总开关：")
		flag("default_log", "默认动作记日志：")
		flag("quic_block", "阻止 QUIC：")
		if da then c:set(ADV, "global", "default_action", da); ch[#ch + 1] = "默认动作：" .. (da == "deny" and "禁止" or "放行") end
		local probs = new_problems(c, plan0, env)
		if #probs > 0 then return { ok = false, errs = probs } end
		c:commit(ADV)
		audit("adv_global", "高级策略设置 " .. table.concat(ch, "，"))
		return adv_done(snap, {})
	end)
end

-- ------------------------------------------------------------ 对象
local OBJ_FIELDS = { addr = { "saddr", "daddr" }, svc = { "svc" }, url = { "url" }, app = { "app" } }

local function obj_norm(d, errs)
	local o = {
		kind = d.kind, group = truthy(d.group), name = clean(d.name), note = clean(d.note), tag = clean(d.tag),
		ent = slist(d.ent, "条目", 500, 128, errs), mem = slist(d.mem, "成员", 200, 40, errs),
	}
	return o
end

function api_adv_obj_save()
	local d = jarg()
	if not d then return out({ ok = false, err = "参数无效" }) end
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local env = a.build_env(c, os.time())
		local plan0 = a.compile(m, env)
		local id = d.id
		local old
		if id ~= nil then
			old = type(id) == "string" and m.objs[id]
			if not old then return { ok = false, err = "对象不存在或已被删除，请刷新页面后重试" } end
		end
		local errs = {}
		local o = obj_norm(d, errs)
		if #errs > 0 then return { ok = false, errs = errs } end
		if old and old.kind ~= o.kind then return { ok = false, errs = { "已经创建的对象不能改变类型；请新建一个对象" } } end
		if not a.KINDS[o.kind] then return { ok = false, errs = { "对象类型不对" } } end
		o.id = id
		if not o.group then o.mem = {} end
		-- 把单个对象改成组: 如果它正被别的组当成员, 会造成组套组
		if old and o.group and not old.group then
			for _, r in ipairs(a.refs(m, id)) do
				if r.type == "group" then return { ok = false, errs = { "它正被组「" .. r.name .. "」当作成员，不能同时变成组（组里不能再放组）" } } end
			end
		end
		local cerrs, ents, notes, name = a.check_obj(m, o)
		if #cerrs > 0 then return { ok = false, errs = cerrs } end
		o.ent, o.name = ents, name
		local snap = fs.readfile(ADV_FILE)
		if not id then
			id = adv_newid("o", function(x) return m.objs[x] ~= nil or m.rmap[x] ~= nil or x == "global" end)
			o.id = id
		end
		wr_obj(c, id, o)
		local probs = new_problems(c, plan0, env)
		if #probs > 0 then return { ok = false, errs = probs } end
		c:commit(ADV)
		audit("adv_obj", (old and "修改" or "新建") .. (o.group and "组" or "对象") .. "「" .. name .. "」（" .. a.KINDS[o.kind] .. "）")
		local refs = old and #a.refs(m, id) or 0
		return adv_done(snap, { id = id, notes = notes, refs = refs })
	end)
end

function api_adv_obj_del()
	local id, rep = fv("id"), fv("replace")
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local env = a.build_env(c, os.time())
		local plan0 = a.compile(m, env)
		local o = id and m.objs[id]
		if not o then return { ok = false, err = "对象不存在或已被删除，请刷新页面后重试" } end
		local refs = a.refs(m, id)
		if #refs > 0 and (rep == nil or rep == "") then
			return { ok = false, err = "in_use", refs = refs, msg = "「" .. o.name .. "」正被 " .. #refs .. " 处引用，不能直接删除。可以先改掉引用，或选一个同类型的对象来替换后再删除。" }
		end
		local snap = fs.readfile(ADV_FILE)
		if #refs > 0 then
			local ro = m.objs[rep]
			if not ro or ro.kind ~= o.kind or rep == id then return { ok = false, err = "用来替换的对象不存在或类型不一致" } end
			for _, r in ipairs(refs) do
				if r.type == "group" and ro.group then return { ok = false, err = "「" .. ro.name .. "」本身是组，不能替换进组里（组里不能再放组）" } end
			end
			local function swap(list)
				local nl, seen, changed = {}, {}, false
				for _, x in ipairs(list) do
					if x == id then x = rep; changed = true end
					if not seen[x] then seen[x] = true; nl[#nl + 1] = x end
				end
				return nl, changed
			end
			for _, r in ipairs(m.rules) do
				for _, f in ipairs(OBJ_FIELDS[o.kind] or {}) do
					local nl, ch = swap(r[f])
					if ch then adv_setlist(c, r.id, f, nl) end
				end
			end
			for _, gid in ipairs(m.ord) do
				local go = m.objs[gid]
				if go.group then
					local nl, ch = swap(go.mem)
					if ch then adv_setlist(c, gid, "mem", nl) end
				end
			end
		end
		c:delete(ADV, id)
		local probs = new_problems(c, plan0, env)
		if #probs > 0 then return { ok = false, errs = probs } end
		c:commit(ADV)
		audit("adv_obj_del", "删除对象「" .. o.name .. "」" .. (#refs > 0 and ("，引用处已替换为「" .. m.objs[rep].name .. "」") or ""))
		return adv_done(snap, { replaced = #refs })
	end)
end

local function copy_name(base, exists, max)
	local n = 1
	while true do
		local suffix = (n == 1) and " 副本" or (" 副本 " .. n)
		local nm = ucut(base, max - #suffix) .. suffix
		if not exists(nm) then return nm end
		n = n + 1
	end
end

function api_adv_obj_copy()
	local id = fv("id")
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local o = id and m.objs[id]
		if not o then return { ok = false, err = "对象不存在或已被删除，请刷新页面后重试" } end
		local nid = adv_newid("o", function(x) return m.objs[x] ~= nil or m.rmap[x] ~= nil or x == "global" end)
		local nm = copy_name(o.name, function(s)
			for _, x in pairs(m.objs) do if x.kind == o.kind and x.name:lower() == s:lower() then return true end end
		end, 64)
		wr_obj(c, nid, { kind = o.kind, group = o.group, name = nm, note = o.note, tag = o.tag, ent = o.ent, mem = o.mem })
		c:commit(ADV)
		audit("adv_obj", "复制对象「" .. o.name .. "」为「" .. nm .. "」")
		return { ok = true, id = nid, name = nm }
	end)
end

-- ------------------------------------------------------------ 策略
function api_adv_rule_check()
	local d = jarg()
	if not d then return out({ ok = false, err = "参数无效" }) end
	local a = A()
	local c = cursor()
	local m = a.load(c)
	local env = a.build_env(c, os.time())
	local errs = {}
	local id = type(d.id) == "string" and m.rmap[d.id] and d.id or "rnew"
	local r = norm_rule(d, id, errs)
	local warns = {}
	if #errs == 0 then errs, warns = rule_problems(m, env, r) end
	out({ ok = #errs == 0, errs = errs, warns = warns })
end

function api_adv_rule_save()
	local d = jarg()
	if not d then return out({ ok = false, err = "参数无效" }) end
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local env = a.build_env(c, os.time())
		local plan0 = a.compile(m, env)
		local id = d.id
		if id ~= nil and not (type(id) == "string" and m.rmap[id]) then
			return { ok = false, err = "策略不存在或已被删除，请刷新页面后重试" }
		end
		local errs = {}
		local r = norm_rule(d, id or "rnew", errs)
		if #errs > 0 then return { ok = false, errs = errs } end
		local errs2, warns = rule_problems(m, env, r)
		if #errs2 > 0 then return { ok = false, errs = errs2, warns = warns } end
		local snap = fs.readfile(ADV_FILE)
		local isnew = not id
		if isnew then
			id = adv_newid("r", function(x) return m.objs[x] ~= nil or m.rmap[x] ~= nil or x == "global" end)
			r.id = id
		end
		wr_rule(c, id, r)
		if isnew then
			local ids = rule_ids(m)
			local at = #ids + 1
			if d.pos == "top" then at = 1
			elseif type(d.after) == "string" and m.rmap[d.after] then
				for i, x in ipairs(ids) do if x == d.after then at = i + 1 end end
			end
			if at <= #ids then
				table.insert(ids, at, id)
				adv_reorder(c, ids)
			end
		end
		local probs = new_problems(c, plan0, env)
		if #probs > 0 then return { ok = false, errs = probs } end
		c:commit(ADV)
		audit("adv_rule", (isnew and "新建" or "修改") .. "策略「" .. r.name .. "」：" .. (r.action == "deny" and "禁止" or "放行") .. (r.enabled and "" or "（已停用）"))
		return adv_done(snap, { id = id, warns = warns })
	end)
end

function api_adv_rule_del()
	local id = fv("id")
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local r = id and m.rmap[id]
		if not r then return { ok = false, err = "策略不存在或已被删除，请刷新页面后重试" } end
		local snap = fs.readfile(ADV_FILE)
		c:delete(ADV, id)
		c:commit(ADV)
		audit("adv_rule_del", "删除策略「" .. r.name .. "」")
		return adv_done(snap, {})
	end)
end

function api_adv_rule_toggle()
	local id, on = fv("id"), fv("enabled") == "1"
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local env = a.build_env(c, os.time())
		local plan0 = a.compile(m, env)
		local r = id and m.rmap[id]
		if not r then return { ok = false, err = "策略不存在或已被删除，请刷新页面后重试" } end
		local snap = fs.readfile(ADV_FILE)
		c:set(ADV, id, "enabled", on and "1" or "0")
		local probs = new_problems(c, plan0, env)
		if #probs > 0 then return { ok = false, errs = probs } end
		c:commit(ADV)
		audit("adv_rule", "策略「" .. r.name .. "」" .. (on and "已启用" or "已停用"))
		return adv_done(snap, {})
	end)
end

function api_adv_rule_copy()
	local id = fv("id")
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local env = a.build_env(c, os.time())
		local plan0 = a.compile(m, env)
		local r = id and m.rmap[id]
		if not r then return { ok = false, err = "策略不存在或已被删除，请刷新页面后重试" } end
		local snap = fs.readfile(ADV_FILE)
		local nid = adv_newid("r", function(x) return m.objs[x] ~= nil or m.rmap[x] ~= nil or x == "global" end)
		local nm = copy_name(r.name, function(s)
			for _, x in ipairs(m.rules) do if x.name == s then return true end end
		end, 64)
		local nr = {}
		for k, v in pairs(r) do nr[k] = v end
		nr.id, nr.name = nid, nm
		wr_rule(c, nid, nr)
		local ids = rule_ids(m)
		local at = #ids + 1
		for i, x in ipairs(ids) do if x == id then at = i + 1 end end
		if at <= #ids then table.insert(ids, at, nid); adv_reorder(c, ids) end
		local probs = new_problems(c, plan0, env)
		if #probs > 0 then return { ok = false, errs = probs } end
		c:commit(ADV)
		audit("adv_rule", "复制策略「" .. r.name .. "」为「" .. nm .. "」（排在原策略下方）")
		return adv_done(snap, { id = nid, name = nm })
	end)
end

-- 单条移动: dir=up|down|top|bottom 或 to=<目标位置, 从 1 开始>
function api_adv_rule_move()
	local id, dir, to = fv("id"), fv("dir"), tonumber(fv("to"))
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		local r = id and m.rmap[id]
		if not r then return { ok = false, err = "策略不存在或已被删除，请刷新页面后重试" } end
		local ids = rule_ids(m)
		local cur
		for i, x in ipairs(ids) do if x == id then cur = i end end
		local want = cur
		if dir == "up" then want = cur - 1
		elseif dir == "down" then want = cur + 1
		elseif dir == "top" then want = 1
		elseif dir == "bottom" then want = #ids
		elseif to then want = math.floor(to) end
		if want < 1 then want = 1 end
		if want > #ids then want = #ids end
		if want == cur then return { ok = true, pos = cur, unchanged = true } end
		local snap = fs.readfile(ADV_FILE)
		table.remove(ids, cur)
		table.insert(ids, want, id)
		adv_reorder(c, ids)
		c:commit(ADV)
		audit("adv_rule", "策略「" .. r.name .. "」的优先级调整到第 " .. want .. " 位")
		return adv_done(snap, { pos = want })
	end)
end

-- 整体排序(拖动): ids 必须正好是现有全部策略的一个排列, 否则说明页面数据已过期
function api_adv_rule_order()
	local ids = jsonc.parse(fv("ids") or "")
	adv_tx(function()
		local a = A()
		local c = cursor()
		local m = a.load(c)
		if type(ids) ~= "table" or #ids ~= #m.rules then return { ok = false, err = "stale", msg = "策略列表已经变化，请刷新后再调整顺序" } end
		local seen = {}
		for _, x in ipairs(ids) do
			if type(x) ~= "string" or not m.rmap[x] or seen[x] then return { ok = false, err = "stale", msg = "策略列表已经变化，请刷新后再调整顺序" } end
			seen[x] = true
		end
		local same = true
		for i, x in ipairs(rule_ids(m)) do if ids[i] ~= x then same = false end end
		if same then return { ok = true, unchanged = true } end
		local snap = fs.readfile(ADV_FILE)
		adv_reorder(c, ids)
		c:commit(ADV)
		audit("adv_rule", "调整了策略的优先级顺序")
		return adv_done(snap, {})
	end)
end

function api_adv_apply()
	adv_tx(function()
		os.execute("/usr/libexec/mwadv-apply >/dev/null 2>&1")
		local st = A().status_read()
		return { ok = st.ok ~= false, err = st.error, status = status_public(st) }
	end)
end

-- ------------------------------------------------------------ 计数 / 日志
function api_adv_counters_reset()
	local id = fv("id") or ""
	if id ~= "" and not id:match("^[%w_]+$") then return out({ ok = false, err = "参数无效" }) end
	local a = A()
	local nixio = require "nixio"
	fs.mkdirr(RUN)
	local lk = nixio.open(RUN .. "/adv.lock", "w")
	if lk then lk:lock("lock") end
	local live = a.counters_live()
	for cn in pairs(live) do
		local rid = cn:match("^c_([%w_]+)$")
		if rid and (id == "" or id == rid) then os.execute("nft reset counter inet mwrt_adv " .. cn .. " >/dev/null 2>&1") end
	end
	local h = a.hits_load()
	for rid, x in pairs(h) do
		if id == "" or id == rid then x.bp, x.bb, x.lp, x.lt = 0, 0, 0, 0 end
	end
	a.hits_save(h)
	if lk then lk:lock("ulock"); lk:close() end
	audit("adv_counters", id == "" and "清零了全部高级策略的命中计数" or ("清零了策略 " .. id .. " 的命中计数"))
	out({ ok = true })
end

local MON = { Jan = 1, Feb = 2, Mar = 3, Apr = 4, May = 5, Jun = 6, Jul = 7, Aug = 8, Sep = 9, Oct = 10, Nov = 11, Dec = 12 }

function api_adv_log()
	local a = A()
	local m = a.load(cursor())
	local names = {}
	for _, r in ipairs(m.rules) do names[r.id] = r.name end
	local rows = {}
	local p = io.popen("logread -e MWADV: 2>/dev/null")
	local all = {}
	if p then
		for l in p:lines() do all[#all + 1] = l end
		p:close()
	end
	for i = #all, math.max(1, #all - 299), -1 do
		local l = all[i]
		local mon, day, hh, mm, ss, yr, rest = l:match("^%a+%s+(%a+)%s+(%d+)%s+(%d+):(%d+):(%d+)%s+(%d+)%s+(.*)$")
		local rid, act, tail = (rest or ""):match("MWADV:([%w_]+):(%a+)%s+(.*)$")
		if rid and MON[mon] then
			rows[#rows + 1] = {
				ts = os.time({ year = tonumber(yr), month = MON[mon], day = tonumber(day), hour = tonumber(hh), min = tonumber(mm), sec = tonumber(ss) }),
				rule = rid, name = (rid == "default") and "默认动作" or (names[rid] or "（已删除的策略）"), action = act,
				src = tail:match("SRC=(%S+)"), dst = tail:match("DST=(%S+)"), proto = tail:match("PROTO=(%S+)"),
				sport = tail:match("SPT=(%d+)"), dport = tail:match("DPT=(%d+)"), ["in"] = tail:match("IN=(%S*)"), ["out"] = tail:match("OUT=(%S*)"),
			}
			if #rows >= 200 then break end
		end
	end
	out({ ok = true, rows = rows })
end
