module("luci.controller.myos_nros", package.seeall)

local fs = require "nixio.fs"
local jsonc = require "luci.jsonc"
local uci = require "luci.model.uci".cursor()

local state_file = "/tmp/myos-nros-cloud-state.json"

function index()
	if not fs.access("/etc/config/myos_nros") then
		return
	end

	local system_page = entry({"admin", "system", "myos_nros"}, cbi("myos_nros"), _("官方 APP 远控设置"), 90)
	system_page.dependent = false
	entry({"admin", "system", "myos_nros", "status"}, call("action_status"), nil, nil, true).leaf = true
end

local function cloud_enabled()
	return tostring(uci:get("myos_nros", "main", "enabled") or "0") == "1"
end

local function remote_web_enabled()
	return tostring(uci:get("myos_nros", "main", "remote_web_enabled") or "1") ~= "0"
end

local function process_running(executable, argument)
	local iter = fs.dir("/proc")
	if not iter then return false end
	for pid in iter do
		if pid:match("^%d+$") then
			local cmdline = fs.readfile("/proc/" .. pid .. "/cmdline")
			if cmdline then
				cmdline = cmdline:gsub("%z", " ")
				if cmdline:find(executable, 1, true) and
				   (not argument or cmdline:find(executable .. " " .. argument, 1, true)) then
					return true
				end
			end
		end
	end
	return false
end

local function rweb_status()
	local iter = fs.dir("/proc")
	if not iter then return false end
	for pid in iter do
		if pid:match("^%d+$") then
			local cmdline = fs.readfile("/proc/" .. pid .. "/cmdline")
			if cmdline then
				cmdline = cmdline:gsub("%z", " ")
				local port = cmdline:match("(%d+):localhost:80")
				if port and cmdline:find("ssh ", 1, true) then
					return true, tonumber(port)
				end
			end
		end
	end
	return false
end

local function normalize_mac(value)
	local mac = tostring(value or ""):gsub("[%s:%-]", ""):upper()
	if not mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") then return "" end
	return mac:gsub("(%x%x)", "%1:"):sub(1, 17)
end

local function valid_identity_mac(value)
	local mac = normalize_mac(value)
	return mac ~= "" and mac ~= "00:00:00:00:00:00" and
		mac ~= "FF:FF:FF:FF:FF:FF" and mac ~= "00:66:88:00:00:00"
end

local function runtime_state()
	local raw = fs.readfile(state_file)
	if not raw then return {}, false end
	local ok, data = pcall(jsonc.parse, raw)
	if not ok or type(data) ~= "table" then return {}, false end
	local updated = tonumber(data.updated or 0) or 0
	local age = os.time() - updated
	if age < 0 then age = 0 end
	return data, updated > 0 and age <= 30, age
end

local function cloud_endpoint()
	local endpoint = tostring(uci:get("mosquitto", "bridge", "address") or "")
	endpoint = endpoint:match("^([^%s]+)") or ""
	if not endpoint:match("^[%w%.:%-]+$") then return "" end
	return endpoint
end

function action_status()
	uci:load("myos_nros")
	uci:load("oem")
	uci:load("mosquitto")

	local enabled = cloud_enabled()
	local mosquitto = process_running("/usr/sbin/mosquitto")
	local command_agent = process_running("/usr/sbin/myos-nros-app-agent", "command")
	local status_agent = process_running("/usr/sbin/myos-nros-app-agent", "status")
	local rweb_active, rweb_port = rweb_status()
	local state, state_fresh, state_age = runtime_state()
	local bridge_online = state_fresh and state.bridge_online == true
	local local_broker_online = state_fresh and state.local_broker_online == true
	local mac = tostring(uci:get("oem", "board", "id") or "")
	local device_code = tostring(uci:get("oem", "board", "device_code") or "")
	local password = tostring(uci:get("mosquitto", "bridge", "remote_password") or "")
	local endpoint = cloud_endpoint()
	local environment = endpoint:match("%.pro%.nradio%.com%.cn$") and "production" or
		(endpoint:match("%.dev%.nradio%.com%.cn$") and "development" or
		(endpoint ~= "" and "custom" or "unconfigured"))
	local identity_ready = valid_identity_mac(mac)
	local device_code_ready = device_code ~= "" and device_code ~= "x11111"
	local credential_ready = password:match("^[0-9A-Fa-f]+$") ~= nil and #password == 32
	local overall

	if not enabled then
		overall = "disabled"
	elseif not identity_ready or endpoint == "" or
	       (environment == "production" and (not device_code_ready or not credential_ready)) then
		overall = "configuration_error"
	elseif not mosquitto or not command_agent or not status_agent then
		overall = "stopped"
	elseif bridge_online then
		overall = "online"
	elseif local_broker_online then
		overall = "connecting"
	else
		overall = "starting"
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json({
		enabled = enabled,
		remote_web_enabled = remote_web_enabled(),
		mosquitto = mosquitto,
		agent = command_agent and status_agent,
		command_agent = command_agent,
		status_agent = status_agent,
		rweb_active = rweb_active,
		rweb_port = rweb_port,
		local_broker_online = local_broker_online,
		bridge_online = bridge_online,
		state_fresh = state_fresh,
		state_age = state_age,
		identity_ready = identity_ready,
		device_code_ready = device_code_ready,
		credential_ready = credential_ready,
		endpoint = endpoint,
		environment = environment,
		overall = overall
	})
end
