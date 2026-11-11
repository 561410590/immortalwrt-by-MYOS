module("luci.controller.modem", package.seeall)

local function read_runtime_file(path)
	local file = io.open(path, "r")
	local data

	if not file then
		return ""
	end

	data = file:read("*all") or ""
	file:close()

	return data
end

local function command_output(cmd)
	local handle = io.popen(cmd, "r")
	local data = ""

	if handle then
		data = handle:read("*all") or ""
		handle:close()
	end

	return data
end

local function ifstatus_text(iface)
	return command_output("ifstatus " .. iface)
end

local function uci_get(key)
	return (command_output("uci -q get " .. key):gsub("%s+$", ""))
end

local function iface_has_manufacturer(iface, manufacturer)
	local status = ifstatus_text(iface)
	return string.find(status, '"manufacturer": "' .. manufacturer .. '"', 1, true)
end

local function cpe1_role()
	local proto = uci_get("network.cpe1.proto")
	local mode = uci_get("network.cpe1.mode")
	local device = uci_get("network.cpe1.device")
	if device == "" then device = uci_get("network.cpe1.ifname") end
	if device == "port5" and (proto == "dhcp" or proto == "static" or proto == "pppoe") then
		return "broadband"
	end

	if iface_has_manufacturer("cpe1", "tdtech_mt5700_odu") or mode == "odu" then
		return "cellular"
	end

	return "unknown"
end

local function mt5700_iface()
	if iface_has_manufacturer("cpe", "tdtech_mt5700") then
		return "cpe"
	end

	if cpe1_role() == "cellular" then
		return "cpe1"
	end

	return nil
end

local function modem_type()
	local content = read_runtime_file("/tmp/modconf.conf")

	if content and string.find(content, "RM520") then
		return "RM520"
	elseif content and string.find(content, "RM500U") then
		return "RM500U"
	elseif content and string.find(content, "MT5700") then
		return "MT5700"
	elseif mt5700_iface() then
		return "MT5700"
	end

	return nil
end

function index()
	entry({"admin", "modem"}, firstchild(), _("内置蜂窝"), 25).dependent=false
	--entry({"admin", "modem", "nets"}, template("zmode/net_status"), _("信号状态"), 97)
	local template_name = "zmode/net_status"
	local mtype = modem_type()
	if mtype == "RM520" then
		template_name = "zmode/net_status_RM520"
	elseif mtype == "RM500U" then
		template_name = "zmode/net_status_RM500U"
	elseif mtype == "MT5700" then
		template_name = "zmode/net_status_MT5700M"
	end
	entry({"admin", "modem", "smsc"}, template("zmode/smsc"), _("短信管理"), 95)
	entry({"admin", "modem", "nets"}, template(template_name), _("模组状态"), 96)
	entry({"admin", "modem", "at"}, template("zmode/at"), _("串口调试"), 97)
	entry({"admin", "modem", "at2"}, template("zmode/at2"), _("网络调试"), 98)
	entry({"admin", "modem", "modem"}, cbi("modem"), _("模组设定"), 100) 
	entry({"admin", "modem", "get_csq"}, call("action_get_csq"))
	entry({"admin", "modem", "poe_status"}, call("action_poe_status"))
	entry({"admin", "modem", "secondary_status"}, call("action_secondary_status"))
	entry({"admin", "modem", "secondary_set"}, post("action_secondary_set"))
	entry({"admin", "modem", "poe_set"}, post("action_poe_set"))
	entry({"admin", "modem", "route_status"}, call("action_route_status"))
	entry({"admin", "modem", "route_set"}, post("action_route_set"))
	entry({"admin", "modem", "radio_snapshot"}, call("action_radio_snapshot"))
	entry({"admin", "modem", "radio_lock_status"}, call("action_radio_lock_status"))
	entry({"admin", "modem", "radio_lock_set"}, post("action_radio_lock_set"))
	entry({"admin", "modem", "send_atcmd"}, call("action_send_atcmd"))
	entry({"admin", "modem", "smscs"}, call("action_smscs"))
end

function action_secondary_status()
	local http = require "luci.http"
	http.prepare_content("application/json")
	http.write_json(require("c5800_secondary").status(true))
end

function action_secondary_set()
	local http = require "luci.http"
	local secondary = require "c5800_secondary"
	local result, err
	if http.formvalue("action") == "sim" then
		result, err = secondary.select(http.formvalue("slot"))
	elseif http.formvalue("action") == "frequency" then
		local input = {}
		for _, key in ipairs({"mobility", "custom_freq", "freq", "earfreq_mode", "custom_earfcn5", "earfcn5", "pci5", "band5", "custom_earfcn4", "earfcn4", "pci4", "band4"}) do input[key] = http.formvalue(key) end
		result, err = secondary.frequency(input)
	else
		local input = {}
		for _, key in ipairs({"mode", "apn", "username", "password", "auth", "pdptype"}) do input[key] = http.formvalue(key) end
		result, err = secondary.configure(input)
	end
	http.prepare_content("application/json")
	if not result then http.status(400, "Bad Request") end
	http.write_json(result or { error = err })
end

function action_radio_snapshot()
	local http = require "luci.http"
	http.prepare_content("application/json")
	http.write_json(require("c5800_radio").snapshot(http.formvalue("iface")))
end

function action_radio_lock_status()
	local http = require "luci.http"
	http.prepare_content("application/json")
	http.write_json(require("c5800_radio_lock").status(http.formvalue("iface")))
end

function action_radio_lock_set()
	local http = require "luci.http"
	local input = {}
	for _, key in ipairs({"rat", "kind", "policy", "mobility", "bands", "arfcn", "scs", "pci", "mode"}) do input[key] = http.formvalue(key) end
	local api = require("c5800_radio_lock")
	local result
	if http.formvalue("layout") == "combined" then
		for _, rat in ipairs({"nr", "lte"}) do
			for _, key in ipairs({"enabled", "bands", "arfcn", "scs", "pci"}) do input[rat.."_"..key] = http.formvalue(rat.."_"..key) end
		end
		result = api.apply_form(http.formvalue("iface"), input)
	else result = api.apply(http.formvalue("iface"), input) end
	http.prepare_content("application/json")
	if result.error then http.status(400, "Bad Request") end
	http.write_json(result)
end


local function poe_state()
	local bit = read_runtime_file("/sys/class/gpio/cpe1-pwr/value"):match("[01]")
	return {
		supported = bit ~= nil,
		secondary_role = cpe1_role(),
		enabled = bit == "0",
		carrier = read_runtime_file("/sys/class/net/port5/carrier"):match("[01]") == "1"
	}
end

local function set_poe_power(enabled)
	local state = poe_state()
	if not state.supported then return false end
	if state.enabled == enabled then return true end
	local file = io.open("/sys/class/gpio/cpe1-pwr/value", "w")
	local written = file and file:write(enabled and "0" or "1")
	if file then file:close() end
	return written and poe_state().enabled == enabled
end

function action_poe_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json(poe_state())
end

function action_poe_set()
	local http = luci.http
	local requested = http.formvalue("enabled")
	http.prepare_content("application/json")
	if requested ~= "0" and requested ~= "1" then
		http.status(400, "Bad Request")
		http.write_json({ error = "供电参数无效" })
		return
	end
	local state = poe_state()
	if not state.supported then
		http.status(503, "Service Unavailable")
		http.write_json({ error = "未检测到 PoE 供电控制器" })
		return
	end
	local enabled = requested == "1"
	if enabled and state.secondary_role == "broadband" then
		http.status(409, "Conflict")
		http.write_json({ error = "普通 WAN 模式不启用 PoE，请先切回副 5G 模式" })
		return
	end
	local power_changed = state.enabled ~= enabled
	if power_changed then
		local written = set_poe_power(enabled)
		state = poe_state()
		if not written or state.enabled ~= enabled then
			http.status(500, "Internal Server Error")
			http.write_json({ error = "供电状态写入失败，请刷新后重试" })
			return
		end
	end
	-- The standalone PoE buttons save the same secondary-5G choice as the line switch.
	if state.secondary_role == "cellular" then
		local uci, sys = require("uci").cursor(), require "luci.sys"
		local disabled, auto = enabled and "0" or "1", enabled and "1" or "0"
		local changed = uci:get("network","cpe1","disabled") ~= disabled or uci:get("network","cpe1","auto") ~= auto
		local rc = 0
		if changed then
			uci:set("network","cpe1","disabled",disabled)
			uci:set("network","cpe1","auto",auto)
			uci:commit("network")
			rc = sys.call("ubus call network reload")
		end
		if changed or power_changed then
			if sys.call("ubus call network.interface.cpe1 " .. (enabled and "up" or "down")) ~= 0 then rc=1 end
		end
		if rc ~= 0 then
			http.status(503, "Service Unavailable")
			http.write_json({error="副 5G 开关已保存，接口应用失败，请查看系统日志"})
			return
		end
	end
	http.write_json(state)
end

local function route_section(uci, package, kind, name, values)
	uci:set(package, name, kind)
	for key,value in pairs(values) do uci:set(package,name,key,value) end
end

local function route_state()
	local uci = require("uci").cursor()
	local power = poe_state()
	local function enabled(name)
		return uci:get("network", name, "proto") ~= nil and
			(name ~= "cpe1" or uci:get("network", name, "disabled") == "0") and
			uci:get("network", name, "disabled") ~= "1" and uci:get("network", name, "auto") ~= "0"
	end
	local function online(name)
		return read_runtime_file("/var/run/mwan3/iface_state/" .. name):match("online") ~= nil
	end
	local mode = uci:get("mwan3", "globals", "net_kind")
	if not mode then mode = uci:get("mwan3", "globals", "download_mode") == "download" and "Overlap" or "Priority" end
	local count = 0
	for _,iface in ipairs({"cpe","cpe1","wan"}) do if enabled(iface) then count=count+1 end end
	if count < 2 then mode = "Single" end
	local priority = uci:get("mwan3", "globals", "priority") or {"poe", "main", "wan"}
	if type(priority) ~= "table" then priority = {priority} end
	local dns = uci:get("network", "wan", "dns") or {}
	if type(dns) ~= "table" then dns = {dns} end
	local ports = {}
	for _,device in ipairs({"lan1","lan2","lan3","port5"}) do
		ports[device] = {
			link = read_runtime_file("/sys/class/net/"..device.."/carrier"):match("^1") ~= nil,
			speed = tonumber(read_runtime_file("/sys/class/net/"..device.."/speed"))
		}
	end
	return {
		mode = mode, priority = priority, ports = ports, power = power,
		dividing = uci:get("mwan3", "globals", "dividing") or "client",
		default = uci:get("mwan3", "globals", "dividing_default") or "poe",
		secondary_role = cpe1_role(), running = read_runtime_file("/var/run/mwan3/mmx_mask") ~= "",
		main_online = online("cpe_4"), poe_online = online("cpe1"),
		lines = {
			main = {enabled=enabled("cpe"), online=online("cpe_4"), weight=tonumber((uci:get("mwan3","main_download","weight"))) or 1},
			poe = {enabled=enabled("cpe1"), online=online("cpe1"), weight=tonumber((uci:get("mwan3","poe_download","weight"))) or 1},
			wan = {enabled=enabled("wan"), online=online("wan"), weight=tonumber((uci:get("mwan3","wan_download","weight"))) or 1}
		},
		wan = {proto=uci:get("network","wan","proto") or "dhcp", ipaddr=uci:get("network","wan","ipaddr") or "",
			netmask=uci:get("network","wan","netmask") or "255.255.255.0", gateway=uci:get("network","wan","gateway") or "",
			username=uci:get("network","wan","username") or "", mtu=uci:get("network","wan","mtu") or "",
			macaddr=uci:get("network","wan","macaddr") or "", peerdns=uci:get("network","wan","peerdns") ~= "0", dns=dns}
	}
end

function action_route_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json(route_state())
end

function action_route_set()
	local http, json = require "luci.http", require "luci.jsonc"
	local uci, sys, dt = require("uci").cursor(), require "luci.sys", require "luci.cbi.datatypes"
	local value = json.parse(http.formvalue("data") or "")
	http.prepare_content("application/json")
	local function fail(message)
		http.status(400, "Bad Request")
		http.write_json({error=message})
	end
	if type(value) ~= "table" or (value.mode ~= "Priority" and value.mode ~= "Dividing" and value.mode ~= "Overlap" and value.mode ~= "Single") then
		fail("请选择备份、分流或本地叠加"); return
	end
	local names, interfaces = {"main","poe","wan"}, {main="cpe_4",poe="cpe1",wan="wan"}
	local old, count = route_state(), 0
	for _,name in ipairs(names) do
		local line = value.lines and value.lines[name]
		local weight = type(line) == "table" and tonumber(line.weight)
		if type(line) ~= "table" or type(line.enabled) ~= "boolean" or not weight or
			weight < 1 or weight > 1000 or weight % 1 ~= 0 then fail("线路带宽比例须为 1–1000 的整数"); return end
		line.weight = weight
		if line.enabled then count = count + 1 end
	end
	if count == 0 then fail("请至少选择一条上网线路"); return end
	if count == 1 then
		value.mode = "Single"
		for _,name in ipairs(names) do if value.lines[name].enabled then value.default=name end end
	elseif value.mode == "Single" then value.mode = "Priority" end
	if value.dividing ~= "client" and value.dividing ~= "protocol" then fail("请选择终端分流或 IP / 域名分流"); return end
	if not interfaces[value.default] or not value.lines[value.default].enabled then fail("默认线路须为已启用的线路"); return end
	local seen = {}
	if type(value.priority) ~= "table" or #value.priority ~= 3 then fail("线路优先级不完整"); return end
	for _,name in ipairs(value.priority) do
		if not interfaces[name] or seen[name] then fail("线路优先级重复或无效"); return end
		seen[name] = true
	end
	local wan = value.wan or {}
	local dns = {}
	if value.lines.wan.enabled then
		if wan.proto ~= "dhcp" and wan.proto ~= "static" and wan.proto ~= "pppoe" then fail("有线上网方式无效"); return end
		if wan.proto == "static" and (not dt.ip4addr(wan.ipaddr or "") or not dt.ip4addr(wan.netmask or "") or not dt.ip4addr(wan.gateway or "")) then
			fail("请填写正确的静态 IPv4 地址、子网掩码和网关"); return
		end
		if wan.mtu ~= "" and (not dt.uinteger(tostring(wan.mtu or "")) or tonumber(wan.mtu) < 576 or tonumber(wan.mtu) > 9000) then fail("MTU 格式错误"); return end
		if wan.macaddr ~= "" and not dt.macaddr(wan.macaddr or "") then fail("MAC 地址格式错误"); return end
		for _,ip in ipairs(wan.dns or {}) do
			if not dt.ipaddr(ip) then fail("DNS 地址格式错误"); return end
			dns[#dns+1] = ip
		end
	end
	-- Apply the requested secondary-line choice even when saved and actual power differ.
	uci:set("mwan3","globals","net_kind",value.mode)
	uci:set("mwan3","globals","download_mode",value.mode == "Overlap" and "download" or "daily")
	uci:set("mwan3","globals","priority",value.priority)
	uci:set("mwan3","globals","dividing",value.dividing)
	uci:set("mwan3","globals","dividing_default",value.default)
	local members, overlap = {}, {}
	local backup = {main="main_backup",poe="poe_primary",wan="wan_primary"}
	local effective = {}
	if value.mode == "Dividing" then effective[1] = value.default end
	for _,name in ipairs(value.priority) do if name ~= effective[1] then effective[#effective+1] = name end end
	for rank,name in ipairs(effective) do
		route_section(uci,"mwan3","member",backup[name],{interface=interfaces[name],metric=tostring(rank),weight="1"})
		route_section(uci,"mwan3","member",name.."_download",{interface=interfaces[name],metric="1",weight=tostring(value.lines[name].weight)})
		if value.lines[name].enabled then
			members[#members+1] = backup[name]; overlap[#overlap+1] = name.."_download"
		end
	end
	route_section(uci,"mwan3","policy","poe_first",{use_member=members,last_resort="default"})
	route_section(uci,"mwan3","policy","dual_download",{use_member=overlap,last_resort="default"})
	-- One all-port LAN rule replaces the former HTTP/QUIC-only download rules.
	uci:delete("mwan3","web_tcp"); uci:delete("mwan3","web_quic")
	route_section(uci,"mwan3","rule","lan_default",{src_iface="lan",family="ipv4",proto="all",sticky="0",
		use_policy=value.mode == "Overlap" and "dual_download" or "poe_first"})
	local network_changed, wan_changed = false, false
	local function netset(section, option, data)
		local previous = uci:get("network",section,option)
		if type(data) == "table" then
			if type(previous) == "table" and table.concat(previous," ") == table.concat(data," ") then return end
		elseif previous == data then return end
		if data == nil or data == "" then uci:delete("network",section,option) else uci:set("network",section,option,data) end
		network_changed = true
	end
	for _,name in ipairs({"main","poe"}) do
		if value.lines[name].enabled ~= old.lines[name].enabled then
			local iface = name == "main" and "cpe" or "cpe1"
			netset(iface,"disabled",value.lines[name].enabled and "0" or "1")
			netset(iface,"auto",value.lines[name].enabled and "1" or "0")
		end
	end
	if value.lines.wan.enabled then
		if not uci:get("network","wan") then route_section(uci,"network","interface","wan",{}); network_changed=true end
		netset("wan","device","lan3"); netset("wan","ifname","lan3")
		netset("wan","proto",wan.proto); netset("wan","disabled","0"); netset("wan","auto","1")
		netset("wan","metric","30"); netset("wan","mtu",wan.mtu); netset("wan","macaddr",wan.macaddr)
		netset("lan3","macaddr",wan.macaddr ~= "" and wan.macaddr or read_runtime_file("/sys/class/net/br-lan/address"):gsub("%s+$",""))
		netset("wan","peerdns",wan.peerdns and "1" or "0"); netset("wan","dns",#dns > 0 and dns or nil)
		if wan.proto == "static" then
			for _,key in ipairs({"ipaddr","netmask","gateway"}) do netset("wan",key,wan[key]) end
		elseif wan.proto == "pppoe" then
			netset("wan","username",wan.username or "")
			if wan.password and wan.password ~= "" then netset("wan","password",wan.password) end
		end
		if not uci:get("network","wan6") then route_section(uci,"network","interface","wan6",{}); network_changed=true end
		netset("wan6","proto","dhcpv6"); netset("wan6","device","@wan"); netset("wan6","auto","1")
		netset("wan6","disabled","0"); netset("wan6","metric","30")
		wan_changed = network_changed
	elseif old.lines.wan.enabled then
		netset("wan","auto","0"); netset("wan","disabled","1")
		if uci:get("network","wan6") then netset("wan6","auto","0"); netset("wan6","disabled","1") end
		wan_changed = true
	end
	if value.lines.wan.enabled ~= old.lines.wan.enabled then
		uci:foreach("network","device",function(s)
			if s.name ~= "br-lan" then return end
			local ports, found = {}, false
			for _,port in ipairs(s.ports or {}) do
				if port == "lan3" then found=true; if not value.lines.wan.enabled then ports[#ports+1]=port end
				else ports[#ports+1]=port end
			end
			if not value.lines.wan.enabled and not found then ports[#ports+1]="lan3" end
			netset(s[".name"],"ports",ports)
		end)
	end
	if value.lines.wan.enabled and not uci:get("mwan3","wan") then
		route_section(uci,"mwan3","interface","wan",{enabled="1",family="ipv4",initial_state="offline",track_ip={"223.5.5.5","119.29.29.29"},
			reliability="1",count="1",timeout="4",interval="5",down="3",up="2"})
		wan_changed = true
	elseif uci:get("mwan3","wan") then uci:set("mwan3","wan","enabled",value.lines.wan.enabled and "1" or "0") end
	local firewall_changed = false
	if value.lines.wan.enabled then
		uci:foreach("firewall","zone",function(s)
			if s.name ~= "wan" then return end
			local list = s.network or {}; if type(list) ~= "table" then list={list} end
			for _,iface in ipairs({"wan","wan6"}) do
				local found=false; for _,n in ipairs(list) do if n==iface then found=true end end
				if not found then list[#list+1]=iface; firewall_changed=true end
			end
			if firewall_changed then uci:set("firewall",s[".name"],"network",list) end
		end)
	end
	uci:commit("mwan3")
	if network_changed then uci:commit("network") end
	if firewall_changed then uci:commit("firewall") end
	local rc = 0
	local power_failed = false
	local poe_power_changed = old.secondary_role ~= "broadband" and
		(value.lines.poe.enabled ~= old.lines.poe.enabled or value.lines.poe.enabled ~= old.power.enabled)
	if poe_power_changed then
		power_failed = not set_poe_power(value.lines.poe.enabled)
	end
	if network_changed then rc = sys.call("ubus call network reload") end
	for _,name in ipairs({"main","poe"}) do
		if value.lines[name].enabled ~= old.lines[name].enabled or (name == "poe" and poe_power_changed) then
			local iface = name == "main" and "cpe" or "cpe1"
			if sys.call("ubus call network.interface." .. iface .. (value.lines[name].enabled and " up" or " down")) ~= 0 then rc=1 end
		end
	end
	if firewall_changed and sys.call("/etc/init.d/firewall reload") ~= 0 then rc = 1 end
	if wan_changed and sys.call("/etc/init.d/mwan3 reload") ~= 0 then rc = 1 end
	if sys.call("/usr/sbin/c5800-route-mode apply") ~= 0 then rc = 1 end
	if rc ~= 0 or power_failed then
		http.status(503, "Service Unavailable")
		http.write_json({error=power_failed and "线路设置已应用，但副 5G 供电写入失败，请刷新状态" or "设置已保存，但应用失败，请查看系统日志"})
		return
	end
	http.write_json(route_state())
end

function action_smscs()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port

	os.execute("/usr/share/modem/smsc.sh \'" .. port .. "\' \'" .. fixed .. "\'")
	smsc = "/tmp/smsc.at"
	file = io.open(smsc, "r")
	if file ~= nil then
		rv["smsc"] = file:read("*all")
		file:close()
	else
		rv["smsc"] = " "
	end
	os.execute("/usr/share/modem/rsmsc.sh")
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end


function action_send_atcmd()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	--fixed = string.gsub(set, "\"", "~")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port

	os.execute("/usr/share/modem/atcmd.sh \'" .. port .. "\' \'" .. fixed .. "\'")
	result = "/tmp/result.at"
	file = io.open(result, "r")
	if file ~= nil then
		rv["result"] = file:read("*all")
		file:close()
	else
		rv["result"] = " "
	end
	os.execute("/usr/share/modem/delatcmd.sh")
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_get_csq()
	local mtype = modem_type()
	if mtype == "RM520" then
		rm520_rm500U()
	elseif mtype == "RM500U" then
		rm520_rm500U()
	elseif mtype == "MT5700" then
		MT5700()
	else
		luci.http.prepare_content("application/json")
		luci.http.write_json({})
	end
end

function rm520_rm500U()
	os.execute("/usr/share/modem/zinfo.sh")
	local file
	stat = "/tmp/cpe_cell.file"
	file = io.open(stat, "r")
	local rv ={}
	rv["stsss"] = file:read("*line")
	rv["modem"] = file:read("*line")
	rv["conntype"] = file:read("*line")
	rv["firmware"] = file:read("*line")
	rv["temper"] = file:read("*line")
	rv["date"] = file:read("*line")
	--------------------------------
	rv["simsel"] = file:read("*line")
	rv["cops"] = file:read("*line")
	rv["imei"] = file:read("*line")
	rv["imsi"] = file:read("*line")
	rv["iccid"] = file:read("*line")
	rv["phone"] = file:read("*line")
	--------------------------------
	rv["mode"] = file:read("*line")
	rv["per"] = file:read("*line")
	rv["rssi"] = file:read("*line")
	rv["rsrq"] = file:read("*line")
	rv["rscp"] = file:read("*line")
	rv["sinr"] = file:read("*line")
	-------------------------------
	rv["mcc"] = file:read("*line")
	rv["lac"] = file:read("*line")
	rv["cid"] = file:read("*line")
	rv["band"] = file:read("*line")
	rv["rfcn"] = file:read("*line")
	rv["pci"] = file:read("*line")
	rv["apn"] = file:read("*line")
	rv["down"] = file:read("*line")
	rv["up"] = file:read("*line")
	--------------------------------
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function MT5700()
	local iface = mt5700_iface() or "cpe"
	os.execute("CPE_IFACE='" .. iface .. "' /usr/share/modem/zinfo_mt5700.sh '" .. iface .. "'")
    local file, file2
    local stat = "/tmp/cpe_cell.file"
    local stat2 = "/tmp/stsss_u.file"
    file = io.open(stat, "r")
    file2 = io.open(stat2, "r")
	local rv ={}
	if file == nil then
		rv["stsss"] = "cpe/cpe1 interface data not ready"
		luci.http.prepare_content("application/json")
		luci.http.write_json(rv)
		return
	end
	rv["cpe1_role"] = cpe1_role()
	rv["stsss"] = file:read("*line")
	rv["modem"] = file:read("*line")
	rv["conntype"] = file:read("*line")
	rv["firmware"] = file:read("*line")
	rv["temper"] = file:read("*line")
	rv["date"] = file:read("*line")
	--------------------------------
	rv["simsel"] = file:read("*line")
	rv["cops"] = file:read("*line")
	rv["imei"] = file:read("*line")
	rv["imsi"] = file:read("*line")
	rv["iccid"] = file:read("*line")
	rv["phone"] = file:read("*line")
	--------------------------------
	rv["mode"] = file:read("*line")
	rv["per"] = file:read("*line")
	rv["rssi"] = file:read("*line")
	rv["rsrq"] = file:read("*line")
	rv["rscp"] = file:read("*line")
	rv["sinr"] = file:read("*line")
	-------------------------------
	rv["mcc"] = file:read("*line")
	rv["lac"] = file:read("*line")
	rv["cid"] = file:read("*line")
	rv["band"] = file:read("*line")
	rv["rfcn"] = file:read("*line")
	rv["pci"] = file:read("*line")
	rv["apn"] = file:read("*line")
	rv["down"] = file:read("*line")
	rv["up"] = file:read("*line")
	rv["qci"] = file:read("*line")
	rv["zbjh"] = file:read("*line")
	rv["r2cc"] = file:read("*line")
	rv["r3cc"] = file:read("*line")
	--------------------------------
	rv["cell1"] = file:read("*line")
	rv["cell2"] = file:read("*line")
	rv["cell3"] = file:read("*line")
	rv["cell4"] = file:read("*line")
	rv["cell5"] = file:read("*line")
-------------------------------
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

local function status_section(iface)
	local text = read_runtime_file("/tmp/infocd/cache/cpestatus")
	local marker = '"name": "' .. iface .. '"'
	local start = string.find(text, marker, 1, true)
	local next_start

	if not start then
		return ""
	end

	if iface == "cpe" then
		next_start = string.find(text, '"name": "cpe1"', start + #marker, true)
	end

	if next_start then
		return string.sub(text, start, next_start - 1)
	end

	return string.sub(text, start)
end

local function status_field(section, key)
	local value = string.match(section, '"' .. key .. '"%s*:%s*"([^"]*)"')

	if value ~= nil then
		return value
	end

	value = string.match(section, '"' .. key .. '"%s*:%s*([%-%d]+)')
	if value ~= nil then
		return value
	end

	return ""
end

local function isp_name(code)
	if code == "46000" or code == "46002" or code == "46004" or code == "46007" or code == "46008" or code == "46020" then
		return "中国移动"
	elseif code == "46001" or code == "46006" or code == "46009" then
		return "中国联通"
	elseif code == "46003" or code == "46005" or code == "46011" then
		return "中国电信"
	elseif code == "46015" then
		return "中国广电"
	end

	return code ~= "" and code or "-"
end

local function signal_percent(rsrp)
	local value = tonumber(rsrp)

	if value == nil then
		return "-"
	elseif value >= -85 then
		return "100"
	elseif value >= -90 then
		return "90"
	elseif value >= -95 then
		return "80"
	elseif value >= -105 then
		return "50"
	end

	return "10"
end

local function bandwidth_text(section)
	local down = tonumber(status_field(section, "DLBW"))
	local up = tonumber(status_field(section, "ULBW"))

	if down == nil or up == nil then
		return ""
	end

	return string.format(" (Bandwidth %d MHz Down | %d MHz Up)", down / 1000, up / 1000)
end

local function rate_mbps(section, key)
	local value = tonumber(status_field(section, key))

	if value == nil then
		return "-"
	end

	return tostring(math.floor(value / 1000)) .. "mbps"
end

local function field_or_dash(value)
	if value ~= nil and value ~= "" then
		return value
	end

	return "-"
end

local function iface_device(iface)
	local device = uci_get("network." .. iface .. ".device")

	if device == "" then
		device = uci_get("network." .. iface .. ".ifname")
	end

	if device == "" and iface == "cpe" then
		device = "eth1"
	elseif device == "" and iface == "cpe1" then
		device = "port5"
	end

	return field_or_dash(device)
end

local function fill_iface_summary(rv, prefix, iface)
	local json = require "luci.jsonc"
	local device = iface_device(iface)
	local addresses = { [4] = {}, [6] = {} }
	local online = false
	for _, name in ipairs({ iface, iface .. "_4", iface .. "_6" }) do
		local status = json.parse(ifstatus_text(name)) or {}
		if status.up == true then
			for _, family in ipairs({ 4, 6 }) do
				for _, entry in ipairs(status["ipv" .. family .. "-address"] or {}) do
					local address = entry.address or ""
					if address ~= "" and not address:match("^fe80:") then
						addresses[family][#addresses[family] + 1] = address
						online = true
					end
				end
			end
		end
	end
	rv[prefix .. "iface"] = iface
	rv[prefix .. "ifdev"] = device
	rv[prefix .. "connected"] = online
	rv[prefix .. "ipv4"] = field_or_dash(table.concat(addresses[4], ", "))
	rv[prefix .. "ipv6"] = field_or_dash(table.concat(addresses[6], ", "))
	rv[prefix .. "carrier"] = device:match("^[%w_.%-]+$") and
		read_runtime_file("/sys/class/net/" .. device .. "/carrier"):match("[01]") or "-"
	rv[prefix .. "proto"] = field_or_dash(uci_get("network." .. iface .. ".proto"))
	rv[prefix .. "defaultroute"] = field_or_dash(uci_get("network." .. iface .. ".defaultroute"))
	rv[prefix .. "metric"] = field_or_dash(uci_get("network." .. iface .. ".metric"))
	rv[prefix .. "odu"] = "-"
end

local function fill_carriers(rv, prefix, signal)
	local radio = require("c5800_secondary").carriers(signal)
	rv[prefix .. "carriers"] = radio.carriers
	rv[prefix .. "carrier_count"] = radio.count
	rv[prefix .. "band_summary"] = radio.bands
	rv[prefix .. "zbjh"] = radio.count > 1 and (radio.count .. "CC · " .. radio.detail) or "未聚合"
end

local function fill_mt5700_from_cache(rv, prefix, iface)
	local section = status_section(iface)
	local rsrp
	local rsrq
	local sinr
	local band
	fill_iface_summary(rv, prefix, iface)
	rv[prefix .. "present"] = false

	if section == "" then
		rv[prefix .. "stsss"] = iface .. " cache not ready"
		return false
	end

	rsrp = status_field(section, "rsrp")
	rsrq = status_field(section, "rsrq")
	sinr = status_field(section, "sinr")
	band = status_field(section, "band")

	rv[prefix .. "stsss"] = "接口缓存 " .. iface
	rv[prefix .. "odu"] = field_or_dash(status_field(section, "odu"))
	rv[prefix .. "modem"] = "TD Tech Ltd."
	rv[prefix .. "conntype"] = status_field(section, "model")
	rv[prefix .. "present"] = rv[prefix .. "conntype"] ~= "" and
		rv[prefix .. "conntype"] ~= iface and rv[prefix .. "carrier"] ~= "0"
	rv[prefix .. "firmware"] = status_field(section, "revision")
	rv[prefix .. "temper"] = status_field(section, "model_temp")
	rv[prefix .. "date"] = os.date("%Y-%m-%d %H:%M:%S")
	rv[prefix .. "simsel"] = status_field(section, "sim_name")
	rv[prefix .. "simno"] = status_field(section, "simno")
	rv[prefix .. "pin"] = status_field(section, "CPIN")
	if rv[prefix .. "simsel"] == "" and rv[prefix .. "simno"] ~= "" then
		rv[prefix .. "simsel"] = "SIM " .. rv[prefix .. "simno"]
	end
	rv[prefix .. "cops"] = isp_name(status_field(section, "isp"))
	rv[prefix .. "imei"] = status_field(section, "imei")
	rv[prefix .. "imsi"] = status_field(section, "imsi")
	rv[prefix .. "iccid"] = status_field(section, "iccid")
	rv[prefix .. "phone"] = status_field(section, "simnumber")
	rv[prefix .. "mode"] = status_field(section, "mode")
	rv[prefix .. "per"] = signal_percent(rsrp)
	rv[prefix .. "rssi"] = status_field(section, "rssi") ~= "" and status_field(section, "rssi") or rsrp
	rv[prefix .. "rsrq"] = rsrq ~= "" and (rsrq .. " dB") or "-"
	rv[prefix .. "rscp"] = rsrp
	rv[prefix .. "sinr"] = sinr
	rv[prefix .. "mcc"] = status_field(section, "isp")
	rv[prefix .. "lac"] = status_field(section, "tac")
	rv[prefix .. "tac"] = status_field(section, "tac")
	rv[prefix .. "cid"] = status_field(section, "cell")
	rv[prefix .. "band"] = band ~= "" and ("N" .. band .. bandwidth_text(section)) or "-"
	rv[prefix .. "raw_band"] = band
	rv[prefix .. "rfcn"] = status_field(section, "earfcn") ~= "" and status_field(section, "earfcn") or status_field(section, "dl_fcn")
	rv[prefix .. "dl_fcn"] = field_or_dash(status_field(section, "dl_fcn"))
	rv[prefix .. "ul_fcn"] = field_or_dash(status_field(section, "ul_fcn"))
	rv[prefix .. "dlbw"] = field_or_dash(status_field(section, "DLBW"))
	rv[prefix .. "ulbw"] = field_or_dash(status_field(section, "ULBW"))
	rv[prefix .. "pci"] = status_field(section, "pci")
	rv[prefix .. "apn"] = status_field(section, "apn") ~= "" and status_field(section, "apn") or "-"
	if rv[prefix .. "ipv4"] == "-" then
		rv[prefix .. "ipv4"] = field_or_dash(status_field(section, "V4"))
	end
	if rv[prefix .. "ipv6"] == "-" then
		rv[prefix .. "ipv6"] = field_or_dash(status_field(section, "V6"))
	end
	rv[prefix .. "ipv6pd"] = field_or_dash(status_field(section, "PD"))
	rv[prefix .. "down"] = rate_mbps(section, "NR5G_AMBR_DL")
	rv[prefix .. "up"] = rate_mbps(section, "NR5G_AMBR_UL")
	rv[prefix .. "qci"] = status_field(section, "CQI") ~= "" and ("CQI: " .. status_field(section, "CQI")) or "-"
	fill_carriers(rv, prefix, function(key) return status_field(section, key) end)
	rv[prefix .. "r2cc"] = "-"
	rv[prefix .. "r3cc"] = "-"
	rv[prefix .. "cell1"] = "-"
	rv[prefix .. "cell2"] = "-"
	rv[prefix .. "cell3"] = "-"
	rv[prefix .. "cell4"] = "-"
	rv[prefix .. "cell5"] = "-"

	return true
end

local function fill_ak_secondary(rv)
	local state = require("c5800_secondary").status(true)
	local signal = state.signal or {}
	local prefix = "cpe1_"
	fill_iface_summary(rv, prefix, "cpe1")
	rv[prefix .. "present"] = state.connected and state.name ~= "副设备信息未读取"
	rv[prefix .. "conntype"] = state.name
	rv[prefix .. "modem"] = field_or_dash(signal.model)
	rv[prefix .. "odu"] = state.name
	rv[prefix .. "firmware"] = field_or_dash(state.version)
	rv[prefix .. "temper"] = field_or_dash(signal.model_temp)
	rv[prefix .. "simsel"] = (state.slots[state.cur] or {}).label or field_or_dash(signal.sim_name)
	rv[prefix .. "simno"] = tostring(state.cur)
	rv[prefix .. "pin"] = field_or_dash(signal.CPIN)
	rv[prefix .. "cops"] = isp_name(signal.isp or "")
	rv[prefix .. "mode"] = field_or_dash(signal.mode)
	rv[prefix .. "rscp"] = field_or_dash(signal.rsrp)
	rv[prefix .. "rsrq"] = field_or_dash(signal.rsrq)
	rv[prefix .. "rssi"] = field_or_dash(signal.rssi)
	rv[prefix .. "sinr"] = field_or_dash(signal.sinr)
	rv[prefix .. "raw_band"] = field_or_dash(signal.band)
	rv[prefix .. "rfcn"] = field_or_dash(signal.earfcn)
	rv[prefix .. "pci"] = field_or_dash(signal.pci)
	rv[prefix .. "imei"] = field_or_dash(signal.imei)
	rv[prefix .. "imsi"] = field_or_dash(signal.imsi)
	rv[prefix .. "iccid"] = field_or_dash(signal.iccid)
	rv[prefix .. "mcc"] = field_or_dash(signal.isp)
	rv[prefix .. "tac"] = field_or_dash(signal.tac)
	rv[prefix .. "cid"] = field_or_dash(signal.cell)
	rv[prefix .. "apn"] = field_or_dash(signal.apn)
	rv[prefix .. "dl_fcn"] = field_or_dash(signal.dl_fcn)
	rv[prefix .. "ul_fcn"] = field_or_dash(signal.ul_fcn)
	rv[prefix .. "dlbw"] = field_or_dash(signal.DLBW)
	rv[prefix .. "ulbw"] = field_or_dash(signal.ULBW)
	rv[prefix .. "qci"] = field_or_dash(signal.CQI)
	fill_carriers(rv, prefix, signal)
	rv[prefix .. "stsss"] = "AK 原厂实时状态"
end

function MT5700()
	local rv = {}

	if not fill_mt5700_from_cache(rv, "", "cpe") then
		rv["stsss"] = "cpe/cpe1 interface data not ready"
	end

	rv["cpe1_role"] = cpe1_role()
	if rv["cpe1_role"] == "cellular" then
		fill_ak_secondary(rv)
	elseif rv["cpe1_role"] == "broadband" then
		fill_iface_summary(rv, "cpe1_", "cpe1")
		rv["cpe1_stsss"] = "cpe1 broadband mode"
	else
		fill_iface_summary(rv, "cpe1_", "cpe1")
		rv["cpe1_stsss"] = "cpe1 unknown mode"
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end
