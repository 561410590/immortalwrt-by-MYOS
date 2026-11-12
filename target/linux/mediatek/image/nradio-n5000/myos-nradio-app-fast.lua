#!/usr/bin/lua

local ok_json, cjson = pcall(require, "cjson.safe")
if not ok_json then cjson = require "cjson" end
local ok_crypto, app_crypto = pcall(require, "myos_nradio_app_crypto")
local ok_md5, md5 = pcall(require, "md5")
local shared = require "myos_nradio_data"
local app_session = require "myos_nradio_app_session"
local app_uci = require("luci.model.uci").cursor()

local cache_file = "/tmp/myos-nradio-app-summary.json"
local auth_dir = "/tmp/myos-nradio-app-auth-sessions"
local auth_replay_file = "/tmp/myos-nradio-app-auth-replay"
local auth_replay_lock = auth_replay_file .. ".lock"
local auth_secret = "f2e6e232e75f33d5f3d5b040c93d0d"
local auth_clock_skew = 300
local response_trans_id
local app_probe_cipher = "+2j4zUClduWNHc2LNm8p6w=="

local function trim(v)
	return (tostring(v or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function read1(path)
	local f = io.open(path, "r")
	if not f then return "" end
	local v = trim(f:read("*l") or "")
	f:close()
	return v
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

local function uci_get(config, section, option)
	local p = io.popen("uci -q get " .. config .. "." .. section .. "." .. option .. " 2>/dev/null")
	if not p then return "" end
	local v = trim(p:read("*l") or "")
	p:close()
	return v
end

local function shell_quote(v)
	return "'" .. tostring(v or ""):gsub("'", "'\\''") .. "'"
end

local function run(cmd)
	local result = os.execute(cmd .. " >/dev/null 2>&1")
	return result == true or result == 0
end

local function normalize_mac(v)
	local mac = trim(v):gsub(":", ""):upper()
	mac = mac:gsub("%-MYOS$", "")
	if mac:match("^%x%x%x%x%x%x%x%x%x%x%x%x$") then
		return mac:gsub("(%x%x)", "%1:"):sub(1, 17)
	end
	return trim(v):upper()
end

local function valid_mac(v)
	return tostring(v or ""):match("^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$") ~= nil
end

local function placeholder_mac(mac)
	mac = normalize_mac(mac)
	return mac == "" or mac == "00:00:00:00:00:00" or mac == "FF:FF:FF:FF:FF:FF" or mac == "00:66:88:00:00:00"
end

local function valid_identity_mac(mac)
	return valid_mac(mac) and not placeholder_mac(mac)
end

local function parse_uci(config, stype)
	local sections, by_name = {}, {}
	local p = io.popen("uci -q show " .. config .. " 2>/dev/null")
	if not p then return sections end
	for line in p:lines() do
		local sec, typ = line:match("^" .. config .. "%.([^%.=]+)=(.+)$")
		if sec and typ:gsub("^'", ""):gsub("'$", "") == stype then
			local item = { section = sec }
			sections[#sections + 1] = item
			by_name[sec] = item
		else
			local s, opt, val = line:match("^" .. config .. "%.([^%.]+)%.([^=]+)=(.-)$")
			if s and by_name[s] then by_name[s][opt] = val:gsub("^'", ""):gsub("'$", "") end
		end
	end
	p:close()
	return sections
end

local function device_id()
	if shared then return shared.device_id() end
	local mac = normalize_mac(uci_get("oem", "board", "id"))
	if not valid_identity_mac(mac) then mac = normalize_mac(identity_get("fac_mac")) end
	if not valid_identity_mac(mac) then mac = normalize_mac(identity_get("mac")) end
	if not valid_identity_mac(mac) then mac = normalize_mac(uci_get("mosquitto", "bridge", "remote_clientid")) end
	if not valid_identity_mac(mac) then return "" end
	return mac:gsub(":", ""):upper()
end

local function device_code()
	if shared then return shared.device_code() end
	local code = identity_get("device_code")
	if code ~= "" then return code end
	return uci_get("oem", "board", "device_code")
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
	return uci_get("myos_nros", "main", "local_enabled") ~= "0"
end

local function request()
	local len = tonumber(os.getenv("CONTENT_LENGTH") or "0") or 0
	if len <= 0 or len > 65536 then return {}, false end
	local raw = io.read(len) or ""
	local ok, data = pcall(cjson.decode, raw)
	if not ok or type(data) ~= "table" then return {}, false end
	if data.data then
		if ok_crypto then
			local plain = app_crypto.decrypt_app_data(data.data)
			if plain and plain ~= "" then
				local ok_plain, decoded = pcall(cjson.decode, plain)
				if ok_plain and type(decoded) == "table" then return decoded, true end
			end
		end
		return {}, true
	end
	return data, false
end

local function truthy(v)
	return v == true or v == 1 or v == "1" or v == "true"
end

local function summary() return shared.summary() end

local function access_info(mac)
	local info = { switch = 1, hostname = "" }
	mac = normalize_mac(mac)
	for _, item in ipairs(parse_uci("access_ctl", "client")) do
		if normalize_mac(item.mac) == mac then
			info.switch = tonumber(item.switch) or 1
			info.hostname = item.hostname or ""
			info.section = item.section
			break
		end
	end
	return info
end

local function firewall_sections(mac)
	local sections, name = {}, "MYOS APP block " .. normalize_mac(mac)
	for _, item in ipairs(parse_uci("firewall", "rule")) do
		if item.name == name then sections[#sections + 1] = item.section end
	end
	return sections
end

local function set_firewall_block(mac, block)
	mac = normalize_mac(mac)
	local sections = firewall_sections(mac)
	if not block and #sections == 0 then return true end
	for i = #sections, 1, -1 do run("uci -q delete " .. shell_quote("firewall." .. sections[i])) end
	if block then
		local p = io.popen("uci -q add firewall rule 2>/dev/null")
		local section = p and trim(p:read("*l") or "") or ""
		if p then p:close() end
		if section == "" then return false end
		local valid = run("uci -q set " .. shell_quote("firewall." .. section .. ".name=MYOS APP block " .. mac))
		valid = run("uci -q set " .. shell_quote("firewall." .. section .. ".src=lan")) and valid
		valid = run("uci -q set " .. shell_quote("firewall." .. section .. ".dest=wan")) and valid
		valid = run("uci -q set " .. shell_quote("firewall." .. section .. ".src_mac=" .. mac)) and valid
		valid = run("uci -q set " .. shell_quote("firewall." .. section .. ".target=REJECT")) and valid
		valid = run("uci -q set " .. shell_quote("firewall." .. section .. ".family=any")) and valid
		if not valid then return false end
	end
	if not run("uci -q commit firewall") or not run("/etc/init.d/firewall reload") then
		return false
	end
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
	local info = access_info(mac)
	local section = info.section
	if not section and not run("touch /etc/config/access_ctl") then return nil end
	if not section then
		local p = io.popen("uci -q add access_ctl client 2>/dev/null")
		section = p and trim(p:read("*l") or "") or ""
		if p then p:close() end
	end
	if section == "" then return nil end
	if not run("uci -q set " .. shell_quote("access_ctl." .. section .. ".mac=" .. mac)) then return nil end
	if switch ~= nil then
		switch = tonumber(switch) == 0 and 0 or 1
		if not run("uci -q set " .. shell_quote("access_ctl." .. section .. ".switch=" .. tostring(switch))) then return nil end
	end
	if hostname ~= nil and not run("uci -q set " .. shell_quote("access_ctl." .. section .. ".hostname=" .. hostname)) then return nil end
	if not run("uci -q commit access_ctl") then return nil end
	if switch ~= nil and not set_firewall_block(mac, switch == 0) then return nil end
	local saved = access_info(mac)
	if switch ~= nil and saved.switch ~= switch then return nil end
	if hostname ~= nil and saved.hostname ~= hostname then return nil end
	return saved
end

local function wifi_info()
	if app_uci then
		pcall(function() app_uci:load("wireless") end)
		local list, index = {}, 0
		app_uci:foreach("wireless", "wifi-iface", function(section)
			local mode = trim(section.mode):lower()
			if mode == "" or mode == "ap" then
				local device = tostring(section.device or "")
				list[#list + 1] = {
					rule_name = "wlan" .. tostring(index),
					ruleName = "wlan" .. tostring(index),
					ssid = section.ssid or "",
					password = section.key or "",
					encryption = section.encryption or "none",
					hidden = section.hidden or "0",
					disabled = section.disabled or
						app_uci:get("wireless", device, "disabled") or "0",
					channel = device ~= "" and
						(app_uci:get("wireless", device, "channel") or "auto") or
						"auto",
					band = index,
					max_link = tonumber(section.maxassoc or section.maxstanum or "0") or 0
				}
				index = index + 1
			end
		end)
		if #list == 0 then list[1] = {} end
		return require("luci.controller.nradio.app").wifi_app_list(list)
	end
	local list, cur = {}, nil
	local f = io.open("/etc/config/wireless", "r")
	if f then
		for line in f:lines() do
			if line:match("^config%s+wifi%-iface") then
				cur = { rule_name = "wlan" .. tostring(#list), ruleName = "wlan" .. tostring(#list), hidden = "0", encryption = "none", channel = "auto", disabled = "0", band = #list, max_link = 0 }
				list[#list + 1] = cur
			elseif cur then
				local k, v = line:match("^%s*option%s+([%w_%-]+)%s+'?(.-)'?%s*$")
				if k == "ssid" then cur.ssid = v elseif k == "key" then cur.password = v elseif k == "encryption" then cur.encryption = v elseif k == "disabled" then cur.disabled = v elseif k == "hidden" then cur.hidden = v end
			end
		end
		f:close()
	end
	if #list == 0 then list[1] = {} end
	return require("luci.controller.nradio.app").wifi_app_list(list)
end

local function valid_wifi_text(value, max_len, required)
	if type(value) ~= "string" and type(value) ~= "number" then return false end
	value = tostring(value)
	if required and value == "" then return false end
	return #value <= max_len and not value:find("[%z\r\n]")
end

local function wifi_item_for_index(list, index)
	local wanted = "wlan" .. tostring(index - 1)
	for _, item in ipairs(list or {}) do
		if type(item) == "table" then
			local rule = trim(item.rule_name or item.ruleName)
			if rule == wanted or rule == "t0" then return item end
		end
	end
	return list and list[index]
end

local function apply_wifi(req)
	if type(req.list) ~= "table" or #req.list == 0 or #req.list > 8 then return false end
	local sections = {}
	for _, section in ipairs(parse_uci("wireless", "wifi-iface")) do
		local mode = trim(section.mode):lower()
		if mode == "" or mode == "ap" then sections[#sections + 1] = section end
	end
	if #sections == 0 then return false end
	for index, section in ipairs(sections) do
		local item = wifi_item_for_index(req.list, index)
		if item then
			local ssid = item.ssid
			local password = item.password
			local encryption = item.encryption
			local channel = item.channel
			if tostring(channel or "") == "0" then channel = nil end
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
			local name = trim(section.section)
			if name == "" then return false end
			if ssid ~= nil then run("uci -q set " .. shell_quote("wireless." .. name .. ".ssid=" .. tostring(ssid))) end
			if encryption ~= nil then run("uci -q set " .. shell_quote("wireless." .. name .. ".encryption=" .. encryption)) end
			if password ~= nil then
				if tostring(password) == "" or encryption == "none" then
					run("uci -q delete " .. shell_quote("wireless." .. name .. ".key"))
				else
					run("uci -q set " .. shell_quote("wireless." .. name .. ".key=" .. tostring(password)))
				end
			end
			if hidden ~= nil then run("uci -q set " .. shell_quote("wireless." .. name .. ".hidden=" .. tostring(hidden))) end
			if disabled ~= nil then run("uci -q set " .. shell_quote("wireless." .. name .. ".disabled=" .. tostring(disabled))) end
			local device = trim(section.device)
			if channel ~= nil and channel ~= "" and device ~= "" then run("uci -q set " .. shell_quote("wireless." .. device .. ".channel=" .. channel)) end
		end
	end
	run("uci -q commit wireless")
	os.execute("(wifi reload) >/dev/null 2>&1 &")
	return true
end

local function dhcp_clients()
	if shared and type(shared.clients) == "function" then return shared.clients() end
	local clients, count = {}, 0
	local seen = {}
	local f = io.open("/tmp/dhcp.leases", "r")
	if f then
		for line in f:lines() do
			local expires, mac, ip, host = line:match("^(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
			if mac and ip then
				mac = normalize_mac(mac)
				local access = access_info(mac)
				if access.hostname ~= "" then host = access.hostname end
				count = count + 1
				clients[#clients + 1] = { mac = mac, ipaddr = ip, ip = ip, hostname = host ~= "*" and host or "", type = "wireless", authed = 0, expires = tonumber(expires) or 0, switch = access.switch, switch_off = access.switch == 0 and 1 or 0, qos = 1, exsit = 1, position = 0 }
				seen[mac] = true
			end
		end
		f:close()
	end
	for _, item in ipairs(parse_uci("access_ctl", "client")) do
		local mac = normalize_mac(item.mac)
		if valid_mac(mac) and not seen[mac] then
			local switch = tonumber(item.switch) or 1
			count = count + 1
			clients[#clients + 1] = { mac = mac, ipaddr = "", ip = "", hostname = item.hostname or "", type = "wireless", authed = 0, expires = 0, switch = switch, switch_off = switch == 0 and 1 or 0, qos = 1, exsit = 0, position = 0 }
		end
	end
	return clients, count
end

local function status_info() return shared.runtime() end
local function runtime_info() return status_info() end
local function speed_result(name) return shared.speed(name) end
local function traffic_result() return shared.traffic() end

local function info_result()
 local m = summary()
 local clients, count = dhcp_clients()
 local version = shared.app_software_version()
 return { runtime=runtime_info(), basic={version=version, api_version=version,
  mac=device_id(), name="N5000", active_modem={}, modem_cnt=0},
  diagnosis={netlink=m.up and 1 or 0}, client=clients, client_count=count,
  client_info={count=count,client=clients}, auth_on=0, wifi=wifi_info(),
  intstatus={mode=m.proto,time=m.uptime}, status=status_info(), speed=speed_result() }
end

local function json_out(data, extra_header, status)
	data = data or {}
	if response_trans_id ~= nil and data.trans_id == nil then data.trans_id = tostring(response_trans_id) end
	io.write("Status: " .. (status or "200 OK") .. "\r\n")
	io.write("Content-Type: application/json\r\n")
	if extra_header then io.write(extra_header .. "\r\n") end
	io.write("Access-Control-Allow-Origin: *\r\n")
	io.write("Access-Control-Allow-Headers: Origin,X-Requested-With,Content-Type,Authorization,X-Access-Token\r\n")
	io.write("Access-Control-Allow-Methods: POST,GET,OPTIONS,PUT\r\n\r\n")
	-- The App expects [[]] for a device without a cellular modem.
	local encoded = data.signal and require("luci.jsonc").stringify(data) or cjson.encode(data)
	io.write(encoded or "{}")
end

local endpoint = (os.getenv("PATH_INFO") or ""):match("^/nradio/app/?(.*)$") or "heartbeat"
if endpoint == "" then endpoint = "heartbeat" end
local req, encrypted = request()
response_trans_id = req.trans_id
local enabled = local_enabled()
local authenticated = enabled and endpoint ~= "auth" and authorized(req)

if not enabled then
	json_out({ code = "1", result = "local app disabled", trans_id = req.trans_id }, nil, "404 Not Found")
	return
elseif endpoint == "auth" then
	if not encrypted or not auth_request_valid(req) then
		json_out({ code = "1", result = "invalid auth", trans_id = req.trans_id })
	else
		local token = issue_auth_token()
		if token == "" then
			json_out({ code = "1", result = "auth unavailable", trans_id = req.trans_id })
		else
			json_out({ code = "0", token = token, timeout = 3600, trans_id = req.trans_id }, "Set-Cookie: " .. app_session.cookie(token))
		end
	end
elseif endpoint == "signal" and not encrypted and not authenticated then
	response_trans_id = nil
	json_out({ data = app_probe_cipher })
elseif endpoint == "status" and not encrypted and not authenticated then
	json_out({ code = 0, result = { global = { net_type = shared.app_network_mode() } } })
elseif not authenticated then
	response_trans_id = nil
	json_out({ code = "1" })
elseif endpoint == "heartbeat" then
	json_out({ code = "0" })
elseif endpoint == "info" then
	json_out({ code = 0, result = info_result() })
elseif endpoint == "signal" then
	json_out({ code = "0", mac = device_id(), signal = { {} } })
elseif endpoint == "status" then
	json_out({ code = 0, result = status_info() })
elseif endpoint == "speed" then
	json_out({ code = "0", result = speed_result(req.name), trans_id = req.trans_id })
elseif endpoint == "traffic" then
	json_out({ code = "0", result = traffic_result() })
elseif endpoint == "wifi" or endpoint == "wireless" then
	if req.list ~= nil then
		if trim(req.device_code) ~= device_code() or device_code() == "" or not apply_wifi(req) then json_out({ code = "1", result = "invalid wifi config", trans_id = req.trans_id }); return end
		json_out({ code = "0", result = { list = wifi_info() }, trans_id = req.trans_id })
	else
		json_out({ code = "0", result = { list = wifi_info() } })
	end
elseif endpoint == "client" or endpoint == "terminal" then
	if req.client and (req.switch ~= nil or req.hostname ~= nil) then
		if trim(req.device_code) ~= device_code() or device_code() == "" then json_out({ code = "1", result = "invalid device identity", trans_id = req.trans_id }); return end
		local mac = normalize_mac(req.client)
		local info = set_client_access(mac, req.switch, req.hostname)
		if not info then json_out({ code = "1", result = "invalid client" }); return end
		json_out({ code = "0", result = { client = { client = mac, mac = mac, switch = info.switch, hostname = info.hostname } }, auth_on = 0 })
		return
	end
	local clients, count = dhcp_clients()
	json_out({ code = "0", result = { count = count, client = clients }, auth_on = 0 })
elseif endpoint == "combo" then
	if req.combo ~= nil then
		json_out({ code = "2", result = "unsupported on MYOS" })
	else
		json_out({ code = "0", result = runtime_info() })
	end
elseif endpoint == "runtime" then
	json_out({ code = "0", result = runtime_info() })
	elseif endpoint == "led" then
		if req.led ~= nil or req.schedule ~= nil or req.start_time ~= nil or req.end_time ~= nil then
			if trim(req.device_code) ~= device_code() or device_code() == "" then
				json_out({ code = 2, message = "Invalid device identity", trans_id = req.trans_id })
				return
			end
		end
		json_out(shared.led(req))
	elseif endpoint == "apn" or endpoint == "sync" or endpoint == "lan" then
		json_out({ code = "1", result = "unsupported on MYOS", trans_id = req.trans_id })
elseif endpoint == "wifiauth" then
	json_out({ wifiauth = -1, trans_id = req.trans_id })
elseif endpoint == "cmd" and req.cmd == "service" and req.name == "dropbear" then
	json_out(app_session.ssh_service(req, device_code()))
elseif endpoint == "cmd" and req.cmd == "reboot" and device_code() ~= "" and req.device_code and tostring(req.device_code) == device_code() then
	json_out({ code = "0", result = {} })
	os.execute("(sleep 1; reboot) >/dev/null 2>&1 &")
elseif endpoint == "password" or endpoint == "cmd" then
	json_out({ code = "1", result = "disabled on MYOS" })
else
	json_out({ code = "1", result = "unsupported on MYOS", trans_id = req.trans_id })
end
