local root = assert(arg[1], "source directory is required")
local auth_dir = "/tmp/myos-nradio-app-auth-sessions"
local auth_replay_file = "/tmp/myos-nradio-app-auth-replay"
local auth_replay_lock = auth_replay_file .. ".lock"
local digest = string.rep("a", 32)
local cloud_token = string.rep("f", 32)

local requests = {
	AUTH1 = { device_code = "phone-code", timestamp = "2026-08-13 12:00:00", trans_id = "1", token = digest },
	AUTH2 = { device_code = "phone-code", timestamp = "2026-08-13 12:00:01", trans_id = "2", token = digest },
	AUTH3 = { device_code = "phone-code", timestamp = "2026-08-13 12:00:02", trans_id = "3", token = digest },
	SYNC = { device_code = "phone-code", timestamp = os.date("%Y-%m-%d %H:%M:%S"), trans_id = "sync", token = digest },
	STALE = { device_code = "phone-code", timestamp = "2000-01-01 00:00:00", trans_id = "stale", token = digest },
	BADTIME = { device_code = "phone-code", timestamp = "not-a-time", trans_id = "bad-time", token = digest },
	HEARTBEAT = { device_code = "phone-code", timestamp = "2026-08-13 12:00:03", trans_id = "4" },
	SIGNAL = { device_code = "phone-code", timestamp = "2026-08-13 12:00:04", trans_id = "5" },
	INFO = { device_code = "phone-code", type = "all", trans_id = "info" },
	CPESEL = { device_code = "router-code", mode = { 0 }, cur = { 1 }, timestamp = "2026-08-13 12:00:05",
		trans_id = "cpesel" },
	CPESELNOCODE = { mode = { 0 }, cur = { 1 }, timestamp = "2026-08-13 12:00:05",
		trans_id = "cpesel-no-code" },
	CPESELREAD = { timestamp = "2026-08-13 12:00:05", trans_id = "cpesel-read" },
	CLIENT = { trans_id = "6" },
	CLIENTBLOCK = { device_code = "router-code", client = "AA:BB:CC:DD:EE:FF",
		switch = 0, trans_id = "client-block" },
	CLIENTUNBLOCK = { device_code = "router-code", client = "AA:BB:CC:DD:EE:FF",
		switch = 1, trans_id = "client-unblock" },
	WIFISET = { device_code = "router-code", trans_id = "wifi-set", list = { {
		rule_name = "wlan0", ssid = "APP_AP_NEW", password = "new-password",
		encryption = "psk2", hidden = "0", disabled = "0", channel = "auto"
	} } },
	SMSREAD = { action = "read", trans_id = "7" },
	SMSSEND = { action = "send", phone_num = "10086", msg = "hello",
		device_code = "router-code", trans_id = "8" },
	SMSNOCODE = { action = "send", phone_num = "10086", msg = "hello",
		trans_id = "9" },
	SMSMAC = { action = "send", phone_num = "10086", msg = "hello",
		device_code = "AABBCCDDEEFF", trans_id = "sms-mac" },
	SMSWRONG = { action = "send", phone_num = "10086", msg = "hello",
		device_code = "001122334455", trans_id = "sms-wrong" },
	SMSDEL = { action = "del", ids = "7", trans_id = "sms-del" },
	LEDREAD = { trans_id = "led-read" },
	LEDSET = { device_code = "router-code", led = 1, schedule = 1,
		start_time = "22:30", end_time = "7:0", trans_id = "led-set" },
	LEDNOCODE = { led = 0, trans_id = "led-no-code" },
	LEDBADTIME = { device_code = "router-code", led = 0,
		start_time = "24:00", trans_id = "led-bad-time" },
	RECORDACK = { trans_id = string.rep("b", 32), result = { channel = "smsd" } },
	RECORDBAD = { trans_id = string.rep("c", 32), result = { channel = "smsd" } },
	RECORDCHANNEL = { trans_id = string.rep("b", 32), result = { channel = "other" } }
}

local function json_quote(value)
	return '"' .. tostring(value):gsub("\\", "\\\\"):gsub('"', '\\"'):
		gsub("\n", "\\n"):gsub("\r", "\\r") .. '"'
end

local function is_array(value)
	local count = 0
	for key in pairs(value) do
		if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then return false end
		if key > count then count = key end
	end
	for index = 1, count do if value[index] == nil then return false end end
	return true, count
end

local function json_encode(value)
	local kind = type(value)
	if kind == "nil" then return "null" end
	if kind == "boolean" or kind == "number" then return tostring(value) end
	if kind == "string" then return json_quote(value) end
	assert(kind == "table", "unsupported JSON value")
	local array, count = is_array(value)
	local parts = {}
	if array then
		for index = 1, count do parts[#parts + 1] = json_encode(value[index]) end
		return "[" .. table.concat(parts, ",") .. "]"
	end
	for key, item in pairs(value) do
		parts[#parts + 1] = json_quote(key) .. ":" .. json_encode(item)
	end
	return "{" .. table.concat(parts, ",") .. "}"
end

local function json_decode(value)
	if value == "{}" then return {} end
	if requests[value] then
		local copy = {}
		for key, item in pairs(requests[value]) do copy[key] = item end
		return copy
	end
	local data = tostring(value):match('^%s*{%s*"data"%s*:%s*"([A-Z0-9]+)"%s*}%s*$')
	return data and { data = data } or nil
end

local json = { encode = json_encode, decode = json_decode }
package.preload["cjson.safe"] = function() return json end
local replay_hashes, replay_hash_count = {}, 0
local md5_mock = { sumhexa = function(value)
	value = tostring(value or "")
	if value:match("^device_code") then return digest end
	if not replay_hashes[value] then
		replay_hash_count = replay_hash_count + 1
		replay_hashes[value] = string.format("%032x", replay_hash_count)
	end
	return replay_hashes[value]
end }
package.loaded["md5"] = md5_mock
package.preload["md5"] = function() return md5_mock end
package.preload["myos_nradio_app_crypto"] = function()
	return { decrypt_app_data = function(value) return requests[value] and value or nil end }
end
package.preload["myos_nradio_data"] = function()
	local data = assert(loadfile(root .. "/myos_nradio_data.lua"))()
	local overrides = {
		device_id = function() return "AABBCCDDEEFF" end,
		device_code = function() return "router-code" end,
		summary = function() return { up = true, section = "2_1", dev = "eth2" } end,
		cpe_item = function() return { name = "cpe", signal = -80, rsrp = "-80", up = 1 } end,
		speed = function() return { list = {} } end,
		traffic = function() return { list = {} } end,
		neighbour = function() return { {} } end,
		clients = function()
			return { { client = "AA:BB:CC:DD:EE:FF", mac = "AA:BB:CC:DD:EE:FF" } }, 1
		end,
		sms_supported = function() return true end,
		sms = function(req)
			if req.action == "read" then
				return { code = 0, result = { ready = 1, smslist = { {} } } }
			elseif req.action == "del" then
				return { code = 0, smsdel = { { index = 7, code = 0 } } }
			end
			return { code = 0, item = { { code = 0 } } }
		end,
		invalidate = function() end
	}
	for key, value in pairs(overrides) do data[key] = value end
	return data
end

package.preload["nixio.fs"] = function()
	return { access = function(path, mode)
		return path == "/etc/init.d/ledctrl" and mode == "x"
	end }
end

local uci_state = {
	wireless = {
		{ [".name"] = "ap0", [".type"] = "wifi-iface", mode = "ap",
			device = "radio0", ssid = "APP_AP", key = "app-password",
			encryption = "psk2", hidden = "0", disabled = "0" },
		{ [".name"] = "sta0", [".type"] = "wifi-iface", mode = "sta",
			device = "radio1", ssid = "UPLINK_STA", key = "uplink-password",
			encryption = "psk2", hidden = "0", disabled = "0" }
	},
	access_ctl = {
		{ [".name"] = "client1", [".type"] = "client",
			mac = "AA:BB:CC:DD:EE:FF", switch = "1", hostname = "" }
	},
	firewall = {},
	ledctrl = { { [".name"] = "ledctrl", [".type"] = "service",
		disabled = "0", schedule = "0" } },
	myos_nros = { { [".name"] = "main", [".type"] = "cloud", local_enabled = "1", sms_enabled = "1" } }
}
local uci_counter = 0
local function uci_section(config, name)
	for _, section in ipairs(uci_state[config] or {}) do
		if section[".name"] == name then return section end
	end
end

local cursor = {}
function cursor:get(config, section, option)
	if config == "myos_nros" and section == "main" and option == "local_enabled" then return "1" end
	local item = uci_section(config, section)
	return item and (option and item[option] or item[".type"]) or nil
end
function cursor:get_all(config, section)
	return uci_section(config, section) or {}
end
function cursor:load() return true end
function cursor:unload() return true end
function cursor:foreach(config, section_type, callback)
	for _, section in ipairs(uci_state[config] or {}) do
		if section[".type"] == section_type and callback(section) == false then break end
	end
	return true
end
function cursor:add(config, section_type)
	uci_counter = uci_counter + 1
	local name = "cfg" .. tostring(uci_counter)
	uci_state[config] = uci_state[config] or {}
	uci_state[config][#uci_state[config] + 1] = {
		[".name"] = name,
		[".type"] = section_type
	}
	return name
end
function cursor:set(config, section, option, value)
	local item = uci_section(config, section)
	if not item then return false end
	item[option] = value
	return true
end
function cursor:delete(config, section, option)
	local sections = uci_state[config] or {}
	for index, item in ipairs(sections) do
		if item[".name"] == section then
			if option ~= nil then item[option] = nil else table.remove(sections, index) end
			return true
		end
	end
	return false
end
function cursor:commit() return true end
package.preload["luci.model.uci"] = function()
	return { cursor = function() return cursor end }
end
package.preload["luci.util"] = function()
	return { ubus = function() return {} end, exec = function() return "" end }
end

local http_state = { body = "", output = "", headers = {}, status = nil }
local http = {}
function http.content() return http_state.body end
function http.prepare_content() end
function http.header(name, value) http_state.headers[name] = value end
function http.write(value) http_state.output = http_state.output .. tostring(value or "") end
function http.status(code) http_state.status = code end
package.preload["luci.http"] = function() return http end

local environment = {}
local original_getenv = os.getenv
local original_open = io.open
local original_read = io.read
local original_write = io.write
local original_popen = io.popen
local original_execute = os.execute
local executed_uci_commands = {}
local force_time_synced = false
local led_restarts = 0
os.getenv = function(name) return environment[name] end
os.execute = function(command)
	command = tostring(command or "")
	if command == "/etc/init.d/ledctrl restart >/dev/null 2>&1" then
		led_restarts = led_restarts + 1
		return 0
	end
	if command:match("^uci %-q ") then
		executed_uci_commands[#executed_uci_commands + 1] = command
		return 0
	end
	if command:match("^/etc/init%.d/firewall reload") or
	   command:match("^%(wifi reload%)") then
		return 0
	end
	return original_execute(command)
end
io.popen = function(command, mode)
	if command == "uci -q show wireless 2>/dev/null" then
		local lines = {
			"wireless.ap0='wifi-iface'",
			"wireless.ap0.mode='ap'",
			"wireless.ap0.device='radio0'",
			"wireless.ap0.ssid='APP_AP'",
			"wireless.sta0='wifi-iface'",
			"wireless.sta0.mode='sta'",
			"wireless.sta0.device='radio1'",
			"wireless.sta0.ssid='UPLINK_STA'"
		}
		return {
			lines = function()
				local index = 0
				return function()
					index = index + 1
					return lines[index]
				end
			end,
			close = function() return true end
		}
	end
	return original_popen(command, mode)
end
io.open = function(path, mode)
	if path == "/var/state/dnsmasqsec" and force_time_synced then
		return { close = function() end }
	end
	return original_open(path, mode)
end

local function body(cipher)
	return cipher and ('{"data":"' .. cipher .. '"}') or "{}"
end

local function contains(output, pattern, label)
	assert(output:match(pattern), label .. ": " .. output)
end

local function fast(endpoint, cipher, cookie, access_token)
	local request_body = body(cipher)
	environment = {
		PATH_INFO = "/nradio/app/" .. endpoint,
		CONTENT_LENGTH = tostring(#request_body),
		HTTP_COOKIE = cookie and ("sysauth=" .. cookie) or "",
		HTTP_X_ACCESS_TOKEN = access_token or ""
	}
	local output = {}
	io.read = function() return request_body end
	io.write = function(...)
		for index = 1, select("#", ...) do output[#output + 1] = tostring(select(index, ...)) end
	end
	assert(loadfile(root .. "/myos-nradio-app-fast.lua"))()
	return table.concat(output)
end

local function controller(action, cipher, cookie, access_token)
	local request_body = body(cipher)
	environment = {
		CONTENT_LENGTH = tostring(#request_body),
		HTTP_COOKIE = cookie and ("sysauth=" .. cookie) or "",
		HTTP_X_ACCESS_TOKEN = access_token or ""
	}
	http_state.body = request_body
	http_state.output = ""
	http_state.headers = {}
	http_state.status = nil
	action()
	return http_state.output
end

os.execute("rm -rf " .. auth_dir .. " " .. auth_replay_file .. " " .. auth_replay_lock)

local first = fast("auth", "AUTH1")
local token1 = assert(first:match('"token":"([0-9a-f]+)"'), first)
assert(#token1 == 32, "first session token must be 32 hex characters")
contains(fast("auth", "AUTH1"), '"code":"1"',
	"replayed authentication must fail")
contains(fast("auth", "BADTIME"), '"code":"1"',
	"malformed authentication timestamp must fail")

force_time_synced = true
contains(fast("auth", "SYNC"), '"code":"0"',
	"current synchronized authentication timestamp must work")
contains(fast("auth", "STALE"), '"code":"1"',
	"stale synchronized authentication timestamp must fail")
force_time_synced = false

local second = fast("auth", "AUTH2")
local token2 = assert(second:match('"token":"([0-9a-f]+)"'), second)
assert(#token2 == 32 and token2 ~= token1, "second auth must create another session")

contains(fast("heartbeat", "HEARTBEAT", token1, cloud_token), '"code":"0"',
	"old cookie must survive re-auth and override cloud X-Access-Token")
contains(fast("heartbeat", "HEARTBEAT", token2, cloud_token), '"code":"0"',
	"new cookie must work with cloud X-Access-Token")
contains(fast("heartbeat", "HEARTBEAT", nil, cloud_token), '"code":"1"',
	"cloud X-Access-Token must not authorize local API")
contains(fast("signal", nil, token2, cloud_token), '"data":"%+2j4zUClduWNHc2LNm8p6w=="',
	"plaintext signal must remain protocol probe after re-auth")
contains(fast("signal", "SIGNAL", token1, cloud_token), '"signal":',
	"encrypted signal must accept old local session")
local fast_info = fast("info", "INFO", token1)
contains(fast_info, '"version":"2%.2%.15%.n0%.c0"',
	"fast info must expose current APP protocol version")
contains(fast_info, '"sys_version":"2%.2%.15"',
	"fast info must expose current APP system protocol version")
contains(fast_info, '"type":"0,0"',
	"fast info must expose physical SIM types")
assert(not fast_info:match('"device_code"'),
	"fast info must not overwrite APK session device code")
contains(fast_info, '"ssid":"APP_AP"',
	"fast info must expose AP interfaces")
assert(not fast_info:match("UPLINK_STA"),
	"fast info must exclude non-AP interfaces")
contains(fast("wifi", "WIFISET", token1), '"code":"0"',
	"fast Wi-Fi write must accept latest-APK payload")
local fast_uci_commands = table.concat(executed_uci_commands, "\n")
assert(fast_uci_commands:match("wireless%.ap0%.ssid=APP_AP_NEW"),
	"fast Wi-Fi write must update AP interface")
assert(not fast_uci_commands:match("wireless%.sta0"),
	"fast Wi-Fi write must not modify station interface")
contains(fast("cpesel", "CPESEL", token1), '"code":"0"',
	"fast cpesel must accept latest-APK write with device identity")
contains(fast("cpesel", "CPESELNOCODE", token1), '"code":"1"',
	"fast cpesel write must require device identity")
contains(fast("cpesel", "CPESELREAD", token1), '"code":"0"',
	"fast cpesel read must not require device identity")
contains(fast("client", "CLIENT", token1), '"AA:BB:CC:DD:EE:FF"',
	"fast client route must use shared inventory")
contains(fast("sms", "SMSREAD", token1), '"ready":1',
	"fast SMS read route")
contains(fast("sms", "SMSSEND", token1), '"code":0',
	"fast SMS send with device identity")
contains(fast("sms", "SMSNOCODE", token1), '"code":"1"',
	"fast SMS write requires device identity")
contains(fast("sms", "SMSMAC", token1), '"code":0', "fast SMS accepts APK MAC identity")
contains(fast("sms", "SMSWRONG", token1), '"code":"1"', "fast SMS rejects foreign MAC")
contains(fast("sms", "SMSDEL", token1), '"smsdel":', "fast session-only SMS deletion")
contains(fast("sms", "SMSDEL", nil, cloud_token), '"code":"1"',
	"fast SMS deletion still requires local authentication")
contains(fast("led", "LEDREAD", token1), '"led":1', "fast LED read")
assert(led_restarts == 0, "LED read must not restart service")
contains(fast("led", "LEDSET", nil, cloud_token), '"code":"1"', "fast LED requires local auth")
contains(fast("led", "LEDNOCODE", token1), '"code":2', "fast LED requires write identity")
contains(fast("led", "LEDBADTIME", token1), '"code":2', "fast LED rejects bad time")
assert(led_restarts == 0, "rejected LED writes must not restart service")
contains(fast("led", "LEDSET", token1), '"start_time":"22:30"', "fast LED schedule")
assert(led_restarts == 1, "fast LED write restarts native LED service once")

package.loaded["luci.controller.nradio.app"] = nil
assert(loadfile(root .. "/myos-nradio-app.lua"))()
local app = assert(package.loaded["luci.controller.nradio.app"])
local third = controller(app.action_auth, "AUTH3")
local token3 = assert(third:match('"token":"([0-9a-f]+)"'), third)
assert(#token3 == 32 and token3 ~= token1 and token3 ~= token2,
	"LuCI auth must share multi-session storage")
contains(controller(app.action_heartbeat, "HEARTBEAT", token1, cloud_token), '"code":"0"',
	"LuCI path must ignore cloud X-Access-Token")
contains(controller(app.action_signal, nil, token3, cloud_token),
	'"data":"%+2j4zUClduWNHc2LNm8p6w=="',
	"LuCI plaintext signal must remain protocol probe")
local controller_info = controller(app.action_info, "INFO", token3)
contains(controller_info, '"version":"2%.2%.15%.n0%.c0"', "LuCI local LED protocol version")
contains(controller_info, '"ssid":"APP_AP"',
	"LuCI info must expose AP interfaces")
assert(not controller_info:match("UPLINK_STA"),
	"LuCI info must exclude non-AP interfaces")
contains(controller(app.action_cpesel, "CPESEL", token3), '"code":"0"',
	"LuCI cpesel must accept latest-APK write with device identity")
contains(controller(app.action_cpesel, "CPESELNOCODE", token3), '"code":"1"',
	"LuCI cpesel write must require device identity")
contains(controller(app.action_cpesel, "CPESELREAD", token3), '"code":"0"',
	"LuCI cpesel read must not require device identity")
contains(controller(app.action_client, "CLIENT", token3), '"AA:BB:CC:DD:EE:FF"',
	"LuCI client route must use shared inventory")
contains(controller(app.action_client, "CLIENTBLOCK", token3), '"switch":0',
	"LuCI client block must create and verify firewall rule")
assert(#uci_state.firewall == 1, "LuCI client block firewall rule")
contains(controller(app.action_client, "CLIENTUNBLOCK", token3), '"switch":1',
	"LuCI client unblock must remove and verify firewall rule")
assert(#uci_state.firewall == 0, "LuCI client unblock firewall cleanup")
local wifi_set = controller(app.action_wifi, "WIFISET", token3)
contains(wifi_set, '"ssid":"APP_AP_NEW"',
	"LuCI Wi-Fi write must update AP interface")
assert(not wifi_set:match("UPLINK_STA"),
	"LuCI Wi-Fi response must exclude non-AP interfaces")
assert(uci_section("wireless", "sta0").ssid == "UPLINK_STA",
	"LuCI Wi-Fi write must not modify station interface")
contains(controller(app.action_sms, "SMSREAD", token3), '"ready":1',
	"LuCI SMS read route")
contains(controller(app.action_sms, "SMSNOCODE", token3), '"code":"1"',
	"LuCI SMS write requires device identity")
contains(controller(app.action_sms, "SMSMAC", token3), '"code":0', "LuCI SMS accepts APK MAC identity")
contains(controller(app.action_sms, "SMSWRONG", token3), '"code":"1"', "LuCI SMS rejects foreign MAC")
contains(controller(app.action_sms, "SMSDEL", token3), '"smsdel":', "LuCI session-only SMS deletion")
contains(controller(app.action_sms, "SMSDEL", nil, cloud_token), '"code":"1"',
	"LuCI SMS deletion still requires local authentication")
contains(controller(app.action_led, "LEDREAD", token3), '"schedule":1', "LuCI LED read")
contains(controller(app.action_led, "LEDSET", nil, cloud_token), '"code":"1"', "LuCI LED requires local auth")
contains(controller(app.action_led, "LEDNOCODE", token3), '"code":2', "LuCI LED requires write identity")
contains(controller(app.action_led, "LEDBADTIME", token3), '"code":2', "LuCI LED rejects bad time")
assert(led_restarts == 1, "LuCI LED reads and rejected writes must not restart service")
contains(controller(app.action_led, "LEDSET", token3), '"end_time":"7:0"', "LuCI LED schedule")
assert(led_restarts == 1, "unchanged LED settings must not restart native service")

local patch_file = assert(original_open(root .. "/MYOS-nros-app-patch.sh", "r"))
local patch_source = patch_file:read("*a")
patch_file:close()
local handle_source = assert(patch_source:match(
	"(local function handle%(topic, payload%).-)\nlocal function run%(%)"))
local report_source = assert(patch_source:match(
	"(local function update_sms_record%(now%).-)\nlocal function handle%((.-)"))
local report_locals = assert(patch_source:match("local sms_pending, last_sms_poll = nil, 0"))
local cloud_reply
local sms_reports, sms_acks = {}, 0
local cloud_env = setmetatable({
	ID = "AABBCCDDEEFF", mode = "status", cjson = json,
	uci = cursor, bridge_online = true,
	shared = package.loaded["myos_nradio_data"],
	client = { publish = function(_, topic, payload, qos, retain)
		sms_reports[#sms_reports + 1] = { topic = topic, payload = payload, qos = qos, retain = retain }
	end },
	topic_tail = function(topic) return topic:match("^kp/[^/]+/[^/]+/(.+)$") or topic end,
	reply = function(event, value) cloud_reply = { event = event, value = value } end
}, { __index = _G })
local report_program = report_locals .. "\n" .. report_source .. "\n" .. handle_source ..
	"\nreturn {handle=handle, tick=update_sms_record}"
local cloud_loader
if setfenv and loadstring then
	cloud_loader = assert(loadstring(report_program))
	setfenv(cloud_loader, cloud_env)
else
	cloud_loader = assert(load(report_program, "cloud-app-test", "t", cloud_env))
end
local cloud_api = cloud_loader()
local cloud_handle = cloud_api.handle
cloud_handle("kp/AABBCCDDEEFF/mosca/led", "LEDREAD")
assert(cloud_reply.event == "led" and cloud_reply.value.led == 1,
	"cloud LED reply must use factory top-level fields")
assert(cloud_reply.value.trans_id == "led-read", "cloud LED request correlation")
cloud_handle("kp/AABBCCDDEEFF/mosca/led", "LEDNOCODE")
assert(cloud_reply.value.led == 0 and led_restarts == 2,
	"device-topic cloud LED must accept factory payload without local device code")
cloud_reply = nil
cloud_handle("kp/FFFFFFFFFFFF/mosca/led", "LEDSET")
cloud_handle("kp/001122334455/mosca/led", "LEDSET")
assert(cloud_reply == nil and led_restarts == 2, "cloud LED must reject broadcast and foreign topics")
cloud_handle("kp/AABBCCDDEEFF/mosca/led", "LEDBADTIME")
assert(cloud_reply.value.code == 2 and led_restarts == 2, "cloud LED rejects invalid schedule")
cloud_env.shared.sms_record = function()
	if sms_acks > 0 then return nil end
	return { section = "2_1", trans_id = string.rep("b", 32),
		payload = { trans_id = string.rep("b", 32), count = 1, total = 255,
			channel = "smsd", smslist = { { index = 7, contact = "10086", content = "hello" } } } }
end
cloud_env.shared.sms_record_ack = function(section, trans_id)
	assert(section == "2_1" and trans_id == string.rep("b", 32), "SMS acknowledgement identity")
	sms_acks = sms_acks + 1
	return true
end
cloud_api.tick(30)
assert(#sms_reports == 1 and sms_reports[1].topic == "kp/mosca/AABBCCDDEEFF/sms_record",
	"SMS report uses factory report topic, not reply/sms")
contains(sms_reports[1].payload, '"timestamp":', "SMS report timestamp")
assert(sms_reports[1].qos == 1 and sms_reports[1].retain == false, "SMS reports must not be retained")
cloud_api.tick(60)
assert(#sms_reports == 1, "SMS retry interval is bounded")
cloud_api.tick(90)
cloud_api.tick(150)
cloud_api.tick(210)
assert(#sms_reports == 3, "SMS record uses at most three attempts per unchanged snapshot")
cloud_handle("kp/FFFFFFFFFFFF/mosca/reply/sms_record", "RECORDACK")
cloud_handle("kp/001122334455/mosca/reply/sms_record", "RECORDACK")
cloud_handle("kp/AABBCCDDEEFF/mosca/reply/sms_record", "RECORDBAD")
cloud_handle("kp/AABBCCDDEEFF/mosca/reply/sms_record", "RECORDCHANNEL")
assert(sms_acks == 0, "reject broadcast, foreign, stale and wrong-channel SMS acknowledgements")
cloud_handle("kp/AABBCCDDEEFF/mosca/reply/sms_record", "RECORDACK")
assert(sms_acks == 1, "accept matching device-topic SMS acknowledgement before generic reply filter")
cloud_api.tick(270)
assert(#sms_reports == 3, "acknowledged SMS record stops retries")
sms_acks = 0
cursor:set("myos_nros", "main", "sms_enabled", "0")
cloud_api.tick(330)
assert(#sms_reports == 3, "disabled APP SMS must not publish records")
cursor:set("myos_nros", "main", "sms_enabled", "1")
cloud_env.bridge_online = false
cloud_api.tick(390)
assert(#sms_reports == 3, "offline bridge must not poll or publish SMS")

os.execute("rm -rf " .. auth_dir .. " " .. auth_replay_file .. " " .. auth_replay_lock)
os.getenv = original_getenv
io.open = original_open
io.read = original_read
io.write = original_write
io.popen = original_popen
os.execute = original_execute
print("PASS: APP 3.2.1 auth, LED, SMS identity, CPESEL, Wi-Fi, client, and signal routes")
