local root = assert(arg[1], "source directory is required")
package.path = root .. "/?.lua;" .. package.path

local lease_file = "/tmp/myos-nradio-test-dhcp.leases"
local lease = assert(io.open(lease_file, "w"))
lease:write("1786253600 AA:BB:CC:DD:EE:01 192.168.66.10 phone *\n")
lease:close()

local cursor = {}
local adopted_device_code = ""
local manual_mac = ""
local use_iwinfo = false
local led_available, led_writes, led_fail_write = false, 0, false
local board_name, led_hour, led_minute = "nradio,c2000-ultra", 12, 0
local led_values = {
	["hc:blue:status"] = { brightness = "200", trigger = "timer", delay_on = "250", delay_off = "750" },
	["hc:blue:sig1"] = { brightness = "255", trigger = "none" },
	["hc:blue:sig2"] = { brightness = "0", trigger = "none" },
	["hc:blue:sig3"] = { brightness = "128", trigger = "none" },
	["hc:red:error"] = { brightness = "0", trigger = "none" },
	["mdio:phy:activity"] = { brightness = "255", trigger = "netdev" }
}
for _, node in pairs(led_values) do node.max_brightness = "255" end
local failed_led_read, commit_count, fail_commit_at = nil, 0, nil
local sms_deletes = 0
local app_options, pending_options = {}, {}
local sms_available, sms_delete_fail = true, false
local sms_messages = {
	{ index = 7, sender = "10086", timestamp = 1786248000, content = "balance" },
	{ index = 8, sender = "10010", timestamp = 1786248100,
		content = "welcome", reference = 12, total = 2, part = 1 }
}
local sms_history = {}
local history_read_fail, history_delete_fail = false, false
local original_execute = os.execute
local original_open, original_date = io.open, os.date
os.execute = function(command)
	assert(not command:find("/etc/init.d/", 1, true), "Ultra APP must not invoke imported services")
	return original_execute(command)
end
os.date = function(format, value)
	if format == "*t" and value == nil then return { hour = led_hour, min = led_minute } end
	return original_date(format, value)
end
io.open = function(path, mode)
	if path == "/tmp/sysinfo/board_name" then
		return { read = function() return board_name end, close = function() return true end }
	end
	local name, attribute = path:match("^/sys/class/leds/([^/]+)/([%w_]+)$")
	if name then
		local node = led_values[name]
		if not led_available or not node or node[attribute] == nil then return nil end
		return {
			read = function()
				if path == failed_led_read then return nil end
				return attribute == "trigger" and ("[" .. node.trigger .. "] none timer") or node[attribute]
			end,
			write = function(self, value)
				assert(name:match("^hc:"), "APP must not write PHY LEDs")
				if led_fail_write then return nil, "write failed" end
				led_writes = led_writes + 1
				node[attribute] = value
				if attribute == "brightness" and value == "0" then node.trigger = "none" end
				if attribute == "trigger" then
					if value == "timer" then
						node.delay_on, node.delay_off = "100", "100"
					else
						node.delay_on, node.delay_off = nil, nil
					end
				end
				return self
			end,
			close = function() return true end
		}
	end
	return original_open(path, mode)
end
function cursor:get(config, section, option)
	if config == "dhcp" and section == "@dnsmasq[0]" and
	   option == "leasefile" then return lease_file end
	if config == "oem" and section == "board" and option == "device_code" then
		return adopted_device_code ~= "" and adopted_device_code or nil
	end
	if config == "oem" and section == "board" and option == "id" then
		return "A2:B3:C4:D5:E6:F7"
	end
	if config == "myos_nros" and section == "main" and option == "manual_mac" then
		return manual_mac ~= "" and manual_mac or nil
	end
	if config == "myos_nros" and section == "main" then
		if pending_options[option] ~= nil then return pending_options[option] or nil end
		return app_options[option]
	end
	if config == "oem" and section == "board" and option == nil then
		return "system"
	end
	return nil
end
function cursor:get_all() return {} end
function cursor:load() return true end
function cursor:unload() pending_options = {}; return true end
function cursor:set(config, section, option, value)
	assert(config ~= "ledctrl", "Ultra must use existing APP configuration")
	if config == "oem" and section == "board" and option == "device_code" then
		adopted_device_code = value
	end
	if config == "myos_nros" and section == "main" then pending_options[option] = value end
	return true
end
function cursor:delete(config, section, option)
	assert(config == "myos_nros" and section == "main", "only APP state may be removed")
	pending_options[option] = false
	return true
end
function cursor:commit()
	commit_count = commit_count + 1
	if commit_count == fail_commit_at then return false end
	for key, value in pairs(pending_options) do app_options[key] = value or nil end
	pending_options = {}
	return true
end
function cursor:foreach(config, section_type, callback)
	if config == "qmodem" and section_type == "modem-device" then
		callback({
			[".name"] = "2_1",
			state = "enabled",
			at_port = "/dev/ttyUSB1",
			manufacturer = "huawei"
		})
	elseif config == "access_ctl" and section_type == "client" then
		callback({
			[".name"] = "blocked_laptop",
			mac = "AA:BB:CC:DD:EE:02",
			switch = "0",
			hostname = "blocked-laptop"
		})
	end
	return true
end

package.preload["luci.model.uci"] = function()
	return { cursor = function() return cursor end }
end
package.preload["luci.util"] = function()
	return {
		ubus = function(object, method, args)
			if object == "qmodem" and method == "get_sms" then
				if not sms_available then return { error = "modem unavailable" } end
				return {
					msg = sms_messages,
					sms_capabilities = {
						mem1 = "ME", ME = { used = "2", total = "255" }
					}
				}
			elseif object == "qmodem" and method == "delete_sms" then
				sms_deletes = sms_deletes + 1
				assert(args and args.config_section == "2_1", "SMS delete section")
				for id in args.index:gmatch("%d+") do
					assert(tonumber(id) < 1000000000, "saved-message ID must never reach SIM deletion")
					if not sms_delete_fail then
						for index = #sms_messages, 1, -1 do
							if sms_messages[index].index == tonumber(id) then table.remove(sms_messages, index) end
						end
					end
				end
				return { result = { status = "1" } }
			elseif object == "qmodem_sms" and method == "get_received_history" then
				if history_read_fail then return { error = "history unavailable" } end
				return { received = sms_history }
			elseif object == "qmodem_sms" and method == "delete_sms" then
				assert(args.type == "received", "delete only selected received history")
				if history_delete_fail then
					history_read_fail = true
					return { success = false }
				end
				for _, id in ipairs(args.ids) do
					for index = #sms_history, 1, -1 do
						if sms_history[index].id == id then table.remove(sms_history, index) end
					end
				end
				return { success = true }
			elseif object == "qmodem_sms" and method == "list_sms" then
				error("background APP reads must not invoke auto-delete list_sms")
			elseif object == "qmodem" and method == "send_sms" then
				assert(args and args.config_section == "2_1", "SMS send section")
				assert(args.params.phone_number == "10086", "SMS send phone")
				assert(args.params.message_content == "hello", "SMS send content")
				return { result = { status = "1" } }
			end
			return {}
		end,
		exec = function(command)
			if command == "iw dev 2>/dev/null" then
				if use_iwinfo then return "" end
				return table.concat({
					"phy#0",
					"\tInterface wlan0",
					"\t\taddr 02:11:22:33:44:55",
					"\t\tssid MYOS-5G",
					"\t\ttype AP",
					"\t\tchannel 36 (5180 MHz)"
				}, "\n")
			elseif command == "iwinfo 2>/dev/null" and use_iwinfo then
				return table.concat({
					'ra0       ESSID: "MYOS-2.4G"',
					'          Access Point: 02:11:22:33:44:55',
					'          Mode: Master  Channel: 9 (2.452 GHz)  HT Mode: EHT40'
				}, "\n")
			elseif command:match("iwinfo 'ra0' assoclist") then
				return table.concat({
					"AA:BB:CC:DD:EE:01  -36 dBm / -75 dBm (SNR 39)  0 ms ago",
					"\tRX: 453.0 MBit/s",
					"\tTX: 1921.0 MBit/s"
				}, "\n")
			elseif command:match("iw dev 'wlan0' station dump") then
				return table.concat({
					"Station AA:BB:CC:DD:EE:01 (on wlan0)",
					"\trx bytes:\t1234",
					"\ttx bytes:\t5678",
					"\tsignal:\t-52 dBm",
					"\trx bitrate:\t240.0 MBit/s",
					"\ttx bitrate:\t120.0 MBit/s",
					"\tconnected time:\t90 seconds"
				}, "\n")
			elseif command == "ip -4 neigh show dev br-lan 2>/dev/null" then
				return "192.168.66.20 dev br-lan lladdr AA:BB:CC:DD:EE:02 REACHABLE\n"
			end
			return ""
		end
	}
end
local json_values = {}
local function clone(value)
	if type(value) ~= "table" then return value end
	local result = {}
	for key, item in pairs(value) do result[key] = clone(item) end
	return result
end
local function encode(value)
	if type(value) == "string" then
		return '"' .. value:gsub('[%z\1-\31\\"]', function(char)
			return string.format("\\u%04x", char:byte())
		end) .. '"'
	end
	if type(value) ~= "table" then return value == nil and "null" or tostring(value) end
	local parts, keys = {}, {}
	if #value > 0 then
		for _, item in ipairs(value) do parts[#parts + 1] = encode(item) end
	else
		for key in pairs(value) do keys[#keys + 1] = key end
		table.sort(keys)
		for _, key in ipairs(keys) do parts[#parts + 1] = encode(key) .. ":" .. encode(value[key]) end
	end
	local result = (#value > 0 and "[" or "{") .. table.concat(parts, ",") .. (#value > 0 and "]" or "}")
	json_values[result] = clone(value)
	return result
end
package.preload["cjson.safe"] = function()
	return { encode = encode, decode = function(value) return clone(json_values[value]) end }
end
package.preload["nixio"] = function()
	return {
		gettimeofday = function() return 1786250000, 123456 end,
		getpid = function() return 1 end
	}
end
package.preload["nixio.fs"] = function()
	return {
		access = function(path, mode)
			local name, attribute = path:match("^/sys/class/leds/([^/]+)/([%w_]+)$")
			return led_available and name and led_values[name] and led_values[name][attribute] ~= nil or false
		end,
		mkdirr = function(path)
			os.execute("mkdir -p '" .. tostring(path):gsub("'", "'\\''") .. "'")
			return true
		end,
		chmod = function() return true end,
		stat = function() return nil end
	}
end

local function equal(actual, expected, label)
	if actual ~= expected then
		error(string.format("%s: expected %s, got %s", label,
			tostring(expected), tostring(actual)))
	end
end

local data = require "myos_nradio_data"

local signal = assert(data._test.parse_monsc(
	"^MONSC: NR,460,00,504990,1,C248F7002,380,143076,-82,-9,11\r\nOK"))
equal(signal.mode, "NR SA", "MONSC mode")
equal(signal.earfcn, "504990", "MONSC ARFCN")
equal(signal.cell, "52152987650", "MONSC cell")
equal(signal.pci, "896", "MONSC hexadecimal PCI")
equal(signal.rsrp, "-82", "MONSC RSRP")
equal(signal.rsrq, "-9", "MONSC RSRQ")
equal(signal.sinr, "11", "MONSC SINR")

local nsa = assert(data._test.parse_monsc(
	"^MONSC: LTE-NR,460,00,1650,C248F7002,12A,143076,-93,-12,10\r\nOK"))
equal(nsa.mode, "NR NSA", "MONSC NSA mode")
equal(nsa.cell, "52152987650", "MONSC NSA cell")
equal(nsa.pci, "298", "MONSC NSA PCI")
equal(nsa.rsrp, "-93", "MONSC NSA RSRP")

local carriers = assert(data._test.parse_hfreqinfo(
	"^HFREQINFO: 0,7," ..
	"41,504990,2524950,100000,504990,2524950,100000," ..
	"41,500000,2500000,80000,500000,2500000,40000," ..
	"41,530000,2650000,60000,530000,2650000,30000\r\nOK",
	"NR SA"))
equal(carriers.band_count, 3, "HFREQINFO carrier count")
equal(carriers.band, "41", "HFREQINFO primary band")
equal(carriers.DLBW, "100000", "HFREQINFO primary bandwidth")
equal(carriers.band1, "41", "HFREQINFO second band")
equal(carriers.dl_fcn2, "530000", "HFREQINFO third channel")
equal(carriers.ULBW2, "30000", "HFREQINFO third uplink bandwidth")

local neighbours = data._test.parse_neighbor_cells(table.concat({
	"^MONNC: NR,504990,380,-656,-112,160",
	"^MONNC: LTE,1650,12A,-93,-12,-65",
	"^MONNC: NR-5GC,633984,111,-640,-96,144",
	"^MONNC: eMTC,1800,010,-101,-14,-70",
	"OK"
}, "\r\n"))
equal(#neighbours, 4, "MONNC cell count")
equal(neighbours[1].MODE, "NR", "MONNC NR mode")
equal(neighbours[1].PCI, "896", "MONNC NR PCI")
equal(neighbours[1].RSRP, "-82", "MONNC encoded NR RSRP")
equal(neighbours[1].RSRQ, "-14", "MONNC encoded NR RSRQ")
equal(neighbours[1].SINR, "20", "MONNC encoded NR SINR")
equal(neighbours[2].MODE, "LTE", "MONNC LTE mode")
equal(neighbours[2].PCI, "298", "MONNC LTE PCI")
equal(neighbours[3].MODE, "NR", "MONNC NR-5GC alias")
equal(neighbours[4].MODE, "LTE", "MONNC eMTC alias")

equal(data.app_software_version(), "2.2.12.n0.c0", "APP software version")
equal(data.app_software_version(), "2.2.12.n0.c0", "no LED backend version gate")
equal(data.led({}).result.led, -1, "missing LED backend capability")
equal(data.led({ led = 0 }).code, 2, "missing LED backend write rejected")
led_available = true
equal(data.app_software_version(), "2.2.15.n0.c0", "local and cloud LED version gate")
equal(data.led({}).result.led, 1, "native LED state")
equal(led_writes, 0, "LED reads must not write class state")
equal(data.led({ led = 0, schedule = 0 }).result.led, 0, "LED off")
equal(app_options.led_disabled, "1", "APP LED disabled polarity")
equal(led_values["hc:blue:status"].brightness, "0", "Ultra status LED off")
equal(led_values["hc:blue:sig1"].brightness, "0", "Ultra signal LED off")
equal(led_values["mdio:phy:activity"].brightness, "255", "PHY LED remains untouched")
local writes = led_writes
equal(data.led_tick(), true, "idempotent LED tick")
equal(led_writes, writes, "unchanged LED state causes no writes")
equal(data.led({ led = 1, schedule = 0 }).code, 0, "restore LED state")
equal(led_values["hc:blue:status"].brightness, "200", "restore original brightness")
equal(led_values["hc:blue:status"].trigger, "timer", "restore original trigger")
equal(led_values["hc:blue:status"].delay_on, "250", "restore timer on delay")
equal(led_values["hc:blue:status"].delay_off, "750", "restore timer off delay")
equal(app_options.led_restore, nil, "clear restore state only after successful restoration")
led_hour, led_minute = 23, 0
local led = data.led({ led = 1, schedule = 1, start_time = "22:30", end_time = "07:00" })
equal(led.code, 0, "LED schedule write")
equal(led.result.schedule, 1, "LED schedule enabled")
equal(led.result.end_time, "7:0", "native LED time format")
equal(app_options.led_disabled, "0", "APP LED enabled polarity")
equal(led_values["hc:blue:status"].brightness, "0", "night schedule switches LEDs off")
led_hour, led_minute = 6, 59
equal(data.led_tick(), true, "schedule crosses midnight")
equal(led_values["hc:blue:status"].brightness, "0", "night schedule remains off before end")
led_hour, led_minute = 7, 0
equal(data.led_tick(), true, "schedule end tick")
equal(led_values["hc:blue:status"].brightness, "200", "schedule end restores LEDs")
writes = led_writes
for _, request in ipairs({
	{ led = 2 }, { led = false }, { schedule = "1;reboot" },
	{ led = 0, start_time = "24:00" }, { end_time = "12:60" },
	{ start_time = "-1:00" }, { end_time = "1:2;reboot" }
}) do
	equal(data.led(request).code, 2, "invalid LED setting rejected")
end
equal(led_writes, writes, "invalid LED settings cause no class writes")
equal(app_options.led_disabled, "0", "invalid LED settings cause no partial write")
led_fail_write = true
equal(data.led({ led = 0, schedule = 0 }).code, 2, "LED write failure is not success")
led_fail_write = false
equal(data.led({ led = 0, schedule = 0 }).code, 0, "retry unchanged failed LED request")
equal(data.led({ led = 1, schedule = 0 }).code, 0, "restore after retry")
led_values["hc:blue:status"].trigger = "unknown-trigger"
writes = led_writes
equal(data.led({ led = 0 }).code, 2, "unknown trigger must not be destroyed")
equal(led_writes, writes, "unknown trigger causes no partial class writes")
led_values["hc:blue:status"].trigger = "timer"
equal(data.led({ led = 1 }).code, 0, "leave unchanged LEDs enabled")
failed_led_read = "/sys/class/leds/hc:blue:status/brightness"
writes = led_writes
equal(data.led({ led = 0 }).code, 2, "failed state read must not switch LEDs off")
equal(led_writes, writes, "failed snapshot causes no class writes")
failed_led_read = nil
equal(data.led({ led = 1 }).code, 0, "recover from state read failure")
fail_commit_at = commit_count + 2
writes = led_writes
equal(data.led({ led = 0 }).code, 2, "failed snapshot persistence must not switch LEDs off")
equal(led_writes, writes, "failed persistence preserves original LED state")
equal(app_options.led_restore, nil, "failed snapshot must not be reported as saved")
fail_commit_at = nil
equal(data.led({ led = 1 }).code, 0, "recover from persistence failure")
led_hour, led_minute = 7, 59
equal(data.led({ led = 1, schedule = 1, start_time = "8:0", end_time = "18:0" }).code, 0,
	"day schedule before start")
equal(led_values["hc:blue:status"].brightness, "200", "day schedule leaves LEDs on before start")
led_hour, led_minute = 8, 0
equal(data.led_tick(), true, "day schedule start tick")
equal(led_values["hc:blue:status"].brightness, "0", "day schedule includes start")
led_hour, led_minute = 18, 0
equal(data.led_tick(), true, "day schedule end tick")
equal(led_values["hc:blue:status"].brightness, "200", "day schedule excludes end")
equal(data.led({ schedule = 1, start_time = "0:0", end_time = "0:0" }).code, 0, "equal times mean all-day off")
equal(led_values["hc:blue:status"].brightness, "0", "all-day schedule")
equal(data.led({ schedule = 0 }).code, 0, "disabling schedule restores state")
board_name = "nradio,c2000-max"
equal(data.led_supported(), false, "Ultra backend rejects other boards")
board_name = "nradio,c2000-ultra"
led_available = false
data._test.set_plmn_groups({
	{ company = "China Unicom", plmn = { "46001", "46030" } },
	{ company = "T-Mobile", plmn = { "310260", "310310" } }
})
equal(data.operator_name("46030"), "China Unicom", "PLMN operator name")
equal(data.operator_name("310260"), "T-Mobile", "global PLMN operator name")
equal(data.plmn_equal("310260", "310310"), true,
	"same-company PLMN comparison")
equal(data.plmn_equal("310260", "46030"), false,
	"different-company PLMN comparison")
equal(data._test.operator_plmn({}, "310260123456789", ""), "310260",
	"IMSI PLMN length from database")
local identity_get = data.identity_get
data.identity_get = function(key)
	if key == "fac_mac" then return "0C:43:26:60:10:00" end
	return identity_get(key)
end
equal(data.device_id(), "A2B3C4D5E6F7", "configured APP device MAC priority")
manual_mac = "B2:C3:D4:E5:F6:07"
equal(data.device_id(), "B2C3D4E5F607", "manual label MAC priority")
manual_mac = ""
data.identity_get = identity_get

local clients, client_count = data.clients()
equal(client_count, 2, "merged client count")
local by_mac = {}
for _, client in ipairs(clients) do by_mac[client.mac] = client end
equal(by_mac["AA:BB:CC:DD:EE:01"].type, "wireless", "WiFi client type")
equal(by_mac["AA:BB:CC:DD:EE:01"].rssi, -52, "WiFi client RSSI")
equal(by_mac["AA:BB:CC:DD:EE:01"].download, 240000, "WiFi receive rate")
equal(by_mac["AA:BB:CC:DD:EE:01"].authed, 1, "online client auth state")
equal(by_mac["AA:BB:CC:DD:EE:02"].ip, "192.168.66.20", "neighbor client IP")
equal(by_mac["AA:BB:CC:DD:EE:02"].switch, 0, "blocked client state")
equal(by_mac["AA:BB:CC:DD:EE:02"].hostname, "blocked-laptop", "client alias")

local stations = data.station_status()
equal(stations.list[1].band, "5.2G", "station band")
equal(stations.list[1].online, 1, "station online count")
equal(stations.deny[1], "AA:BB:CC:DD:EE:02", "station deny list")

use_iwinfo = true
clients = data.clients()
by_mac = {}
for _, client in ipairs(clients) do by_mac[client.mac] = client end
equal(by_mac["AA:BB:CC:DD:EE:01"].type, "wireless", "MT7993 client type")
equal(by_mac["AA:BB:CC:DD:EE:01"].rssi, -36, "MT7993 client RSSI")
equal(by_mac["AA:BB:CC:DD:EE:01"].download, 453000,
	"MT7993 receive rate")
equal(data.adopt_device_code("Router_Code-1"), true, "device code adoption")
equal(data.device_code(), "Router_Code-1", "adopted device code")
equal(data.adopt_device_code("different"), false, "device code immutable")
equal(data.sms_request_allowed({ action = "send", device_code = "Router_Code-1" }),
	true, "SMS device code identity")
equal(data.sms_request_allowed({ action = "send", device_code = "A2B3C4D5E6F7" }),
	true, "APP 3.2.1 SMS MAC identity")
equal(data.sms_request_allowed({ action = "send", device_code = "a2:b3:c4:d5:e6:f7" }),
	true, "SMS normalized MAC identity")
equal(data.sms_request_allowed({ action = "send", device_code = "001122334455" }),
	false, "SMS foreign MAC rejected")
equal(data.sms_request_allowed({ action = "send" }), false, "SMS send identity required")
equal(data.sms_request_allowed({ action = "del" }), true, "APP 3.2.1 session-only delete")
equal(data.sms_request_allowed({ action = "del", device_code = "wrong" }),
	false, "SMS supplied wrong identity rejected")

equal(data.sms_supported(), true, "SMS capability")
equal(data.sms({ sim = "2", index = "1" }).result.enabled, "1",
	"SMS capability gate")
equal(data.sms({ action = "modify", enabled = "1", sim = "2" }).code, 0,
	"SMS enable compatibility")
equal(data.sms({ action = "modify", enabled = "1" }).result.action, "modify",
	"factory SMS enable response action")
equal(data.sms({ action = "modify", enabled = "0" }).result.enabled, "0", "APP SMS disable")
equal(data.sms({}).result.enabled, "0", "APP SMS disabled state persists")
equal(data.sms({ action = "read" }).code, 3, "disabled APP SMS blocks reads")
sms_available = false
equal(data.sms({ action = "modify", enabled = "1" }).code, 3, "enable probes actual SMS backend")
equal(app_options.sms_enabled, "0", "failed enable must not change state")
sms_available = true
equal(data.sms({ action = "modify", enabled = "1" }).result.enabled, "1", "enable available SMS backend")
sms_available = false
equal(data.sms({}).code, 3, "missing modem is unknown, not falsely enabled")
sms_available = true
local messages = data.sms({ action = "read" })
equal(messages.code, 0, "SMS read code")
equal(messages.result.ready, 1, "SMS read completion")
equal(messages.result.count, 2, "SMS read count")
equal(messages.result.total, 255, "SMS capacity")
equal(messages.result.smslist[1].contact, "10010", "SMS newest conversation")
equal(messages.result.smslist[1].list[1].multi_sms_index, 1,
	"SMS multipart index")
equal(messages.result.smslist[1].list[1].multi_sms_max, 2, "SMS multipart maximum")
equal(messages.result.smslist[1].list[1].multi_sms_id, 12, "SMS multipart identity")

local new_messages = data.sms({ action = "read", type = "new" })
equal(new_messages.result.smslist[1].contact, "10010", "new SMS order")
local record = assert(data.sms_record(), "new SMS report")
equal(record.payload.channel, "smsd", "factory report channel")
equal(#record.payload.smslist, 2, "factory flat SMS report list")
equal(record.payload.smslist[1].index, 7, "stable report ordering")
equal(#record.trans_id, 32, "SMS report transaction digest")
equal(data.sms_record().trans_id, record.trans_id, "unchanged report transaction is stable")
equal(data.sms_record_ack("other", record.trans_id), false, "foreign SMS channel acknowledgement")
equal(data.sms_record_ack("2_1", record.trans_id, record.fingerprints, record.present), true, "persist report acknowledgement")
equal(data.sms_record(), nil, "acknowledged snapshot is not reported again")
sms_history = { { id = 2, sender = "10086", timestamp = 1786248000, content = "balance", is_read = false } }
equal(data.sms_record(), nil, "SIM-to-history migration must not duplicate cloud notifications")
sms_history = {}
equal(data.sms({ action = "del", type = "new", ids = "7,8" }).code, 0, "notification read acknowledgement")
equal(sms_deletes, 0, "read acknowledgement never deletes SIM messages")
equal(data.sms({ action = "read", type = "new" }).result.smslist[1].contact, nil, "read notifications removed from new list")
equal(data.sms({ action = "read" }).result.count, 2, "read acknowledgement preserves inbox")
sms_messages[1].content = "new message reusing SIM index"
equal(data.sms({ action = "read", type = "new" }).result.smslist[1].index, 7, "reused SIM index is a new unread message")
sms_delete_fail = true
equal(data.sms({ action = "del", ids = "7,8" }).code, 1, "verify deletion instead of trusting false backend success")
sms_delete_fail = false
local deleted = data.sms({ action = "del", ids = { 7, 8 } })
equal(deleted.code, 0, "SMS delete code")
equal(deleted.smsdel[2].index, 8, "SMS delete result")
equal(data.sms({ action = "del", type = "new", ids = "7,8" }).code, -4,
	"notification acknowledgement must not delete modem SMS")
equal(sms_deletes, 2, "only explicit inbox deletion reaches modem")
sms_messages = {
	{ index = 9, sender = "10086", timestamp = 1786250000, content = "saved" },
	{ index = 10, sender = "10086", timestamp = 1786250000, content = "saved" }
}
sms_history = {
	{ id = 3, sender = "10086", timestamp = 1786250000, content = "saved", is_read = false },
	{ id = 4, sender = "10010", timestamp = 1786250001, content = "history only", is_read = false }
}
local saved = data.sms({ action = "read" })
equal(saved.result.count, 2, "merge SIM and saved history without duplicates")
equal(saved.result.smslist[1].list[1].index, 1000000004, "saved history uses distinct APP IDs")
equal(data.sms({ action = "del", ids = "1000000003" }).code, 0, "delete selected saved and SIM copies")
equal(#sms_messages, 0, "physical copy deleted by resolved SIM index")
equal(#sms_history, 1, "unselected saved message preserved")
equal(data.sms({ action = "del", type = "new", ids = "1000000004" }).code, 0, "saved notification read acknowledgement")
equal(#sms_history, 1, "read acknowledgement preserves saved SMS")
history_delete_fail = true
equal(data.sms({ action = "del", ids = "1000000004" }).code, 1, "unavailable history is not proof of deletion")
history_delete_fail, history_read_fail = false, false
equal(#sms_history, 1, "failed history deletion preserves message")
sms_messages, sms_history = {}, {}
for index = 1, 33 do
	sms_messages[index] = { index = index, sender = "10086", timestamp = 1786259000 + index,
		content = "batch message " .. tostring(index) }
end
local first_batch = assert(data.sms_record())
equal(#first_batch.payload.smslist, 32, "bound each SMS report batch")
equal(data.sms_record_ack("2_1", first_batch.trans_id, first_batch.fingerprints, first_batch.present),
	true, "acknowledge first SMS batch")
local second_batch = assert(data.sms_record())
equal(#second_batch.payload.smslist, 1, "next report contains only remaining unacknowledged messages")
equal(data.sms_record_ack("2_1", second_batch.trans_id, second_batch.fingerprints, second_batch.present),
	true, "acknowledge remaining SMS batch")
equal(data.sms_record(), nil, "all acknowledged batches stop reporting")
local sent = data.sms({ action = "send", phone_num = "10086", msg = "hello" })
equal(sent.code, 0, "SMS send code")
equal(data.sms({ action = "send", phone_num = "bad number", msg = "x" }).code,
	-4, "invalid SMS rejected")

local crypto = require "myos_nradio_app_crypto"
equal(crypto.decrypt_app_data("+2j4zUClduWNHc2LNm8p6w=="),
	'{"code":"1"}', "official plaintext signal probe")

os.remove(lease_file)
os.execute = original_execute
io.open, os.date = original_open, original_date
print("PASS: Ultra APP 3.2.1 LED class/schedule, SMS state/records/read safety, and official probe")
