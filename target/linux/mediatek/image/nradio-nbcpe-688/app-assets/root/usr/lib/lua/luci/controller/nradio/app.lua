module("luci.controller.nradio.app", package.seeall)

local uci = require "luci.model.uci".cursor()
local util = require "luci.util"
local http = require "luci.http"
local ok_json, cjson = pcall(require, "cjson.safe")
if not ok_json then cjson = require "cjson" end
local ok_crypto, app_crypto = pcall(require, "myos_nradio_app_crypto")
local ok_md5, md5 = pcall(require, "md5")
local shared = require "myos_nradio_data"
local app_session = require "myos_nradio_app_session"

local cache = { t = 0, data = nil }
local cache_file = "/tmp/myos-nradio-app-summary.json"
local cache_lock = "/tmp/myos-nradio-app-summary.lock"
local cache_ttl = 30
local cache_stale_ttl = 300
local response_ttl = 5
local response_stale_ttl = 300
local response_prefix = "/tmp/myos-nradio-app-response-"
local cellular_record_state_file = "/tmp/myos-cellular-record-enabled"
local auth_dir = "/tmp/myos-nradio-app-auth-sessions"
local auth_replay_file = "/tmp/myos-nradio-app-auth-replay"
local auth_replay_lock = auth_replay_file .. ".lock"
local auth_secret = "f2e6e232e75f33d5f3d5b040c93d0d"
local auth_clock_skew = 300
local app_probe_cipher = "+2j4zUClduWNHc2LNm8p6w=="

local function trim(v)
	return (tostring(v or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function shell_quote(v)
	return "'" .. tostring(v or ""):gsub("'", "'\\''") .. "'"
end

local function read1(path)
	local f = io.open(path, "r")
	if not f then return "" end
	local v = trim(f:read("*l") or "")
	f:close()
	return v
end

local function valid_mac(v)
	return tostring(v or ""):match("^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$") ~= nil
end

local function normalize_mac(v)
	local mac = trim(v):gsub(":", ""):upper()
	mac = mac:gsub("%-MYOS$", "")
	if mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") then
		return mac:gsub("(%x%x)", "%1:"):sub(1, 17)
	end
	return trim(v):upper()
end

local function placeholder_mac(mac)
	mac = normalize_mac(mac)
	return mac == "" or mac == "00:00:00:00:00:00" or mac == "FF:FF:FF:FF:FF:FF" or mac == "00:66:88:00:00:00"
end

local function valid_identity_mac(mac)
	return valid_mac(mac) and not placeholder_mac(mac)
end

local function identity_from_blob(path, key)
	local f = io.open(path, "rb")
	if not f then return "" end
	local data = f:read(65536) or ""
	f:close()
	for entry in data:gmatch("[^%z\r\n]+") do
		local k, v = entry:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
		if k == key then return trim(v) end
	end
	return ""
end

local function bd_dev()
	local f = io.open("/proc/mtd", "r")
	if f then
		for line in f:lines() do
			local dev = line:match("^(mtd%d+):")
			local name = line:match('"(.-)"')
			if dev and name and (name:match("bdinfo") or name:match("oeminfo")) then f:close(); return "/dev/" .. dev end
		end
		f:close()
	end
	local p = io.popen("blkid -t PARTLABEL=bdinfo -o device 2>/dev/null")
	if p then local dev = trim(p:read("*l") or ""); p:close(); if dev ~= "" then return dev end end
	p = io.popen("[ -e /dev/disk/by-partlabel/bdinfo ] && readlink -f /dev/disk/by-partlabel/bdinfo 2>/dev/null")
	if p then local dev = trim(p:read("*l") or ""); p:close(); if dev ~= "" then return dev end end
	return nil
end

local function identity_get(key)
	for _, path in ipairs({"/mnt/app_data/bdinfo", "/etc/bdinfo.bin", "/bdinfo/bdinfo.bin"}) do
		local v = identity_from_blob(path, key)
		if v ~= "" then return v end
	end
	local dev = bd_dev()
	if dev then
		local v = identity_from_blob(dev, key)
		if v ~= "" then return v end
	end
	for _, path in ipairs({"/etc/nradio/identity", "/boot/nradio-identity"}) do
		local v = identity_from_blob(path, key)
		if v ~= "" then return v end
	end
	return ""
end

local function device_id()
	if shared then return shared.device_id() end
	local mac = normalize_mac(uci:get("oem", "board", "id") or "")
	if not valid_identity_mac(mac) then mac = normalize_mac(identity_get("fac_mac")) end
	if not valid_identity_mac(mac) then mac = normalize_mac(identity_get("mac")) end
	if not valid_identity_mac(mac) then mac = normalize_mac(uci:get("mosquitto", "bridge", "remote_clientid") or "") end
	if not valid_identity_mac(mac) then return "" end
	return (mac:gsub(":", ""):upper())
end

local function device_code()
	if shared then return shared.device_code() end
	local code = identity_get("device_code")
	if code ~= "" then return code end
	code = trim(uci:get("oem", "board", "device_code") or "")
	if code ~= "" then return code end
	return ""
end

local function md5_hex(value)
	if value == nil or tostring(value):find("[%z\r\n]") then return "" end
	if ok_md5 and md5 and md5.sumhexa then
		local ok, digest = pcall(md5.sumhexa, tostring(value))
		if ok and digest then return tostring(digest):lower() end
	end
	local p = io.popen("printf %s " .. shell_quote(value) .. " | md5sum 2>/dev/null")
	if not p then return "" end
	local digest = trim(p:read("*l") or ""):match("^([0-9a-fA-F]+)") or ""
	p:close()
	return digest:lower()
end

local function timestamp_epoch(value)
	local y, m, d, h, min, s = value:match("^(%d%d%d%d)%-(%d%d)%-(%d%d) (%d%d):(%d%d):(%d%d)$")
	if not y then return nil end
	local epoch = os.time({ year = tonumber(y), month = tonumber(m), day = tonumber(d), hour = tonumber(h), min = tonumber(min), sec = tonumber(s) })
	if not epoch or os.date("%Y-%m-%d %H:%M:%S", epoch) ~= value then return nil end
	return epoch
end

local function monotonic_time()
	local value = read1("/proc/uptime"):match("^(%d+%.?%d*)")
	return math.floor(tonumber(value) or 0)
end

local function time_synced()
	local f = io.open("/var/state/dnsmasqsec", "r")
	if not f then return false end
	f:close()
	return true
end

local function clear_stale_auth_lock()
	local pipe = io.popen("stat -c %Y " .. shell_quote(auth_replay_lock) ..
		" 2>/dev/null")
	if not pipe then return end
	local modified = tonumber(trim(pipe:read("*l") or ""))
	pipe:close()
	if modified and os.time() - modified > 15 then
		os.execute("rmdir " .. shell_quote(auth_replay_lock) .. " 2>/dev/null")
	end
end

local function reserve_auth_replay(timestamp, trans_id, now)
	local digest = md5_hex(timestamp .. ":" .. trans_id)
	if digest == "" then return false end
	clear_stale_auth_lock()
	local lock_ok = os.execute("mkdir " .. shell_quote(auth_replay_lock) .. " 2>/dev/null")
	if lock_ok ~= true and lock_ok ~= 0 then return false end
	local entries, duplicate = {}, false
	local f = io.open(auth_replay_file, "r")
	if f then
		for line in f:lines() do
			local hash, expires = line:match("^([0-9a-f]+)|(%d+)$")
			if hash and tonumber(expires) and tonumber(expires) > now then
				if hash == digest then duplicate = true end
				entries[#entries + 1] = hash .. "|" .. expires
				if #entries > 127 then table.remove(entries, 1) end
			end
		end
		f:close()
	end
	local accepted = false
	if not duplicate then
		entries[#entries + 1] = digest .. "|" .. tostring(now + auth_clock_skew * 2)
		local tmp = auth_replay_file .. ".tmp"
		f = io.open(tmp, "w")
		if f then
			f:write(table.concat(entries, "\n"), "\n")
			f:close()
			accepted = os.rename(tmp, auth_replay_file) and true or false
			if not accepted then os.remove(tmp) end
		end
	end
	os.execute("rmdir " .. shell_quote(auth_replay_lock) .. " 2>/dev/null")
	return accepted
end

local function auth_request_valid(req)
	req = req or {}
	local code = trim(req.device_code)
	local timestamp = trim(req.timestamp)
	local trans_id = trim(req.trans_id)
	local token = trim(req.token):lower()
	if code == "" or timestamp == "" or trans_id == "" or token == "" then return false end
	if #code > 256 or #timestamp > 32 or #trans_id > 256 or
	   not token:match("^[0-9a-f]+$") or #token ~= 32 then return false end
	local input = "device_code" .. code .. "timestamp" .. timestamp .. "trans_id" .. trans_id .. auth_secret
	if md5_hex(input) ~= token then return false end
	local epoch = timestamp_epoch(timestamp)
	if not epoch then return false end
	local now = os.time()
	if time_synced() and math.abs(now - epoch) > auth_clock_skew then return false end
	if not reserve_auth_replay(timestamp, trans_id, now) then return false end
	if shared and type(shared.adopt_device_code) == "function" and
	   shared.device_code() == "" then
		shared.adopt_device_code(code)
	end
	return true
end

local function write_session(path, expires)
	local pid = read1("/proc/self/stat"):match("^(%d+)") or "0"
	local tmp = path .. ".tmp." .. pid
	local f = io.open(tmp, "w")
	if not f then return false end
	f:write(tostring(expires), "\n")
	f:close()
	if not os.rename(tmp, path) then os.remove(tmp); return false end
	os.execute("chmod 600 " .. shell_quote(path) .. " 2>/dev/null")
	return true
end

local function issue_auth_token()
	local f = io.open("/dev/urandom", "rb")
	if not f then return "" end
	local raw = f:read(16) or ""
	f:close()
	if #raw ~= 16 then return "" end
	local bytes = {}
	for i = 1, #raw do bytes[#bytes + 1] = string.format("%02x", raw:byte(i)) end
	local token = app_session.create(table.concat(bytes))
	if token == "" then return "" end
	local created = os.execute("mkdir -p " .. shell_quote(auth_dir) .. " 2>/dev/null")
	if created ~= true and created ~= 0 then return "" end
	os.execute("chmod 700 " .. shell_quote(auth_dir) .. " 2>/dev/null")
	local now = monotonic_time()
	local pipe = io.popen("ls -1 " .. shell_quote(auth_dir) .. " 2>/dev/null")
	if pipe then
		for name in pipe:lines() do
			if #name == 32 and name:match("^[0-9a-f]+$") then
				local expires = tonumber(read1(auth_dir .. "/" .. name))
				if not expires or expires <= now then os.remove(auth_dir .. "/" .. name) end
			end
		end
		pipe:close()
	end
	local path = auth_dir .. "/" .. token
	if not write_session(path, now + 3600) then app_session.destroy(token); return "" end
	return token
end

local function authorized(req)
	req = type(req) == "table" and req or {}
	local cookie = tostring(os.getenv("HTTP_COOKIE") or "")
	local token = trim(req.token)
	if token == "" then
		token = tostring(os.getenv("HTTP_AUTHORIZATION") or ""):match(
			"^[Bb]earer%s+([0-9A-Fa-f]+)$") or ""
	end
	if token == "" then
		for name, value in cookie:gmatch("([^%s;=]+)=([^;]+)") do
			if name == "sysauth" or name == "sysauth_http" or name == "sysauth_https" then token = value; break end
		end
	end
	token = token:lower()
	if #token ~= 32 or not token:match("^[0-9a-f]+$") then return false end
	local path = auth_dir .. "/" .. token
	local now = monotonic_time()
	local expires = tonumber(read1(path))
	if not expires or expires <= now or not app_session.valid(token) then os.remove(path); return false end
	write_session(path, now + 3600)
	return true
end

local function local_enabled()
	uci:load("myos_nros")
	return tostring(uci:get("myos_nros", "main", "local_enabled") or "1") ~= "0"
end

local function ubus(path, method, data)
	local ok, res = pcall(util.ubus, path, method, data or {})
	if ok and type(res) == "table" then return res end
	return {}
end

local function read_summary_cache(now, max_age)
	local f = io.open(cache_file, "r")
	if not f then return nil end
	local raw = f:read("*a") or ""
	f:close()
	local ok, d = pcall(cjson.decode, raw)
	if not ok or type(d) ~= "table" then d = {} end
	if type(d) == "table" and tonumber(d.t) and type(d.data) == "table" and now - tonumber(d.t) < (max_age or cache_ttl) then
		return d.data
	end
	return nil
end

local function acquire_summary_lock()
	local ok = os.execute("mkdir " .. cache_lock .. " 2>/dev/null")
	return ok == true or ok == 0
end

local function release_summary_lock()
	os.execute("rmdir " .. cache_lock .. " 2>/dev/null")
end

local function write_summary_cache(now, data)
	local tmp = cache_file .. ".tmp"
	local f = io.open(tmp, "w")
	if not f then return end
	f:write(cjson.encode({ t = now, data = data }) or "{}")
	f:close()
	os.rename(tmp, cache_file)
end

local function cache_key(key)
	return response_prefix .. tostring(key or "default"):gsub("[^%w_-]", "_") .. ".json"
end

local function read_response_cache(key, now, max_age)
	local f = io.open(cache_key(key), "r")
	if not f then return nil end
	local raw = f:read("*a") or ""
	f:close()
	local ok, d = pcall(cjson.decode, raw)
	if not ok or type(d) ~= "table" then return nil end
	if tonumber(d.t) and type(d.data) == "table" and now - tonumber(d.t) < (max_age or response_ttl) then
		return d.data
	end
	return nil
end

local function write_response_cache(key, now, data)
	local path = cache_key(key)
	local tmp = path .. ".tmp"
	local f = io.open(tmp, "w")
	if not f then return end
	f:write(cjson.encode({ t = now, data = data }) or "{}")
	f:close()
	os.rename(tmp, path)
end

local function response_lock(key)
	return response_prefix .. tostring(key or "default"):gsub("[^%w_-]", "_") .. ".lock"
end

local function acquire_response_lock(key)
	local ok = os.execute("mkdir " .. response_lock(key) .. " 2>/dev/null")
	return ok == true or ok == 0
end

local function release_response_lock(key)
	os.execute("rmdir " .. response_lock(key) .. " 2>/dev/null")
end

local function json_out(data)
	http.prepare_content("application/json")
	http.header("Access-Control-Allow-Origin", "*")
	http.header("Access-Control-Allow-Headers", "Origin,X-Requested-With,Content-Type,Authorization,X-Access-Token")
	http.header("Access-Control-Allow-Methods", "POST,GET,OPTIONS,PUT")
	http.write(cjson.encode(data or {}) or "{}")
end

local request_encrypted = false
local response_encrypted = false
local function request()
	request_encrypted = false
	response_encrypted = false
	local len = tonumber(os.getenv("CONTENT_LENGTH") or "0") or 0
	if len > 65536 then return {}, false end
	local raw = http.content() or ""
	if raw == "" then return {}, false end
	local ok, data = pcall(cjson.decode, raw)
	if not ok or type(data) ~= "table" then return {}, false end
	if data.data then
		if ok_crypto then
			local plain = app_crypto.decrypt_app_data(data.data)
			if plain and plain ~= "" then
				local plain_ok, decoded = pcall(cjson.decode, plain)
				if plain_ok and type(decoded) == "table" then
					request_encrypted = true
					return decoded, true
				end
			end
		end
		return {}, true
	end
	return data, false
end

local function reply(data, req)
	req = req or {}
	if req.trans_id and not data.trans_id then data.trans_id = tostring(req.trans_id) end
	if response_encrypted then data = { data = app_crypto.encrypt_app_data(cjson.encode(data)) } end
	json_out(data)
end

local function require_auth(req)
	if not local_enabled() then
		http.status(404, "Not Found")
		reply({ code = "1", result = "local app disabled" }, req)
		return false
	end
	if authorized(req) then
		response_encrypted = request_encrypted
		return true
	end
	json_out({ code = "1" })
	return false
end

local function truthy(v)
	return v == true or v == 1 or v == "1" or v == "true"
end

local function cached_reply(key, ttl, req, make)
	local now = os.time()
	local data = read_response_cache(key, now, ttl or response_ttl)
	if data then return reply(data, req) end
	local locked = acquire_response_lock(key)
	if not locked then
		data = read_response_cache(key, now, response_stale_ttl)
		if data then return reply(data, req) end
		for _ = 1, 2 do
			os.execute("sleep 1")
			now = os.time()
			data = read_response_cache(key, now, response_stale_ttl)
			if data then return reply(data, req) end
		end
	end
	data = make()
	write_response_cache(key, now, data)
	if locked then release_response_lock(key) end
	return reply(data, req)
end

local function find_qmodem()
	local sec, net
	uci:foreach("qmodem", "modem-device", function(s)
		if not sec and (s.state == "enabled" or not s.state) then
			sec = s[".name"]
			net = s.alias or s[".name"]
			return false
		end
	end)
	if not sec and uci:get("qmodem", "2_1") then
		sec = "2_1"
		net = uci:get("qmodem", sec, "alias") or sec
	end
	sec = sec or "2_1"
	net = net or sec
	if not tostring(sec):match("^[%w_%-]+$") then sec = "2_1" end
	if not tostring(net):match("^[%w_%-]+$") then net = sec end
	return sec, net
end

local function map_info(t)
	local m = {}
	for _, item in ipairs((t or {}).modem_info or {}) do
		local k = trim(item.key or item.name or item.title)
		if k ~= "" then
			m[k] = item.value or item.val or item.result or ""
			m[k:lower()] = m[k]
		end
	end
	for k, v in pairs(t or {}) do
		if type(v) ~= "table" then
			m[k] = v
			m[tostring(k):lower()] = v
		end
	end
	return m
end

local function pick(m, keys)
	for _, k in ipairs(keys) do
		local v = trim(m[k] or m[k:lower()])
		if v ~= "" and v ~= "N/A" and v ~= "nil" then return v end
	end
	return ""
end

local function first_ip(s, key)
	local a = s[key] or {}
	if a[1] and a[1].address then
		local mask = a[1].mask and ("/" .. tostring(a[1].mask)) or ""
		return tostring(a[1].address), tostring(a[1].address) .. mask
	end
	return "", ""
end

local function stats(dev)
	dev = trim(dev)
	if dev == "" or not dev:match("^[%w_.:%-]+$") then return "0", "0" end
	local rx = tonumber(read1("/sys/class/net/" .. dev .. "/statistics/rx_bytes")) or 0
	local tx = tonumber(read1("/sys/class/net/" .. dev .. "/statistics/tx_bytes")) or 0
	return tostring(tx), tostring(rx)
end

local function csv_field(v, idx)
	local n = 1
	for item in (tostring(v or "") .. ","):gmatch("([^,]*),") do
		if n == idx then return trim(item) end
		n = n + 1
	end
	return ""
end

local function sim_gpio_prefix(sec)
	for _, prefix in ipairs({ trim(sec), trim(uci:get("network", "globals", "default_cellular") or ""), "cpe" }) do
		if prefix:match("^[%w_%-]+$") and read1("/sys/class/gpio/" .. prefix .. "-sel0/value") ~= "" then return prefix end
	end
	return ""
end

local function read_sim_gpio(sec)
	local prefix = sim_gpio_prefix(sec)
	if prefix == "" then return "" end
	local value = ""
	for i = 0, 7 do
		local bit = read1("/sys/class/gpio/" .. prefix .. "-sel" .. tostring(i) .. "/value")
		if bit == "" then break end
		value = bit .. value
	end
	return value
end

local function write_sim_gpio(sec, gpio)
	gpio = trim(gpio)
	if gpio == "" or not gpio:match("^[01]+$") then return gpio == "" end
	local prefix = sim_gpio_prefix(sec)
	if prefix == "" then return false end
	for i = 0, #gpio - 1 do
		local bit = gpio:sub(#gpio - i, #gpio - i)
		local f = io.open("/sys/class/gpio/" .. prefix .. "-sel" .. tostring(i) .. "/value", "w")
		if not f then return false end
		f:write(bit)
		f:close()
	end
	return true
end

local function sim_target(cur)
	local gval = uci:get("cpesel", "sim", "gval") or ""
	local entry = csv_field(gval, cur)
	if gval ~= "" and entry == "" then return nil, nil end
	local gpio, usim = entry:match("^([^%-]*)%-(%d+)$")
	local slot = tonumber(usim)
	if slot then slot = slot - 1 else slot = cur - 1 end
	return gpio or "", slot
end

local function logical_sim_cur(sec, sim_slot)
	local gval = uci:get("cpesel", "sim", "gval") or ""
	local gpio = read_sim_gpio(sec)
	local usim = sim_slot ~= nil and tostring(sim_slot + 1) or ""
	if gval ~= "" and (gpio ~= "" or usim ~= "") then
		local idx, configured = 1, tonumber(uci:get("cpesel", "sim", "cur") or "")
		local gpio_candidate, gpio_count, slot_candidate, slot_count, configured_match = nil, 0, nil, 0, nil
		for entry in (gval .. ","):gmatch("([^,]*),") do
			local g, u = trim(entry):match("^([^%-]*)%-(%d+)$")
			if g and u and gpio ~= "" and usim ~= "" and g == gpio and u == usim then return idx end
			if g and gpio ~= "" and g == gpio then
				gpio_candidate, gpio_count = idx, gpio_count + 1
				if idx == configured then configured_match = idx end
			end
			if u and usim ~= "" and u == usim then slot_candidate, slot_count = idx, slot_count + 1 end
			idx = idx + 1
		end
		if configured_match then return configured_match end
		if gpio_count == 1 then return gpio_candidate end
		if slot_count == 1 then return slot_candidate end
	end
	return nil
end

local function clean_num(v)
	v = trim(v):gsub("dBm", ""):gsub("dB", ""):gsub("%%", "")
	return trim(v)
end

local function pretty_mode(mode)
	mode = trim(mode)
	if mode == "NR" or mode == "NR5G" or mode:find("SA") then return "NR SA" end
	if mode:find("NSA") then return "NR NSA" end
	return mode
end

local function band_mode(mode)
	mode = trim(mode)
	if mode:find("NR") or mode:find("5G") or mode:find("SA") then return "5G" end
	if mode:find("LTE") or mode:find("4G") then return "4G" end
	return mode
end

local function operator_by_iccid(iccid)
	iccid = trim(iccid)
	if iccid:match("^898600") or iccid:match("^898602") or iccid:match("^898604") or iccid:match("^898607") then return "China Mobile" end
	if iccid:match("^898601") or iccid:match("^898606") or iccid:match("^898609") then return "China Unicom" end
	if iccid:match("^898603") or iccid:match("^898611") then return "China Telecom" end
	return ""
end

local plmn_operator = {
	["46000"] = "China Mobile", ["46002"] = "China Mobile", ["46004"] = "China Mobile", ["46007"] = "China Mobile", ["46008"] = "China Mobile",
	["46013"] = "China Mobile", ["46020"] = "China Mobile", ["46021"] = "China Mobile", ["46022"] = "China Mobile", ["46023"] = "China Mobile",
	["46024"] = "China Mobile", ["46025"] = "China Mobile", ["46026"] = "China Mobile", ["46027"] = "China Mobile", ["46028"] = "China Mobile", ["46029"] = "China Mobile",
	["46003"] = "China Telecom", ["46005"] = "China Telecom", ["46011"] = "China Telecom", ["46012"] = "China Telecom",
	["46001"] = "China Unicom", ["46006"] = "China Unicom", ["46009"] = "China Unicom", ["46010"] = "China Unicom",
	["46015"] = "China Broadnet"
}

local function operator_by_plmn(plmn)
	local n = trim(plmn):gsub("%D", "")
	return plmn_operator[n] or ""
end

local function merge_info(dst, src)
	for k, v in pairs(src or {}) do
		if trim(dst[k]) == "" then dst[k] = v end
	end
	return dst
end

local function summary()
	if shared then return shared.summary() end
	local now = os.time()
	if cache.data and now - cache.t < cache_ttl then return cache.data end
	local disk = read_summary_cache(now, cache_ttl)
	if disk then
		cache = { t = now, data = disk }
		return disk
	end
	local locked = acquire_summary_lock()
	if not locked then
		disk = read_summary_cache(now, cache_stale_ttl)
		if disk then
			cache = { t = now, data = disk }
			return disk
		end
		for _ = 1, 3 do
			os.execute("sleep 1")
			now = os.time()
			disk = read_summary_cache(now, cache_stale_ttl)
			if disk then
				cache = { t = now, data = disk }
				return disk
			end
		end
	end
	local sec, net = find_qmodem()
	local cell = map_info(ubus("qmodem", "cell_info", { config_section = sec }))
	merge_info(cell, map_info(ubus("qmodem", "info", { config_section = sec })))
	local if4 = ubus("network.interface." .. net, "status")
	local if6 = ubus("network.interface." .. net .. "v6", "status")
	if not if6.up then if6 = ubus("network.interface." .. net .. "_6", "status") end
	if not if6.up then if6 = ubus("network.interface." .. net .. "6", "status") end
	local ip4, ip4p = first_ip(if4, "ipv4-address")
	local ip6, ip6p = first_ip(if6, "ipv6-address")
	local dev = if4.device or if6.device or net
	local tx, rx = stats(dev)
	local imei = pick(cell, {"IMEI", "imei"})
	if imei == "" then imei = identity_get("imei") end
	if imei == "" then imei = uci:get("oem", "board", "imei") or "" end
	local iccid = pick(cell, {"ICCID", "iccid"})
	local imsi = pick(cell, {"IMSI", "imsi"})
	local sim_status = pick(cell, {"SIM Status", "sim_status", "CPIN", "cpin"})
	local mode = pick(cell, {"Mode", "MODE", "RAT", "Network", "network_mode"})
	local raw_isp = pick(cell, {"ISP", "Operator", "COPS", "MCCMNC"})
	local mcc = pick(cell, {"MMC", "MCC", "mcc"})
	local mnc = pick(cell, {"MNC", "mnc"})
	local plmn = raw_isp:match("^%d+$") and raw_isp or ""
	if plmn == "" and mcc ~= "" and mnc ~= "" then plmn = mcc .. mnc end
	local operator = operator_by_plmn(plmn)
	if operator == "" and raw_isp ~= "" and not raw_isp:match("^%d+$") then operator = raw_isp end
	if operator == "" then operator = operator_by_iccid(iccid) end
	local sim_slot = tonumber(pick(cell, {"SIM Slot", "sim_slot", "slot"}))
	local simno = tostring(logical_sim_cur(sec, sim_slot) or (sim_slot ~= nil and sim_slot + 1) or (tonumber(uci:get("cpesel", "sim", "cur") or "1") or 1))
	local data = {
		section = sec,
		network = net,
		dev = dev,
		up = if4.up or if6.up,
		uptime = tonumber(if4.uptime or if6.uptime or 0) or 0,
		ip4 = ip4,
		ip4p = ip4p,
		ip6 = ip6,
		ip6p = ip6p,
		dns4 = if4["dns-server"] or {},
		dns6 = if6["dns-server"] or {},
		tx = tx,
		rx = rx,
		mode = pretty_mode(mode),
		isp = plmn ~= "" and plmn or raw_isp,
		operator = operator,
		operatorName = operator,
		imei = imei,
		imsi = imsi,
		iccid = iccid,
		simno = simno,
		simstatus = sim_status ~= "" and sim_status:upper() or (iccid ~= "" and "READY" or ""),
		model = pick(cell, {"name", "model", "Model"}),
		revision = pick(cell, {"revision", "Revision"}),
		temp = clean_num(pick(cell, {"Temperature", "temperature", "model_temp", "temp"})),
		cell = pick(cell, {"Cell ID", "cell", "CellID"}),
		lac = pick(cell, {"LAC", "lac"}),
		tac = pick(cell, {"TAC", "tac"}),
		pci = pick(cell, {"Physical Cell ID", "PCI", "pci"}),
		earfcn = pick(cell, {"ARFCN", "EARFCN", "earfcn", "arfcn"}),
		band = pick(cell, {"Band", "band", "NR Band", "LTE Band", "NR_BAND", "LTE_BAND"}),
		rsrp = clean_num(pick(cell, {"RSRP", "rsrp"})),
		rsrq = clean_num(pick(cell, {"RSRQ", "rsrq"})),
		sinr = clean_num(pick(cell, {"SINR", "sinr", "SNR", "snr"})),
		rssi = clean_num(pick(cell, {"RSSI", "rssi", "RxLev"})),
		dlbw = clean_num(pick(cell, {"DL Bandwidth", "DLBW", "dl_bw"})),
		ulbw = clean_num(pick(cell, {"UL Bandwidth", "ULBW", "ul_bw"})),
		cqi = clean_num(pick(cell, {"CQI", "cqi"}))
	}
	if data.model == "" then data.model = "MT5700M" end
	cache = { t = now, data = data }
	write_summary_cache(now, data)
	if locked then release_summary_lock() end
	return data
end

local function cpe_item()
	if shared then return shared.cpe_item() end
	local m = summary()
	local sim_state = { cur = tonumber(m.simno) or tonumber(uci:get("cpesel", "sim", "cur") or "1") or 1, max = tonumber(uci:get("cpesel", "sim", "max") or "2") or 2 }
	local signal = tonumber(m.rsrp) or tonumber(m.rssi) or -999
	local item = {
		name = "cpe",
		cpeno = 1,
		using = true,
		up = m.up and 1 or 0,
		signal = signal,
		traffic = tostring((tonumber(m.tx) or 0) + (tonumber(m.rx) or 0)),
		device_traffic = tostring((tonumber(m.tx) or 0) + (tonumber(m.rx) or 0)),
		upload = m.tx,
		download = m.rx,
		control = uci:get("qmodem", m.section, "at_port") or "",
		hwid = "",
		mode = m.mode,
		isp = m.isp,
		operator = m.operator or "",
		operatorName = m.operator or "",
		sim_isp = m.operator or "",
		imei = m.imei,
		imsi = m.imsi,
		iccid = m.iccid,
		cell = m.cell,
		cellid = m.cell,
		lac = m.lac,
		pci = m.pci,
		earfcn = m.earfcn,
		tac = m.tac,
		rsrp = m.rsrp,
		RSRP = m.rsrp,
		sinr = m.sinr,
		SINR = m.sinr,
		rsrq = m.rsrq,
		RSRQ = m.rsrq,
		rssi = m.rssi,
		RSSI = m.rssi,
		rscp = "",
		model = m.model,
		revision = m.revision,
		nrcap = "1",
		simno = tostring(sim_state.cur),
		simmode = "auto",
		simtype = "0",
		switchmode = "1",
		flow_overflow = "",
		sim_flow = "0",
		sim_mon = "0",
		simnumber = "",
		simcount = tostring(sim_state.max),
		model_temp = m.temp,
		status = m.up and 0 or -1,
		gateway_if = m.up and 1 or -1,
		apn = "",
		CPIN = m.simstatus,
		band = m.band,
		dl_pci = m.pci,
		dl_fcn = m.earfcn,
		ul_fcn = m.earfcn,
		DLBW = m.dlbw,
		ULBW = m.ulbw,
		band_count = 1,
		CQI = m.cqi,
		odu = "0",
		net_prefer = -1
	}
	if m.ip4p ~= "" then item.V4 = { ipaddrs = { m.ip4p }, dns = m.dns4 } end
	if m.ip6p ~= "" then item.V6 = { ipaddrs = { m.ip6p }, dns = m.dns6 } end
	return item
end

local function at_signal_item()
	if shared then return shared.cpe_item({ focused = true }) end
	local item = cpe_item()
	item.rsrp = item.rsrp ~= "" and item.rsrp or tostring(item.signal or "")
	item.RSRP = item.rsrp
	return item
end

local function cellular_record_enabled()
	local v = uci:get("cellular_record", "basic", "enabled")
	if v == nil then v = read1(cellular_record_state_file) end
	return truthy(v) and 1 or 0
end

local function set_cellular_record_enabled(v)
	local enabled = truthy(v) and "1" or "0"
	local ok = false
	if uci:get("cellular_record", "basic") ~= nil then
		ok = pcall(function()
			uci:set("cellular_record", "basic", "enabled", enabled)
			uci:commit("cellular_record")
		end)
	end
	if not ok then
		local f = io.open(cellular_record_state_file, "w")
		if f then f:write(enabled); f:close() end
	end
	return tonumber(enabled) or 0
end

local function wifi_info()
	local list, idx = {}, 0
	uci:foreach("wireless", "wifi-iface", function(s)
		local mode = trim(s.mode):lower()
		if mode ~= "" and mode ~= "ap" then return end
		idx = idx + 1
		local dev = s.device or ""
		local disabled = s.disabled or uci:get("wireless", dev, "disabled") or "0"
		list[#list + 1] = {
			ssid = s.ssid or "",
			password = s.key or "",
			rule_name = "wlan" .. tostring(idx - 1),
			ruleName = "wlan" .. tostring(idx - 1),
			hidden = s.hidden or "0",
			encryption = s.encryption or "none",
			channel = uci:get("wireless", dev, "channel") or "auto",
			disabled = disabled,
			band = idx - 1,
			max_link = tonumber(s.maxassoc or s.max_inactivity or 0) or 0
		}
	end)
	if #list == 0 then list[1] = {} end
	return list
end

local function valid_wifi_text(value, max_len, required)
	if type(value) ~= "string" and type(value) ~= "number" then return false end
	value = tostring(value or "")
	if required and value == "" then return false end
	return #value <= max_len and not value:find("[%z\r\n]")
end

local function wifi_item_for_index(list, index)
	local wanted = "wlan" .. tostring(index - 1)
	for _, item in ipairs(list or {}) do
		if type(item) == "table" and trim(item.rule_name or item.ruleName) == wanted then return item end
	end
	return list and list[index]
end

local function apply_wifi(req)
	if type(req.list) ~= "table" or #req.list == 0 or #req.list > 8 then return false end
	local sections = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		local mode = trim(s.mode):lower()
		if mode == "" or mode == "ap" then sections[#sections + 1] = s end
	end)
	if #sections == 0 then return false end
	for index, section in ipairs(sections) do
		local item = wifi_item_for_index(req.list, index)
		if item then
			local ssid = item.ssid
			local password = item.password
			local encryption = item.encryption
			local channel = item.channel
			local hidden = item.hidden
			local disabled = item.disabled
			if ssid ~= nil and not valid_wifi_text(ssid, 32, true) then return false end
			if password ~= nil and not valid_wifi_text(password, 64, false) then return false end
			if encryption ~= nil then
				encryption = trim(encryption):lower()
				if encryption == "" or not encryption:match("^[%w%+%-]+$") then return false end
			end
			if channel ~= nil then
				channel = trim(channel):lower()
				if channel ~= "" and channel ~= "auto" and not channel:match("^%d%d?%d?$") then return false end
			end
			if hidden ~= nil and not tostring(hidden):match("^[01]$") then return false end
			if disabled ~= nil and not tostring(disabled):match("^[01]$") then return false end
			if ssid ~= nil then uci:set("wireless", section[".name"], "ssid", tostring(ssid)) end
			if encryption ~= nil then uci:set("wireless", section[".name"], "encryption", encryption) end
			if password ~= nil then
				if tostring(password) == "" or encryption == "none" then
					uci:delete("wireless", section[".name"], "key")
				else
					uci:set("wireless", section[".name"], "key", tostring(password))
				end
			end
			if hidden ~= nil then uci:set("wireless", section[".name"], "hidden", tostring(hidden)) end
			if disabled ~= nil then uci:set("wireless", section[".name"], "disabled", tostring(disabled)) end
			if channel ~= nil and channel ~= "" then uci:set("wireless", section.device, "channel", channel) end
		end
	end
	uci:commit("wireless")
	os.execute("(wifi reload) >/dev/null 2>&1 &")
	return true
end

local function require_device(req)
	local code = trim(req and req.device_code)
	if code ~= "" and code == device_code() then return true end
	reply({ code = "1", result = "invalid device identity" }, req)
	return false
end

local function diagnosis()
	local m = summary()
	return {
		netlink = m.up and 1 or 0,
		speedlimit = 0,
		list = { {
			using = true,
			nosignal = (tonumber(m.rsrp) or tonumber(m.rssi)) and 0 or 1,
			model = m.model ~= "" and 0 or 1,
			register = ((m.isp or "") ~= "" or (m.operator or "") ~= "") and 0 or 1,
			sim = m.iccid ~= "" and 0 or 1,
			isp = (m.isp or "") ~= "" and m.isp or (m.operator or ""),
			operator = m.operator or ""
		} }
	}
end

local function cpesel_max()
	if shared.is_native_nbcpe() then return 2 end
	local gval = uci:get("cpesel", "sim", "gval") or ""
	local count = 0
	for _ in (gval .. ","):gmatch("([^,]+),") do count = count + 1 end
	return count > 0 and count or
		(tonumber(uci:get("cpesel", "sim", "max") or "2") or 2)
end

local function cpesel_info()
	local m = summary()
	local op = (m.operator or "") ~= "" and m.operator or operator_by_iccid(m.iccid)
	-- cur/default 往下一律是 APP 侧编号;逻辑编号留在 logical_* 里,给
	-- stype 这种按物理槽位存的配置取值用。
	local logical_cur = tonumber(m.simno) or tonumber(uci:get("cpesel", "sim", "cur") or "1") or 1
	local cur = shared.sim_swap(logical_cur)
	local mode = tonumber(uci:get("cpesel", "sim", "mode") or "0") or 0
	local max = cpesel_max()
	local stype = uci:get("cpesel", "sim", "stype") or "0,0"
	if shared.is_native_nbcpe() then stype = "0,4" end
	local gval = uci:get("cpesel", "sim", "gval") or ""
	local logical_default = tonumber(uci:get("cpesel", "sim", "default") or tostring(logical_cur)) or logical_cur
	local default = shared.sim_swap(logical_default)
	local iccid, info = {}, {}
	for i = 1, max do
		-- i 是 APP 侧显示位;stype 按物理槽位存,取值要换回逻辑编号,
		-- 否则一个外置槽是 eSIM 时类型会跟着错位。
		local slot = shared.sim_swap(i)
		iccid[i] = (i == cur) and m.iccid or ""
		local sim_type = stype:match("^" .. string.rep("[^,]*,", slot - 1) .. "([^,]*)") or "0"
		info[i] = { cpesel = tostring(i), operator = (i == cur) and op or "", iccid = iccid[i], type = sim_type ~= "" and sim_type or "0", cur = i }
	end
	return { {
		cur = cur,
		type = stype,
		stype = stype,
		default = tostring(default),
		gval = gval,
		adv_sim = "0",
		max = tostring(max),
		iccid = iccid,
		info = info,
		mode = mode,
		roaming = "",
		apn_cfg = "",
		ippass = "",
		mobility = "",
		freq_time = "",
		endtime = "",
		starttime = "",
		peak_hour = "0",
		peaktime = "",
		freqmode = "auto",
		earfreq_mode = "",
		freq5 = { enabled = "0", earfcn = "", pci = "", band = "" },
		freq4 = { enabled = "0", earfcn = "", pci = "" },
		band = { enabled = "0", freq = "" },
		apn = { enabled = "0", name = "", username = "", password = "", auth = "", pdptype = "" }
	} }
end

local function apply_cpesel(req)
	local cur = req.cur
	if type(cur) == "table" then cur = cur[1] end
	cur = tonumber(cur)
	if not cur then return true end
	cur = shared.sim_swap(cur)
	local mode = req.mode
	if type(mode) == "table" then mode = mode[1] end
	mode = tonumber(mode) or 0
	local max = cpesel_max()
	if cur < 1 or cur > max then return false end
	if shared.is_native_nbcpe() or shared.is_native_c5800() then
		local result = shared.apply_native_cpesel(cur, mode)
		cache = { t = 0, data = nil }
		return result
	end
	if read1("/tmp/sysinfo/board_name") == "nradio,c2000-ultra" then
		local result = os.execute("/etc/init.d/c2000ultra-sim-select select_sim " ..
			shell_quote(tostring(cur)) .. " " .. shell_quote(tostring(mode)) .. " >/dev/null 2>&1")
		if result ~= true and result ~= 0 then return false end
		uci:unload("cpesel")
		cache = { t = 0, data = nil }
		if shared then shared.invalidate() end
		return true
	end
	local configured_cur = tonumber(uci:get("cpesel", "sim", "cur") or "1") or 1
	local modem = summary()
	local old_cur = tonumber(modem.simno) or configured_cur
	local old_mode = tonumber(uci:get("cpesel", "sim", "mode") or "0") or 0
	if cur == old_cur and mode == old_mode then
		if configured_cur ~= cur then
			uci:set("cpesel", "sim", "cur", tostring(cur))
			uci:commit("cpesel")
		end
		return true
	end
	local target_gpio, target_slot, sec
	if cur ~= old_cur then
		target_gpio, target_slot = sim_target(cur)
		if target_slot == nil then return false end
		sec = modem.section ~= "" and modem.section or "2_1"
		local old_gpio = read_sim_gpio(sec)
		if not write_sim_gpio(sec, target_gpio) then return false end
		local ok, result = pcall(util.ubus, "qmodem", "set_sim_slot", { config_section = sec, slot = tostring(target_slot) })
		local modem_result = type(result) == "table" and trim(result.result) or ""
		local modem_status = type(result) == "table" and trim(result.status) or ""
		local switched = modem_status == "1" or modem_result:upper():find("OK", 1, true) ~= nil
		if not ok or type(result) ~= "table" or not switched then
			write_sim_gpio(sec, old_gpio)
			return false
		end
	end
	uci:set("cpesel", "sim", "cpesel")
	uci:set("cpesel", "sim", "cur", tostring(cur))
	uci:set("cpesel", "sim", "mode", tostring(mode))
	uci:commit("cpesel")
	if cur ~= old_cur then
		if sec:match("^[%w_%-]+$") then
			os.execute("(sleep 2; ubus call qmodem modem_redial " .. shell_quote('{"config_section":"' .. sec .. '"}') .. " >/dev/null 2>&1 || ifup " .. shell_quote(sec) .. " >/dev/null 2>&1) &")
		end
	end
	cache = { t = 0, data = nil }
	if shared then shared.invalidate() end
	return true
end

local function access_section(mac)
	local found
	mac = normalize_mac(mac)
	uci:foreach("access_ctl", "client", function(s)
		if normalize_mac(s.mac) == mac then found = s[".name"]; return false end
	end)
	return found
end

local function access_info(mac)
	local info = { switch = 1, hostname = "" }
	mac = normalize_mac(mac)
	uci:foreach("access_ctl", "client", function(s)
		if normalize_mac(s.mac) == mac then
			info.switch = tonumber(s.switch) or 1
			info.hostname = s.hostname or ""
			return false
		end
	end)
	return info
end

local function firewall_sections(mac)
	local name, sections = "MYOS APP block " .. normalize_mac(mac), {}
	uci:foreach("firewall", "rule", function(s)
		if s.name == name then sections[#sections + 1] = s[".name"] end
	end)
	return sections
end

local function remove_firewall_block(mac)
	for _, section in ipairs(firewall_sections(mac)) do
		uci:delete("firewall", section)
	end
end

local function set_firewall_block(mac, block)
	mac = normalize_mac(mac)
	remove_firewall_block(mac)
	if block then
		local section = uci:add("firewall", "rule")
		if not section then return false end
		uci:set("firewall", section, "name", "MYOS APP block " .. mac)
		uci:set("firewall", section, "src", "lan")
		uci:set("firewall", section, "dest", "wan")
		uci:set("firewall", section, "src_mac", mac)
		uci:set("firewall", section, "target", "REJECT")
		uci:set("firewall", section, "family", "any")
	end
	if not uci:commit("firewall") then return false end
	local result = os.execute("/etc/init.d/firewall reload >/dev/null 2>&1")
	if result ~= true and result ~= 0 then return false end
	local present = #firewall_sections(mac) > 0
	return block and present or not present
end

local function set_client_access(mac, switch, hostname)
	mac = normalize_mac(mac)
	if not valid_mac(mac) then return nil end
	if hostname ~= nil then
		hostname = trim(hostname)
		if #hostname > 64 or hostname:find("[%z\r\n]") then return nil end
	end
	local section = access_section(mac)
	if not section then
		local f = io.open("/etc/config/access_ctl", "a")
		if f then f:close() end
	end
	if not section then section = uci:add("access_ctl", "client") end
	if not section then return nil end
	uci:set("access_ctl", section, "mac", mac)
	if switch ~= nil then
		switch = tonumber(switch) == 0 and 0 or 1
		uci:set("access_ctl", section, "switch", tostring(switch))
	end
	if hostname ~= nil then uci:set("access_ctl", section, "hostname", hostname) end
	if not uci:commit("access_ctl") then return nil end
	if switch ~= nil and not set_firewall_block(mac, switch == 0) then return nil end
	local saved = access_info(mac)
	if switch ~= nil and saved.switch ~= switch then return nil end
	if hostname ~= nil and saved.hostname ~= hostname then return nil end
	return saved
end

local function cellular_info()
	local m = summary()
	local sms = type(shared.sms_supported) == "function" and shared.sms_supported() and 1 or 0
	return { {
		earfcn = m.earfcn,
		pci = m.pci,
		nrcap = 1,
		nrrc = 0,
		ippass = 0,
		blacklist_band = "",
		compatibility = 0,
		automatic = 0,
		sms = sms,
		command_equal = 0,
		mobility = 0,
		simisolate = 1
	} }
end

local function status_info()
	local m = summary()
	local usage = type(shared.system_usage) == "function" and
		shared.system_usage() or { cpu_percent = 0, mem_percent = 0 }
	return {
		global = { netlink = m.up and 1 or 0, uptime = m.uptime,
			cpu_percent = usage.cpu_percent, mem_percent = usage.mem_percent,
			net_prefer = "auto" },
		cpe = shared and shared.cpe_items() or { cpe_item() },
		link = { { port = 5, mode = 1, speed = 4, status = m.up and 0 or 1 } },
		wans = { { name = "cpe", status = m.up and 0 or 1, uptime = m.uptime,
			upload = tonumber(m.tx) or 0, download = tonumber(m.rx) or 0,
			ipaddrs = { { ipaddr = m.ip4p } } } }
	}
end

local function runtime_info() return status_info() end

local function speed_result(name)
	if shared then return shared.speed(name) end
	local m = summary()
	local n = trim(name)
	if n == "" then n = "cpe" end
	local tx, rx = stats(m.dev)
	return { list = { { name = n, upload = tonumber(tx) or 0, download = tonumber(rx) or 0 } } }
end

local function traffic_result()
	if shared then return shared.traffic() end
	local m = summary()
	local tx, rx = stats(m.dev)
	local total = (tonumber(tx) or 0) + (tonumber(rx) or 0)
	return { list = { { name = "cpe", upload = tonumber(tx) or 0, download = tonumber(rx) or 0, traffic = total } } }
end

local function neighbour_info(filter)
	if shared then return shared.neighbour(filter) end
	local m = summary()
	local raw = ubus("qmodem", "get_neighborcell", { config_section = m.section })
	local source = raw.neighborcell or raw.neighbour or raw.result or {}
	if type(source.neighborcell) == "table" then source = source.neighborcell end
	if type(source.neighbour) == "table" then source = source.neighbour end
	local cells = {}
	local function add(item, fallback_mode)
		if type(item) ~= "table" then return end
		local mode = trim(item.MODE or item.mode or item.rat or fallback_mode):upper()
		if mode:find("5G", 1, true) then mode = "NR" end
		if mode:find("4G", 1, true) then mode = "LTE" end
		if filter == "4" and mode ~= "LTE" then return end
		if filter == "5" and mode ~= "NR" then return end
		local earfcn = trim(item.EARFCN or item.ARFCN or item.earfcn or item.arfcn)
		local pci = trim(item.PCI or item.pci)
		local rsrp = clean_num(item.RSRP or item.rsrp)
		if earfcn == "" and pci == "" and rsrp == "" then return end
		cells[#cells + 1] = {
			MODE = mode,
			EARFCN = earfcn,
			BAND = trim(item.BAND or item.band),
			PCI = pci,
			RSRP = rsrp,
			RSRQ = clean_num(item.RSRQ or item.rsrq),
			SINR = clean_num(item.SINR or item.sinr),
			RSSI = clean_num(item.RSSI or item.rssi),
			RSCP = clean_num(item.RSCP or item.rscp),
			ECNO = clean_num(item.ECNO or item.ecno),
			RXLEV = clean_num(item.RXLEV or item.rxlev)
		}
	end
	for _, mode in ipairs({ "NR", "LTE", "WCDMA" }) do
		for _, item in ipairs(type(source[mode]) == "table" and source[mode] or {}) do add(item, mode) end
	end
	if #cells == 0 then
		for _, item in ipairs(source) do
			if type(item) == "table" and item[1] then
				for _, nested in ipairs(item) do add(nested) end
			else
				add(item)
			end
		end
	end
	if #cells == 0 then return { {} } end
	table.sort(cells, function(a, b) return (tonumber(a.RSRP) or -999) > (tonumber(b.RSRP) or -999) end)
	return { cells }
end

local function dhcp_clients()
	if shared and type(shared.clients) == "function" then return shared.clients() end
	local clients, count = {}, 0
	local seen = {}
	local f = io.open("/tmp/dhcp.leases", "r")
	if not f then return {}, 0 end
	for line in f:lines() do
		local expires, mac, ip, host = line:match("^(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
		if mac and ip then
			mac = normalize_mac(mac)
			local access = access_info(mac)
			if access.hostname ~= "" then host = access.hostname end
			count = count + 1
			clients[#clients + 1] = {
				mac = mac, ipaddr = ip, ip = ip, hostname = host ~= "*" and host or "",
				type = "wireless", authed = 0, expires = tonumber(expires) or 0,
				switch = access.switch, switch_off = access.switch == 0 and 1 or 0, qos = 1, exsit = 1, position = 0
			}
			seen[mac] = true
		end
	end
	f:close()
	uci:foreach("access_ctl", "client", function(s)
		local mac = normalize_mac(s.mac)
		if valid_mac(mac) and not seen[mac] then
			local switch = tonumber(s.switch) or 1
			count = count + 1
			clients[#clients + 1] = { mac = mac, ipaddr = "", ip = "", hostname = s.hostname or "", type = "wireless", authed = 0, expires = 0, switch = switch, switch_off = switch == 0 and 1 or 0, qos = 1, exsit = 0, position = 0 }
		end
	end)
	return clients, count
end

local function info_result()
	local m = summary()
	local clients, count = dhcp_clients()
	local app_api_version = shared.app_software_version()
	local sys_version = app_api_version:match("^(%d+%.%d+%.%d+)")
	return {
		runtime = runtime_info(),
		basic = {
			version = app_api_version,
			sys_version = sys_version,
			api_version = app_api_version,
			mac = device_id(),
			name = shared.app_device_name(),
			net_prefer = uci:get("network", "globals", "net_prefer") or "",
			active_modem = { 1 },
			modem_cnt = 1
		},
		diagnosis = diagnosis(),
		cpesel = cpesel_info(),
		cpecfg = { {} },
		apn = {},
		client = clients,
		client_count = count,
		client_info = { count = count, client = clients },
		auth_on = 0,
		wifi = wifi_info(),
		bat = { percent = "-1", charging = false },
		cellular = cellular_info(),
		cpe = cpe_item(),
		intstatus = { mode = (m.operator or "") ~= "" and (m.operator .. "," .. band_mode(m.mode)) or band_mode(m.mode), time = m.uptime },
		status = status_info(),
		speed = speed_result("cpe")
	}
end

function index()
	local page = entry({"nradio", "app"}, alias("nradio", "app", "heartbeat"), nil, nil, true)
	entry({"nradio", "app", "auth"}, call("action_auth"), nil, nil, true).leaf = true
	entry({"nradio", "app", "heartbeat"}, call("action_heartbeat"), nil, nil, true).leaf = true
	entry({"nradio", "app", "signal"}, call("action_signal"), nil, nil, true).leaf = true
	entry({"nradio", "app", "info"}, call("action_info"), nil, nil, true).leaf = true
	entry({"nradio", "app", "status"}, call("action_status"), nil, nil, true).leaf = true
	entry({"nradio", "app", "speed"}, call("action_speed"), nil, nil, true).leaf = true
	entry({"nradio", "app", "traffic"}, call("action_traffic"), nil, nil, true).leaf = true
	entry({"nradio", "app", "wifi"}, call("action_wifi"), nil, nil, true).leaf = true
	entry({"nradio", "app", "wireless"}, call("action_wifi"), nil, nil, true).leaf = true
	entry({"nradio", "app", "runtime"}, call("action_runtime"), nil, nil, true).leaf = true
	entry({"nradio", "app", "cpesel"}, call("action_cpesel"), nil, nil, true).leaf = true
	entry({"nradio", "app", "cmd"}, call("action_cmd"), nil, nil, true).leaf = true
	entry({"nradio", "app", "client"}, call("action_client"), nil, nil, true).leaf = true
	entry({"nradio", "app", "terminal"}, call("action_client"), nil, nil, true).leaf = true
	entry({"nradio", "app", "combo"}, call("action_combo"), nil, nil, true).leaf = true
	entry({"nradio", "app", "neighbour"}, call("action_neighbour"), nil, nil, true).leaf = true
	entry({"nradio", "app", "earfcn"}, call("action_earfcn"), nil, nil, true).leaf = true
	entry({"nradio", "app", "sms"}, call("action_sms"), nil, nil, true).leaf = true
	entry({"nradio", "app", "led"}, call("action_led"), nil, nil, true).leaf = true
	entry({"nradio", "app", "apn"}, call("action_disabled"), nil, nil, true).leaf = true
	entry({"nradio", "app", "sync"}, call("action_disabled"), nil, nil, true).leaf = true
	entry({"nradio", "app", "lan"}, call("action_disabled"), nil, nil, true).leaf = true
	entry({"nradio", "app", "password"}, call("action_disabled"), nil, nil, true).leaf = true
	entry({"nradio", "app", "wifiauth"}, call("action_wifiauth"), nil, nil, true).leaf = true
end

function action_auth()
	local req, encrypted = request()
	if not local_enabled() then
		http.status(404, "Not Found")
		return reply({ code = "1", result = "local app disabled" }, req)
	end
	if not encrypted or not auth_request_valid(req) then return reply({ code = "1", result = "invalid auth" }, req) end
	local token = issue_auth_token()
	if token == "" then return reply({ code = "1", result = "auth unavailable" }, req) end
	http.header("Set-Cookie", app_session.cookie(token))
	reply({ code = "0", token = token, timeout = 3600 }, req)
end

function action_heartbeat()
	local req = request()
	if not require_auth(req) then return end
	reply({ code = "0" }, req)
end

function action_signal()
	local req, encrypted = request()
	if local_enabled() and not encrypted then
		return json_out({ data = app_probe_cipher })
	end
	if not require_auth(req) then return end
	if truthy(req.at_signal) then
		return reply({ code = "0", at_signal = at_signal_item() }, req)
	end
	cached_reply("signal", response_ttl, req, function() return { code = "0", signal = shared and shared.cpe_items() or { cpe_item() } } end)
end

function action_info()
	local req = request()
	if not require_auth(req) then return end
	cached_reply("info", response_ttl, req, function() return { code = "0", result = info_result() } end)
end

function action_status()
	local req = request()
	if not require_auth(req) then return end
	cached_reply("status", response_ttl, req, function() return { code = 0, result = status_info() } end)
end

function action_speed()
	local req = request()
	if not require_auth(req) then return end
	local name = trim(req.name)
	if name == "" then name = "cpe" end
	reply({ code = "0", result = speed_result(name) }, req)
end

function action_traffic()
	local req = request()
	if not require_auth(req) then return end
	cached_reply("traffic", response_ttl, req, function() return { code = "0", result = traffic_result() } end)
end

function action_wifi()
	local req = request()
	if not require_auth(req) then return end
	if req.list ~= nil then
		if not require_device(req) then return end
		if not apply_wifi(req) then return reply({ code = "1", result = "invalid wifi config" }, req) end
		os.remove(cache_key("wifi"))
		return reply({ code = "0", result = { list = wifi_info() } }, req)
	end
	cached_reply("wifi", response_ttl, req, function() return { code = "0", result = { list = wifi_info() } } end)
end

function action_cpesel()
	local req = request()
	if not require_auth(req) then return end
	if req.cur ~= nil then
		if not require_device(req) then return end
		if not apply_cpesel(req) then return reply({ code = "1", result = "invalid sim" }, req) end
	end
	reply({ code = "0", result = { cpesel = cpesel_info(true) } }, req)
end

function action_client()
	local req = request()
	if not require_auth(req) then return end
	if req.client and (req.switch ~= nil or req.hostname ~= nil) then
		if not require_device(req) then return end
		local mac = normalize_mac(req.client)
		local info = set_client_access(mac, req.switch, req.hostname)
		if not info then return reply({ code = "1", result = "invalid client" }, req) end
		os.remove(cache_key("client"))
		return reply({ code = "0", result = { client = { client = mac, mac = mac, switch = info.switch, hostname = info.hostname } }, auth_on = 0 }, req)
	end
	cached_reply("client", response_ttl, req, function()
		local clients, count = dhcp_clients()
		return { code = "0", result = { count = count, client = clients }, auth_on = 0 }
	end)
end

function action_combo()
	local req = request()
	if not require_auth(req) then return end
	if req.combo ~= nil then return reply({ code = "2", result = "unsupported on MYOS" }, req) end
	cached_reply("combo", response_ttl, req, function() return { code = "0", result = runtime_info() } end)
end

function action_runtime()
	local req = request()
	if not require_auth(req) then return end
	cached_reply("runtime", response_ttl, req, function() return { code = "0", result = runtime_info() } end)
end

function action_neighbour()
	local req = request()
	if not require_auth(req) then return end
	local filter = trim(req.type)
	if filter ~= "4" and filter ~= "5" then filter = "" end
	cached_reply("neighbour_" .. filter, 30, req, function()
		return { code = "0", result = { neighbour = neighbour_info(filter) } }
	end)
end

function action_earfcn()
	local req = request()
	if not require_auth(req) then return end
	local action = tonumber(req.action or 2)
	if action ~= 2 or req.mode ~= nil or req.band ~= nil or req.earfcn ~= nil or req.earfcns ~= nil or req.adv ~= nil or req.earfreq_mode ~= nil then
		return reply({ code = "1", result = "unsupported on MYOS" }, req)
	end
	local m = summary()
	reply({
		code = "0",
		result = {
			index = tonumber(req.index) or 1,
			action = 2,
			sim = tonumber(req.sim) or shared.sim_swap(m.simno) or 1,
			mode = "auto",
			band = { status = "0", freq = "" },
			earfcn = {}
		}
	}, req)
end

function action_wifiauth()
	local req = request()
	if not require_auth(req) then return end
	reply({ wifiauth = -1 }, req)
end

function action_sms()
	local req = request()
	if not require_auth(req) then return end
	if not shared.sms_request_allowed(req) then
		return reply({ code = "1", result = "invalid device identity" }, req)
	end
	reply(shared.sms(req), req)
end

function action_led()
	local req = request()
	if not require_auth(req) then return end
	if req.led ~= nil or req.schedule ~= nil or req.start_time ~= nil or req.end_time ~= nil then
		if trim(req.device_code) ~= device_code() or device_code() == "" then
			return reply({ code = 2, message = "Invalid device identity" }, req)
		end
	end
	reply(shared.led(req), req)
end

function action_disabled()
	local req = request()
	if not require_auth(req) then return end
	reply({ code = "1", result = "unsupported on MYOS" }, req)
end

function action_cmd()
	local req = request()
	if not require_auth(req) then return end
	if req.cmd == "service" and req.name == "dropbear" then
		return reply(app_session.ssh_service(req, device_code()), req)
	elseif req.cmd == "service" and req.name == "cellular_record" then
		if not require_device(req) then return end
		local enabled = cellular_record_enabled()
		if req.enabled ~= nil then enabled = set_cellular_record_enabled(req.enabled) end
		reply({ code = "0", name = "cellular_record", enabled = enabled, cellular_record = { enabled = enabled } }, req)
	elseif req.cmd == "reboot" and req.device_code and tostring(req.device_code) == device_code() then
		if device_code() == "" then return reply({ code = "1", result = "missing device identity" }, req) end
		reply({ code = "0", result = {} }, req)
		os.execute("(sleep 1; reboot) >/dev/null 2>&1 &")
	else
		reply({ code = "1", result = "disabled on MYOS" }, req)
	end
end
