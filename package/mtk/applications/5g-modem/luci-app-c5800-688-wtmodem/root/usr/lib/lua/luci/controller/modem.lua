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
	entry({"admin", "modem", "poe_set"}, post("action_poe_set"))
	entry({"admin", "modem", "route_status"}, call("action_route_status"))
	entry({"admin", "modem", "route_set"}, post("action_route_set"))
	entry({"admin", "modem", "radio_snapshot"}, call("action_radio_snapshot"))
	entry({"admin", "modem", "radio_lock_status"}, call("action_radio_lock_status"))
	entry({"admin", "modem", "radio_lock_set"}, post("action_radio_lock_set"))
	entry({"admin", "modem", "send_atcmd"}, call("action_send_atcmd"))
	entry({"admin", "modem", "smscs"}, call("action_smscs"))
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
	for _, key in ipairs({"rat", "kind", "mobility", "bands", "arfcn", "scs", "pci", "mode"}) do input[key] = http.formvalue(key) end
	local result = require("c5800_radio_lock").apply(http.formvalue("iface"), input)
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
	if state.enabled ~= enabled then
		local file = io.open("/sys/class/gpio/cpe1-pwr/value", "w")
		local written = file and file:write(enabled and "0" or "1")
		if file then file:close() end
		state = poe_state()
		if not written or state.enabled ~= enabled then
			http.status(500, "Internal Server Error")
			http.write_json({ error = "供电状态写入失败，请刷新后重试" })
			return
		end
	end
	http.write_json(state)
end

local function route_state()
	return {
		mode = uci_get("mwan3.globals.download_mode"),
		secondary_role = cpe1_role(),
		running = read_runtime_file("/var/run/mwan3/mmx_mask") ~= "",
		main_online = read_runtime_file("/var/run/mwan3/iface_state/cpe_4"):match("online") ~= nil,
		poe_online = read_runtime_file("/var/run/mwan3/iface_state/cpe1"):match("online") ~= nil
	}
end

function action_route_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json(route_state())
end

function action_route_set()
	local http = luci.http
	local mode = http.formvalue("mode")
	http.prepare_content("application/json")
	if mode ~= "daily" and mode ~= "download" then
		http.status(400, "Bad Request")
		http.write_json({ error = "线路策略参数无效" })
		return
	end
	if require("luci.sys").call("sh /usr/sbin/c5800-route-mode " .. mode) ~= 0 then
		http.status(503, "Service Unavailable")
		http.write_json({ error = "线路策略应用失败，请刷新状态" })
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
	rv[prefix .. "zbjh"] = status_field(section, "band_count") ~= "" and ("NR5G band_count: " .. status_field(section, "band_count")) or "-"
	rv[prefix .. "r2cc"] = "-"
	rv[prefix .. "r3cc"] = "-"
	rv[prefix .. "cell1"] = "-"
	rv[prefix .. "cell2"] = "-"
	rv[prefix .. "cell3"] = "-"
	rv[prefix .. "cell4"] = "-"
	rv[prefix .. "cell5"] = "-"

	return true
end

function MT5700()
	local rv = {}

	if not fill_mt5700_from_cache(rv, "", "cpe") then
		rv["stsss"] = "cpe/cpe1 interface data not ready"
	end

	rv["cpe1_role"] = cpe1_role()
	if rv["cpe1_role"] == "cellular" then
		fill_mt5700_from_cache(rv, "cpe1_", "cpe1")
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
