module("luci.controller.nradio.app", package.seeall)
local uci = require "luci.model.uci".cursor()
local util = require "luci.util"

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

function wifi_app_list(list)
	local first, second = list[1], list[2]
	if #list == 2 and first.ssid == second.ssid and
	   first.password == second.password and first.encryption == second.encryption and
	   first.hidden == second.hidden and first.disabled == second.disabled then
		first.rule_name, first.ruleName, first.channel = "t0", "t0", "0"
		return { first }
	end
	return list
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
	return wifi_app_list(list)
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
		if type(item) == "table" then
			local rule = trim(item.rule_name or item.ruleName)
			if rule == wanted or rule == "t0" then return item end
		end
	end
	return list and list[index]
end

function apply_wifi(req)
	uci:load("wireless")
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
	if not block and #firewall_sections(mac) == 0 then return true end
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

function set_client_access(mac, switch, hostname)
	uci:load("access_ctl")
	uci:load("firewall")
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

function index() end
