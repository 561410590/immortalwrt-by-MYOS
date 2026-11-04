module("luci.controller.zz_mwrt_ports", package.seeall)

function index()
 for _, name in ipairs({"port_get","port_set","port_probe","port_probe_ignore"}) do
  entry({"admin","modem",name},call("action_"..name)).leaf=true
 end
end

local PORT_WAN_IDLE = "eth0"   -- 改回 LAN 后 wan/wan6 指回的空壳口(出厂值)
local PORT_TXT_MAX = 128       -- 宽带账号/密码长度上限(字节)

-- 本机支持就返回那个唯一的物理网口名,否则 nil。
-- 机型判定和 modem_info.htm 顶部同一套:先拦 H5000M/E87N,其余 ≥4 核是 Ultra。
-- 再要求恰好一个物理口(有 phydev 的) —— 机型之外再兜一层,哪天 MAX 换了
-- 硬件版本多出一个口,这里不会还按单口去拆网桥。
local function port_pick()
	local fs = require "nixio.fs"
	local port
	for ifn in (fs.dir("/sys/class/net") or function() end) do
		if fs.access("/sys/class/net/" .. ifn .. "/phydev") then
			if port then return nil end
			port = ifn
		end
	end
	if port and port:match("^[%w%._-]+$") then return port end
	return nil
end

-- 网桥和 wan 的现状。ports 统一成数组(list 读出来是 table,单值写法是字符串)。
local function port_state(uci, port)
	local st = { lan_dev = uci:get("network", "lan", "device") or "br-lan", ports = {} }
	uci:foreach("network", "device", function(s)
		if not st.br and s.name == st.lan_dev and s.type == "bridge" then
			st.br = s[".name"]
			if type(s.ports) == "table" then
				for _, v in ipairs(s.ports) do st.ports[#st.ports + 1] = v end
			elseif type(s.ports) == "string" then
				for v in s.ports:gmatch("%S+") do st.ports[#st.ports + 1] = v end
			end
		end
	end)
	for _, v in ipairs(st.ports) do
		if v == port then st.in_br = true end
	end
	if st.in_br then
		st.mode = "lan"
	elseif uci:get("network", "wan", "device") == port then
		st.mode = "wan"
	else
		st.mode = "none"
	end
	return st
end

-- 浏览器是不是经这个网口接进来的:REMOTE_ADDR → ARP 表里的 MAC → 网桥转发表
-- 里学到它的那个端口。是的话一切成 WAN 本页就断,前端得提前说清楚。
-- brforward 的格式见 fancontrol.lua 的 devpop_clients(每条 16 字节)。
local function port_client_via(port, lan_dev)
	local fs = require "nixio.fs"
	local ra = (luci.http.getenv("REMOTE_ADDR") or ""):gsub("^::ffff:", "")
	local mac
	local f = io.open("/proc/net/arp", "r")
	if f then
		for line in f:lines() do
			local ip, hw, dev = line:match("^(%S+)%s+%S+%s+%S+%s+(%S+)%s+%S+%s+(%S+)")
			if ip == ra and dev == lan_dev then mac = hw:lower() break end
		end
		f:close()
	end
	if not mac then return false end
	local no = tonumber(((fs.readfile("/sys/class/net/" .. lan_dev .. "/brif/" .. port .. "/port_no") or ""):match("0x(%x+)")) or "", 16)
	if not no then return false end
	local bf = io.open("/sys/class/net/" .. lan_dev .. "/brforward", "rb")
	local data = bf and bf:read("*a") or ""
	if bf then bf:close() end
	for i = 1, #data - 15, 16 do
		local b1, b2, b3, b4, b5, b6, pno, is_local = data:byte(i, i + 7)
		if is_local == 0 and string.format("%02x:%02x:%02x:%02x:%02x:%02x", b1, b2, b3, b4, b5, b6) == mac then
			return data:byte(i + 12) * 256 + pno == no
		end
	end
	return false
end

-- IPv4 点分 → 整数,不合法返回 nil。Lua 5.1 没有位运算,网段比较用除法取整。
local function ip4n(s)
	local a, b, c, d = tostring(s or ""):match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$")
	if not a then return nil end
	a, b, c, d = tonumber(a), tonumber(b), tonumber(c), tonumber(d)
	if a > 255 or b > 255 or c > 255 or d > 255 then return nil end
	return ((a * 256 + b) * 256 + c) * 256 + d
end
-- 子网掩码 → 前缀长度;不是连续的 1 返回 nil
local function mask_bits(s)
	local n = ip4n(s)
	if not n then return nil end
	for b = 0, 32 do
		if n == 2 ^ 32 - 2 ^ (32 - b) then return b end
	end
	return nil
end
local function same_net(x, y, bits)
	local k = 2 ^ (32 - bits)
	return math.floor(x / k) == math.floor(y / k)
end
-- LAN 的地址和前缀。ipaddr 可能是 "a.b.c.d/24" 写法,也可能配 netmask。
local function lan_net(uci)
	local ip = uci:get("network", "lan", "ipaddr")
	if type(ip) == "table" then ip = ip[1] end
	ip = tostring(ip or "")
	local a, bits = ip:match("^([%d%.]+)/(%d+)$")
	if a then return a, ip4n(a), tonumber(bits) end
	return ip, ip4n(ip), mask_bits(uci:get("network", "lan", "netmask") or "255.255.255.0")
end
local function port_list(v)
	if type(v) == "table" then return table.concat(v, " ") end
	return v or ""
end

-- 网口对端识别:网口当 LAN 用时,插上来的是上级路由器/光猫(多半该改成 WAN),
-- 还是电脑这类终端。真正去测的是 /usr/bin/mwrt-portprobe(python3,在这个口上
-- 直接发 DHCP/PPPoE 发现包,约 2 秒),这里只按需在后台拉起它、读它写的结果。
-- 结果跟着网口的 carrier_up_count 走:拔插一次换一个号,重新识别。同一次插着的期间,
-- 没认出来的隔 PROBE_GAP 秒补测、最多 PROBE_TRIES 次(对端可能刚上电还没起 DHCP),
-- 认出来的不再测。
-- 「忽略」只管这一次插着:/tmp 里记"网口名 carrier_up_count",拔插一次号就变,
-- 下次插上照常检测、是路由器照样弹。从 WAN 改回 LAN 也算忽略这一次 —— 用户是看着
-- 它插着才选的 LAN;可网络重载会让网口掉一下线、号跟着变,所以先记成"网口名 set 时间",
-- 等重载完由 port_set 的后台尾巴钉到那时的号上(没钉上的 PROBE_SETTLE 秒后作废)。
local PROBE_BIN = "/usr/bin/mwrt-portprobe"
local PROBE_OUT = "/tmp/mwrt-portprobe.json"
local PROBE_IGN = "/tmp/mwrt-portprobe.ign"
local PROBE_TRIES, PROBE_GAP, PROBE_STALE = 4, 20, 15   -- 次数;补测间隔、"测了一半没了"判定(秒)
local PROBE_SETTLE = 60

local function probe_read()
	local s = (require "nixio.fs").readfile(PROBE_OUT)
	local r = s and require("luci.jsonc").parse(s)
	return type(r) == "table" and r or nil
end
local function probe_ignored(port, cnt)
	local p, v, t = ((require "nixio.fs").readfile(PROBE_IGN) or ""):match("^(%S+) (%S+) ?(%d*)")
	if p ~= port then return false end
	if v == "set" then return os.time() - (tonumber(t) or 0) < PROBE_SETTLE end
	return cnt ~= nil and tonumber(v) == cnt
end

-- 读现状。GET,只读。**不回传宽带密码**,只告诉前端有没有存过(留空 = 不修改)。
function action_port_get()
	luci.http.prepare_content("application/json")
	local port = port_pick()
	if not port then
		luci.http.write_json({ ok = false, error = "本机型不支持网口切换" })
		return
	end
	local uci = require "luci.model.uci".cursor()
	local st = port_state(uci, port)
	local function w(k) return uci:get("network", "wan", k) end
	local ipaddr, netmask = w("ipaddr"), w("netmask") or ""
	if type(ipaddr) == "table" then ipaddr = ipaddr[1] end
	ipaddr = tostring(ipaddr or "")
	local a, bits = ipaddr:match("^([%d%.]+)/(%d+)$")
	if a then
		ipaddr = a
		local n = 2 ^ 32 - 2 ^ (32 - tonumber(bits))
		netmask = string.format("%d.%d.%d.%d", math.floor(n / 16777216) % 256,
			math.floor(n / 65536) % 256, math.floor(n / 256) % 256, n % 256)
	end
	local lan_ip = lan_net(uci)
	local fs = require "nixio.fs"
	luci.http.write_json({
		ok = true,
		port = port,
		mode = st.mode,
		carrier = (fs.readfile("/sys/class/net/" .. port .. "/carrier") or ""):match("^1") ~= nil,
		via = st.in_br and port_client_via(port, st.lan_dev) or false,
		proto = w("proto") or "dhcp",
		username = w("username") or "",
		has_pass = (w("password") or "") ~= "",
		ipaddr = ipaddr,
		netmask = netmask,
		gateway = w("gateway") or "",
		dns = port_list(w("dns")),
		lan_ip = lan_ip,
		failover = uci:get("Mwrt-Assurance", "wan_failover", "enabled") == "1",
		fo_ok = uci:get("Mwrt-Assurance", "wan_failover") ~= nil,
	})
end

-- 全局 mwrt-portsw.js 轮询的对端识别。GET,不改配置;该测的时候在后台拉起探测脚本,这一次先回 probing。
-- 只回前端要显示的:不回对端 MAC(忽略接口自己从结果文件里取)。
function action_port_probe()
	luci.http.prepare_content("application/json")
	local port = port_pick()
	if not port then
		luci.http.write_json({ ok = false, error = "本机型不支持网口切换" })
		return
	end
	local fs = require "nixio.fs"
	local uci = require "luci.model.uci".cursor()
	local st = port_state(uci, port)
	local sys = "/sys/class/net/" .. port .. "/"
	local res = { ok = true, port = port, mode = st.mode, state = "off",
		carrier = (fs.readfile(sys .. "carrier") or ""):match("^1") ~= nil }
	if st.mode ~= "lan" or not res.carrier or not fs.access(PROBE_BIN, "x") then
		luci.http.write_json(res)
		return
	end
	local cnt = tonumber((fs.readfile(sys .. "carrier_up_count") or ""):match("%d+"))
	local r = probe_read()
	if r and not (r["if"] == port and r.cnt == cnt) then r = nil end
	local age = r and os.time() - (tonumber(r.t) or 0) or 0
	local run
	if not r then
		run = true
	elseif r.state == "probing" then
		run = age >= PROBE_STALE
	elseif r.state ~= "router" then
		run = (tonumber(r.tries) or 1) < PROBE_TRIES and age >= PROBE_GAP
	end
	if run then os.execute(PROBE_BIN .. " " .. port .. " >/dev/null 2>&1 &") end
	res.state = r and r.state or "probing"
	if res.state == "router" then
		local ipn, bits = ip4n(r.ip), mask_bits(r.mask or "")
		local lan_ip, ln, lb = lan_net(uci)
		res.kind = r.kind
		res.gw = r.gw or ""
		res.ac = r.ac or ""
		res.lan_ip = lan_ip
		res.conflict = (ipn and bits and ln and lb and same_net(ipn, ln, math.min(bits, lb))) and true or false
		res.ignored = probe_ignored(port, cnt)
	end
	luci.http.write_json(res)
end

-- 接入检测提示框的「忽略」。POST + token。钉的是提示框依据的那份结果的号:
-- 用户点之前已经拔插过的话,号对不上、等于没忽略,新插的这次照样弹。
function action_port_probe_ignore()
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end
	luci.http.prepare_content("application/json")
	local port = port_pick()
	if not port then
		luci.http.write_json({ ok = false, error = "本机型不支持网口切换" })
		return
	end
	local r = probe_read()
	local cnt = r and r["if"] == port and r.state == "router" and tonumber(r.cnt)
	if cnt then (require "nixio.fs").writefile(PROBE_IGN, port .. " " .. cnt .. "\n") end
	luci.http.write_json({ ok = true })
end

-- 写。**必须 POST 且带 CSRF token**(同 wifi_set)。
-- 前端已经校验过一遍,那只是为了少跑一个来回;真正的闸门在这里。
function action_port_set()
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end
	luci.http.prepare_content("application/json")
	local function fail(msg) luci.http.write_json({ ok = false, error = msg }) end

	local port = port_pick()
	if not port then return fail("本机型不支持网口切换") end
	local uci = require "luci.model.uci".cursor()
	local st = port_state(uci, port)
	if not st.br then return fail("没有找到局域网网桥的配置，无法切换") end

	local function fv(k) return ((luci.http.formvalue(k) or ""):gsub("^%s+", ""):gsub("%s+$", "")) end
	local mode, proto = fv("mode"), fv("proto")
	if mode ~= "lan" and mode ~= "wan" then return fail("网口模式参数不正确") end
	-- 已经是 LAN 再选 LAN:什么都不用动,也不去重载网络
	if mode == "lan" and st.mode == "lan" then
		luci.http.write_json({ ok = true, changed = false, mode = "lan" })
		return
	end

	local is_wan = mode == "wan"
	local username, password = fv("username"), luci.http.formvalue("password") or ""
	local ipaddr, netmask, gateway = fv("ipaddr"), fv("netmask"), fv("gateway")
	local dns = {}
	if is_wan then
		if proto ~= "dhcp" and proto ~= "pppoe" and proto ~= "static" then
			return fail("上网方式参数不正确")
		end
		if proto == "pppoe" then
			if username == "" then return fail("请填写宽带账号。") end
			if #username > PORT_TXT_MAX or username:find("%c") then
				return fail("宽带账号太长或含有不可见字符。")
			end
			if password ~= "" and (#password > PORT_TXT_MAX or password:find("%c")) then
				return fail("宽带密码太长或含有不可见字符。")
			end
			if password == "" and (uci:get("network", "wan", "password") or "") == "" then
				return fail("请填写宽带密码。")
			end
		elseif proto == "static" then
			local ipn = ip4n(ipaddr)
			local o1 = ipn and math.floor(ipn / 16777216)
			if not ipn or o1 == 0 or o1 == 127 or o1 >= 224 then return fail("请填写正确的 IP 地址。") end
			local bits = mask_bits(netmask)
			if not bits or bits < 1 then return fail("请填写正确的子网掩码。") end
			if bits <= 30 then
				local k = 2 ^ (32 - bits)
				if ipn % k == 0 or ipn % k == k - 1 then
					return fail("这个 IP 是网段的网络地址或广播地址，不能给本机用。")
				end
			end
			if gateway ~= "" then
				local gwn = ip4n(gateway)
				if not gwn then return fail("请填写正确的网关地址。") end
				if gwn == ipn then return fail("网关不能和本机 IP 相同。") end
				if bits <= 30 and not same_net(ipn, gwn, bits) then
					return fail("网关不在这个 IP 的网段里，请核对 IP 地址、子网掩码和网关。")
				end
			end
			local lan_ip, ln, lb = lan_net(uci)
			if ln and lb and same_net(ipn, ln, math.min(bits, lb)) then
				return fail("这个 IP 和局域网（" .. lan_ip .. "）在同一网段，会冲突，请换一个网段。")
			end
			for v in fv("dns"):gmatch("[^%s,，]+") do
				if not ip4n(v) then return fail("DNS 服务器地址不正确：" .. v) end
				dns[#dns + 1] = v
			end
		end
	end

	local via = st.in_br and port_client_via(port, st.lan_dev) or false

	-- 网桥成员:拿掉/放回这个口,其余(hnat)原样;放回时排在最前,和出厂顺序一致。
	-- 万一拿完一个不剩,标 bridge_empty,免得 br-lan 起不来把无线也带掉。
	local np = {}
	for _, v in ipairs(st.ports) do
		if v ~= port then np[#np + 1] = v end
	end
	if not is_wan then table.insert(np, 1, port) end
	if #np > 0 then
		uci:set("network", st.br, "ports", np)
	else
		uci:delete("network", st.br, "ports")
		uci:set("network", st.br, "bridge_empty", "1")
	end

	local fo_ok = uci:get("Mwrt-Assurance", "wan_failover") ~= nil
	local pif = port
	if is_wan then
		uci:set("network", "wan", "device", port)
		if uci:get("network", "wan6") then uci:set("network", "wan6", "device", port) end
		uci:delete("network", "wan", "disabled")
		if proto == "dhcp" then
			uci:set("network", "wan", "proto", "dhcp")
			for _, k in ipairs({ "username", "password", "ipaddr", "netmask", "gateway", "dns" }) do
				uci:delete("network", "wan", k)
			end
		elseif proto == "pppoe" then
			uci:set("network", "wan", "proto", "pppoe")
			uci:set("network", "wan", "username", username)
			if password ~= "" then uci:set("network", "wan", "password", password) end
			for _, k in ipairs({ "ipaddr", "netmask", "gateway" }) do
				uci:delete("network", "wan", k)
			end
			pif = uci:get("network", "wan", "pppname") or "pppoe-wan"
		else
			uci:set("network", "wan", "proto", "static")
			uci:set("network", "wan", "ipaddr", ipaddr)
			uci:set("network", "wan", "netmask", netmask)
			if gateway ~= "" then
				uci:set("network", "wan", "gateway", gateway)
			else
				uci:delete("network", "wan", "gateway")
			end
			if #dns > 0 then
				uci:set("network", "wan", "dns", dns)
			else
				uci:delete("network", "wan", "dns")
			end
			for _, k in ipairs({ "username", "password" }) do
				uci:delete("network", "wan", k)
			end
		end
	else
		for _, s in ipairs({ "wan", "wan6" }) do
			if uci:get("network", s, "device") == port then
				uci:set("network", s, "device", PORT_WAN_IDLE)
			end
		end
		uci:set("network", "wan", "proto", "dhcp")
		for _, k in ipairs({ "ipaddr", "netmask", "gateway", "dns" }) do
			uci:delete("network", "wan", k)
		end
	end
	if fo_ok then
		uci:set("Mwrt-Assurance", "wan_failover", "enabled", is_wan and "1" or "0")
		uci:set("Mwrt-Assurance", "wan_failover", "primary_if", pif)
	end

	local ok, err = uci:commit("network")
	if not ok then
		uci:revert("network")
		if fo_ok then uci:revert("Mwrt-Assurance") end
		return fail("保存网络配置失败：" .. tostring(err or "未知错误"))
	end
	if fo_ok then uci:commit("Mwrt-Assurance") end
	if not is_wan then (require "nixio.fs").writefile(PROBE_IGN, port .. " set " .. os.time() .. "\n") end

	-- **异步重载。** 理由同 wifi_set:浏览器要是正经这个口连着,网络一动响应就发不出去。
	-- 提交本身已经会让 procd 约 1 秒后重载 network(reload_trigger),这里再显式来一次兜底,
	-- 配置没变时是空操作。防火墙随后补一次,让 eth1 进出 wan 区域立即生效。
	-- 改回 LAN 时顺手清掉还挂在这个口上的默认路由(网关存活切换时装的那种不归 netifd 管),
	-- 再等网口重新起来,把上面的"忽略这一次"钉到那时的拔插计数上。
	local tail = is_wan and "" or
		("; for i in 1 2 3 4; do ip route del default dev " .. port .. " 2>/dev/null || break; done" ..
		"; sleep 8; c=$(cat /sys/class/net/" .. port .. "/carrier_up_count 2>/dev/null)" ..
		"; grep -q '^" .. port .. " set ' " .. PROBE_IGN .. " && [ -n \"$c\" ] && echo \"" .. port .. " $c\" > " .. PROBE_IGN)
	os.execute("(sleep 2; /etc/init.d/network reload; sleep 6; /etc/init.d/firewall reload" ..
		tail .. ") >/dev/null 2>&1 &")

	luci.http.write_json({
		ok = true, changed = true, mode = mode,
		proto = is_wan and proto or nil,
		failover = fo_ok and is_wan or false,
		primary = fo_ok and pif or nil,
		via = via,
		lan_ip = (lan_net(uci)),
	})
end

