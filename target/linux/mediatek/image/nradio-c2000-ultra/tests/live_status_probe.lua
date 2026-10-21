-- Exercise the running status service through its local MQTT broker.
-- Print only response shape and link state, never SIM identity or credentials.
local mosq = require "mosquitto"
local json = require "cjson.safe"
local shared = require "myos_nradio_data"
local id = shared.device_id()
assert(type(id) == "string" and id:match("^%x%x%x%x%x%x%x%x%x%x%x%x$"), "invalid device id")
local nonce = "ultra-status-" .. tostring(os.time())
local client = mosq.new(nonce)
local connected, subscribed_at, sent = false, nil, false
local pending = { cpeinfo = true, runtime = true, info = true }
local failures = {}

local function check_items(items, label)
	assert(type(items) == "table" and #items == 1, label .. ": expected one Ultra modem")
	assert(type(items[1]) == "table", label .. ": missing modem object")
	assert(items[1].cpeno == 1, label .. ": missing modem number")
	print(label .. " items=" .. #items .. " up=" .. tostring(items[1].up) ..
		" interface=" .. tostring(items[1].interface))
end

client.ON_CONNECT = function()
	connected = true
	client:subscribe("kp/mosca/" .. id .. "/reply/+")
	subscribed_at = os.time()
end
client.ON_MESSAGE = function(_, topic, payload)
	local event = topic:match("/reply/([^/]+)$")
	local data = json.decode(payload or "")
	if not pending[event] or type(data) ~= "table" or data.trans_id ~= nonce .. ":" .. event then return end
	pending[event] = nil
	local ok, err = pcall(function()
		if event == "cpeinfo" then
			check_items(data.list, event)
		elseif event == "runtime" then
			check_items(data.result and data.result.cpe, event)
		else
			check_items(data.cpestatus and data.cpestatus.list, "info.cpestatus")
			check_items(data.runtime and data.runtime.result and data.runtime.result.cpe, "info.runtime")
		end
	end)
	if not ok then failures[#failures + 1] = tostring(err) end
end

assert(client:connect("127.0.0.1") ~= false, "local MQTT connect failed")
local deadline = os.time() + 25
while os.time() < deadline and next(pending) do
	client:loop(100, 10)
	if connected and not sent and os.time() > subscribed_at then
		for event in pairs(pending) do
			local request = event == "cpeinfo" and "cpestatus" or event
			client:publish("kp/" .. id .. "/live-check/" .. request,
				assert(json.encode({ trans_id = nonce .. ":" .. event })), 1, false)
		end
		sent = true
	end
end
client:disconnect()
for event in pairs(pending) do failures[#failures + 1] = event .. ": no response before deadline" end
for _, err in ipairs(failures) do io.stderr:write(err .. "\n") end
os.exit(#failures == 0 and 0 or 1)
