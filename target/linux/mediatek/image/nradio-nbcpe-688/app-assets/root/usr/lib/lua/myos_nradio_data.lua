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

local function with_modem_lock(producer)
	for _ = 1, 40 do
		if acquire_lock("modem_io") then
			local ok, value = pcall(producer)
			release_lock("modem_io")
			return ok and value or nil
		end
		sleep_milliseconds(50)
	end
	return nil
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
	if not valid_identity_mac(mac) and not M.is_native_nbcpe() then mac = factory_mac() end
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
	if factory ~= "" then return factory end
	-- Older NBCPE systems have no factory device_code. Their signed APP
	-- authentication accepts the real device MAC; report that same identity.
	return M.device_id()
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

function M.find_qmodem()
	if M.is_native_nbcpe() then
		return "AK68", "cpe", uci:get_all("network", "cpe") or {}
	end
	if M.is_native_c5800() then
		local alias = uci:get("network", "globals", "default_cellular")
		if alias ~= "cpe" and alias ~= "cpe1" then alias = "cpe" end
		return alias, alias, uci:get_all("network", alias) or {}
	end
	pcall(function() uci:load("qmodem") end)
	local section, alias, values
	uci:foreach("qmodem", "modem-device", function(item)
		if not section and tostring(item.state or "enabled") ~= "disabled" then
			section = item[".name"]
			alias = item.alias or section
			values = item
			return false
		end
	end)
	if not section and uci:get("qmodem", "2_1") then
		section = "2_1"
		alias = uci:get("qmodem", section, "alias") or section
		values = uci:get_all("qmodem", section) or {}
	end
	section = tostring(section or "2_1")
	alias = tostring(alias or section)
	if not section:match("^[A-Za-z0-9_%-]+$") then section = "2_1" end
	if not alias:match("^[A-Za-z0-9_%-]+$") then alias = section end
	return section, alias, values or {}
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

local function csv_field(value, wanted)
	local index = 1
	for item in (tostring(value or "") .. ","):gmatch("([^,]*),") do
		if index == wanted then return trim(item) end
		index = index + 1
	end
	return ""
end

local function sim_gpio_prefix(section)
	for _, prefix in ipairs({
		trim(section), trim(uci:get("network", "globals", "default_cellular") or ""),
		"cpe"
	}) do
		if prefix:match("^[A-Za-z0-9_%-]+$") and
		   read1("/sys/class/gpio/" .. prefix .. "-sel0/value") ~= "" then
			return prefix
		end
	end
	return ""
end

local function read_sim_gpio(section)
	local prefix = sim_gpio_prefix(section)
	if prefix == "" then return "" end
	local value = ""
	for index = 0, 7 do
		local bit = read1("/sys/class/gpio/" .. prefix .. "-sel" ..
			tostring(index) .. "/value")
		if bit == "" then break end
		value = bit .. value
	end
	return value
end

local function logical_sim_slot(section, modem_slot)
	local mapping = uci:get("cpesel", "sim", "gval") or ""
	local gpio = read_sim_gpio(section)
	local usim = modem_slot ~= nil and tostring(modem_slot + 1) or ""
	local configured = tonumber(uci:get("cpesel", "sim", "cur") or "")
	local gpio_candidate, gpio_count, slot_candidate, slot_count, configured_match
	local index = 1
	for entry in (mapping .. ","):gmatch("([^,]*),") do
		local entry_gpio, entry_usim = trim(entry):match("^([^%-]*)%-(%d+)$")
		if entry_gpio and entry_usim and gpio ~= "" and usim ~= "" and
		   entry_gpio == gpio and entry_usim == usim then
			return index
		end
		if entry_gpio and gpio ~= "" and entry_gpio == gpio then
			gpio_candidate, gpio_count = index, (gpio_count or 0) + 1
			if index == configured then configured_match = index end
		end
		if entry_usim and usim ~= "" and entry_usim == usim then
			slot_candidate, slot_count = index, (slot_count or 0) + 1
		end
		index = index + 1
	end
	if configured_match then return configured_match end
	if gpio_count == 1 then return gpio_candidate end
	if slot_count == 1 then return slot_candidate end
	return configured or (modem_slot ~= nil and modem_slot + 1) or 1
end

local function digits_only(value)
	return tostring(value or ""):gsub("[^0-9]", "")
end

local OPERATOR_NAMES = {
	["46000"] = "China Mobile", ["46002"] = "China Mobile",
	["46004"] = "China Mobile", ["46007"] = "China Mobile",
	["46008"] = "China Mobile", ["46013"] = "China Mobile",
	["46020"] = "China Mobile", ["46021"] = "China Mobile",
	["46022"] = "China Mobile", ["46023"] = "China Mobile",
	["46024"] = "China Mobile", ["46025"] = "China Mobile",
	["46026"] = "China Mobile", ["46027"] = "China Mobile",
	["46028"] = "China Mobile", ["46029"] = "China Mobile",
	["46001"] = "China Unicom", ["46006"] = "China Unicom",
	["46009"] = "China Unicom", ["46010"] = "China Unicom",
	["46030"] = "China Unicom",
	["46003"] = "China Telecom", ["46005"] = "China Telecom",
	["46011"] = "China Telecom", ["46012"] = "China Telecom",
	["46015"] = "China Broadnet"
}

local plmn_index

local function build_plmn_index(groups)
	local index = {}
	for _, group in ipairs(type(groups) == "table" and groups or {}) do
		local company = trim(group.company)
		if company ~= "" and type(group.plmn) == "table" then
			for _, value in ipairs(group.plmn) do
				local plmn = digits_only(value)
				if (#plmn == 5 or #plmn == 6) and index[plmn] == nil then
					index[plmn] = company
				end
			end
		end
	end
	return index
end

local function load_plmn_index()
	if plmn_index ~= nil then return plmn_index end
	local raw = read_file(PLMN_DATA_PATH, MAX_PLMN_BYTES + 1)
	local groups = raw and #raw <= MAX_PLMN_BYTES and json_decode(raw) or nil
	plmn_index = build_plmn_index(groups)
	return plmn_index
end

function M.operator_name(plmn)
	plmn = digits_only(plmn)
	return load_plmn_index()[plmn] or OPERATOR_NAMES[plmn] or ""
end

function M.plmn_equal(first, second)
	first, second = digits_only(first), digits_only(second)
	if first == "" or second == "" then return false end
	if first == second then return true end
	local first_company = M.operator_name(first)
	return first_company ~= "" and first_company == M.operator_name(second)
end

local function operator_plmn(fields, imsi, operator)
	for _, name in ipairs({
		"plmn", "operator_numeric", "network_operator", "mcc_mnc", "mccmnc"
	}) do
		local value = digits_only(fields[name])
		if #value == 5 or #value == 6 then return value end
	end
	local numeric = digits_only(operator)
	if #numeric == 5 or #numeric == 6 then return numeric end
	local subscriber = digits_only(imsi)
	if #subscriber >= 5 then
		local six = subscriber:sub(1, 6)
		local five = subscriber:sub(1, 5)
		if #six == 6 and M.operator_name(six) ~= "" then return six end
		if M.operator_name(five) ~= "" then return five end
		return subscriber:sub(1, subscriber:sub(1, 3) == "460" and 5 or
			math.min(6, #subscriber))
	end
	return ""
end

local function app_network_mode(value)
	local mode = trim(value)
	local upper = mode:upper()
	if upper:find("EN%-DC") or upper:find("LTE%-NR") or
	   upper:find("NR5G%-NSA") or
	   upper:find("NR%-NSA") or upper == "NR NSA" then
		return "NR NSA"
	elseif upper:find("NR5G%-SA") or upper:find("NR%-SA") or
	       upper:find("NR", 1, true) or upper:find("5G", 1, true) then
		return "NR SA"
	elseif upper:find("LTE", 1, true) or upper:find("4G", 1, true) then
		return "LTE"
	end
	return mode
end

local function modem_online(value)
	value = trim(value):lower()
	return value == "1" or value == "yes" or value == "true" or
		value == "online" or value == "connected" or value == "up"
end

function M.is_native_nbcpe()
	return require("myos_nbcpe_data").detect()
end

function M.is_native_c5800()
	if M.is_native_nbcpe() then return false end
	local board = (read1("/tmp/sysinfo/board_name") .. " " ..
		read1("/tmp/sysinfo/model")):lower()
	return board:find("c5800", 1, true) ~= nil or
		board:find("hcmt7981", 1, true) ~= nil
end

local function native_cache_entry(source, iface, depth)
	if type(source) ~= "table" or depth > 8 then return nil end
	if source.name == iface then return source end
	if type(source[iface]) == "table" then return source[iface] end
	for _, item in pairs(source) do
		local found = native_cache_entry(item, iface, depth + 1)
		if found then return found end
	end
end

local function build_base_summary(options)
	if M.is_native_nbcpe() then return require("myos_nbcpe_data").summary() end
	options = type(options) == "table" and options or {}
	for _, config in ipairs({ "qmodem", "network", "cpesel", "oem" }) do
		pcall(function() uci:load(config) end)
	end
	local section, alias, qconfig = M.find_qmodem()
	local native = M.is_native_c5800()
	if native and (options.network == "cpe" or options.network == "cpe1") then
		section, alias = options.network, options.network
		qconfig = uci:get_all("network", alias) or {}
	end
	local fields = {}
	if native then
		-- NROS already owns both modems. Read its status cache and netifd data;
		-- never start QModem or issue a second set of AT commands.
		local raw = read_file("/tmp/infocd/cache/cpestatus", MAX_CACHE_BYTES + 1)
		local cache = raw and #raw <= MAX_CACHE_BYTES and json_decode(raw) or {}
		collect_fields(fields, native_cache_entry(cache, alias, 0) or {})
		local status = safe_ubus("network.interface." .. alias, "status", {})
		collect_fields(fields, status.data or {})
		fields.network_mode = fields.mode
		fields.cell_id = fields.cell
		fields.sim_status = fields.cpin or fields.simstatus
	else with_modem_lock(function()
		collect_fields(fields, safe_ubus("qmodem", "info", {
			config_section = section
		}))
		if first_field(fields, "network_mode", "mode", "isp", "operator") == "" then
			collect_fields(fields, safe_ubus("qmodem", "network_info", {
				config_section = section
			}))
		end
		if first_field(fields, "imei", "imsi", "sim_status", "cpin") == "" then
			collect_fields(fields, safe_ubus("qmodem", "sim_info", {
				config_section = section
			}))
		end
		if first_field(fields, "rsrp", "rssi", "cell_id", "cellid") == "" then
			collect_fields(fields, safe_ubus("qmodem", "cell_info", {
				config_section = section
			}))
		end
		return true
	end)
	end

	local ipv4 = safe_ubus("network.interface." .. alias, "status", {})
	if native and not (type(ipv4["ipv4-address"]) == "table" and ipv4["ipv4-address"][1]) then
		local dynamic = safe_ubus("network.interface." .. alias .. "_4", "status", {})
		if dynamic.up then ipv4 = dynamic end
	end
	local ipv6 = safe_ubus("network.interface." .. alias .. "v6", "status", {})
	if not ipv6.up then
		ipv6 = safe_ubus("network.interface." .. alias .. "_6", "status", {})
	end
	if not ipv6.up then
		ipv6 = safe_ubus("network.interface." .. alias .. "6", "status", {})
	end
	local ip4, ip4_prefix = first_ip(ipv4, "ipv4-address")
	local ip6, ip6_prefix = first_ip(ipv6, "ipv6-address")
	local device = trim(ipv4.device or ipv6.device or
		qconfig.device or qconfig.ifname or qconfig.network_interface or qconfig.interface or alias)
	local tx, rx, tx_packets, rx_packets = interface_stats(device)
	local imsi = first_field(fields, "imsi",
		"international_mobile_subscriber_identity")
	local iccid = first_field(fields, "iccid")
	local raw_operator = first_field(fields, "isp", "operator", "carrier")
	local plmn = operator_plmn(fields, imsi, raw_operator)
	local operator = M.operator_name(plmn)
	if operator == "" then operator =
		(not raw_operator:match("^%d+$") and raw_operator or "")
	end
	local modem_slot = tonumber(first_field(fields, "sim_slot", "slot"))
	local connect_status = first_field(fields, "connect_status", "online", "status")
	local online = ipv4.up or ipv6.up or modem_online(connect_status)
	local model = first_field(fields, "model", "name")
	if model == "" then model = trim(qconfig.model or qconfig.name or (native and "" or "MT5700M")) end
	local manufacturer = first_field(fields, "manufacturer")
	if manufacturer == "" then manufacturer = trim(qconfig.manufacturer or "") end
	local mode = app_network_mode(first_field(fields, "network_mode", "mode", "rat"))
	local band = first_field(fields, "band", "nr_sa_band", "nr_nsa_band",
		"nr_band", "lte_band"):gsub("^[BbNn]", "")
	local at_port = trim(qconfig.override_at_port or qconfig.at_port or
		first_field(fields, "at_port", "control"))
	local sim_status = first_field(fields, "sim_status", "cpin")
	if native and sim_status == "0" then sim_status = "READY" end
	if sim_status == "" and iccid ~= "" then sim_status = "READY" end

	return {
		section = section,
		network = alias,
		dev = device,
		at_port = native and "" or at_port,
		native_nros = native,
		up = online and true or false,
		uptime = tonumber(ipv4.uptime or ipv6.uptime or 0) or 0,
		ip4 = ip4,
		ip4p = ip4_prefix,
		ip6 = ip6,
		ip6p = ip6_prefix,
		dns4 = ipv4["dns-server"] or {},
		dns6 = ipv6["dns-server"] or {},
		tx = tx,
		rx = rx,
		tx_packets = tx_packets,
		rx_packets = rx_packets,
		mode = mode,
		isp = plmn ~= "" and plmn or raw_operator,
		operator = operator,
		operatorName = operator,
		manufacturer = manufacturer,
		model = model,
		revision = first_field(fields, "revision"),
		imei = first_field(fields, "imei",
			"international_mobile_equipment_identity"),
		imsi = imsi,
		iccid = iccid,
		simno = native and tostring(fields.simno or fields.sim_no or
			uci:get("cpesel", alias == "cpe1" and "sim1" or "sim", "cur") or "1") or
			tostring(logical_sim_slot(section, modem_slot)),
		simstatus = sim_status,
		cell = first_field(fields, "cell_id", "cellid", "cell"),
		lac = first_field(fields, "lac"),
		tac = first_field(fields, "tac"),
		pci = first_field(fields, "physical_cell_id", "pci", "dl_pci"),
		earfcn = first_field(fields, "arfcn", "earfcn", "dl_fcn"),
		band = band,
		rsrp = first_field(fields, "rsrp"),
		rsrq = first_field(fields, "rsrq"),
		sinr = first_field(fields, "sinr", "snr"),
		rssi = first_field(fields, "rssi", "rxlev"),
		dlbw = first_field(fields, "dlbw", "dl_bandwidth", "downlink_bandwidth"),
		ulbw = first_field(fields, "ulbw", "ul_bandwidth", "uplink_bandwidth"),
		cqi = first_field(fields, "cqi", "qci"),
		ambr_dl = first_field(fields, "nr5g_ambr_dl", "ambr_dl"),
		ambr_ul = first_field(fields, "nr5g_ambr_ul", "ambr_ul"),
		temp = first_field(fields, "temperature", "model_temp")
	}
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

local function trim_field(value)
	value = trim(value)
	local result = value:gsub('^"(.*)"$', "%1")
	return result
end

local function split_csv(value)
	local result = {}
	for field in (tostring(value or "") .. ","):gmatch("(.-),") do
		result[#result + 1] = trim_field(field)
	end
	return result
end

local function numeric_field(value)
	value = trim_field(value)
	return value:match("^[+-]?%d+%.?%d*$") and value or ""
end

local function decimal_hex_field(value)
	value = trim_field(value)
	if not value:match("^[0-9A-Fa-f]+$") then return "" end
	local number = 0
	for index = 1, #value do
		local digit = tonumber(value:sub(index, index), 16)
		if not digit then return "" end
		number = number * 16 + digit
	end
	return string.format("%.0f", number)
end

local function inferred_band(mode, channel)
	local value = tonumber(channel)
	local upper = tostring(mode or ""):upper()
	if not value then return "" end
	local ranges
	if upper == "LTE" then
		ranges = {
			{ 0, 599, 1 }, { 1200, 1949, 3 }, { 2400, 2649, 5 },
			{ 3450, 3799, 8 }, { 36200, 36349, 34 },
			{ 37750, 38249, 38 }, { 38250, 38649, 39 },
			{ 38650, 39649, 40 }, { 39650, 41589, 41 }
		}
	elseif upper == "NR" or upper == "NR5G" then
		ranges = {
			{ 422000, 434000, 1 }, { 361000, 376000, 3 },
			{ 173800, 178800, 5 }, { 185000, 192000, 8 },
			{ 151600, 159600, 28 }, { 499200, 537999, 41 },
			{ 620000, 680000, 78 }, { 693334, 733333, 79 }
		}
	else
		return ""
	end
	for _, range in ipairs(ranges) do
		if value >= range[1] and value <= range[2] then
			return tostring(range[3])
		end
	end
	return ""
end

local function normalized_metric(value, threshold)
	local number = tonumber(trim_field(value))
	if not number then return "" end
	if math.abs(number) > threshold then number = number / 8 end
	if number == math.floor(number) then return tostring(math.floor(number)) end
	return (string.format("%.1f", number):gsub("%.0$", ""))
end

local function parse_monsc(response)
	local line = tostring(response or ""):match("%^MONSC:%s*([^\r\n]+)")
	if not line then return nil end
	local fields = split_csv(line)
	local mode = tostring(fields[1] or ""):upper()
	local result = {}
	if mode == "NR" or mode == "NR-5GC" then
		result.mode = "NR SA"
		result.earfcn = numeric_field(fields[4])
		result.cell = decimal_hex_field(fields[6])
		result.pci = decimal_hex_field(fields[7])
		result.tac = numeric_field(fields[8])
		result.rsrp = numeric_field(fields[9])
		result.rsrq = numeric_field(fields[10])
		result.sinr = numeric_field(fields[11])
		result.band = inferred_band("NR", result.earfcn)
	elseif mode == "LTE" or mode == "EMTC" or mode == "NB-IOT" or
	       mode == "LTE-NR" then
		result.mode = mode == "LTE-NR" and "NR NSA" or "LTE"
		result.earfcn = numeric_field(fields[4])
		result.cell = decimal_hex_field(fields[5])
		result.pci = decimal_hex_field(fields[6])
		result.tac = numeric_field(fields[7])
		result.rsrp = numeric_field(fields[8])
		result.rsrq = numeric_field(fields[9])
	else
		return nil
	end
	return (result.rsrp ~= "" or result.rsrq ~= "" or result.sinr ~= "") and
		result or nil
end

local function hfreq_carriers(response, wanted_rat)
	local carriers = {}
	for line in tostring(response or ""):gmatch("%^HFREQINFO:%s*([^\r\n]+)") do
		local fields = split_csv(line)
		if numeric_field(fields[2]) == tostring(wanted_rat) then
			local offset = 3
			while offset + 6 <= #fields and #carriers < MAX_CARRIERS do
				local band = numeric_field(fields[offset])
				local dl_fcn = numeric_field(fields[offset + 1])
				local dlbw = numeric_field(fields[offset + 3])
				if band == "" or dl_fcn == "" or dlbw == "" then break end
				carriers[#carriers + 1] = {
					band = band,
					dl_fcn = dl_fcn,
					dlbw = dlbw,
					ul_fcn = numeric_field(fields[offset + 4]),
					ulbw = numeric_field(fields[offset + 6]),
					dl_mode = tostring(wanted_rat) == "7" and "NR" or "LTE"
				}
				offset = offset + 7
			end
		end
	end
	return carriers
end

local function parse_hfreqinfo(response, mode)
	local nr = hfreq_carriers(response, 7)
	local lte = hfreq_carriers(response, 6)
	local upper = tostring(mode or ""):upper()
	local carriers = {}
	local function append(source)
		for _, carrier in ipairs(source) do
			if #carriers >= MAX_CARRIERS then break end
			carriers[#carriers + 1] = carrier
		end
	end
	if upper:find("NSA", 1, true) or upper:find("EN-DC", 1, true) then
		append(nr)
		append(lte)
	elseif upper:find("NR", 1, true) or upper:find("5G", 1, true) then
		append(nr)
	elseif upper:find("LTE", 1, true) then
		append(lte)
	elseif #nr > 0 then
		append(nr)
	else
		append(lte)
	end
	if #carriers == 0 then return nil end
	local result = { band_count = #carriers }
	for index, carrier in ipairs(carriers) do
		local suffix = index == 1 and "" or tostring(index - 1)
		for _, name in ipairs({ "band", "dl_fcn", "ul_fcn", "dlbw", "ulbw",
			"dl_mode" }) do
			result[name .. suffix] = carrier[name]
		end
		result["DLBW" .. suffix] = carrier.dlbw
		result["ULBW" .. suffix] = carrier.ulbw
	end
	return result
end

local QUERY_COMMANDS = {
	monsc = "AT^MONSC",
	hfreq = "AT^HFREQINFO?",
	monnc = "AT^MONNC",
	iccid = "AT^ICCID?"
}

local function huawei_supported(summary)
	local text = table.concat({
		tostring(summary.model or ""), tostring(summary.manufacturer or "")
	}, " "):lower()
	return text:find("mt5700", 1, true) ~= nil or
		text:find("huawei", 1, true) ~= nil or
		text:find("td tech", 1, true) ~= nil
end

local function query_at(summary, kind)
	local command = QUERY_COMMANDS[kind]
	local section = trim(summary.section)
	local port = trim(summary.at_port)
	if not command or not section:match("^[A-Za-z0-9_%-]+$") or
	   not port:match("^/dev/[A-Za-z0-9_./:%-]+$") then
		return nil
	end
	local reply = with_modem_lock(function()
		return safe_ubus("qmodem", "send_at", {
			config_section = section,
			params = { at = command, port = port, use_ubus_flag = "1" }
		})
	end)
	if type(reply) ~= "table" then return nil end
	local value = type(reply.at_cfg) == "table" and reply.at_cfg or reply
	if tostring(value.status or "") ~= "1" then return nil end
	local response = tostring(value.res or "")
	return response ~= "" and response or nil
end

local function cached_monsc(summary, maximum_age)
	return cached_value("monsc_" .. summary.section, maximum_age, function()
		return parse_monsc(query_at(summary, "monsc")) or {}
	end)
end

local function cached_hfreq(summary, maximum_age)
	local mode = normalized_name(summary.mode)
	return cached_value("hfreq_" .. summary.section .. "_" .. mode,
		maximum_age, function()
		return parse_hfreqinfo(query_at(summary, "hfreq"), summary.mode) or {}
	end)
end

function M.summary(options)
	options = type(options) == "table" and options or {}
	local base = (M.is_native_c5800() or M.is_native_nbcpe()) and build_base_summary(options) or
		cached_value("base", BASE_CACHE_SECONDS, build_base_summary)
	local result = copy_table(base)
	if not next(result) then result = build_base_summary() end
	if not result.native_nros and huawei_supported(result) then
		if tostring(result.iccid or "") == "" then
			local card = cached_value("iccid_" .. result.section, 300, function()
				local response = query_at(result, "iccid") or ""
				local iccid = response:match("%^ICCID:%s*([A-Za-z0-9]+)")
				return iccid and { value = iccid } or {}
			end)
			if card.value then result.iccid = tostring(card.value) end
		end
		local signal = options.focused and cached_monsc(result,
			FOCUSED_SIGNAL_CACHE_SECONDS) or
			(cache_read("monsc_" .. result.section, STALE_CACHE_SECONDS, true) or {})
		for _, key in ipairs({
			"mode", "band", "cell", "earfcn", "pci", "tac", "rsrp", "rsrq", "sinr"
		}) do
			if signal[key] ~= nil and tostring(signal[key]) ~= "" then
				result[key] = signal[key]
			end
		end
		local topology
		if options.focused then
			topology = cached_hfreq(result, CARRIER_CACHE_SECONDS)
		else
			topology = cache_read("hfreq_" .. result.section .. "_" ..
				normalized_name(result.mode), STALE_CACHE_SECONDS, true) or {}
		end
		for key, value in pairs(topology) do result[key] = value end
	end
	result.cellid = result.cell
	result.signal = tonumber(result.rsrp) or tonumber(result.rssi) or -999
	result.signalStrength = tostring(result.signal)
	result.tx, result.rx, result.tx_packets, result.rx_packets =
		interface_stats(result.dev)
	return result
end

-- Optional APP slot mapping; preserve the board's numbering unless configured.
-- The same involution applies to display values and incoming slot changes.
function M.sim_swap(slot)
	local number = tonumber(slot)
	if not number then return nil end
	local settings = require("luci.model.uci").cursor()
	if settings:get("myos_nros", "main", "app_sim_swap") == "1" and
	   (number == 1 or number == 2) then
		return 3 - number
	end
	return number
end

function M.apply_native_cpesel(slot, mode)
	if M.is_native_nbcpe() then
		local result = with_modem_lock(function()
			return require("myos_nbcpe_modem").apply_sim(slot, mode)
		end)
		M.invalidate()
		return result == true
	end
	if not M.is_native_c5800() then return false end
	local settings = require("luci.model.uci").cursor()
	local mapping = settings:get("cpesel", "sim", "gval") or ""
	local maximum = tonumber(settings:get("cpesel", "sim", "max")) or 0
	local count = 0
	for _ in (mapping .. ","):gmatch("([^,]+),") do count = count + 1 end
	if count > 0 then maximum = count end
	if not slot or slot % 1 ~= 0 or slot < 1 or slot > maximum then return false end
	if tostring(slot) == settings:get("cpesel", "sim", "cur") and
	   tostring(mode) == settings:get("cpesel", "sim", "mode") then return true end
	settings:set("cpesel", "sim", "cur", tostring(slot))
	settings:set("cpesel", "sim", "mode", tostring(mode))
	if not settings:commit("cpesel") then return false end
	local result = os.execute("/etc/init.d/cpesel reload >/dev/null 2>&1")
	M.invalidate()
	return result == true or result == 0
end

local function app_sim_slot(logical)
	logical = tonumber(logical)
	if logical == 3 then return "4", "1" end
	if logical == 1 or logical == 2 then return "0", tostring(M.sim_swap(logical)) end
	return "", tostring(logical or "")
end

local function sim_count()
	if M.is_native_nbcpe() then return 2 end
	local mapping = tostring(uci:get("cpesel", "sim", "gval") or "")
	local maximum = uci:get("cpesel", "sim", "max")
	local count = 0
	for _ in (mapping .. ","):gmatch("([^,]+),") do count = count + 1 end
	return count > 0 and count or tonumber(maximum) or 3
end

local function ready_cpin(value)
	value = trim(value):lower()
	return (value == "ready" or value == "0") and "0" or value
end

function M.cpe_item(options)
	local summary = M.summary(options)
	local simtype, simno = app_sim_slot(summary.simno)
	if summary.native_nbcpe then
		simtype = summary.sim_slot == 0 and "0" or (summary.sim_slot == 1 and "4" or "")
		simno = (summary.sim_slot == 0 or summary.sim_slot == 1) and "1" or ""
	end
	local traffic_total = (tonumber(summary.tx) or 0) +
		(tonumber(summary.rx) or 0)
	local model_temp = tonumber(tostring(summary.temp or ""):match(
		"[-+]?%d+%.?%d*")) or 0
	local item = {
		name = summary.native_nbcpe and "cpe" or (summary.native_nros and summary.network or "cpe"),
		cpeno = summary.network == "cpe1" and 2 or 1,
		using = true,
		up = summary.up and 1 or 0,
		online = summary.up and 1 or 0,
		netlink = summary.up and 1 or 0,
		status = summary.up and 0 or -1,
		gateway_if = summary.up and 1 or -1,
		signal = summary.signal,
		signalStrength = summary.signalStrength,
		traffic = tostring((tonumber(summary.tx) or 0) +
			(tonumber(summary.rx) or 0)),
		device_traffic = tostring((tonumber(summary.tx) or 0) +
			(tonumber(summary.rx) or 0)),
		upload = summary.tx,
		download = summary.rx,
		control = summary.at_port or "",
		hwid = summary.model or "",
		real_name = summary.model or "",
		driver = summary.manufacturer or "",
		manufacturer = summary.manufacturer or "",
		mode = summary.mode or "",
		isp = summary.isp or "",
		operator = summary.operator or "",
		operatorName = summary.operator or "",
		sim_isp = summary.operator or "",
		imei = summary.imei or "",
		imsi = summary.imsi or "",
		iccid = summary.iccid or "",
		cell = summary.cell or "",
		cellid = summary.cell or "",
		lac = summary.lac or "",
		tac = summary.tac or "",
		pci = summary.pci or "",
		earfcn = summary.earfcn or "",
		band = summary.band or "",
		dl_pci = summary.pci or "",
		dl_mode = summary.dl_mode or "",
		dl_fcn = summary.dl_fcn or summary.earfcn or "",
		ul_fcn = summary.ul_fcn or "",
		dlbw = summary.dlbw or "",
		ulbw = summary.ulbw or "",
		DLBW = summary.DLBW or summary.dlbw or "",
		ULBW = summary.ULBW or summary.ulbw or "",
		CQI = summary.cqi or "",
		NR5G_AMBR_DL = summary.ambr_dl or "",
		NR5G_AMBR_UL = summary.ambr_ul or "",
		rsrp = summary.rsrp or "",
		RSRP = summary.rsrp or "",
		rsrq = summary.rsrq or "",
		RSRQ = summary.rsrq or "",
		sinr = summary.sinr or "",
		SINR = summary.sinr or "",
		rssi = summary.rssi or "",
		RSSI = summary.rssi or "",
		rscp = "",
		model = summary.model or "",
		revision = summary.revision or "",
		model_temp = model_temp,
		nrcap = tostring(summary.mode or ""):find("NR", 1, true) and "1" or "0",
		CPIN = ready_cpin(summary.simstatus),
		simtype = simtype,
		simno = simno,
		simcount = tostring(sim_count()),
		switchmode = "1",
		simmode = tostring(uci:get("cpesel", "sim", "mode") or "1"),
		flow_overflow = "",
		sim_flow = traffic_total,
		sim_mon = traffic_total,
		odu = summary.native_nros and summary.network == "cpe1" and "1" or "0",
		net_prefer = -1,
		interface = summary.dev or "",
		band_count = tonumber(summary.band_count) or
			(summary.band ~= "" and 1 or 0)
	}
	for carrier = 1, MAX_CARRIERS - 1 do
		local suffix = tostring(carrier)
		for _, prefix in ipairs({
			"band", "dl_fcn", "ul_fcn", "dlbw", "ulbw", "DLBW", "ULBW", "dl_mode"
		}) do
			if summary[prefix .. suffix] ~= nil then
				item[prefix .. suffix] = summary[prefix .. suffix]
			end
		end
	end
	if summary.ip4p ~= "" then
		item.V4 = { ipaddrs = { summary.ip4p }, dns = summary.dns4 }
	end
	if summary.ip6p ~= "" then
		item.V6 = { ipaddrs = { summary.ip6p }, dns = summary.dns6 }
	end
	return item
end

function M.cpe_items(options)
	if not M.is_native_c5800() then return { M.cpe_item(options) } end
	local result = { M.cpe_item({ network = "cpe" }) }
	local config = uci:get_all("network", "cpe1") or {}
	if config.proto == "nrcpe" or config.proto == "odu" or
		(config.proto == "wwan" and config.mode == "odu") then
		result[#result + 1] = M.cpe_item({ network = "cpe1" })
	end
	return result
end

function M.speed(name)
	local summary = M.summary()
	local tx, rx, tx_packets, rx_packets = interface_stats(summary.dev)
	return { list = { {
		name = trim(name) ~= "" and trim(name) or "cpe",
		upload = tx,
		download = rx,
		tx_packets = tx_packets,
		rx_packets = rx_packets
	} } }
end

function M.traffic()
	local value = M.speed("cpe")
	local item = value.list[1]
	item.traffic = (tonumber(item.upload) or 0) + (tonumber(item.download) or 0)
	return value
end

local function parse_neighbor_cells(response)
	local cells = {}
	for body in tostring(response or ""):gmatch("%^MONNC:%s*([^\r\n]+)") do
		local fields = split_csv(body)
		local mode = tostring(fields[1] or ""):upper()
		if mode == "NR5G" or mode == "NR-5GC" then mode = "NR" end
		if mode == "EMTC" or mode == "NB-IOT" then mode = "LTE" end
		if mode == "NR" or mode == "LTE" then
			local earfcn = numeric_field(fields[2])
			local pci = decimal_hex_field(fields[3])
			if earfcn ~= "" and pci ~= "" then
				local cell = {
					MODE = mode,
					EARFCN = earfcn,
					PCI = pci,
					BAND = inferred_band(mode, earfcn),
					RSRP = normalized_metric(fields[4], mode == "NR" and 200 or 1000),
					RSRQ = normalized_metric(fields[5], mode == "NR" and 100 or 1000),
					lockneed = { MODE = "2", EARFCN = "1", PCI = "1" }
				}
				if mode == "NR" then
					cell.SINR = normalized_metric(fields[6], 100)
					cell.lockneed.BAND = "1"
				else
					cell.RXLEV = numeric_field(fields[6])
				end
				cells[#cells + 1] = cell
			end
		end
	end
	return cells
end

local function table_field(value, ...)
	if type(value) ~= "table" then return "" end
	for index = 1, select("#", ...) do
		local field = value[select(index, ...)]
		if field ~= nil and trim(field) ~= "" then return trim(field) end
	end
	return ""
end

local function append_qmodem_cells(cells, source, mode)
	for _, item in ipairs(type(source) == "table" and source or {}) do
		if type(item) == "table" then
			local earfcn = table_field(item, "EARFCN", "ARFCN", "earfcn", "arfcn")
			local pci = table_field(item, "PCI", "pci")
			if earfcn ~= "" and pci ~= "" then
				local band = table_field(item, "BAND", "band", "NR BAND"):
					gsub("^[BbNn]", "")
				cells[#cells + 1] = {
					MODE = mode,
					EARFCN = earfcn,
					PCI = pci,
					BAND = band ~= "" and band or inferred_band(mode, earfcn),
					RSRP = table_field(item, "RSRP", "rsrp", "ss_rsrp", "rxlev"),
					RSRQ = table_field(item, "RSRQ", "rsrq"),
					SINR = table_field(item, "SINR", "sinr", "ss_sinr"),
					RSSI = table_field(item, "RSSI", "rssi"),
					RXLEV = table_field(item, "RXLEV", "rxlev")
				}
			end
		end
	end
end

function M.neighbour(filter)
	if M.is_native_nbcpe() then return require("myos_nbcpe_data").neighbour() end
	filter = tostring(filter or "")
	local summary = M.summary()
	local cells = {}
	if huawei_supported(summary) then
		local cached = cached_value("monnc_" .. summary.section, 30, function()
			local parsed = parse_neighbor_cells(query_at(summary, "monnc"))
			return #parsed > 0 and { cells = parsed } or {}
		end)
		cells = type(cached.cells) == "table" and cached.cells or {}
	end
	if #cells == 0 then
		local raw = with_modem_lock(function()
			return safe_ubus("qmodem", "get_neighborcell", {
				config_section = summary.section
			})
		end) or {}
		local source = raw.neighborcell or raw.neighbour or raw.result or {}
		if type(source.result) == "table" then source = source.result end
		if type(source.neighborcell) == "table" then source = source.neighborcell end
		if type(source.neighbour) == "table" then source = source.neighbour end
		append_qmodem_cells(cells, source.NR, "NR")
		append_qmodem_cells(cells, source.LTE, "LTE")
		append_qmodem_cells(cells, source.WCDMA, "WCDMA")
	end
	local filtered = {}
	for _, cell in ipairs(cells) do
		if (filter ~= "4" and filter ~= "5") or
		   (filter == "4" and cell.MODE == "LTE") or
		   (filter == "5" and cell.MODE == "NR") then
			filtered[#filtered + 1] = cell
		end
	end
	table.sort(filtered, function(left, right)
		return (tonumber(left.RSRP) or -999) > (tonumber(right.RSRP) or -999)
	end)
	return { filtered }
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
	    "ip -4 neigh show dev br-lan 2>/dev/null") or ""):gmatch("[^\r\n]+") do
		local ip, mac, state = line:match(
			"^(%S+).-lladdr%s+(%S+)%s+(%S+)%s*$")
		if state ~= "FAILED" and state ~= "INCOMPLETE" then
			local item = client_for(mac, true)
			if item and ip then item.ip, item.ipaddr = ip, ip end
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

	for _, interface in ipairs(wireless_interfaces()) do
		for _, station in ipairs(interface.stations) do
			local item = client_for(station.mac, true)
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

local function sms_capabilities(value, requested_type)
	local capabilities = type(value) == "table" and value.sms_capabilities or nil
	if type(capabilities) ~= "table" and type(value) == "table" and
	   type(value.result) == "table" then
		capabilities = value.result.sms_capabilities
	end
	capabilities = type(capabilities) == "table" and capabilities or {}
	local kind = tostring(requested_type or capabilities.mem1 or "ME")
	local selected = type(capabilities[kind]) == "table" and capabilities[kind] or
		capabilities.ME or capabilities.SM or {}
	return kind, selected
end

local function sms_raw_messages(value)
	if type(value) ~= "table" then return {} end
	if type(value.msg) == "table" then return value.msg end
	if type(value.received) == "table" then return value.received end
	if type(value.result) == "table" then return sms_raw_messages(value.result) end
	return {}
end

local function sms_timestamp(value)
	local numeric = tonumber(value)
	if numeric then return math.floor(numeric) end
	local text = tostring(value or "")
	local year, month, day, hour, minute, second = text:match(
		"^(%d%d%d%d)[%-%/]?(%d%d)[%-%/]?(%d%d)[ T,](%d%d):(%d%d):(%d%d)")
	if not year then
		year, month, day, hour, minute, second = text:match(
			"^(%d%d)/(%d%d)/(%d%d),(%d%d):(%d%d):(%d%d)")
		if year then year = tostring(2000 + tonumber(year)) end
	end
	if not year then return 0 end
	return os.time({
		year = tonumber(year), month = tonumber(month), day = tonumber(day),
		hour = tonumber(hour), min = tonumber(minute), sec = tonumber(second)
	}) or 0
end

local function normalize_sms_read(value, requested_type)
	local official = type(value) == "table" and value.smslist and value or
		(type(value) == "table" and type(value.result) == "table" and
		 value.result.smslist and value.result or nil)
	if official then
		official.code = tonumber(official.code or 0) or 0
		official.ready = tonumber(official.ready or 1) or 1
		if type(official.smslist) ~= "table" or #official.smslist == 0 then
			official.smslist = { {} }
		end
		return official
	end

	local groups, order, flat, count = {}, {}, {}, 0
	for _, message in pairs(sms_raw_messages(value)) do
		if type(message) == "table" then
			local contact = trim(message.contact or message.sender or
				message.phone_number or message.number)
			if contact ~= "" then
				if not groups[contact] then
					groups[contact] = { contact = contact, undeal = 0, list = {} }
					order[#order + 1] = contact
				end
				local stat = tonumber(message.stat or message.status) or 0
				local item = {
					index = tonumber(message.index) or tostring(message.index or ""),
					multi_sms_index = tonumber(message.multi_sms_index or message.part or message.segment) or 0,
					multi_sms_max = tonumber(message.multi_sms_max or message.total) or 0,
					multi_sms_id = tonumber(message.multi_sms_id or message.original_reference or message.reference) or 0,
					timestamp = sms_timestamp(message.timestamp or message.time or message.date),
					content = tostring(message.content or message.msg or message.message or ""),
					stat = stat,
					role = tonumber(message.role or 0) or 0
				}
				if message.reference ~= nil then item.reference = message.reference end
				if message.total ~= nil then item.total = message.total end
				groups[contact].list[#groups[contact].list + 1] = item
				if stat == 0 then groups[contact].undeal = groups[contact].undeal + 1 end
				local flat_item = {}
				for key, field in pairs(item) do flat_item[key] = field end
				flat_item.contact = contact
				flat[#flat + 1] = flat_item
				count = count + 1
			end
		end
	end
	for _, contact in ipairs(order) do
		table.sort(groups[contact].list, function(left, right)
			return tonumber(left.timestamp or 0) < tonumber(right.timestamp or 0)
		end)
	end
	table.sort(order, function(left, right)
		local l = groups[left].list[#groups[left].list] or {}
		local r = groups[right].list[#groups[right].list] or {}
		return tonumber(l.timestamp or 0) > tonumber(r.timestamp or 0)
	end)
	local smslist = {}
	if tostring(requested_type or "") == "new" then
		table.sort(flat, function(left, right)
			return tonumber(left.timestamp or 0) > tonumber(right.timestamp or 0)
		end)
		for _, item in ipairs(flat) do
			if item.stat == 0 and item.role == 0 then smslist[#smslist + 1] = item end
		end
	else
		for _, contact in ipairs(order) do smslist[#smslist + 1] = groups[contact] end
	end
	if #smslist == 0 then smslist[1] = {} end
	local kind, capabilities = sms_capabilities(value, requested_type)
	return {
		code = 0,
		total = tonumber(capabilities.total) or count,
		count = count,
		used = tonumber(capabilities.used) or count,
		smslist = smslist,
		type = kind,
		ready = 1
	}
end

local function sms_ids(data)
	local raw = data.ids or data.index
	local result = {}
	if type(raw) == "table" then
		for _, id in ipairs(raw) do result[#result + 1] = tostring(id) end
	else
		for id in tostring(raw or ""):gmatch("[^,%s]+") do
			result[#result + 1] = id
		end
	end
	if #result == 0 or #result > 128 then return nil end
	for _, id in ipairs(result) do
		if not id:match("^%d+$") then return nil end
	end
	return result
end

local SMS_SENDAT = "/usr/bin/sendat"
local SMS_DECODER = "/usr/bin/pdu_decoder"
local SMS_SENDER = "/usr/bin/smssend"

local function sms_helpers_available()
	return ok_fs and type(fs.access) == "function" and
		fs.access("/usr/bin/python3") and fs.access(SMS_SENDAT) and fs.access(SMS_DECODER)
end

local function sms_popen(command)
	local pipe = io.popen(command)
	if not pipe then return "" end
	local value = pipe:read("*a") or ""
	pipe:close()
	return value
end

local function sms_at(command, timeout)
	if M.is_native_nbcpe() then return require("myos_nbcpe_modem").at(command, timeout) end
	return sms_popen(SMS_SENDAT .. " 1 " .. shell_quote(command) .. " " ..
		tostring(timeout or 8) .. " 2>/dev/null")
end

local function sms_direct_snapshot(section, maximum_age)
	if M.is_native_nbcpe() then
		local age = tonumber(maximum_age) or 5
		local name = "sms_" .. section
		local source = age > 0 and cache_read(name, age, false) or nil
		if not source then
			source = require("myos_nbcpe_modem").read_raw()
			if source then cache_write(name, source) end
		end
		return source
	end
	if not sms_helpers_available() then return nil end
	local name = "sms_" .. section
	local age = tonumber(maximum_age) or 5
	local source = age > 0 and cache_read(name, age, false) or nil
	if not source then
		source = json_decode(sms_popen(SMS_SENDAT ..
			" 1 at+cmgl=4 20 2>/dev/null | " .. SMS_DECODER .. " --json 2>/dev/null"))
		if type(source) ~= "table" or source.ok ~= true or
		   type(source.messages) ~= "table" then return nil end
		local used, total = sms_at("AT+CPMS?", 5):match('%+CPMS:%s*"[^"]*",(%d+),(%d+)')
		source.used, source.total = tonumber(used), tonumber(total)
		cache_write(name, source)
	end
	local messages = {}
	for _, message in ipairs(source.messages or {}) do
		local indices = type(message.indices) == "table" and message.indices or {}
		if tonumber(indices[1]) then
			messages[#messages + 1] = {
				index = tonumber(indices[1]), sim_indices = indices,
				contact = message.sender, timestamp = sms_timestamp(message.dt),
				content = message.text, stat = tonumber(message.stat) or 0,
				multi_sms_index = 0, multi_sms_max = 0, multi_sms_id = 0,
				merged = true
			}
		end
	end
	return { msg = messages, _myos_direct = true,
		sms_capabilities = { mem1 = "SM", SM = {
			used = source.used or #messages, total = source.total or #messages
		} } }
end

local function qmodem_sms_success(value)
	if type(value) ~= "table" or value.error ~= nil then return false end
	local source = type(value.result) == "table" and value.result or value
	return tostring(source.status or "0") == "1"
end

local function sms_snapshot(section, maximum_age)
	local raw = sms_direct_snapshot(section, maximum_age) or
		(M.is_native_nbcpe() and { error = "AK68 SMS unavailable" } or
		 safe_ubus("qmodem", "get_sms", { config_section = section }))
	local source = type(raw.result) == "table" and raw.result or raw
	if raw.error ~= nil or source.error ~= nil or source.status == "failed" or
	   (type(source.msg) ~= "table" and type(source.received) ~= "table") or
	   not ok_md5 or type(md5.sumhexa) ~= "function" then return nil end
	local read_marks = json_decode(uci:get("myos_nros", "main", "sms_read") or "") or {}
	local messages, fingerprints, refs, merged_fingerprints = {}, {}, {}, {}
	local function add(message, history)
		if type(message) ~= "table" then return end
		local contact = trim(message.contact or message.sender or message.phone_number or message.number)
		local index = tonumber(history and message.id or message.index)
		if contact == "" or not index or index < 0 or index % 1 ~= 0 or index >= 1000000000 then return end
		local item = {}
		for key, value in pairs(message) do item[key] = value end
		item.contact = contact
		item.timestamp = sms_timestamp(message.timestamp or message.time or message.date)
		item.content = tostring(message.content or message.msg or message.message or "")
		item.reference = message.original_reference or message.reference
		local fingerprint = section .. ":" .. md5.sumhexa(table.concat({ contact,
			tostring(item.timestamp), item.content, tostring(item.reference or ""),
			tostring(item.part or item.multi_sms_index or 0) }, "\0"))
		local merged_key = table.concat({ contact, tostring(item.timestamp), item.content }, "\0")
		local reference = fingerprints[fingerprint] or
			(history and merged_fingerprints[merged_key] or nil)
		if not reference then
			reference = { fingerprint = fingerprint, item = item, sim_indices = {}, history_ids = {} }
			fingerprints[fingerprint] = reference
			messages[#messages + 1] = item
		else
			item = reference.item
		end
		if message.merged then merged_fingerprints[merged_key] = reference end
		if history then
			reference.history_ids[index] = true
			reference.history_id = math.min(reference.history_id or index, index)
		else
			local indices = type(message.sim_indices) == "table" and message.sim_indices or { index }
			for _, value in ipairs(indices) do
				local sim_index = tonumber(value)
				if sim_index and sim_index >= 0 and sim_index % 1 == 0 and sim_index < 1000000000 then
					reference.sim_indices[sim_index] = true
					reference.sim_index = math.min(reference.sim_index or sim_index, sim_index)
				end
			end
		end
		if item.index then refs[tostring(item.index)] = nil end
		-- Separate saved-message IDs from physical SIM indexes; never pass these IDs to AT.
		item.index = reference.history_id and (1000000000 + reference.history_id) or reference.sim_index
		item.stat = (read_marks[reference.fingerprint] or read_marks[fingerprint] or message.is_read == true or
			tostring(message.is_read) == "1") and 1 or (tonumber(message.stat or message.status) or 0)
		refs[tostring(item.index)] = reference
	end
	for _, item in pairs(sms_raw_messages(raw)) do add(item, false) end
	-- Read history only. list_sms may auto-delete SIM messages and must not be used here.
	local history = M.is_native_nbcpe() and {} or
		safe_ubus("qmodem_sms", "get_received_history", { config_section = section })
	local history_available = history.error == nil and type(history.received) == "table"
	if history_available then
		for _, item in ipairs(history.received) do add(item, true) end
	end
	raw.msg = messages
	return { raw = raw, refs = refs, marks = read_marks, section = section,
		history_available = history_available, direct = raw._myos_direct == true }
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

-- The caller must authenticate the local session before checking SMS identity.
function M.sms_request_allowed(data)
	if data.action ~= "send" and data.action ~= "del" then return true end
	if data.action == "del" and data.device_code == nil then return true end
	if type(data.device_code) ~= "string" then return false end
	local code = trim(data.device_code)
	if code == "" then return false end
	if code == M.device_code() then return true end
	-- APP 3.2.1 sends basic.mac, not the device code, from both SMS editors.
	local mac = code:gsub(":", ""):upper()
	return mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") ~= nil and mac == M.device_id()
end

function M.sms_supported()
	if M.is_native_nbcpe() then return require("myos_nbcpe_modem").supported() end
	local _, _, config = M.find_qmodem()
	local port = trim(config.sms_at_port or config.override_at_port or config.at_port)
	if port ~= "" then return true end
	if not sms_helpers_available() then return false end
	local cached = read1("/tmp/sendat.atport")
	if cached ~= "" and fs.access(cached) then return true end
	for _, prefix in ipairs({ "/dev/ttyUSB", "/dev/ttyACM" }) do
		for index = 0, 5 do
			if fs.access(prefix .. index) then return true end
		end
	end
	return false
end

function M.sms(data)
	data = type(data) == "table" and data or {}
	local section = M.find_qmodem()
	local sim = tonumber(data.sim)
	if not sim and (data.action == nil or data.action == "modify") then
		sim = M.sim_swap(tonumber(M.summary().simno) or 1)
	end
	return with_modem_lock(function()
		uci:unload("myos_nros")
		uci:load("myos_nros")
		local enabled = uci:get("myos_nros", "main", "sms_enabled") ~= "0"
		if M.is_native_nbcpe() then enabled = enabled and require("myos_nbcpe_modem").sms_enabled() end
		if data.action == "modify" then
			local value = tostring(data.enabled or "")
			if value ~= "0" and value ~= "1" then return { code = 2, message = "Invalid SMS setting" } end
			if value == "1" and not M.sms_supported() then return { code = 3, message = "SMS modem is unavailable" } end
			-- NBCPE uses the native AK68 SMS/IMS switch on explicit APP changes.
			if M.is_native_nbcpe() and not require("myos_nbcpe_modem").set_sms_enabled(value) then
				return { code = 3, message = "Cannot apply AK68 SMS setting" }
			end
			if value == "1" then
				os.remove("/tmp/cache_sms_" .. section)
				cache_remove("sms_" .. section)
			end
			if value == "1" and (not M.sms_supported() or
			   (not M.is_native_nbcpe() and not sms_snapshot(section))) then
				return { code = 3, message = "SMS modem is unavailable" }
			end
			if not uci:set("myos_nros", "main", "sms_enabled", value) or not uci:commit("myos_nros") then
				uci:unload("myos_nros")
				return { code = 2, message = "Cannot save APP SMS setting" }
			end
			return { code = 0, result = { index = tostring(data.index or "1"),
				sim = sim, action = "modify", enabled = value } }
		elseif data.action == nil then
			if enabled and (not M.sms_supported() or not sms_snapshot(section)) then
				return { code = 3, message = "SMS status is unavailable" }
			end
			return { code = 0, result = { index = tostring(data.index or "1"),
				sim = sim, enabled = enabled and "1" or "0" } }
		end
		if not enabled then return { code = 3, message = "APP SMS is disabled" } end
		if not M.sms_supported() then return { code = -4, message = "SMS modem is unavailable" } end
		if data.action == "read" then
			local snapshot = sms_snapshot(section)
			if not snapshot then return { code = -4, message = "SMS read failed" } end
			return { code = 0, result = normalize_sms_read(snapshot.raw, data.type) }
		elseif data.action == "del" then
			if data.type == "new" and trim(data.ids or data.index) == "" then
				return { code = 0, smsdel = {} }
			end
			local ids = sms_ids(data)
			if not ids then return { code = -4, message = "Invalid SMS id" } end
			local snapshot = sms_snapshot(section)
			if not snapshot then return { code = -4, message = "SMS read failed" } end
			for _, id in ipairs(ids) do
				if not snapshot.refs[id] then return { code = -4, message = "Unknown SMS id; refresh first" } end
			end
			if data.type == "new" then
				-- Keep APP read marks and the modem's own read flag in step.
				local present = {}
				for _, ref in pairs(snapshot.refs) do present[ref.fingerprint] = true end
				for key in pairs(snapshot.marks) do
					if key:sub(1, #section + 1) == section .. ":" and not present[key] then snapshot.marks[key] = nil end
				end
				for _, id in ipairs(ids) do snapshot.marks[snapshot.refs[id].fingerprint] = true end
				if snapshot.direct then
					local marked, seen = 0, {}
					for _, id in ipairs(ids) do
						for index in pairs(snapshot.refs[id].sim_indices) do
							if not seen[index] and marked < 32 then
								sms_at(string.format("AT+CMGR=%d", index), 10)
								seen[index], marked = true, marked + 1
							end
						end
					end
					if marked > 0 then cache_remove("sms_" .. section) end
				end
				local encoded = json_encode(snapshot.marks)
				if not encoded or #encoded > MAX_CACHE_BYTES or
				   not uci:set("myos_nros", "main", "sms_read", encoded) or not uci:commit("myos_nros") then
					uci:unload("myos_nros")
					return { code = 2, message = "Cannot save SMS read state" }
				end
				local acknowledged = {}
				for _, id in ipairs(ids) do acknowledged[#acknowledged + 1] = { index = tonumber(id), code = 0 } end
				return { code = 0, smsdel = acknowledged }
			end
			local sim_ids, history_ids = {}, {}
			for _, id in ipairs(ids) do
				local ref = snapshot.refs[id]
				for index in pairs(ref.sim_indices) do sim_ids[#sim_ids + 1] = tostring(index) end
				for index in pairs(ref.history_ids) do history_ids[#history_ids + 1] = index end
			end
			if #sim_ids > 0 then
				if snapshot.direct then
					table.sort(sim_ids, function(left, right) return tonumber(left) > tonumber(right) end)
					local seen = {}
					for _, index in ipairs(sim_ids) do
						if not seen[index] then
							sms_at(string.format("AT+CMGD=%d", tonumber(index)), 10)
							seen[index] = true
						end
					end
				else
					safe_ubus("qmodem", "delete_sms", { config_section = section, index = table.concat(sim_ids, " ") })
				end
				os.remove("/tmp/cache_sms_" .. section)
				cache_remove("sms_" .. section)
			end
			if #history_ids > 0 then
				safe_ubus("qmodem_sms", "delete_sms", { config_section = section, type = "received", ids = history_ids })
			end
			local after = sms_snapshot(section, 0)
			local sim_left, history_left = {}, {}
			for _, ref in pairs(after and after.refs or {}) do
				for index in pairs(ref.sim_indices) do sim_left[index] = true end
				for index in pairs(ref.history_ids) do history_left[index] = true end
			end
			local deleted, code = {}, 0
			for _, id in ipairs(ids) do
				local ref = snapshot.refs[id]
				local success = after and (not next(ref.history_ids) or after.history_available)
				for index in pairs(ref.sim_indices) do if sim_left[index] then success = false end end
				for index in pairs(ref.history_ids) do if history_left[index] then success = false end end
				deleted[#deleted + 1] = { index = tonumber(id), code = success and 0 or 1 }
				if not success then code = 1 end
			end
			return { code = code, smsdel = deleted }
		elseif data.action == "send" then
			local phone = data.phone_num or data.phone_number
			local message = data.msg or data.message_content
			if type(phone) ~= "string" or not phone:match("^%+?[%d*#]+$") or
			   #phone > 32 or type(message) ~= "string" or message == "" or
			   #message > 2048 or message:find("%z") then
				return { code = -4, message = "Invalid SMS" }
			end
			local success
			if M.is_native_nbcpe() then
				success = require("myos_nbcpe_modem").send(phone, message)
			elseif sms_helpers_available() and fs.access(SMS_SENDER) and fs.access("/usr/bin/pdul.lua") then
				local result = json_decode(sms_popen(SMS_SENDER .. " " ..
					shell_quote(phone) .. " " .. shell_quote(message) .. " 2>/dev/null"))
				success = type(result) == "table" and result.ok == true
			else
				local raw = safe_ubus("qmodem", "send_sms", {
					config_section = section,
					params = { phone_number = phone, message_content = message }
				})
				success = qmodem_sms_success(raw)
			end
			return { code = success and 0 or -1,
				item = { { code = success and 0 or -1, index = "", timestamp = os.time() } } }
		end
		return { code = -4, message = "Invalid SMS action" }
	end) or { code = -4, message = "SMS modem is busy or unavailable" }
end

function M.sms_record()
	local section = M.find_qmodem()
	local response = M.sms({ action = "read", type = "new" })
	if tonumber(response.code) ~= 0 then return nil end
	local result, list, fingerprints, present = response.result, {}, {}, {}
	local reported = json_decode(uci:get("myos_nros", "main", "sms_reported") or "") or {}
	for _, item in ipairs(result.smslist or {}) do
		if item.contact and item.index then
			-- A SIM message can move into QModem history without becoming a new notification.
			local fingerprint = section .. ":" .. md5.sumhexa(table.concat({ item.contact,
				tostring(item.timestamp), item.content, tostring(item.multi_sms_id), tostring(item.multi_sms_index) }, "\0"))
			present[fingerprint] = true
			if not reported[fingerprint] and #list < 32 then
				fingerprints[#fingerprints + 1] = fingerprint
				list[#list + 1] = { contact = item.contact, content = item.content, index = item.index,
					multi_sms_max = item.multi_sms_max, multi_sms_id = item.multi_sms_id,
					multi_sms_index = item.multi_sms_index, timestamp = item.timestamp }
			end
		end
	end
	if #list == 0 then return nil end
	table.sort(list, function(a, b) return a.index < b.index end)
	local payload = { count = result.count, total = result.total, channel = "smsd", smslist = list }
	local encoded = json_encode(payload)
	if not encoded or #encoded > MAX_CACHE_BYTES then return nil end
	local trans_id = md5.sumhexa(encoded)
	payload.trans_id = trans_id
	return { section = section, payload = payload, trans_id = trans_id,
		fingerprints = fingerprints, present = present }
end

function M.sms_record_ack(section, trans_id, fingerprints, present)
	if section ~= M.find_qmodem() or type(trans_id) ~= "string" or
	   #trans_id ~= 32 or not trans_id:match("^%x+$") or
	   type(fingerprints) ~= "table" or #fingerprints == 0 or type(present) ~= "table" then return false end
	return with_modem_lock(function()
		uci:unload("myos_nros")
		uci:load("myos_nros")
		local reported = json_decode(uci:get("myos_nros", "main", "sms_reported") or "") or {}
		for key in pairs(reported) do
			if key:sub(1, #section + 1) == section .. ":" and not present[key] then reported[key] = nil end
		end
		for _, fingerprint in ipairs(fingerprints) do
			if type(fingerprint) ~= "string" or fingerprint:sub(1, #section + 1) ~= section .. ":" or
			   #fingerprint ~= #section + 33 or not fingerprint:sub(#section + 2):match("^%x+$") then return false end
			reported[fingerprint] = true
		end
		local encoded = json_encode(reported)
		local saved = encoded and #encoded <= MAX_CACHE_BYTES and
			uci:set("myos_nros", "main", "sms_reported", encoded) and
			uci:set("myos_nros", "main", "sms_record", section .. ":" .. trans_id) and uci:commit("myos_nros")
		if not saved then uci:unload("myos_nros") end
		return saved
	end) and true or false
end

function M.invalidate()
	for _, name in ipairs({ "base" }) do cache_remove(name) end
	local section = M.find_qmodem()
	cache_remove("sms_" .. section)
	for _, prefix in ipairs({ "monsc_", "monnc_" }) do
		cache_remove(prefix .. section)
		cache_remove("failed_" .. prefix .. section)
	end
	for _, mode in ipairs({ "nr_sa", "nr_nsa", "lte", "" }) do
		local name = "hfreq_" .. section .. "_" .. mode
		cache_remove(name)
		cache_remove("failed_" .. name)
	end
end

function M.app_device_name()
	if M.is_native_nbcpe() then return require("myos_nbcpe_data").device_name() end
	if read1("/tmp/sysinfo/board_name") == "nradio,c2000-ultra" then
		return "C2000-798"
	end
	local name = uci:get("oem", "board", "pname")
	return name and name ~= "" and name or (M.is_native_c5800() and "C5800-688" or "C2000-788")
end

function M.app_software_version()
	-- Expose the LED feature gate only when its native backend is installed.
	if M.led_supported() then return "2.2.15.n0.c0" end
	return APP_SOFTWARE_VERSION
end

function M.system_version()
	return read1("/etc/openwrt_version")
end

M._test = {
	parse_monsc = parse_monsc,
	parse_hfreqinfo = parse_hfreqinfo,
	parse_neighbor_cells = parse_neighbor_cells,
	normalize_sms_read = normalize_sms_read,
	sms_ids = sms_ids,
	operator_plmn = operator_plmn,
	set_plmn_groups = function(groups)
		plmn_index = build_plmn_index(groups)
		return plmn_index
	end
}

return M
