-- mwadv: 行为管理「高级策略」的数据模型 / 校验 / 编译器(控制器与 mwadv-apply 共用)
--
-- 配置: /etc/config/mwrt_adv
--   config global 'global'   enabled / default_action(allow|deny) / default_log / quic_block(1=丢弃转发的 UDP 443, 让浏览器回落 TCP 以便识别域名)
--   config obj 'o...'        kind=addr|svc|url|app  group=0|1  name note tag  list ent  list mem
--   config rule 'r...'       name enabled note  list src  list dst  list saddr  list daddr  list svc  list app  list url
--                            action(allow|deny)  log  tdays tstart tstop
--   策略的先后顺序就是 UCI 里 rule 段的先后顺序(越靠前优先级越高, 命中第一条即停), 没有单独的数字优先级,
--   所以不会出现重复优先级; 排序接口用 uci reorder 重排。
--
-- 运行: 编译成 table inet mwrt_adv (forward 钩子, 优先级 -5)。每条策略按连接首包方向(ct original)匹配,
--   原方向与回程方向各生成一条, 两个方向的包得到同一个结论。应用/URL 通过 OAF 写入 ct mark 低 16 位的识别结果匹配。
local uci = require "uci"
local fs = require "nixio.fs"

local M = {}
M.CONF = "mwrt_adv"
M.RUN = "/tmp/mwbehav"
M.ETC = "/etc/mwrt_behav"
M.URL_ID_FILE = M.ETC .. "/adv_urlid.tsv"
M.FEAT_ADV = M.ETC .. "/adv_feature.cfg"
M.HITS = M.RUN .. "/adv_hits.tsv"
M.STATUS = M.RUN .. "/adv_status.json"
M.CUR_NFT = M.RUN .. "/adv.nft.cur"
M.ID_LO, M.ID_HI = 60001, 65000
M.MAX_URL = 200          -- 下发给识别引擎的域名条目上限(每条连接要逐条匹配, 太多会拖慢 CPU)
M.MAX_RULES_NFT = 3000   -- 一次最多生成的 nft 规则条数
M.KINDS = { addr = "地址", svc = "服务", url = "URL/域名", app = "应用" }

local function trim(s) return (tostring(s or ""):gsub("^%s+", ""):gsub("%s+$", "")) end
M.trim = trim

local function lst(v)
	if type(v) == "table" then return v end
	if v == nil or v == "" then return {} end
	local r = {}
	for x in tostring(v):gmatch("%S+") do r[#r + 1] = x end
	return r
end
M.lst = lst

local function read_file(p)
	local f = io.open(p, "r")
	if not f then return nil end
	local s = f:read("*a")
	f:close()
	return s
end
M.read_file = read_file

local function has_ctrl(s) return s:find("[%c]") ~= nil end

-- ------------------------------------------------------------ 地址
local function p4(s)
	local a, b, c, d = s:match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$")
	if not a or #a > 3 or #b > 3 or #c > 3 or #d > 3 then return nil end
	a, b, c, d = tonumber(a), tonumber(b), tonumber(c), tonumber(d)
	if a > 255 or b > 255 or c > 255 or d > 255 then return nil end
	return ((a * 256 + b) * 256 + c) * 256 + d
end
local function f4(n)
	return string.format("%d.%d.%d.%d", math.floor(n / 16777216) % 256, math.floor(n / 65536) % 256, math.floor(n / 256) % 256, n % 256)
end

local function groups6(str)
	local g = {}
	if str == "" then return g end
	for x in (str .. ":"):gmatch("(.-):") do
		if x == "" or #x > 4 then return nil end
		g[#g + 1] = tonumber(x, 16)
	end
	return g
end
local function p6(s)
	if not s:match("^[%x:]+$") or s:find(":::", 1, true) then return nil end
	local _, dbl = s:gsub("::", "")
	if dbl > 1 then return nil end
	local i, j = s:find("::", 1, true)
	if i then
		local h, t = groups6(s:sub(1, i - 1)), groups6(s:sub(j + 1))
		if not h or not t or #h + #t > 7 then return nil end
		local r = {}
		for k = 1, #h do r[k] = h[k] end
		for k = #h + 1, 8 - #t do r[k] = 0 end
		for k = 1, #t do r[8 - #t + k] = t[k] end
		return r
	end
	local g = groups6(s)
	if not g or #g ~= 8 then return nil end
	return g
end
local function cmp6(a, b)
	for i = 1, 8 do
		if a[i] ~= b[i] then return a[i] < b[i] and -1 or 1 end
	end
	return 0
end
local function f6(g)    -- 压缩写法(最长的连续 0 段缩成 ::)
	local bs, bl, cs, cl = 0, 0, 0, 0
	for i = 1, 8 do
		if g[i] == 0 then
			if cl == 0 then cs = i end
			cl = cl + 1
			if cl > bl then bs, bl = cs, cl end
		else cl = 0 end
	end
	if bl < 2 then
		local t = {}
		for i = 1, 8 do t[i] = string.format("%x", g[i]) end
		return table.concat(t, ":")
	end
	local a, b = {}, {}
	for i = 1, bs - 1 do a[#a + 1] = string.format("%x", g[i]) end
	for i = bs + bl, 8 do b[#b + 1] = string.format("%x", g[i]) end
	return table.concat(a, ":") .. "::" .. table.concat(b, ":")
end
local function f6full(g)
	local t = {}
	for i = 1, 8 do t[i] = string.format("%x", g[i]) end
	return table.concat(t, ":")
end
local function prefix6(g, len)
	local lo, hi = {}, {}
	for i = 1, 8 do
		local b = len - (i - 1) * 16
		if b < 0 then b = 0 elseif b > 16 then b = 16 end
		local blk = 2 ^ (16 - b)
		lo[i] = math.floor(g[i] / blk) * blk
		hi[i] = lo[i] + blk - 1
	end
	return lo, hi
end

-- 解析一个地址条目: 单地址 / CIDR / 起止范围, IPv4 与 IPv6 都支持
-- 返回 {fam, lo, hi, text, type, key} 或 nil, 错误原因
function M.parse_addr(s)
	s = trim(s)
	if s == "" then return nil, "地址不能为空" end
	if has_ctrl(s) or #s > 100 then return nil, "地址格式不对" end
	local a, b = s:match("^([%x%.:]+)%s*%-%s*([%x%.:]+)$")
	if a then
		local x, y = p4(a), p4(b)
		if x and y then
			if x > y then return nil, "地址范围的起始地址不能大于结束地址" end
			return { fam = 4, lo = x, hi = y, type = "range", text = f4(x) .. "-" .. f4(y), key = "4:" .. x .. "-" .. y }
		end
		local x6, y6 = p6(a), p6(b)
		if x6 and y6 then
			if cmp6(x6, y6) > 0 then return nil, "地址范围的起始地址不能大于结束地址" end
			return { fam = 6, lo = x6, hi = y6, type = "range", text = f6(x6) .. "-" .. f6(y6), key = "6:" .. f6full(x6) .. "-" .. f6full(y6) }
		end
		if (p4(a) or p6(a)) and (p4(b) or p6(b)) then return nil, "地址范围的两端必须同为 IPv4 或同为 IPv6" end
		return nil, "地址范围格式不对"
	end
	local ip, len = s:match("^([%x%.:]+)/(%d+)$")
	if ip then
		len = tonumber(len)
		local v4 = p4(ip)
		if v4 then
			if len > 32 then return nil, "IPv4 掩码长度应在 0-32 之间" end
			local blk = 2 ^ (32 - len)
			local lo = math.floor(v4 / blk) * blk
			local hi = lo + blk - 1
			return { fam = 4, lo = lo, hi = hi, type = "cidr", text = f4(lo) .. "/" .. len, norm = (lo ~= v4), key = "4:" .. lo .. "-" .. hi }
		end
		local v6 = p6(ip)
		if v6 then
			if len > 128 then return nil, "IPv6 前缀长度应在 0-128 之间" end
			local lo, hi = prefix6(v6, len)
			return { fam = 6, lo = lo, hi = hi, type = "cidr", text = f6(lo) .. "/" .. len, norm = (cmp6(lo, v6) ~= 0), key = "6:" .. f6full(lo) .. "-" .. f6full(hi) }
		end
		return nil, "网段地址格式不对"
	end
	local v4 = p4(s)
	if v4 then return { fam = 4, lo = v4, hi = v4, type = "host", text = f4(v4), key = "4:" .. v4 .. "-" .. v4 } end
	local v6 = p6(s)
	if v6 then return { fam = 6, lo = v6, hi = v6, type = "host", text = f6(v6), key = "6:" .. f6full(v6) .. "-" .. f6full(v6) } end
	if s:find("[/%-]") then return nil, "地址格式不对（范围写作 起始-结束，网段写作 地址/掩码）" end
	return nil, "不是有效的 IPv4 / IPv6 地址"
end

local function addr_text(e, fam)
	local function t(x) return fam == 4 and f4(x) or f6full(x) end
	if fam == 4 then
		if e.lo == e.hi then return f4(e.lo) end
		return f4(e.lo) .. "-" .. f4(e.hi)
	end
	if cmp6(e.lo, e.hi) == 0 then return t(e.lo) end
	return t(e.lo) .. "-" .. t(e.hi)
end

-- 合并重叠区间(nft 的区间集合不接受相互重叠的元素)
local function merge_addr(list, fam)
	table.sort(list, function(a, b)
		if fam == 4 then return a.lo < b.lo end
		return cmp6(a.lo, b.lo) < 0
	end)
	local out = {}
	for _, e in ipairs(list) do
		local last = out[#out]
		if last then
			local ov
			if fam == 4 then ov = e.lo <= last.hi else ov = cmp6(e.lo, last.hi) <= 0 end
			if ov then
				if fam == 4 then
					if e.hi > last.hi then last.hi = e.hi end
				elseif cmp6(e.hi, last.hi) > 0 then last.hi = e.hi end
			else
				out[#out + 1] = { lo = e.lo, hi = e.hi }
			end
		else
			out[#out + 1] = { lo = e.lo, hi = e.hi }
		end
	end
	return out
end

-- ------------------------------------------------------------ 服务
local PROTO = {
	tcp = { n = "tcp", ports = true, label = "TCP" },
	udp = { n = "udp", ports = true, label = "UDP" },
	tcpudp = { exp = { "tcp", "udp" }, ports = true, label = "TCP+UDP" },
	sctp = { n = "sctp", ports = true, label = "SCTP" },
	icmp = { n = "icmp", label = "ICMP" },
	icmpv6 = { n = "ipv6-icmp", label = "ICMPv6" },
	gre = { n = "gre", label = "GRE" },
	esp = { n = "esp", label = "ESP" },
	ah = { n = "ah", label = "AH" },
	any = { label = "任意协议" },
}
M.PROTO = PROTO

local function parse_ports(s)
	s = trim(s)
	if s == "" then return nil end
	local rs = {}
	for part in (s .. ","):gmatch("([^,]*),") do
		part = trim(part)
		if part ~= "" then
			local a, b = part:match("^(%d+)$"), nil
			if a then b = a else a, b = part:match("^(%d+)%s*%-%s*(%d+)$") end
			if not a then return nil, "端口格式不对：「" .. part .. "」（单个端口写 80，范围写 8000-8100，多个用逗号分隔）" end
			a, b = tonumber(a), tonumber(b)
			if a < 1 or b > 65535 then return nil, "端口应在 1-65535 之间" end
			if a > b then return nil, "端口范围的起始端口不能大于结束端口" end
			rs[#rs + 1] = { a, b }
		end
	end
	if #rs == 0 then return nil end
	table.sort(rs, function(x, y) return x[1] < y[1] end)
	local m = {}
	for _, r in ipairs(rs) do
		local l = m[#m]
		if l and r[1] <= l[2] + 1 then
			if r[2] > l[2] then l[2] = r[2] end
		else
			m[#m + 1] = { r[1], r[2] }
		end
	end
	local t = {}
	for _, r in ipairs(m) do t[#t + 1] = (r[1] == r[2]) and tostring(r[1]) or (r[1] .. "-" .. r[2]) end
	return m, table.concat(t, ",")
end

-- 服务条目存成 "协议|目的端口|源端口", 例: tcp|80,443,8000-8100|   udp||   ip47||(其他协议号)
function M.parse_svc(s)
	s = trim(s)
	if has_ctrl(s) or #s > 200 then return nil, "服务条目格式不对" end
	local pr, dp, sp = s:match("^([%w]+)|([^|]*)|([^|]*)$")
	if not pr then return nil, "服务条目格式不对" end
	pr = pr:lower()
	local e = { proto = pr }
	local num = pr:match("^ip(%d+)$")
	if num then
		num = tonumber(num)
		if num < 1 or num > 255 then return nil, "协议号应在 1-255 之间" end
		e.n = tostring(num)
		e.label = "协议 " .. num
		e.ports = false
	else
		local p = PROTO[pr]
		if not p then return nil, "不支持的协议：" .. pr end
		e.n, e.exp, e.label, e.ports = p.n, p.exp, p.label, p.ports
	end
	dp, sp = trim(dp), trim(sp)
	if (dp ~= "" or sp ~= "") and not e.ports then return nil, e.label .. " 没有端口，请清空端口" end
	if dp ~= "" then
		local r, t = parse_ports(dp)
		if not r then return nil, t or "目的端口格式不对" end
		e.dports, e.dtext = r, t
	end
	if sp ~= "" then
		local r, t = parse_ports(sp)
		if not r then return nil, t or "源端口格式不对" end
		e.sports, e.stext = r, t
	end
	e.text = pr .. "|" .. (e.dtext or "") .. "|" .. (e.stext or "")
	e.key = e.text
	return e
end

-- ------------------------------------------------------------ URL / 域名
-- 条目: example.com (只匹配这个域名本身) / *.example.com (匹配它的所有子域名, 不含 example.com 自己)
function M.parse_url(s)
	s = trim(s):lower()
	if s == "" then return nil, "域名不能为空" end
	if has_ctrl(s) or s:find("%s") then return nil, "域名里不能有空格" end
	if s:find("://", 1, true) or s:find("/", 1, true) or s:find("?", 1, true) then
		return nil, "只支持域名，不支持完整网址或路径（HTTPS 的路径无法读取）。请只填写域名，例如 www.example.com"
	end
	if s:find(":", 1, true) then return nil, "域名里不要带端口号" end
	if s:sub(-1) == "." then s = s:sub(1, -2) end
	local wild = false
	if s:sub(1, 2) == "*." then wild = true; s = s:sub(3) end
	if s:find("*", 1, true) then return nil, "通配符 * 只能写在最前面，形如 *.example.com" end
	if #s > 90 then return nil, "域名太长（最多 90 个字符）" end
	local n = 0
	for lab in (s .. "."):gmatch("(.-)%.") do
		n = n + 1
		if lab == "" then return nil, "域名里有空的标签（连续的点或开头的点）" end
		if #lab > 63 then return nil, "域名的每一段最长 63 个字符" end
		if not lab:match("^[a-z0-9_]$") and not lab:match("^[a-z0-9_][a-z0-9_%-]*[a-z0-9_]$") then
			if lab:find("[\128-\255]") then return nil, "请使用 punycode 形式的域名（xn--…），不要直接填中文域名" end
			return nil, "域名只能包含字母、数字、连字符和下划线"
		end
	end
	if n < 2 then return nil, "域名至少要有两段，例如 example.com（不能只写 com）" end
	if s:match("^%d+%.%d+%.%d+%.%d+$") then return nil, "这里填域名；要匹配 IP 地址请使用地址对象" end
	local e = { wild = wild, apex = s, text = (wild and "*." or "") .. s, key = (wild and "*." or "") .. s }
	-- 下发给识别引擎的正则(引擎只支持 . * + ? ^ $ [] \ ): 命中文本已被规范成小写、无端口、无结尾点号
	local esc = s:gsub("%.", "\\.")
	e.regex = wild and ("^.+\\." .. esc .. "$") or ("^" .. esc .. "$")
	if #e.regex > 120 then return nil, "域名太长，识别引擎无法下发" end
	return e
end

-- ------------------------------------------------------------ 应用库
local lib_cache, lib_sig
function M.app_lib()
	local path = M.ETC .. "/feature.cfg"
	if not fs.access(path) then path = "/etc/appfilter/feature.cfg" end
	local fst = fs.stat(path)
	local sig = path .. ":" .. (fst and (fst.mtime .. ":" .. fst.size) or "-")   -- 控制器进程常驻: 导入/恢复特征库后要重读
	if lib_cache and lib_sig == sig then return lib_cache end
	lib_sig = sig
	local ids, classes, corder = {}, {}, {}
	local f = io.open(path, "r")
	local cur
	if f then
		for l in f:lines() do
			local ck, cid, cn = l:match("^#class%s+(%S+)%s+(%d+)%s+(.+)$")
			if ck then
				classes[tonumber(cid)] = { id = tonumber(cid), key = ck, name = (cn:gsub("%s+$", "")) }
				corder[#corder + 1] = tonumber(cid)
				cur = tonumber(cid)
			else
				local id, nm = l:match("^(%d+)%s+([^:%[]+):")
				if id then ids[tonumber(id)] = { id = tonumber(id), name = (nm:gsub("%s+$", "")), cls = cur or math.floor(tonumber(id) / 1000) } end
			end
		end
		f:close()
	end
	lib_cache = { ids = ids, classes = classes, corder = corder, ok = next(ids) ~= nil }
	return lib_cache
end

function M.parse_app(s)
	s = trim(s)
	local kind, n = s:match("^(%a+):(%d+)$")
	if not kind or (kind ~= "app" and kind ~= "cls") then return nil, "应用条目格式不对" end
	n = tonumber(n)
	local lib = M.app_lib()
	if lib.ok then
		if kind == "app" and not lib.ids[n] then return nil, "识别库里没有这个应用（编号 " .. n .. "）" end
		if kind == "cls" and not lib.classes[n] then return nil, "识别库里没有这个分类（编号 " .. n .. "）" end
	end
	local e = { kind = kind, id = n, text = kind .. ":" .. n, key = kind .. ":" .. n }
	if kind == "app" then e.lo, e.hi = n, n else e.lo, e.hi = n * 1000, n * 1000 + 999 end
	return e
end

local PARSERS = { addr = M.parse_addr, svc = M.parse_svc, url = M.parse_url, app = M.parse_app }
function M.parse_entry(kind, s) return PARSERS[kind](s) end

-- ------------------------------------------------------------ 模型读取
function M.load(c)
	c = c or uci.cursor()
	local m = { g = {}, objs = {}, ord = {}, rules = {}, rmap = {} }
	local g = c:get_all(M.CONF, "global") or {}
	m.g = {
		enabled = g.enabled ~= "0",
		default_action = (g.default_action == "deny") and "deny" or "allow",
		default_log = g.default_log == "1",
		quic_block = g.quic_block == "1",
	}
	c:foreach(M.CONF, "obj", function(s)
		local o = {
			id = s[".name"], kind = s.kind, group = s.group == "1", name = s.name or "", note = s.note or "", tag = s.tag or "",
			ent = lst(s.ent), mem = lst(s.mem),
		}
		m.objs[o.id] = o
		m.ord[#m.ord + 1] = o.id
	end)
	c:foreach(M.CONF, "rule", function(s)
		local r = {
			id = s[".name"], name = s.name or "", note = s.note or "", enabled = s.enabled ~= "0",
			src = lst(s.src), dst = lst(s.dst), saddr = lst(s.saddr), daddr = lst(s.daddr),
			svc = lst(s.svc), app = lst(s.app), url = lst(s.url),
			action = (s.action == "deny") and "deny" or "allow", log = s.log == "1",
			tdays = s.tdays or "", tstart = s.tstart or "", tstop = s.tstop or "",
		}
		m.rules[#m.rules + 1] = r
		m.rmap[r.id] = r
	end)
	return m
end

-- 对象展开: 对象/组 -> 解析后的条目列表; 返回 entries, 错误
function M.expand(m, id, kind, seen)
	local o = m.objs[id]
	if not o then return nil, "引用的对象已不存在（" .. id .. "）" end
	if o.kind ~= kind then return nil, "对象「" .. o.name .. "」的类型不对" end
	local res = {}
	local function add(list)
		for _, t in ipairs(list) do
			local e, err = PARSERS[kind](t)
			if not e then return nil, "对象「" .. o.name .. "」里的条目「" .. t .. "」无效：" .. tostring(err) end
			res[#res + 1] = e
		end
		return true
	end
	local ok, err = add(o.ent)
	if not ok then return nil, err end
	if o.group then
		for _, mid in ipairs(o.mem) do
			local mo = m.objs[mid]
			if not mo then return nil, "组「" .. o.name .. "」里的成员已不存在（" .. mid .. "）" end
			if mo.group or mo.kind ~= kind then return nil, "组「" .. o.name .. "」的成员「" .. mo.name .. "」类型不对或是嵌套组" end
			local ok2, err2 = add(mo.ent)
			if not ok2 then return nil, err2 end
		end
	end
	return res
end

-- 谁在引用某个对象: 返回 {{type="rule"|"group", id=, name=}}
function M.refs(m, id)
	local r = {}
	local o = m.objs[id]
	if not o then return r end
	local field = ({ addr = { "saddr", "daddr" }, svc = { "svc" }, url = { "url" }, app = { "app" } })[o.kind] or {}
	for _, ru in ipairs(m.rules) do
		local hit
		for _, f in ipairs(field) do
			for _, x in ipairs(ru[f]) do if x == id then hit = f end end
		end
		if hit then r[#r + 1] = { type = "rule", id = ru.id, name = ru.name, field = hit } end
	end
	for _, oid in ipairs(m.ord) do
		local go = m.objs[oid]
		if go.group then
			for _, x in ipairs(go.mem) do if x == id then r[#r + 1] = { type = "group", id = go.id, name = go.name } end end
		end
	end
	return r
end

-- ------------------------------------------------------------ 校验
local function nm_ok(name)
	name = trim(name)
	if name == "" then return nil, "名称不能为空" end
	if has_ctrl(name) then return nil, "名称里不能有控制字符" end
	if #name > 64 then return nil, "名称太长（最多 64 个字节，约 21 个汉字）" end
	return name
end
M.name_ok = nm_ok

local SID = "^[%a_][%w_]*$"
function M.sid_ok(s) return type(s) == "string" and s:match(SID) ~= nil and #s <= 40 end

-- 校验单个对象(保存前调用); other = 其它对象表(查重名/成员); 返回 errors, 规整后的 ent, 提示
function M.check_obj(m, o)
	local errs, notes = {}, {}
	local name, e = nm_ok(o.name)
	if not name then errs[#errs + 1] = e else
		for _, x in pairs(m.objs) do
			if x.id ~= o.id and x.kind == o.kind and x.name:lower() == name:lower() then
				errs[#errs + 1] = "已经有同类型的对象叫「" .. name .. "」，名称不能重复"
				break
			end
		end
	end
	if not M.KINDS[o.kind] then errs[#errs + 1] = "对象类型不对" end
	if #trim(o.note) > 200 then errs[#errs + 1] = "备注最多 200 个字节" end
	if #trim(o.tag) > 32 or has_ctrl(trim(o.tag)) then errs[#errs + 1] = "分组标签最多 32 个字节" end
	local ents, seen = {}, {}
	if M.KINDS[o.kind] then
		for _, t in ipairs(o.ent) do
			local en, err = PARSERS[o.kind](t)
			if not en then errs[#errs + 1] = "「" .. trim(t) .. "」：" .. tostring(err)
			elseif seen[en.key] then errs[#errs + 1] = "条目「" .. en.text .. "」重复了"
			else
				seen[en.key] = true
				ents[#ents + 1] = en.text
				if en.norm then notes[#notes + 1] = "「" .. trim(t) .. "」已规整为网段 " .. en.text end
			end
		end
	end
	if o.group then
		local ms = {}
		for _, id in ipairs(o.mem) do
			local mo = m.objs[id]
			if not mo then errs[#errs + 1] = "成员对象不存在（" .. tostring(id) .. "）"
			elseif mo.group then errs[#errs + 1] = "「" .. mo.name .. "」本身是组，组里不能再放组"
			elseif mo.kind ~= o.kind then errs[#errs + 1] = "「" .. mo.name .. "」是" .. (M.KINDS[mo.kind] or "?") .. "对象，放不进这个组"
			elseif ms[id] then errs[#errs + 1] = "成员「" .. mo.name .. "」重复了"
			else ms[id] = true end
		end
		if #o.mem == 0 and #ents == 0 then errs[#errs + 1] = "组里至少要有一个成员或一个条目" end
	elseif #ents == 0 and #errs == 0 then
		errs[#errs + 1] = "至少要填一个条目"
	end
	return errs, ents, notes, name
end

local function dim_ok(list, what, errs, allow_any_label)
	if #list == 0 then
		errs[#errs + 1] = what .. "还没有选择：请选具体对象，或明确选「任意」"
		return false
	end
	local any = false
	for _, x in ipairs(list) do if x == "*" then any = true end end
	if any and #list > 1 then
		errs[#errs + 1] = what .. "里「任意」不能和其它选项同时选"
		return false
	end
	return true
end

-- 校验一条策略(保存前调用); env 可选(含 zones), 用来检查区域是否存在
function M.check_rule(m, r, env)
	local errs, warns = {}, {}
	local name, e = nm_ok(r.name)
	if not name then errs[#errs + 1] = e end
	if #trim(r.note) > 200 then errs[#errs + 1] = "备注最多 200 个字节" end
	if r.action ~= "allow" and r.action ~= "deny" then errs[#errs + 1] = "动作必须是放行或禁止" end
	local function scope(list, what)
		if not dim_ok(list, what, errs) then return end
		for _, x in ipairs(list) do
			if x ~= "*" then
				local k, v = x:match("^(%a+):(.+)$")
				if k == "zone" and v and v:match("^[%w_%-]+$") then
					if env and env.zones and not env.zones[v] then errs[#errs + 1] = what .. "里的安全域「" .. v .. "」不存在" end
				elseif k == "if" and v and v:match("^[%w%._%-]+$") and #v <= 15 then
					if env and env.ifaces and not env.ifaces[v] then warns[#warns + 1] = what .. "里的接口「" .. v .. "」当前不存在，策略会在它出现后才对它生效" end
				else
					errs[#errs + 1] = what .. "里有无效的选项「" .. tostring(x) .. "」"
				end
			end
		end
	end
	scope(r.src, "源区域/接口")
	scope(r.dst, "目的区域/接口")
	local function objdim(list, kind, what)
		if not dim_ok(list, what, errs) then return end
		for _, x in ipairs(list) do
			if x ~= "*" then
				local o = m.objs[x]
				if not o then errs[#errs + 1] = what .. "引用的对象已不存在"
				elseif o.kind ~= kind then errs[#errs + 1] = what .. "引用的对象类型不对"
				else
					local en, err = M.expand(m, x, kind)
					if not en then errs[#errs + 1] = err
					elseif #en == 0 then errs[#errs + 1] = what .. "里的「" .. o.name .. "」是空的，没有任何条目" end
				end
			end
		end
	end
	objdim(r.saddr, "addr", "源地址")
	objdim(r.daddr, "addr", "目的地址")
	objdim(r.svc, "svc", "服务")
	objdim(r.app, "app", "应用")
	objdim(r.url, "url", "URL/域名")
	local function isany(l) return #l == 1 and l[1] == "*" end
	if env and env.url_engine == "none" and
		((#r.app > 0 and not isany(r.app)) or (#r.url > 0 and not isany(r.url))) then
		errs[#errs + 1] = "当前 OAF 内核未向高级策略提供连接级应用与域名标识"
	end
	if not isany(r.app) and not isany(r.url) and #r.app > 0 and #r.url > 0 then
		errs[#errs + 1] = "应用和 URL/域名不能放在同一条策略里：识别引擎每条连接只记录一个识别结果。请拆成两条策略，或只保留其中一项"
	end
	if r.tdays ~= "" or r.tstart ~= "" or r.tstop ~= "" then
		local a, b = r.tstart:match("^(%d%d?):(%d%d)$"), r.tstop:match("^(%d%d?):(%d%d)$")
		if not a or not b or not r.tstart:match("^%d%d?:%d%d$") or tonumber(r.tstart:match("^(%d+)")) > 23 or tonumber(r.tstart:match(":(%d+)$")) > 59
			or not r.tstop:match("^%d%d?:%d%d$") or tonumber(r.tstop:match("^(%d+)")) > 23 or tonumber(r.tstop:match(":(%d+)$")) > 59 then
			errs[#errs + 1] = "时间计划的开始/结束时间格式应为 HH:MM"
		elseif r.tstart == r.tstop and false then
			errs[#errs + 1] = "开始和结束时间不能相同"
		end
		if not r.tdays:match("^[1-7 ]+$") then errs[#errs + 1] = "时间计划请至少选一个星期" end
	end
	return errs, warns
end

-- ------------------------------------------------------------ 编译
local function hm(s)
	local h, mi = tostring(s or ""):match("^(%d+):(%d+)$")
	if not h then return nil end
	return tonumber(h) * 60 + tonumber(mi)
end

function M.sched_state(r, now)
	if (r.tdays or "") == "" and (r.tstart or "") == "" and (r.tstop or "") == "" then return "always" end
	local a, b = hm(r.tstart), hm(r.tstop)
	if not a or not b then return "bad" end
	local lt = os.date("*t", now)
	local wday = (lt.wday + 5) % 7 + 1
	local minute = lt.hour * 60 + lt.min
	local days = {}
	for d in tostring(r.tdays):gmatch("%d") do days[tonumber(d)] = true end
	local inwin
	if a == b then inwin = days[wday]
	elseif a < b then inwin = days[wday] and minute >= a and minute < b
	else
		local prev = (wday + 5) % 7 + 1
		inwin = (days[wday] and minute >= a) or (days[prev] and minute < b)
	end
	return inwin and "in" or "out"
end

local function qs(s)   -- 进 nft 的名字一律只能是 [%w._-], 否则抛错(绝不拼接未校验的内容)
	if not s:match("^[%w%._%-]+$") then error("unsafe token: " .. tostring(s)) end
	return '"' .. s .. '"'
end

local function dev_set(list)
	local t = {}
	for _, d in ipairs(list) do t[#t + 1] = qs(d) end
	return "{ " .. table.concat(t, ", ") .. " }"
end

local function svc_frags(ents)
	local protoonly, dset, sset, both = {}, {}, {}, {}
	local po, dl, sl = {}, {}, {}
	local seen = {}
	local function once(k) if seen[k] then return false end seen[k] = true; return true end
	local function rtext(rs)
		local t = {}
		for _, r in ipairs(rs) do t[#t + 1] = (r[1] == r[2]) and tostring(r[1]) or (r[1] .. "-" .. r[2]) end
		return t
	end
	for _, e in ipairs(ents) do
		if e.proto == "any" then return nil, true end
		local protos = e.exp or { e.n }
		if not e.dports and not e.sports then
			for _, p in ipairs(protos) do if once("p" .. p) then po[#po + 1] = p end end
		elseif e.dports and not e.sports then
			for _, p in ipairs(protos) do for _, r in ipairs(rtext(e.dports)) do if once("d" .. p .. r) then dl[#dl + 1] = p .. " . " .. r end end end
		elseif e.sports and not e.dports then
			for _, p in ipairs(protos) do for _, r in ipairs(rtext(e.sports)) do if once("s" .. p .. r) then sl[#sl + 1] = p .. " . " .. r end end end
		else
			for _, p in ipairs(protos) do
				if once("b" .. p .. e.dtext .. e.stext) then
					both[#both + 1] = "ct original protocol " .. p .. " ct original proto-dst { " .. table.concat(rtext(e.dports), ", ") ..
						" } ct original proto-src { " .. table.concat(rtext(e.sports), ", ") .. " }"
				end
			end
		end
	end
	local fr = {}
	if #po > 0 then fr[#fr + 1] = "ct original protocol { " .. table.concat(po, ", ") .. " }" end
	if #dl > 0 then fr[#fr + 1] = "ct original protocol . ct original proto-dst { " .. table.concat(dl, ", ") .. " }" end
	if #sl > 0 then fr[#fr + 1] = "ct original protocol . ct original proto-src { " .. table.concat(sl, ", ") .. " }" end
	for _, b in ipairs(both) do fr[#fr + 1] = b end
	return fr, false
end

local function addr_side(m, ids, errs_out)
	-- 返回 any(bool), v4列表, v6列表
	if #ids == 1 and ids[1] == "*" then return true end
	local v4, v6 = {}, {}
	for _, id in ipairs(ids) do
		local es, err = M.expand(m, id, "addr")
		if not es then errs_out[#errs_out + 1] = err
		else
			for _, e in ipairs(es) do
				if e.fam == 4 then v4[#v4 + 1] = e else v6[#v6 + 1] = e end
			end
		end
	end
	return false, merge_addr(v4, 4), merge_addr(v6, 6)
end

local function addr_set(list, fam)
	local t = {}
	for _, e in ipairs(list) do t[#t + 1] = addr_text(e, fam) end
	return "{ " .. table.concat(t, ", ") .. " }"
end

local function markset(ranges)
	table.sort(ranges, function(a, b) return a[1] < b[1] end)
	local mg = {}
	for _, r in ipairs(ranges) do
		local l = mg[#mg]
		if l and r[1] <= l[2] + 1 then
			if r[2] > l[2] then l[2] = r[2] end
		else mg[#mg + 1] = { r[1], r[2] } end
	end
	local t = {}
	for _, r in ipairs(mg) do t[#t + 1] = (r[1] == r[2]) and tostring(r[1]) or (r[1] .. "-" .. r[2]) end
	return "{ " .. table.concat(t, ", ") .. " }"
end

-- URL 条目 -> 自定义识别 ID(稳定分配, 持久化在 adv_urlid.tsv)
function M.load_urlids()
	local map, used, nxt = {}, {}, M.ID_LO
	local f = io.open(M.URL_ID_FILE, "r")
	if f then
		for l in f:lines() do
			local id, key = l:match("^(%d+)\t(.+)$")
			id = tonumber(id)
			if id and id >= M.ID_LO and id <= M.ID_HI then map[key] = id; used[id] = key; if id >= nxt then nxt = id + 1 end end
		end
		f:close()
	end
	return { map = map, used = used, nxt = nxt }
end
local function alloc_id(st, key)
	if st.map[key] then return st.map[key] end
	local id
	if st.nxt <= M.ID_HI then id = st.nxt; st.nxt = st.nxt + 1
	else
		-- 编号用尽: 回收已经不在任何策略里的旧条目
		for k, v in pairs(st.map) do
			if not (st.live and st.live[k]) then st.map[k] = nil; st.used[v] = nil end
		end
		for i = M.ID_LO, M.ID_HI do if not st.used[i] then id = i; break end end
	end
	if not id then return nil end
	st.map[key] = id; st.used[id] = key
	return id
end

-- 编译全部策略。env = { zones = {name={devs}}, now=, clock_ok=, url_engine="full"|"basic"|"none", urlids=<load_urlids() 的副本> }
-- 返回 plan = { nft(string), rules = {id -> {state=, n=, msg=}}, errors = {...}, warnings = {...}, urls = {key -> {id=, regex=}} }
function M.compile(m, env)
	local plan = { rules = {}, errors = {}, warnings = {}, urls = {}, nrules = 0 }
	local T = {}
	local function w(s) T[#T + 1] = s end
	if not m.g.enabled then plan.nft = nil; plan.off = true; return plan end
	local ust = env.urlids or M.load_urlids()
	ust.live = plan.urls
	local counters = {}
	local body = {}
	local nbody = 0
	local url_count = 0

	for pos, r in ipairs(m.rules) do
		local st = { state = "active", n = 0, pos = pos }
		plan.rules[r.id] = st
		local rerrs = {}
		if not M.sid_ok(r.id) then
			st.state = "error"; st.msg = "策略编号无效"
		elseif not r.enabled then
			st.state = "disabled"
		else
			local cerrs = M.check_rule(m, r, env)
			if #cerrs > 0 then
				st.state = "error"; st.msg = cerrs[1]
			else
				local sch = M.sched_state(r, env.now)
				if sch == "bad" then st.state = "error"; st.msg = "时间计划无效"
				elseif sch ~= "always" and not env.clock_ok then st.state = "sched_wait"; st.msg = "路由器时间尚未校准，带时间计划的策略暂不生效"
				elseif sch == "out" then st.state = "sched_wait"; st.msg = "不在计划时间内" end
			end
		end
		if st.state == "active" or st.state == "sched_wait" then
			-- URL 条目要无论是否在时间窗内都下发给引擎(避免窗口翻转时重载识别库)
			local isany = function(l) return #l == 1 and l[1] == "*" end
			local urlids = {}
			if not isany(r.url) then
				for _, id in ipairs(r.url) do
					local es = M.expand(m, id, "url")
					for _, e in ipairs(es or {}) do
						if not plan.urls[e.key] then
							if env.url_engine == "none" then
								st.warn = "识别引擎未运行，URL/域名策略暂不生效"
							else
								local okreg = (env.url_engine == "full") or (#e.regex <= 31)
								if not okreg then
									st.warn = "当前内核识别模块较旧，域名「" .. e.text .. "」过长无法下发（升级固件后生效）"
								else
									url_count = url_count + 1
									if url_count > M.MAX_URL then
										st.state = "error"; st.msg = "URL/域名条目总数超过上限 " .. M.MAX_URL; break
									end
									local nid = alloc_id(ust, e.key)
									if not nid then st.state = "error"; st.msg = "自定义识别编号用尽"; break end
									plan.urls[e.key] = { id = nid, regex = e.regex, text = e.text }
								end
							end
						end
						if plan.urls[e.key] then urlids[#urlids + 1] = plan.urls[e.key].id end
					end
					if st.state == "error" then break end
				end
			end
			if st.state == "active" then
				local errs2 = {}
				-- 区域/接口 -> 设备名
				local function side(list)
					if #list == 1 and list[1] == "*" then return true end
					local devs, seen = {}, {}
					for _, x in ipairs(list) do
						local k, v = x:match("^(%a+):(.+)$")
						if k == "zone" then
							for _, d in ipairs(env.zones[v] or {}) do if not seen[d] then seen[d] = true; devs[#devs + 1] = d end end
						elseif k == "if" and not seen[v] then seen[v] = true; devs[#devs + 1] = v end
					end
					return false, devs
				end
				local sany, sdev = side(r.src)
				local dany, ddev = side(r.dst)
				local sa_any, s4, s6 = addr_side(m, r.saddr, errs2)
				local da_any, d4, d6 = addr_side(m, r.daddr, errs2)
				local frs, svc_any = nil, true
				if not isany(r.svc) then
					local ents = {}
					for _, id in ipairs(r.svc) do
						local es, err = M.expand(m, id, "svc")
						if not es then errs2[#errs2 + 1] = err else for _, e in ipairs(es) do ents[#ents + 1] = e end end
					end
					frs, svc_any = svc_frags(ents)
				end
				local markm
				if not isany(r.app) then
					local rs = {}
					for _, id in ipairs(r.app) do
						local es, err = M.expand(m, id, "app")
						if not es then errs2[#errs2 + 1] = err else for _, e in ipairs(es) do rs[#rs + 1] = { e.lo, e.hi } end end
					end
					if #rs > 0 then markm = "ct mark & 0xffff " .. markset(rs) end
				elseif not isany(r.url) then
					local rs = {}
					for _, id in ipairs(urlids) do rs[#rs + 1] = { id, id } end
					if #rs > 0 then markm = "ct mark & 0xffff " .. markset(rs)
					else st.state = "error"; st.msg = st.warn or "没有可下发的 URL/域名条目" end
				end
				if #errs2 > 0 then st.state = "error"; st.msg = errs2[1] end
				if st.state == "active" and not sany and #sdev == 0 then st.state = "no_iface"; st.msg = "源区域当前没有可用接口（例如对应的拨号/蜂窝接口未连接）" end
				if st.state == "active" and not dany and #ddev == 0 then st.state = "no_iface"; st.msg = "目的区域当前没有可用接口（例如对应的拨号/蜂窝接口未连接）" end
				if st.state == "active" then
					-- 地址族: 两端都"任意"时不限族, 否则按有条目的族各生成一份
					local fams = {}
					if sa_any and da_any then fams[1] = { nil }
					else
						local function has(sa, l) return sa or #l > 0 end
						if has(sa_any, s4) and has(da_any, d4) then fams[#fams + 1] = { 4, s4, d4 } end
						if has(sa_any, s6) and has(da_any, d6) then fams[#fams + 1] = { 6, s6, d6 } end
						if #fams == 0 then st.state = "error"; st.msg = "条件互相矛盾：源地址与目的地址没有共同的地址族（IPv4 / IPv6）" end
					end
					if st.state == "active" then
						local fr = svc_any and { "" } or frs
						if not svc_any and #fr == 0 then st.state = "error"; st.msg = "服务对象里没有有效条目" end
						if st.state == "active" then
							local cname = "c_" .. r.id
							counters[#counters + 1] = cname
							local verdict = (r.action == "deny") and "drop" or "accept"
							local dirs
							if sany and dany then dirs = { { "", "" } }
							else
								-- 原方向: 入口是源侧、出口是目的侧; 回程方向反过来, 两个方向得到同一个结论
								local o, rp = {}, {}
								if not sany then o[#o + 1] = "iifname " .. dev_set(sdev) end
								if not dany then o[#o + 1] = "oifname " .. dev_set(ddev); rp[#rp + 1] = "iifname " .. dev_set(ddev) end
								if not sany then rp[#rp + 1] = "oifname " .. dev_set(sdev) end
								dirs = { { table.concat(o, " "), "ct direction original" }, { table.concat(rp, " "), "ct direction reply" } }
							end
							for _, f in ipairs(fams) do
								local am = {}
								if f[1] == 4 then
									am[#am + 1] = "meta nfproto ipv4"
									if not sa_any then am[#am + 1] = "ct original ip saddr " .. addr_set(f[2], 4) end
									if not da_any then am[#am + 1] = "ct original ip daddr " .. addr_set(f[3], 4) end
								elseif f[1] == 6 then
									am[#am + 1] = "meta nfproto ipv6"
									if not sa_any then am[#am + 1] = "ct original ip6 saddr " .. addr_set(f[2], 6) end
									if not da_any then am[#am + 1] = "ct original ip6 daddr " .. addr_set(f[3], 6) end
								end
								for _, s in ipairs(fr) do
									local parts = {}
									for _, p in ipairs({ table.concat(am, " "), s, markm or "" }) do if p ~= "" then parts[#parts + 1] = p end end
									local common = table.concat(parts, " ")
									for di, d in ipairs(dirs) do
										local pre = {}
										if d[1] ~= "" then pre[#pre + 1] = d[1] end
										if d[2] ~= "" then pre[#pre + 1] = d[2] end
										if common ~= "" then pre[#pre + 1] = common end
										local line = table.concat(pre, " ")
										if r.log and di == 1 then
											local lg = line
											if d[2] == "" then lg = line .. (line ~= "" and " " or "") .. "ct direction original" end
											body[#body + 1] = "    " .. lg .. ' limit rate 6/minute burst 3 packets log prefix "MWADV:' .. r.id .. ":" .. r.action .. ' "'
											st.n = st.n + 1; nbody = nbody + 1
										end
										body[#body + 1] = "    " .. line .. (line ~= "" and " " or "") .. 'counter name "' .. cname .. '" ' .. verdict .. ' comment "' .. r.id .. '"'
										st.n = st.n + 1; nbody = nbody + 1
									end
								end
							end
							if nbody > M.MAX_RULES_NFT then st.state = "error"; st.msg = "条件组合展开后规则过多，请精简对象" end
						end
					end
				end
			end
		end
		if st.state ~= "active" and st.n > 0 then
			-- 编译中途失败: 撤销这条策略已写入的行
			for _ = 1, st.n do body[#body] = nil end
			nbody = nbody - st.n
			st.n = 0
			for i = #counters, 1, -1 do if counters[i] == "c_" .. r.id then table.remove(counters, i) end end
		elseif st.state ~= "active" then
			for i = #counters, 1, -1 do if counters[i] == "c_" .. r.id then table.remove(counters, i) end end
		end
		if st.state == "error" then plan.errors[#plan.errors + 1] = { rule = r.id, name = r.name, msg = st.msg } end
		if st.warn then plan.warnings[#plan.warnings + 1] = { rule = r.id, name = r.name, msg = st.warn } end
	end

	-- 兜底: 没有命中任何策略的流量
	local def = m.g.default_action
	plan.urlids = ust
	plan.nrules = nbody
	if nbody == 0 and def == "allow" and not m.g.default_log and not m.g.quic_block then
		-- 没有任何生效的策略且默认放行: 不接入转发路径, 不增加任何开销
		plan.idle = true
		return plan
	end
	table.insert(counters, 1, "c_default")
	if m.g.quic_block then table.insert(counters, 2, "c_quic") end
	w("add table inet mwrt_adv")
	w("delete table inet mwrt_adv")
	w("table inet mwrt_adv {")
	for _, cn in ipairs(counters) do w("  counter " .. cn .. " { }") end
	w("  chain gate {")
	w("    type filter hook forward priority -5; policy accept;")
	if m.g.quic_block then w('    meta l4proto udp udp dport 443 counter name "c_quic" drop comment "quic"') end
	for _, l in ipairs(body) do w(l) end
	if def == "deny" then
		w("    ct state related accept")
		if m.g.default_log then w('    limit rate 6/minute burst 3 packets log prefix "MWADV:default:deny "') end
		w('    counter name "c_default" drop comment "default"')
	else
		w('    counter name "c_default" accept comment "default"')
	end
	w("  }")
	w("}")
	plan.nft = table.concat(T, "\n") .. "\n"
	plan.nrules = nbody
	plan.urlids = ust
	return plan
end

-- 自定义识别特征文本(给 mwbehavd 追加到识别库之后)
function M.features_text(plan)
	local list = {}
	for key, u in pairs(plan.urls or {}) do list[#list + 1] = { id = u.id, regex = u.regex } end
	table.sort(list, function(a, b) return a.id < b.id end)
	local t = {}
	for _, u in ipairs(list) do t[#t + 1] = string.format("%d adv%d:[tcp;;;%s;;]", u.id, u.id, u.regex) end
	return #t > 0 and (table.concat(t, "\n") .. "\n") or ""
end

function M.urlids_text(plan)
	-- 只增不减: 已分配过的编号不立刻复用, 避免旧连接上留着的识别结果被新域名误命中
	local t = {}
	for key, id in pairs((plan.urlids or {}).map or {}) do t[#t + 1] = { id, key } end
	table.sort(t, function(a, b) return a[1] < b[1] end)
	local o = {}
	for _, x in ipairs(t) do o[#o + 1] = x[1] .. "\t" .. x[2] end
	return #o > 0 and (table.concat(o, "\n") .. "\n") or ""
end

-- ------------------------------------------------------------ 环境(区域/接口/引擎能力)
function M.build_env(c, now)
	c = c or uci.cursor()
	local jsonc = require "luci.jsonc"
	local env = { now = now or os.time(), zones = {}, ifaces = {}, zone_list = {}, if_list = {} }
	env.clock_ok = env.now > 1704067200
	local dump = {}
	local p = io.popen("ubus call network.interface dump 2>/dev/null")
	if p then
		local j = jsonc.parse(p:read("*a") or "")
		p:close()
		if j and j.interface then dump = j.interface end
	end
	local l3 = {}
	for _, it in ipairs(dump) do
		local d = it.l3_device or it.device
		if it.up and d and d ~= "" then
			l3[it.interface] = d
			if not env.ifaces[d] then env.ifaces[d] = true; env.if_list[#env.if_list + 1] = { dev = d, net = it.interface } end
		end
	end
	c:foreach("firewall", "zone", function(s)
		local name = s.name
		if not name or not name:match("^[%w_%-]+$") then return end
		local devs, seen = {}, {}
		for _, n in ipairs(lst(s.network)) do
			local d = l3[n]
			if d and not seen[d] then seen[d] = true; devs[#devs + 1] = d end
		end
		if #lst(s.network) == 0 and #lst(s.device) == 0 and l3[name] and not seen[l3[name]] then devs[#devs + 1] = l3[name]; seen[l3[name]] = true end
		for _, d in ipairs(lst(s.device)) do
			d = d:gsub("%+$", "")
			if d:match("^[%w%._%-]+$") and not seen[d] then seen[d] = true; devs[#devs + 1] = d end
		end
		env.zones[name] = devs
		env.zone_list[#env.zone_list + 1] = { name = name, devs = devs, nets = lst(s.network) }
	end)
	local v = (read_file("/proc/sys/oaf/version") or ""):gsub("%s+$", "")
	env.oaf_version = v
	env.url_engine = "none"
	env.url_engine_ver = v
	return env
end

-- 识别守护进程的 pid(不用 pidof: busybox 的 pidof 会把 /etc/init.d/mwbehavd 脚本本身也算进去)
function M.daemon_pid()
	local d = fs.dir("/proc")
	if not d then return nil end
	for n in d do
		if n:match("^%d+$") then
			local t = fs.readlink("/proc/" .. n .. "/exe")
			if t and t:find("^/usr/bin/mwbehavd") then return tonumber(n) end
		end
	end
	return nil
end

-- ------------------------------------------------------------ 计数器
function M.counters_live()
	local r = {}
	local p = io.popen("nft list counters table inet mwrt_adv 2>/dev/null")
	if not p then return r end
	local s = p:read("*a") or ""
	p:close()
	for n, pk, by in s:gmatch("counter%s+([%w_%.%-]+)%s*{%s*packets%s+(%d+)%s+bytes%s+(%d+)") do
		r[n] = { p = tonumber(pk) or 0, b = tonumber(by) or 0 }
	end
	return r
end

function M.hits_load()
	local h = {}
	local f = io.open(M.HITS, "r")
	if f then
		for l in f:lines() do
			local id, bp, bb, lp, lt = l:match("^(%S+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
			if id then h[id] = { bp = tonumber(bp), bb = tonumber(bb), lp = tonumber(lp), lt = tonumber(lt) } end
		end
		f:close()
	end
	return h
end
function M.hits_save(h)
	local t = {}
	for id, x in pairs(h) do t[#t + 1] = string.format("%s\t%d\t%d\t%d\t%d", id, x.bp, x.bb, x.lp, x.lt) end
	local f = io.open(M.HITS .. ".tmp", "w")
	if not f then return end
	f:write(table.concat(t, "\n"), #t > 0 and "\n" or "")
	f:close()
	os.rename(M.HITS .. ".tmp", M.HITS)
end

-- 把当前内核计数并入累计值; fold=true 表示马上要换表(计数器会归零), 把 live 折进 base
function M.hits_sync(now, fold, keep)
	local live = M.counters_live()
	local h = M.hits_load()
	for cn, v in pairs(live) do
		local id = cn:match("^c_(.+)$")
		if id then
			local x = h[id] or { bp = 0, bb = 0, lp = 0, lt = 0 }
			if v.p > x.lp then x.lt = now end
			x.lp = v.p
			if fold then x.bp = x.bp + v.p; x.bb = x.bb + v.b; x.lp = 0 end
			h[id] = x
		end
	end
	if keep then
		for id in pairs(h) do if not keep[id] then h[id] = nil end end
	end
	M.hits_save(h)
	return h, live
end

function M.status_read()
	local jsonc = require "luci.jsonc"
	return jsonc.parse(read_file(M.STATUS) or "") or {}
end

return M
