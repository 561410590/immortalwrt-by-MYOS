-- Host-only: actual NBCPE adapter/shared Lua, fixture cpetools/decoder/UCI/filesystem.
local root = assert(arg[1], "app-assets or assembled root path required")
if arg[2] ~= "--root" then root = root .. "/root" end
package.path = root .. "/usr/lib/lua/?.lua;" .. package.path
local native_open = io.open
local settings, encoded, calls, launches, passed = {}, {}, {}, {}, 0
local files = { ["/tmp/sysinfo/board_name"] = "HCMT7981-emmc-NB68",
	["/tmp/modconf-AK68.conf"] = "MT5700M-CN", ["/tmp/cpe_cell-AK68.file"] = "V100" }
local mailbox = {}
local available = true
local function key(config, section, option) return config .. "." .. section .. "." .. tostring(option) end
local cursor = {}
function cursor:get(config, section, option) return settings[key(config, section, option)] end
function cursor:set(config, section, option, value) settings[key(config, section, value and option or nil)] = value or option; return true end
function cursor:get_all() return {} end
function cursor:load() return true end
function cursor:unload() return true end
function cursor:commit() return true end
package.preload["luci.model.uci"] = function() return { cursor = function() return cursor end } end
package.preload["nixio.fs"] = function() return {
	access = function(path) return available and (path:find("AK68", 1, true) ~= nil or
		path == "/usr/bin/cpetools.sh" or path == "/etc/init.d/cpesel") end,
	mkdirr = function() return true end, chmod = function() return true end, stat = function() return nil end
} end
package.preload["cjson.safe"] = function() return {
	encode = function(value) local text = "json:" .. tostring(#encoded + 1); encoded[#encoded + 1] = value; return text end,
	decode = function(text) if text == "status" then return { sim_slot = "0", sources = {}, interfaces = {} } end
		if text == "sendok" then return { code = 0, index = "12" } end
		return encoded[tonumber(tostring(text):match("^json:(%d+)$"))] end
} end
package.preload["md5"] = function() return { sumhexa = function(text)
	local hash = 0; for index = 1, #text do hash = (hash * 31 + text:byte(index)) % 1000000007 end
	return string.format("%032x", hash)
end } end
local function response(command)
	calls[#calls + 1] = command
	if command == "AT+CMGL=4" then
		local rows = {}
		for _, message in ipairs(mailbox) do
			rows[#rows + 1] = "+CMGL: " .. message.index .. "," .. message.stat .. ",,1\r\n" .. message.pdu
		end
		return table.concat(rows, "\r\n") .. "\r\nOK\r\n"
	elseif command == "AT+CPMS?" then return '+CPMS: "SM",2,50,"SM",2,50,"SM",2,50\r\nOK\r\n'
	elseif command:match("^AT%+CMGD=") then
		local id = tonumber(command:match("=(%d+)$"))
		for index = #mailbox, 1, -1 do if mailbox[index].index == id then table.remove(mailbox, index) end end
	elseif command:match("^AT%+CMGR=") then
		local id = tonumber(command:match("=(%d+)$"))
		for _, message in ipairs(mailbox) do if message.index == id then message.stat = 1 end end
	elseif command ~= "AT+CMGF=0" and not command:match("^AT%+CMGS=") then error("unexpected fixture AT: " .. command) end
	return "\r\nOK\r\n"
end
package.preload["luci.util"] = function() return {
	ubus = function() error("NBCPE must not call qmodem") end,
	exec = function(command)
		if command == "/usr/libexec/myos-nbcpe-status status 2>/dev/null" then return "status" end
		if command == "/bin/busybox timeout 15 /usr/bin/cpetools.sh -i cpe -c iccid 2>/dev/null" then
			calls[#calls + 1] = "official iccid"
			return "89860012345678901234\n"
		end
		if command:find("pdu_decoder-AK68", 1, true) then
			local pdu = command:match("printf '%%s\\n' '(%x+)'")
			for _, message in ipairs(mailbox) do if message.pdu == pdu then return message.decoded end end
			error("unknown PDU fixture")
		end
		local length, pdu = command:match("^/bin/busybox timeout 90 /usr/bin/cpetools.sh %-i cpe %-c sms_send '(%d+)' '(%x+)' 2>/dev/null$")
		if length then
			assert(tonumber(length) == #pdu / 2 - 1)
			calls[#calls + 1] = "sms_send " .. length .. " " .. pdu
			return "sendok"
		end
		assert(command:match("^/bin/busybox timeout %d+ /usr/bin/cpetools.sh %-i cpe %-t 0 %-c "))
		return response(assert(command:match(" %-c '(.*)' 2>/dev/null$")))
	end
} end
io.open = function(path, mode)
	assert(not mode or not mode:find("w", 1, true) or path:find("/tmp/myos-nradio-data/", 1, true), path)
	local text = files[path]; if not text then return nil end
	return { read = function(_, format) return format == "*l" and text:match("[^\n]*") or text end, close = function() end }
end
os.execute = function(command)
	if command:match("^mkdir ") or command:match("^rmdir ") then return 0 end
	assert(command == "/sbin/ifup cpe >/tmp/myos-nbcpe-modem.log 2>&1" or
		command == "/sbin/ifdown cpe >/tmp/myos-nbcpe-modem.log 2>&1" or
		command == "/etc/init.d/cpesel reload >/tmp/myos-nbcpe-modem.log 2>&1", command)
	launches[#launches + 1] = command; return 0
end
os.remove = function() return true end
local function test(name, body) body(); passed = passed + 1; print("PASS " .. name) end
local modem = require "myos_nbcpe_modem"
local shared = require "myos_nradio_data"
local function ui(values)
	for name, value in pairs(values) do cursor:set("modem-AK68", "@ndis[0]", name, value) end
	return modem.apply_ui()
end
test("native capability no I/O", function() assert(shared.sms_supported() and #calls == 0) end)
test("native default SMS disabled no AT", function()
	local result = shared.sms({}); assert(result.code == 0 and result.result.enabled == "0" and #calls == 0)
end)
test("explicit SMS enable official per-slot IMS and WWAN owner", function()
	assert(shared.sms({ action = "modify", enabled = "1" }).code == 0)
	assert(cursor:get("cpecfg", "cpesim1", "force_ims") == "1" and #launches == 1 and #calls == 0)
end)
local function fill()
	mailbox = {
		{ index = 7, stat = 0, pdu = "00", decoded = "From:+8613800000000\nTextlen=5\nDate/Time:10/09/26 01:00:00\nhello\n" },
		{ index = 9, stat = 1, pdu = "11", decoded = "From:10086\nTextlen=3\nDate/Time:10/09/26 01:01:00\nfee\n" }
	}
end
fill()
test("native read indexes and APP grouping", function()
	local result = shared.sms({ action = "read", type = "new" })
	assert(result.code == 0 and result.result.count == 2 and #result.result.smslist == 1)
	assert(result.result.smslist[1].index == 7 and result.result.smslist[1].content == "hello")
end)
test("native read mark preserves mailbox", function()
	assert(shared.sms({ action = "del", type = "new", ids = "7" }).code == 0 and #mailbox == 2)
	assert(mailbox[1].stat == 1)
end)
test("native physical delete and readback", function()
	assert(shared.sms({ action = "del", ids = "9" }).code == 0 and #mailbox == 1 and mailbox[1].index == 7)
end)
test("unknown deletion rejected", function() assert(shared.sms({ action = "del", ids = "123" }).code ~= 0 and #mailbox == 1) end)
test("single SMS official sms_send and China local number", function()
	calls = {}; assert(shared.sms({ action = "send", phone_num = "13800000000", msg = "hello" }).code == 0)
	assert(#calls == 1 and calls[1]:match("^sms_send %d+ 0001000D91"))
end)
test("60 UCS2 characters has no UDH", function()
	calls = {}; assert(modem.send("+8613800000000", string.rep("a", 60)))
	assert(#calls == 1 and calls[1]:find(" 0001", 1, true))
end)
test("61 characters sends two independent native PDUs", function()
	calls = {}; assert(modem.send("+8613800000000", string.rep("中", 61)))
	assert(#calls == 2 and calls[1]:find(" 0041", 1, true) and calls[2]:find(" 0041", 1, true))
end)
test("UTF16 surrogate pair uses official sender", function()
	files["/tmp/cpe_cell-AK68.file"] = "RM520"; calls = {}; assert(modem.send("+1234", "😀"))
	assert(calls[1]:find(" 0001", 1, true) and calls[1]:find("D83DDE00", 1, true))
end)
test("invalid SMS has no AT", function()
	calls = {}; assert(not modem.send("*#", "test") and not modem.send("123", "\255")); assert(#calls == 0)
end)
test("multipart inbox merges and retains both physical indexes", function()
	mailbox = {
		{ index = 3, stat = 0, pdu = "22", decoded = "From:10086\nDate/Time:10/09/26 01:00:00\nReference number: 42\nSMS segment 1 of 2\nfirst\n" },
		{ index = 8, stat = 1, pdu = "33", decoded = "From:10086\nDate/Time:10/09/26 01:00:00\nReference number: 42\nSMS segment 2 of 2\nsecond\n" }
	}
	local raw = assert(modem.read_raw()); assert(#raw.msg == 1 and raw.msg[1].content == "firstsecond" and #raw.msg[1].sim_indices == 2)
	assert(shared.sms({ action = "del", ids = "3" }).code == 0 and #mailbox == 0)
end)
test("SIM official cur1/cur2 mapping and idempotence", function()
	assert(shared.apply_native_cpesel(1, 0) and cursor:get("cpesel", "sim", "cur") == "1")
	assert(shared.apply_native_cpesel(2, 0) and cursor:get("cpesel", "sim", "cur") == "2")
	local count = #launches; assert(shared.apply_native_cpesel(2, 0) and #launches == count)
	assert(not shared.apply_native_cpesel(3, 0) and #launches == count)
end)
test("RM520 uses the same official SIM controller", function()
	files["/tmp/modconf-AK68.conf"] = "RM520N-CN"; assert(shared.apply_native_cpesel(1, 0))
	assert(launches[#launches]:find("/etc/init.d/cpesel reload", 1, true))
end)
test("explicit SMS disable native and APP settings", function()
	files["/tmp/modconf-AK68.conf"] = "MT5700M-CN"
	assert(shared.sms({ action = "modify", enabled = "0" }).code == 0)
	assert(cursor:get("cpecfg", "cpesim1", "force_ims") == "0" and shared.sms({}).result.enabled == "0")
end)
test("RM520 APP SMS gate without nonexistent native switch", function()
	files["/tmp/modconf-AK68.conf"] = "RM520N-CN"
	local count = #launches
	assert(shared.sms({ action = "modify", enabled = "1" }).code == 0 and #launches == count)
	assert(cursor:get("cpecfg", "cpesim1", "force_ims") == "0" and modem.sms_enabled())
end)
test("MT5700 embedded slot APP gate preserves native IMS policy", function()
	files["/tmp/modconf-AK68.conf"] = "MT5700M-CN"
	assert(shared.apply_native_cpesel(2, 0))
	local count = #launches
	assert(shared.sms({ action = "modify", enabled = "1" }).code == 0 and #launches == count)
	assert(cursor:get("cpecfg", "cpesim2", "force_ims") == nil and modem.sms_enabled())
end)
test("native UI MT5700 SIM SA APN roaming and NR lock", function()
	files["/tmp/modconf-AK68.conf"] = "MT5700M-CN"
	cursor:set("cpesel", "sim", "cur", "1")
	assert(ui({ enable = "1", simsel = "1", smode = "2", nrmode = "1", apnconfig = "fixture.apn",
		datarroaming = "1", bandlist_sa = "78", freqlock = "1", earfcn = "640000", cellid = "0", pincode = "" }))
	assert(cursor:get("cpesel", "sim", "cur") == "2")
	assert(cursor:get("cpecfg", "cpesim2", "mode") == "sa_only")
	assert(cursor:get("cpecfg", "cpesim2", "apn") == "fixture.apn" and cursor:get("cpecfg", "cpesim2", "custom_apn") == "1")
	assert(cursor:get("cpecfg", "cpesim2", "roaming") == "1")
	assert(cursor:get("cpecfg", "cpesim2", "custom_earfcn5") == "1" and cursor:get("cpecfg", "cpesim2", "pci5") == "0")
	assert(cursor:get("cpecfg", "cpesim2", "freq") == "nr-78")
	assert(launches[#launches]:find("/sbin/ifup cpe", 1, true))
	assert(launches[#launches - 1]:find("/etc/init.d/cpesel reload", 1, true))
end)
test("native UI automatic APN and frequency lock clear", function()
	assert(ui({ smode = "0", apnconfig = "", bandlist_sa = "0", bandlist_lte = "0", freqlock = "0" }))
	assert(cursor:get("cpecfg", "cpesim2", "mode") == "auto")
	assert(cursor:get("cpecfg", "cpesim2", "custom_apn") == "0" and cursor:get("cpecfg", "cpesim2", "apn") == "")
	assert(cursor:get("cpecfg", "cpesim2", "custom_freq") == "0")
	assert(cursor:get("cpecfg", "cpesim2", "custom_earfcn4") == "0" and cursor:get("cpecfg", "cpesim2", "custom_earfcn5") == "0")
end)
test("native UI PIN is scoped to current real ICCID", function()
	cursor:set("cpecfg", "89860012345678901234", "dail", "protect")
	assert(ui({ pincode = "1234" }))
	assert(cursor:get("cpecfg", "89860012345678901234", "pin") == "1234")
	assert(cursor:get("cpecfg", "89860012345678901234", "dail") == "protect")
	assert(cursor:get("network", "cpe", "pincode") == nil)
end)
test("native UI cannot assign current PIN to another SIM", function()
	local count = #launches
	assert(not ui({ simsel = "0", pincode = "1234" }))
	assert(#launches == count and cursor:get("cpesel", "sim", "cur") == "2")
end)
test("native UI LTE frequency lock accepts PCI zero", function()
	assert(ui({ simsel = "1", pincode = "", smode = "1", bandlist_lte = "1", freqlock = "1", earfcn = "100", cellid = "0" }))
	assert(cursor:get("cpecfg", "cpesim2", "mode") == "lte")
	assert(cursor:get("cpecfg", "cpesim2", "custom_earfcn4") == "1" and cursor:get("cpecfg", "cpesim2", "pci4") == "0")
	assert(cursor:get("cpecfg", "cpesim2", "custom_earfcn5") == "0")
end)
test("native UI repeated unchanged apply avoids reinitialization", function()
	local count = #launches
	assert(modem.apply_ui() and #launches == count)
end)
test("native UI disable uses official WWAN shutdown", function()
	assert(ui({ enable = "0" }))
	assert(cursor:get("network", "cpe", "disabled") == "1")
	assert(launches[#launches]:find("/sbin/ifdown cpe", 1, true))
end)
test("unavailable backend capability", function() available = false; assert(not shared.sms_supported()) end)
io.open = native_open
print("NBCPE_MODEM_FIXTURE_PASS " .. passed .. "/" .. passed .. " real_AT=0")
