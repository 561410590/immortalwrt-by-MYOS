-- Host-only contract check: all filesystem/command dependencies are fixtures.
local root = assert(arg[1], "app-assets path required")
if arg[2] ~= "--root" then root = root .. "/root" end
local fixture = {}
local board = "nradio_wt9111_No2-manper_NB68"
local calls = 0
package.preload["luci.util"] = function()
	return {
		exec = function(command)
			assert(command == "/usr/libexec/myos-nbcpe-status status 2>/dev/null", command)
			calls = calls + 1
			return "fixture"
		end
	}
end
package.preload["cjson.safe"] = function()
	return { decode = function(value) assert(value == "fixture"); return fixture end }
end
package.preload["luci.model.uci"] = function()
	local cursor = {}
	function cursor:get() return nil end
	function cursor:get_all() return {} end
	function cursor:load() return true end
	function cursor:unload() return true end
	return { cursor = function() return cursor end }
end
local native_open = io.open
io.open = function(path, mode)
	if path == "/tmp/sysinfo/board_name" then
		return { read = function() return board end, close = function() end }
	end
	-- No device, credential, network or modem files are read by this fixture.
	return nil
end
package.path = root .. "/usr/lib/lua/?.lua;" .. package.path
local nbcpe = require "myos_nbcpe_data"
assert(nbcpe.detect())
board = "HCMT7981-emmc-NB68"
assert(nbcpe.detect())
board = "hcmt7981-emmc"
assert(not nbcpe.detect())
board = "nradio_wt9111_No2-manper_NB68"
fixture = {
	connect_status = "Yes", cache_state = "complete", sim_slot = "0",
	sim_label = "外置SIM卡",
	sources = { { kind = "cell", age = 3, stale = false, entries = {
		{ key = "name", value = "MT5700" }, { key = "revision", value = "fixture" },
		{ key = "RSRP", value = -93 }, { key = "RSRQ", value = -11 },
		{ key = "SINR", value = 22 }, { key = "network_mode", value = "NR5G-SA" },
		{ key = "Band", value = "n41" }, { key = "ICCID", value = "test-only" },
		{ key = "MCC", value = "460" }, { key = "MNC", value = "00" }
	} } },
	interfaces = { { interface = "cpe_4", device = "port5", up = true,
		["ipv4-address"] = { { address = "192.0.2.10", mask = 24 } }, uptime = 9
	} }
}
local shared = require "myos_nradio_data"
assert(shared.is_native_nbcpe() and not shared.is_native_c5800())
assert(shared.app_device_name() == "NBCPE-688")
local summary = shared.summary({ focused = true })
assert(summary.native_nbcpe and summary.cache_only and not summary.stale)
assert(summary.section == "AK68" and summary.network == "cpe_4")
assert(summary.up and summary.dev == "port5" and summary.ip4p == "192.0.2.10/24")
assert(summary.model == "MT5700" and summary.rsrp == "-93")
assert(summary.mode == "NR SA" and summary.band == "41" and summary.isp == "46000")
local item = shared.cpe_item()
assert(item.name == "cpe" and item.simcount == "2")
assert(item.simtype == "0" and item.simno == "1" and item.RSRP == "-93")
assert(#shared.cpe_items() == 1 and #shared.neighbour()[1] == 0)
assert(not shared.sms_supported())
fixture = { sources = {}, interfaces = {}, cache_state = "missing" }
local empty = shared.summary()
assert(empty.model == "" and empty.signal == -999 and not empty.up and empty.stale)
assert(empty.simno == "" and empty.iccid == "")
io.open = native_open
print("APP_CONTRACT_PASS cache_only_calls=" .. calls)
