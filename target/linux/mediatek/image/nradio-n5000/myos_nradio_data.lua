local uci = require("luci.model.uci").cursor()
local util = require "luci.util"
local ok_json, json = pcall(require, "cjson.safe")
if not ok_json then json = require "cjson" end
local ok_nixio, nixio = pcall(require, "nixio")
local ok_fs, fs = pcall(require, "nixio.fs")
local ok_md5, md5 = pcall(require, "md5")

local M = {}

local APP_SOFTWARE_VERSION = "2.2.12.n0.c0"
local CACHE_DIR = "/tmp/myos-nradio-data"
local BASE_CACHE_SECONDS = 30
local SIGNAL_CACHE_SECONDS = 3
local FOCUSED_SIGNAL_CACHE_SECONDS = 1
local CARRIER_CACHE_SECONDS = 10
local FAILURE_BACKOFF_SECONDS = 5
local STALE_CACHE_SECONDS = 300
local MAX_CACHE_BYTES = 1024 * 1024
local MAX_CARRIERS = 8
local PLMN_DATA_PATH = "/etc/plmn.json"
local MAX_PLMN_BYTES = 128 * 1024

local function trim(value)
	local result = tostring(value or ""):gsub("^%s+", "")
	result = result:gsub("%s+$", "")
	return result
end

local function shell_quote(value)
	return "'" .. tostring(value or ""):gsub("'", "'\\''") .. "'"
end

local function precise_time()
	if ok_nixio and nixio and type(nixio.gettimeofday) == "function" then
		local ok, seconds, microseconds = pcall(nixio.gettimeofday)
		seconds = ok and tonumber(seconds) or nil
		if seconds then
			return seconds + (tonumber(microseconds) or 0) / 1000000
		end
	end
	return tonumber(os.time()) or 0
end

local function read_file(path, maximum)
	local file = io.open(path, "rb")
	if not file then return nil end
	local value = file:read(maximum or "*a")
	file:close()
	return value
end

local function read1(path)
	local file = io.open(path, "r")
	if not file then return "" end
	local value = trim(file:read("*l") or "")
	file:close()
	return value
end

local function json_decode(value)
	local ok, decoded = pcall(json.decode, value or "")
	return ok and type(decoded) == "table" and decoded or nil
end

local function json_encode(value)
	local ok, encoded = pcall(json.encode, value)
	return ok and type(encoded) == "string" and encoded or nil
end

local function ensure_cache_dir()
	if ok_fs and fs then
		pcall(fs.mkdirr, CACHE_DIR)
		pcall(fs.chmod, CACHE_DIR, "0700")
	else
		os.execute("mkdir -p " .. shell_quote(CACHE_DIR) ..
			" 2>/dev/null && chmod 700 " .. shell_quote(CACHE_DIR) ..
			" 2>/dev/null")
	end
end

local function cache_name(name)
	return tostring(name or ""):gsub("[^A-Za-z0-9_.%-]", "_")
end

local function cache_path(name)
	name = cache_name(name)
	return name ~= "" and (CACHE_DIR .. "/" .. name .. ".json") or nil
end

local function cache_read(name, maximum_age, allow_stale)
	local path = cache_path(name)
	if not path then return nil end
	local raw = read_file(path, MAX_CACHE_BYTES + 1)
	if not raw or #raw == 0 or #raw > MAX_CACHE_BYTES then return nil end
	local container = json_decode(raw)
	if not container or type(container.value) ~= "table" then return nil end
	local updated = tonumber(container.updated) or 0
	local age = precise_time() - updated
	if age < 0 or (not allow_stale and age >= maximum_age) or
	   (allow_stale and age >= maximum_age) then
		return nil
	end
	return container.value
end

local function cache_write(name, value)
	local path = cache_path(name)
	if not path or type(value) ~= "table" then return false end
	ensure_cache_dir()
	local encoded = json_encode({ updated = precise_time(), value = value })
	if not encoded or #encoded > MAX_CACHE_BYTES then return false end
	local pid = ok_nixio and nixio and type(nixio.getpid) == "function" and
		nixio.getpid() or 0
	local temporary = path .. ".tmp." .. tostring(pid) .. "." ..
		tostring(math.floor(precise_time() * 1000000))
	local file = io.open(temporary, "wb")
	if not file then return false end
	file:write(encoded)
	file:close()
	if ok_fs and fs then pcall(fs.chmod, temporary, "0600") end
	local renamed = os.rename(temporary, path)
	if not renamed then os.remove(temporary) end
	return renamed and true or false
end

local function cache_remove(name)
	local path = cache_path(name)
	if path then os.remove(path) end
end

local function lock_path(name)
	return CACHE_DIR .. "/" .. cache_name(name) .. ".lock"
end

local function acquire_lock(name)
	ensure_cache_dir()
	local path = lock_path(name)
	if ok_fs and fs and type(fs.stat) == "function" then
		local stat = fs.stat(path)
		local modified = stat and tonumber(stat.mtime)
		if modified and os.time() - modified > 15 then
			os.execute("rmdir " .. shell_quote(path) .. " 2>/dev/null")
		end
	end
	local result = os.execute("mkdir " .. shell_quote(path) .. " 2>/dev/null")
	return result == true or result == 0
end

local function release_lock(name)
	os.execute("rmdir " .. shell_quote(lock_path(name)) .. " 2>/dev/null")
end

local function sleep_milliseconds(milliseconds)
	if ok_nixio and nixio and type(nixio.nanosleep) == "function" then
		nixio.nanosleep(0, math.floor(milliseconds * 1000000))
	else
		os.execute("sleep 1")
	end
end

local function cached_value(name, maximum_age, producer)
	local current = cache_read(name, maximum_age, false)
	if current then return current end
	local stale = cache_read(name, STALE_CACHE_SECONDS, true)
	if cache_read("failed_" .. name, FAILURE_BACKOFF_SECONDS, false) then
		return stale or {}
	end
	if not acquire_lock(name) then return stale or {} end
	current = cache_read(name, maximum_age, false)
	if current then
		release_lock(name)
		return current
	end
	local ok, value = pcall(producer)
	if ok and type(value) == "table" and next(value) then
		cache_write(name, value)
		cache_remove("failed_" .. name)
		current = value
	else
		cache_write("failed_" .. name, { failed = true })
		current = stale or {}
	end
	release_lock(name)
	return current
end

local function valid_mac(value)
	return tostring(value or ""):match(
		"^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$") ~= nil
end

local function normalize_mac(value)
	local mac = trim(value):gsub(":", ""):upper():gsub("%-MYOS$", "")
	if mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") then
		return mac:gsub("(%x%x)", "%1:"):sub(1, 17)
	end
	return trim(value):upper()
end

local function valid_identity_mac(value)
	local mac = normalize_mac(value)
	return valid_mac(mac) and mac ~= "00:00:00:00:00:00" and
		mac ~= "FF:FF:FF:FF:FF:FF" and mac ~= "00:66:88:00:00:00"
end

local function identity_from_blob(path, key)
	if not tostring(key or ""):match("^[A-Za-z0-9_]+$") then return "" end
	local data = read_file(path, 65536)
	if not data or data == "" then return "" end
	-- Decode the same bdinfo v1/v2 text offsets used by myos-factory-wifi.sh.
	-- Binary MAC bytes are not delimiters for the first text key.
	if data:sub(1, 3) == "\0\0\0" then
		local version = data:byte(4)
		if version == 1 then data = data:sub(385)
		elseif version == 2 then data = data:sub(16) end
	end
	data = data:gsub("%z", "\n"):gsub("\255", "\n")
	local wanted = tostring(key):lower()
	for line in (data .. "\n"):gmatch("([^\r\n]*)[\r\n]+") do
		local name, value = line:match(
			"^%s*([A-Za-z0-9_]+)%s*=%s*(.-)%s*$")
		if name and name:lower() == wanted and trim(value) ~= "" then
			return trim(value)
		end
		local lower = line:lower()
		local start = 1
		while start <= #line do
			local position = lower:find(wanted, start, true)
			if not position then break end
			local before = position > 1 and line:sub(position - 1, position - 1) or ""
			local suffix = line:sub(position + #wanted)
			local _, equals_end = suffix:find("^%s*=%s*")
			if (before == "" or not before:match("[A-Za-z0-9_]")) and equals_end then
				local recovered = trim(suffix:sub(equals_end + 1))
				if recovered ~= "" then return recovered end
			end
			start = position + #wanted
		end
	end
	return ""
end

local function bd_device()
	local file = io.open("/proc/mtd", "r")
	if file then
		for line in file:lines() do
			local number = line:match("^mtd(%d+):")
			local name = line:match('"(.-)"')
			if number and (name == "bdinfo" or name == "oeminfo") then
				file:close()
				return "/dev/mtd" .. number
			end
		end
		file:close()
	end
	local pipe = io.popen("blkid -t PARTLABEL=bdinfo -o device 2>/dev/null")
	if pipe then
		local path = trim(pipe:read("*l") or "")
		pipe:close()
		if path ~= "" then return path end
	end
	pipe = io.popen(
		"[ -e /dev/disk/by-partlabel/bdinfo ] && " ..
		"readlink -f /dev/disk/by-partlabel/bdinfo 2>/dev/null")
	if pipe then
		local path = trim(pipe:read("*l") or "")
		pipe:close()
		if path ~= "" then return path end
	end
	return nil
end

local function factory_mac()
	local paths = { "/dev/disk/by-partlabel/factory" }
	local pipe = io.popen("blkid -t PARTLABEL=factory -o device 2>/dev/null")
	if pipe then
		for path in pipe:lines() do
			path = trim(path)
			if path ~= "" then paths[#paths + 1] = path end
		end
		pipe:close()
	end
	for _, path in ipairs(paths) do
		local file = io.open(path, "rb")
		if file then
			local positioned = file:seek("set", 4)
			local raw = positioned and file:read(6) or nil
			file:close()
			if raw and #raw == 6 then
				local bytes = {}
				for index = 1, 6 do
					bytes[index] = string.format("%02X", raw:byte(index))
				end
				local mac = table.concat(bytes, ":")
				if valid_identity_mac(mac) then return mac end
			end
		end
	end
	return ""
end

function M.identity_get(key)
	for _, path in ipairs({
		"/mnt/app_data/bdinfo", "/etc/bdinfo.bin", "/bdinfo/bdinfo.bin"
	}) do
		local value = identity_from_blob(path, key)
		if value ~= "" then return value end
	end
	local device = bd_device()
	if device then
		local value = identity_from_blob(device, key)
		if value ~= "" then return value end
	end
	for _, path in ipairs({ "/etc/nradio/identity", "/boot/nradio-identity" }) do
		local value = identity_from_blob(path, key)
		if value ~= "" then return value end
	end
	return ""
end

function M.device_id()
	local mac = normalize_mac(uci:get("myos_nros", "main", "manual_mac") or "")
	if not valid_identity_mac(mac) then
		mac = normalize_mac(uci:get("oem", "board", "id") or "")
	end
	if not valid_identity_mac(mac) then mac = normalize_mac(M.identity_get("fac_mac")) end
	if not valid_identity_mac(mac) then mac = normalize_mac(M.identity_get("mac")) end
	if not valid_identity_mac(mac) then mac = factory_mac() end
	if not valid_identity_mac(mac) then
		mac = normalize_mac(uci:get("mosquitto", "bridge", "remote_clientid") or "")
	end
	return valid_identity_mac(mac) and mac:gsub(":", ""):upper() or ""
end

function M.device_code()
	local factory = trim(M.identity_get("device_code"))
	local configured = trim(uci:get("oem", "board", "device_code") or "")
	if factory ~= "" and factory:lower() ~= "x11111" then return factory end
	if configured ~= "" then return configured end
	return factory
end

function M.adopt_device_code(value)
	value = trim(value)
	local current = M.device_code()
	if current ~= "" then return current == value end
	if value == "" or #value > 128 or not value:match("^[%w_.%-]+$") then
		return false
	end
	local ok = pcall(function()
		uci:load("oem")
		if not uci:get("oem", "board") then
			uci:section("oem", "system", "board", {})
		end
		uci:set("oem", "board", "device_code", value)
		uci:commit("oem")
	end)
	return ok and M.device_code() == value
end

local function safe_ubus(object, method, data)
	if type(object) ~= "string" or type(method) ~= "string" or
	   not object:match("^[A-Za-z0-9_.:%-]+$") or
	   not method:match("^[A-Za-z0-9_%-]+$") then
		return {}
	end
	local ok, result = pcall(util.ubus, object, method, data or {})
	return ok and type(result) == "table" and result or {}
end

local function normalized_name(value)
	local result = tostring(value or ""):lower():gsub("[^a-z0-9]+", "_")
	result = result:gsub("^_+", ""):gsub("_+$", "")
	return result
end

local function collect_fields(fields, source)
	if type(source) ~= "table" then return end
	for key, value in pairs(source) do
		if key ~= "modem_info" and type(value) ~= "table" and
		   value ~= nil and tostring(value) ~= "" then
			local name = normalized_name(key)
			if name ~= "" and fields[name] == nil then fields[name] = value end
		end
	end
	for _, item in ipairs(type(source.modem_info) == "table" and
	    source.modem_info or {}) do
		if type(item) == "table" and item.value ~= nil then
			for _, key in ipairs({ item.key, item.full_name, item.name, item.title }) do
				local name = normalized_name(key)
				if name ~= "" and fields[name] == nil and
				   tostring(item.value) ~= "" then
					fields[name] = item.value
				end
			end
		end
	end
end

local function first_field(fields, ...)
	for index = 1, select("#", ...) do
		local value = fields[normalized_name(select(index, ...))]
		if value ~= nil and trim(value) ~= "" and trim(value) ~= "N/A" and
		   trim(value) ~= "nil" then
			return trim(value)
		end
	end
	return ""
end

local function first_ip(status, key)
	local list = type(status[key]) == "table" and status[key] or {}
	if type(list[1]) == "table" and list[1].address then
		local address = tostring(list[1].address)
		return address, address .. (list[1].mask and
			("/" .. tostring(list[1].mask)) or "")
	end
	return "", ""
end

local function interface_stats(device)
	device = trim(device)
	if device == "" or not device:match("^[A-Za-z0-9_.:%-]+$") then
		return 0, 0, 0, 0
	end
	local base = "/sys/class/net/" .. device .. "/statistics/"
	return tonumber(read1(base .. "tx_bytes")) or 0,
		tonumber(read1(base .. "rx_bytes")) or 0,
		tonumber(read1(base .. "tx_packets")) or 0,
		tonumber(read1(base .. "rx_packets")) or 0
end

local previous_cpu_total, previous_cpu_idle

local function cpu_counters()
	local line = read1("/proc/stat")
	if not line:match("^cpu%s") then return nil end
	local values = {}
	for value in line:gmatch("%d+") do values[#values + 1] = tonumber(value) or 0 end
	if #values < 4 then return nil end
	local total = 0
	for index = 1, math.min(#values, 8) do total = total + values[index] end
	return total, values[4] + (values[5] or 0)
end

function M.system_usage()
	local total, idle = cpu_counters()
	if total and (not previous_cpu_total or total <= previous_cpu_total) then
		previous_cpu_total, previous_cpu_idle = total, idle
		sleep_milliseconds(100)
		total, idle = cpu_counters()
	end
	local cpu = 0
	if total and previous_cpu_total then
		local elapsed = total - previous_cpu_total
		local idle_elapsed = idle - previous_cpu_idle
		if elapsed > 0 then
			cpu = math.floor(((elapsed - idle_elapsed) * 100 / elapsed) + 0.5)
			if cpu < 0 then cpu = 0 elseif cpu > 100 then cpu = 100 end
		end
		previous_cpu_total, previous_cpu_idle = total, idle
	end
	local system = safe_ubus("system", "info", {})
	local memory = type(system.memory) == "table" and system.memory or {}
	local memory_total = tonumber(memory.total or 0) or 0
	local memory_available = tonumber(memory.available or memory.free or 0) or 0
	local mem = memory_total > 0 and math.floor(
		((memory_total - memory_available) * 100 / memory_total) + 0.5) or 0
	return { cpu_percent = cpu, mem_percent = mem }
end

local function copy_table(source)
	local result = {}
	for key, value in pairs(type(source) == "table" and source or {}) do
		if type(value) == "table" then
			local nested = {}
			for nested_key, nested_value in pairs(value) do
				nested[nested_key] = nested_value
			end
			result[key] = nested
		else
			result[key] = value
		end
	end
	return result
end

function M.summary()
 local wan = safe_ubus("network.interface.wan", "status", {})
 local wan6 = safe_ubus("network.interface.wan6", "status", {})
 local ip4, ip4p = first_ip(wan, "ipv4-address")
 local ip6, ip6p = first_ip(wan6.up and wan6 or wan, "ipv6-address")
 local dev = wan.l3_device or wan.device or "eth1"
 local tx, rx = interface_stats(dev)
 return { up = wan.up == true, dev = dev, proto = wan.proto or "",
  uptime = tonumber(wan.uptime) or 0, ip4 = ip4, ip4p = ip4p,
  ip6 = ip6, ip6p = ip6p, tx = tx, rx = rx }
end

function M.links()
 local links = {}
 for _, port in ipairs({ {"wan", "eth1"}, {"lan", "eth0"} }) do
  local path = "/sys/class/net/" .. port[2] .. "/"
  links[#links+1] = { name = port[1], device = port[2],
   status = read1(path .. "carrier") == "1" and 0 or 1,
   speed_mbps = tonumber(read1(path .. "speed")) }
 end
 return links
end

function M.runtime()
 local m, usage = M.summary(), M.system_usage()
 return { global = {netlink = m.up and 1 or 0, uptime = m.uptime,
  cpu_percent = usage.cpu_percent, mem_percent = usage.mem_percent},
  link = M.links(), wans = {{name="wan", device=m.dev, proto=m.proto,
  status=m.up and 0 or 1, uptime=m.uptime, upload=m.tx, download=m.rx,
  ipaddrs={{ipaddr=m.ip4p}}}} }
end

function M.speed(name)
	local summary = M.summary()
	local tx, rx, tx_packets, rx_packets = interface_stats(summary.dev)
	return { list = { {
		name = trim(name) ~= "" and trim(name) or "wan",
		upload = tx,
		download = rx,
		tx_packets = tx_packets,
		rx_packets = rx_packets
	} } }
end

function M.traffic()
	local value = M.speed("wan")
	local item = value.list[1]
	item.traffic = (tonumber(item.upload) or 0) + (tonumber(item.download) or 0)
	return value
end

local function wireless_band(frequency)
	local value = tonumber(frequency)
	if not value then return "" end
	return value < 3000 and "2.4G" or (value < 5500 and "5.2G" or "5.8G")
end

local function wireless_interfaces()
	local interfaces, current = {}, nil
	local iw_output = tostring(util.exec("iw dev 2>/dev/null") or "")
	for line in iw_output:gmatch(
	    "[^\r\n]+") do
		local ifname = line:match("^%s*Interface%s+(%S+)")
		if ifname and ifname:match("^[A-Za-z0-9_.:%-]+$") then
			current = { ifname = ifname, stations = {} }
			interfaces[#interfaces + 1] = current
		elseif current then
			local address = line:match("^%s*addr%s+(%S+)")
			local ssid = line:match("^%s*ssid%s+(.+)$")
			local kind = line:match("^%s*type%s+(%S+)")
			local channel, frequency = line:match(
				"^%s*channel%s+(%d+)%s+%((%d+)%s+MHz%)")
			if address and valid_mac(address) then
				current.mac = address:upper()
			elseif ssid then
				current.ssid = trim(ssid)
			elseif kind then
				current.kind = kind:lower()
			elseif frequency then
				current.channel = tonumber(channel) or channel
				current.frequency = tonumber(frequency)
			end
		end
	end
	if #interfaces == 0 then
		local detected = {}
		for line in tostring(util.exec("iwinfo 2>/dev/null") or ""):gmatch(
		    "[^\r\n]+") do
			local ifname, raw_ssid = line:match("^(%S+)%s+ESSID:%s*(.-)%s*$")
			if ifname and ifname:match("^[A-Za-z0-9_.:%-]+$") then
				local ssid = raw_ssid:match('^"(.-)"$') or
					(raw_ssid ~= "unknown" and raw_ssid or "")
				current = {
					ifname = ifname,
					ssid = ssid,
					stations = {},
					source = "iwinfo"
				}
				detected[#detected + 1] = current
			elseif current then
				local address = line:match("^%s*Access Point:%s*(%S+)")
				local kind = line:match("^%s*Mode:%s*(%S+)")
				local channel, frequency = line:match(
					"Channel:%s*(%d+)%s*%(([%d%.]+)%s*GHz%)")
				if address and valid_mac(address) then
					current.mac = address:upper()
				elseif kind then
					current.kind = kind:lower() == "master" and "ap" or kind:lower()
				end
				if frequency then
					current.channel = tonumber(channel) or channel
					current.frequency = math.floor((tonumber(frequency) or 0) * 1000 + 0.5)
				end
			end
		end
		for _, interface in ipairs(detected) do
			if interface.kind == "ap" and interface.ssid ~= "" and
			   interface.mac ~= "00:00:00:00:00:00" then
				interfaces[#interfaces + 1] = interface
			end
		end
	end
	for _, interface in ipairs(interfaces) do
		if interface.kind == "ap" then
			local command
			if interface.source == "iwinfo" then
				command = "iwinfo " .. shell_quote(interface.ifname) ..
					" assoclist 2>/dev/null"
			else
				command = "iw dev " .. shell_quote(interface.ifname) ..
					" station dump 2>/dev/null"
			end
			local station
			for line in tostring(util.exec(command) or ""):gmatch("[^\r\n]+") do
				local mac = line:match("^%s*Station%s+(%S+)")
				local iwinfo_signal
				if interface.source == "iwinfo" then
					mac, iwinfo_signal = line:match(
						"^%s*(%S+)%s+([%-+]?%d+)%s+dBm")
				end
				if mac and valid_mac(mac) then
					station = {
						mac = mac:upper(),
						ifname = interface.ifname,
						ssid = interface.ssid or "",
						channel = interface.channel or "",
						frequency = interface.frequency or "",
						rxbytes = 0,
						txbytes = 0,
						rxrate = 0,
						txrate = 0,
						rssi = tonumber(iwinfo_signal) or 0,
						connected = 0
					}
					interface.stations[#interface.stations + 1] = station
				elseif station then
					local direction, bitrate = line:match(
						"^%s*([RT]X):%s*([%d%.]+)%s+MBit/s")
					if direction == "RX" then
						station.rxrate = math.floor((tonumber(bitrate) or 0) * 1000)
					elseif direction == "TX" then
						station.txrate = math.floor((tonumber(bitrate) or 0) * 1000)
					end
					local key, value = line:match("^%s*([^:]+):%s*(.-)%s*$")
					key = tostring(key or ""):lower()
					if key == "rx bytes" then
						station.rxbytes = tonumber(tostring(value):match("%d+")) or 0
					elseif key == "tx bytes" then
						station.txbytes = tonumber(tostring(value):match("%d+")) or 0
					elseif key == "signal" then
						station.rssi = tonumber(tostring(value):match("[-+]?%d+")) or 0
					elseif key == "connected time" then
						station.connected = tonumber(tostring(value):match("%d+")) or 0
					elseif key == "rx bitrate" then
						station.rxrate = math.floor((tonumber(tostring(value):match(
							"[%d%.]+")) or 0) * 1000)
					elseif key == "tx bitrate" then
						station.txrate = math.floor((tonumber(tostring(value):match(
							"[%d%.]+")) or 0) * 1000)
					end
				end
			end
		end
	end
	return interfaces
end

local function denied_station_macs()
	local result, seen = {}, {}
	local function add(value)
		if type(value) == "table" then
			for _, item in pairs(value) do add(item) end
			return
		end
		for item in tostring(value or ""):gmatch("[^,%s]+") do
			local mac = normalize_mac(item)
			if valid_mac(mac) and not seen[mac] then
				seen[mac] = true
				result[#result + 1] = mac
			end
		end
	end
	pcall(function()
		uci:load("wireless")
		uci:foreach("wireless", "wifi-iface", function(section)
			if tostring(section.macfilter or ""):lower() == "deny" then
				add(section.maclist)
			end
		end)
	end)
	pcall(function()
		uci:load("access_ctl")
		uci:foreach("access_ctl", "client", function(section)
			if tostring(section.switch or "1") == "0" then add(section.mac) end
		end)
	end)
	table.sort(result)
	return result
end

function M.clients()
	local clients = {}
	local own_mac = normalize_mac(read1("/sys/class/net/br-lan/address"))
	local function client_for(value, online)
		local mac = normalize_mac(value)
		if not valid_mac(mac) or mac == "00:00:00:00:00:00" or
		   mac == "FF:FF:FF:FF:FF:FF" or mac == own_mac then
			return nil
		end
		if not clients[mac] then
			clients[mac] = {
				client = mac,
				mac = mac,
				real_mac = mac,
				ip = "",
				ipaddr = "",
				hostname = "",
				name = "",
				type = "wired",
				ifname = "br-lan",
				wire = 1,
				wireless = 0,
				authed = online and 1 or 0,
				expires = 0,
				switch = 1,
				switch_off = 0,
				qos = 1,
				exsit = online and 1 or 0,
				position = 0,
				rssi = 0,
				rxbytes = 0,
				txbytes = 0,
				rxrate = 0,
				txrate = 0,
				upload = 0,
				download = 0,
				traffic = 0,
				assoctime = "0s"
			}
		elseif online then
			clients[mac].exsit = 1
			clients[mac].authed = 1
		end
		return clients[mac]
	end

	local leasefile = "/tmp/dhcp.leases"
	pcall(function()
		leasefile = trim(uci:get("dhcp", "@dnsmasq[0]", "leasefile") or
			leasefile)
	end)
	local file = io.open(leasefile, "r")
	if file then
		for line in file:lines() do
			local expires, mac, ip, hostname =
				line:match("^(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
			local item = client_for(mac, true)
			if item and ip then
				item.ip = ip
				item.ipaddr = ip
				item.hostname = hostname == "*" and "" or hostname
				item.name = item.hostname
				item.expires = tonumber(expires) or 0
			end
		end
		file:close()
	end

	for line in tostring(util.exec(
	    "ip neigh show dev br-lan 2>/dev/null") or ""):gmatch("[^\r\n]+") do
		local ip, mac, state = line:match(
			"^(%S+).-lladdr%s+(%S+)%s+(%S+)%s*$")
		if state ~= "FAILED" and state ~= "INCOMPLETE" then
			local item = client_for(mac, true)
			if item and ip and (not ip:find(":", 1, true) or item.ip == "") then
				item.ip, item.ipaddr = ip, ip
			end
		end
	end

	file = io.open("/proc/net/arp", "r")
	if file then
		for line in file:lines() do
			local ip, _, flags, mac, _, device =
				line:match("^(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
			if device == "br-lan" and flags ~= "0x0" then
				local item = client_for(mac, true)
				if item and ip then item.ip, item.ipaddr = ip, ip end
			end
		end
		file:close()
	end

	local interfaces = wireless_interfaces()
	local wireless_ports = {}
	for _, interface in ipairs(interfaces) do
		local port = tonumber(read1("/sys/class/net/" .. interface.ifname ..
			"/brport/port_no"))
		if port then wireless_ports[port] = interface end
	end
	for line in tostring(util.exec("brctl showmacs br-lan 2>/dev/null") or ""):gmatch("[^\r\n]+") do
		local port, mac, is_local = line:match("^%s*(%d+)%s+(%S+)%s+(%S+)")
		local interface = wireless_ports[tonumber(port)]
		local item = clients[normalize_mac(mac)]
		if item and interface and is_local == "no" then
			item.type, item.wire, item.wireless = "wireless", 0, 1
			item.ifname = interface.ifname
			item.ssid = interface.ssid or ""
			item.channel = interface.channel or ""
			item.frequency = interface.frequency or ""
			item.band = wireless_band(interface.frequency)
		end
	end
	for _, interface in ipairs(interfaces) do
		for _, station in ipairs(interface.stations) do
			-- LAN 设备作为终端；MLD 链路关联仅补充无线信息。
			local item = clients[normalize_mac(station.mac)]
			if item then
				item.type = "wireless"
				item.ifname = station.ifname
				item.wire = 0
				item.wireless = 1
				item.ssid = station.ssid
				item.channel = station.channel
				item.frequency = station.frequency
				item.band = wireless_band(station.frequency)
				item.rssi = station.rssi
				item.rxbytes = station.rxbytes
				item.txbytes = station.txbytes
				item.rxrate = station.rxrate
				item.txrate = station.txrate
				item.download = station.rxrate
				item.upload = station.txrate
				item.traffic = station.rxbytes + station.txbytes
				item.assoctime = tostring(station.connected) .. "s"
			end
		end
	end

	pcall(function()
		uci:load("access_ctl")
		uci:foreach("access_ctl", "client", function(section)
			local item = client_for(section.mac, false)
			if item then
				item.switch = tonumber(section.switch) == 0 and 0 or 1
				item.switch_off = item.switch == 0 and 1 or 0
				local hostname = trim(section.hostname)
				if hostname ~= "" then item.hostname, item.name = hostname, hostname end
			end
		end)
	end)

	local denied = {}
	for _, mac in ipairs(denied_station_macs()) do denied[mac] = true end
	local keys = {}
	for mac, item in pairs(clients) do
		if denied[mac] then item.switch, item.switch_off = 0, 1 end
		keys[#keys + 1] = mac
	end
	table.sort(keys, function(left, right)
		local l, r = clients[left], clients[right]
		if l.exsit ~= r.exsit then return l.exsit > r.exsit end
		return left < right
	end)
	local result = {}
	for _, mac in ipairs(keys) do result[#result + 1] = clients[mac] end
	local count = #result
	if count == 0 then result[1] = {} end
	return result, count
end

function M.station_status()
	local interfaces = wireless_interfaces()
	local list = {}
	for _, interface in ipairs(interfaces) do
		if interface.kind == "ap" then
			local stations = {}
			for _, station in ipairs(interface.stations) do
				stations[#stations + 1] = station.mac
			end
			list[#list + 1] = {
				band = wireless_band(interface.frequency),
				online = #interface.stations,
				servied = 0,
				list = #stations > 0 and stations or { "" },
				mac = interface.mac or ""
			}
		end
	end
	if #list == 0 then list[1] = "" end
	local deny = denied_station_macs()
	if #deny == 0 then deny[1] = "" end
	return { list = list, deny = deny }
end

local LED_ENGINE = "/usr/bin/mwrt-ledsched"

function M.led_supported()
	if not (ok_fs and type(fs.access) == "function") then return false end
	if not fs.access(LED_ENGINE) then return false end
	-- At least one LED whose brightness we can actually drive.
	local names = fs.dir and fs.dir("/sys/class/leds")
	if not names then return false end
	for name in names do
		if name ~= "." and name ~= ".." and
		   fs.access("/sys/class/leds/" .. name .. "/brightness") then
			return true
		end
	end
	return false
end

function M.led_tick()
	if not M.led_supported() then return true end
	if not acquire_lock("app_led") then return false end
	local ok, result = pcall(function()
		-- Lua 5.1's os.execute returns the raw exit status; 0 means applied.
		return os.execute(LED_ENGINE .. " apply >/dev/null 2>&1") == 0
	end)
	release_lock("app_led")
	return ok and result or false
end

-- Current state, expressed the way the APP thinks about it.
--
-- The APP has two independent booleans -- "LEDs off right now" and "use a time
-- window" -- and they have to stay independent: turning the LEDs off and back
-- on must not silently forget the configured window. So they map onto two
-- separate things in mwrt_led rather than onto one tri-state:
--   led=0 (forced dark) <-> main.manual=off, the same override the web UI's
--                           "Turn off now" button sets
--   schedule=1          <-> main.enabled=1 + main.mode=schedule
-- mode=always_off is only reachable from the web UI; it reads back as led=0
-- too, so the two front-ends never disagree about whether the LEDs are dark.
local function led_state(settings)
	local enabled = settings:get("mwrt_led", "main", "enabled") == "1"
	local mode = settings:get("mwrt_led", "main", "mode")
	local manual = settings:get("mwrt_led", "main", "manual")
	local dark = manual == "off" or (enabled and mode == "always_off")
	return dark, enabled and mode ~= "always_off"
end

function M.led(data)
	data = type(data) == "table" and data or {}
	local writing = data.led ~= nil or data.schedule ~= nil or
		data.start_time ~= nil or data.end_time ~= nil
	if not M.led_supported() then
		if writing then return { code = 2, message = "LED control is unavailable" } end
		return { code = 0, result = { led = -1, schedule = 0 } }
	end
	local settings = require("luci.model.uci").cursor()

	-- The APP sends partial updates (only the field the user touched), so a
	-- missing key must leave the corresponding setting alone.
	local values, clear = {}, {}

	if data.led ~= nil then
		local value = tostring(data.led)
		if value ~= "0" and value ~= "1" then
			return { code = 2, message = "Invalid LED setting" }
		end
		if value == "0" then
			values.manual = "off"
		else
			-- Back to whatever the schedule says.
			clear.manual = true
			-- Also lift a web-UI "always off", otherwise asking for the LEDs
			-- back would appear to do nothing.
			if settings:get("mwrt_led", "main", "mode") == "always_off" then
				values.enabled = "0"
			end
		end
	end

	if data.schedule ~= nil then
		local value = tostring(data.schedule)
		if value ~= "0" and value ~= "1" then
			return { code = 2, message = "Invalid LED setting" }
		end
		if value == "1" then
			values.enabled, values.mode = "1", "schedule"
		else
			values.enabled = "0"
		end
	end

	for key, option in pairs({ start_time = "starttime", end_time = "endtime" }) do
		if data[key] ~= nil then
			local hour, minute = tostring(data[key]):match("^(%d%d?):(%d%d?)$")
			hour, minute = tonumber(hour), tonumber(minute)
			if not hour or not minute or hour > 23 or minute > 59 then
				return { code = 2, message = "Invalid LED schedule time" }
			end
			-- Zero-padded so the value also reads correctly in the web UI's
			-- <input type="time">; the engine accepts either form.
			values[option] = string.format("%02d:%02d", hour, minute)
		end
	end

	if writing then
		if not acquire_lock("app_led") then return { code = 2, message = "LED control is busy" } end
		local ok, failure = pcall(function()
			settings = require("luci.model.uci").cursor()
			local changed = false
			for option, value in pairs(values) do
				if settings:get("mwrt_led", "main", option) ~= value then
					if not settings:set("mwrt_led", "main", option, value) then
						settings:unload("mwrt_led")
						return "Cannot update LED settings"
					end
					changed = true
				end
			end
			for option in pairs(clear) do
				if settings:get("mwrt_led", "main", option) then
					settings:delete("mwrt_led", "main", option)
					changed = true
				end
			end
			if changed and not settings:commit("mwrt_led") then
				settings:unload("mwrt_led")
				return "Cannot save LED settings"
			end
		end)
		release_lock("app_led")
		if not ok or failure then
			return { code = 2, message = ok and failure or "Cannot save LED settings" }
		end
		if not M.led_tick() then
			return { code = 2, message = "Cannot apply LED settings" }
		end
		settings = require("luci.model.uci").cursor()
	end

	local now_disabled, now_schedule = led_state(settings)
	return { code = 0, result = {
		led = now_disabled and 0 or 1,
		schedule = now_schedule and 1 or 0,
		start_time = settings:get("mwrt_led", "main", "starttime") or "0:0",
		end_time = settings:get("mwrt_led", "main", "endtime") or "0:0"
	} }
end

function M.invalidate()
 cache_remove("base")
end

function M.app_device_name() return "N5000" end

function M.app_software_version()
	-- Expose the LED feature gate only when its native backend is installed.
	if M.led_supported() then return "2.2.15.n0.c0" end
	return APP_SOFTWARE_VERSION
end

function M.system_version()
	return read1("/etc/openwrt_version")
end

return M
