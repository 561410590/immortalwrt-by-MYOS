local M = {}
local util = require "luci.util"
local fs = require "nixio.fs"
local nixio = require "nixio"
local uci_model = require "luci.model.uci"

local function success(result)
	return { code = "0", result = result }
end

local function failure(message)
	return { code = "1", message = message }
end

local function native_config(name)
	local path = "/etc/config/" .. name
	if fs.access(path) then return true end
	local file = nixio.open(path, nixio.open_flags("wronly", "creat", "excl"), "600")
	if not file then return fs.access(path) end
	file:close()
	return true
end

local function context(uci, req)
	local index = tonumber(req.index or 1)
	if not index or index % 1 ~= 0 or index < 1 then return end
	local native = {}
	uci:foreach("network", "interface", function(s)
		if s.proto == "wwan" and (not s.share_channel or s.share_channel == s[".name"]) then
			native[#native + 1] = s[".name"]
		end
	end)
	table.sort(native)
	if native[index] then
		local iface = native[index]
		local selector = iface == "cpe" and "sim" or "sim" .. (iface:match("(%d+)$") or (index - 1))
		local current = tonumber((uci:get("cpesel", selector, "cur"))) or 1
		local slot = tonumber(req.sim or current)
		if not slot or slot < 1 or slot % 1 ~= 0 then return end
		return { index=index, iface=iface, slot=slot, current=current, package="cpecfg", section=iface.."sim"..slot }
	end
	local devices = {}
	uci:foreach("qmodem", "modem-device", function(s)
		if not s.state or s.state == "" or s.state == "enabled" then devices[#devices+1] = s end
	end)
	local device = devices[index]
	if not device then return end
	local current = tonumber(uci:get("cpesel", "sim", "cur")) or 1
	local slot = tonumber(req.sim or current)
	if slot ~= 1 and slot ~= 2 then return end
	return { index=index, slot=slot, current=current, package="qmodem", section=device[".name"],
		suffix=slot == 2 and "2" or "" }
end

function M.apn_list()
	local profiles = {}
	uci_model.cursor():foreach("apn", "rule", function(profile)
		profile[".anonymous"], profile[".type"], profile[".index"] = nil, nil, nil
		profiles[#profiles + 1] = profile
	end)
	return profiles
end

local function profile_name(name)
	return type(name) == "string" and #name <= 64 and name:match("^[%w_]+$")
end

local function safe_at_text(value)
	return type(value) == "string" and not value:find('[%c"]')
end

local function profile_fields(req)
	local apn = req.apn or ""
	local username, password = req.username or "", req.password or ""
	local auth = tostring(req.auth or "0")
	local pdp = type(req.pdptype) == "string" and req.pdptype:lower() or "ipv4v6"
	if type(apn) ~= "string" or not apn:match("^[%w_.%-]*$") or
		not safe_at_text(username) or not safe_at_text(password) or
		not auth:match("^[0-3]$") or not ({ipv4=true,ipv6=true,ipv4v6=true,ip=true})[pdp] then
		return nil
	end
	return { apn=apn, username=username, password=password, auth=auth,
		pdptype=pdp, custom_apn=tostring(req.enabled or req.custom_apn or "1") == "0" and "0" or "1" }
end

local function restart_data(ctx)
	if ctx.package == "qmodem" then
		return util.ubus("qmodem", "modem_redial", {config_section=ctx.section}) ~= nil
	end
	return require("luci.sys").call("/sbin/ifup " .. util.shellquote(ctx.iface) .. " >/dev/null 2>&1") == 0
end

local function selected_profile(uci, ctx)
	local key = ctx.package == "qmodem" and ("myos_apn_profile" .. ctx.suffix) or "apn_cfg"
	return uci:get(ctx.package, ctx.section, key) or ""
end

local function select_profile(uci, ctx, name, profile)
	local qmodem = ctx.package == "qmodem"
	if not qmodem and not uci:get(ctx.package, ctx.section) then
		if not native_config(ctx.package) then return false end
		uci:set(ctx.package, ctx.section, "cpesim")
	end
	profile = profile or { apn="", username="", password="", auth="0", pdptype="ipv4v6", custom_apn="0" }
	if qmodem then
		for _, key in ipairs({"apn", "username", "password", "auth"}) do
			uci:set("qmodem", ctx.section, key .. ctx.suffix, profile[key] or "")
		end
		uci:set("qmodem", ctx.section, "myos_apn_profile" .. ctx.suffix, name)
		uci:set("qmodem", ctx.section, "pdp_type", ({ipv4="IP",ip="IP",ipv6="IPV6",ipv4v6="IPV4V6"})[profile.pdptype] or "IPV4V6")
	else
		for _, key in ipairs({"apn", "username", "password", "auth", "pdptype", "custom_apn"}) do
			uci:set(ctx.package, ctx.section, key, profile[key] or "")
		end
		uci:set(ctx.package, ctx.section, "apn_cfg", name)
	end
	return uci:commit(ctx.package) ~= false
end

function M.apn(req)
	local uci = uci_model.cursor()
	local ctx = context(uci, req)
	if not ctx then return failure("Cellular modem unavailable") end
	local action, name = tonumber(req.action), req.profile or "app"
	local active = selected_profile(uci, ctx)
	local selected = req.sim_profile
	if req.action ~= nil and action ~= 0 and action ~= 1 then return failure("Invalid APN action") end
	if not profile_name(name) or (selected ~= nil and selected ~= "" and not profile_name(selected)) then
		return failure("Invalid APN profile")
	end
	if action == 1 then
		local profile = profile_fields(req)
		if not profile then return failure("Invalid APN settings") end
		if not native_config("apn") then return failure("APN configuration unavailable") end
		uci:set("apn", name, "rule")
		for field,value in pairs(profile) do uci:set("apn", name, field, value) end
		if not uci:commit("apn") then return failure("Failed to save APN profile") end
		if active == name and selected == nil then selected = name end
	elseif action == 0 then
		if uci:get("apn", name) then
			uci:delete("apn", name)
			if not uci:commit("apn") then return failure("Failed to remove APN profile") end
		end
		if active == name then selected = "" end
	end
	if selected ~= nil then
		local profile = selected ~= "" and uci:get_all("apn", selected) or nil
		if selected ~= "" and (not profile or profile[".type"] ~= "rule") then
			return failure("APN profile not found")
		end
		if not select_profile(uci, ctx, selected, profile) then return failure("Failed to apply APN") end
		if ctx.slot == ctx.current and not restart_data(ctx) then return failure("APN saved but could not restart the modem") end
		active = selected
	end
	return success({ index=ctx.index, action=action, sim=ctx.slot, apn_cfg=active })
end

function M.apn_info(items)
	local uci = uci_model.cursor()
	for index,item in ipairs(items or {}) do
		local ctx = context(uci, {index=index, sim=item.cur})
		if ctx then
			local selected = selected_profile(uci, ctx)
			local profile = selected ~= "" and uci:get_all("apn", selected) or nil
			if profile then
				item.apn_cfg = selected
				item.apn = { enabled=profile.custom_apn, name=profile.apn, username=profile.username,
					password=profile.password, auth=profile.auth, pdptype=profile.pdptype }
			end
		end
	end
	return items
end

local function native_radio_state(uci, ctx)
	local cfg = uci:get_all("cpecfg", ctx.section) or {}
	return {
		mode = cfg.mode or "auto",
		band = {status=cfg.custom_freq or "0", freq=cfg.freq or ""},
		earfcn = {
			{MODE="NR", status=cfg.custom_earfcn5 or "0", EARFCN=cfg.earfcn5 or "", PCI=cfg.pci5 or "", BAND=cfg.band5 or "", earfcn5_mode=cfg.earfcn5_mode or ""},
			{MODE="LTE", status=cfg.custom_earfcn4 or "0", EARFCN=cfg.earfcn4 or "", PCI=cfg.pci4 or "", BAND=cfg.band4 or "", earfcn4_mode=cfg.earfcn4_mode or ""}
		}
	}
end

local function qmodem_radio_state(ctx)
	local request = {config_section=ctx.section}
	local prefer = util.ubus("qmodem", "get_network_prefer", request)
	local bands = util.ubus("qmodem", "get_lockband", request)
	local cells = util.ubus("qmodem", "get_neighborcell", request)
	prefer = type(prefer) == "table" and prefer.network_prefer or nil
	bands = type(bands) == "table" and bands.lockband or nil
	cells = type(cells) == "table" and cells.neighborcell or nil
	if type(prefer) ~= "table" and type(bands) ~= "table" and type(cells) ~= "table" then return end
	prefer, bands = prefer or {}, bands or {}
	local lte, nr = tostring(prefer["4G"]) == "1", tostring(prefer["5G"]) == "1"
	local freq = {}
	for _, rat in ipairs({"NR", "LTE"}) do
		local item = bands[rat]
		local lock = type(item) == "table" and item.lock_band
		local available = type(item) == "table" and item.available_band
		if type(lock) == "table" and #lock > 0 and (type(available) ~= "table" or #lock < #available) then
			freq[#freq+1] = rat:lower() .. "-" .. table.concat(lock, ":")
		end
	end
	local radio = {mode=nr and (lte and "auto" or "sa_only") or (lte and "lte" or "auto"),
		band={status=#freq > 0 and "1" or "0", freq=table.concat(freq, ",")}, earfcn={}}
	local detail = type(cells) == "table" and cells.lockcell_status or nil
	if type(detail) == "table" then
		for _, rat in ipairs({"NR", "LTE"}) do
			if detail[rat] ~= nil then
				radio.earfcn[#radio.earfcn+1] = { MODE=rat,
					status=detail[rat] == "locked" and "1" or "0", EARFCN=detail[rat.."_Freq"] or "",
					PCI=detail[rat.."_PCI"] or "", BAND=detail[rat.."_Band"] or "" }
			end
		end
	end
	return radio
end

local function native_radio_settings(req)
	local settings = {}
	if req.mode ~= nil then
		if type(req.mode) ~= "string" or not ({auto=true,sa_only=true,nsa=true,lte=true})[req.mode] then return end
		settings.mode = req.mode
	end
	if req.band ~= nil then
		if type(req.band) ~= "table" then return end
		local freq, enabled = req.band.freq or "", tostring(req.band.status or "0")
		if enabled ~= "0" and enabled ~= "1" then return end
		if type(freq) ~= "string" or #freq > 256 or (enabled == "1" and freq == "") then return end
		for item in freq:gmatch("[^,]+") do
			if not item:match("^nr%-%d[%d:]*$") and not item:match("^lte%-%d[%d:]*$") then return end
		end
		settings.freq, settings.custom_freq = freq, enabled
		settings.custom_earfcn4, settings.custom_earfcn5 = "0", "0"
		if enabled == "1" then settings.earfreq_mode = "band" end
	end
	local function cell(entry, enabled)
		if type(entry) ~= "table" or type(entry.MODE) ~= "string" then return false end
		local number = entry.MODE == "LTE" and "4" or entry.MODE:match("^NR") and "5" or nil
		if not number then return false end
		local f, pci, band = tostring(entry.EARFCN or ""), tostring(entry.PCI or ""), tostring(entry.BAND or "")
		if not f:match("^%d*$") or not pci:match("^%d*$") or not band:match("^%d*$") or (enabled and f == "") then
			return false
		end
		settings["custom_earfcn"..number] = enabled and "1" or "0"
		settings["earfcn"..number], settings["pci"..number], settings["band"..number] = f, pci, band
		if enabled then settings.earfreq_mode, settings.custom_freq = "earfcn"..number, "0" end
		return true
	end
	if req.earfcns ~= nil then
		if type(req.earfcns) ~= "table" then return end
		for _,entry in ipairs(req.earfcns) do
			if type(entry) ~= "table" or not cell(entry, tostring(entry.status) == "1") then return end
		end
	elseif req.earfcn ~= nil and (tonumber(req.action) == 0 or tonumber(req.action) == 1) then
		if not cell(req.earfcn, tonumber(req.action) == 1) then return end
	end
	return settings
end

local function qmodem_radio_write(ctx, req)
	if ctx.slot ~= ctx.current or req.adv ~= nil then
		return nil, "QModem applies radio settings to the active SIM"
	end
	if not native_radio_settings(req) then return nil, "Invalid radio settings" end
	local calls = {}
	if req.mode ~= nil then
		local modes = {auto={"3G","4G","5G"}, lte={"4G"}, sa_only={"5G"}, nsa={"4G","5G"}}
		calls[#calls+1] = {"set_network_prefer", modes[req.mode]}
	end
	if req.band ~= nil then
		local freq = req.band.freq or ""
		if tostring(req.band.status or "0") == "1" then
			for rat, list in freq:gmatch("([%a]+)%-([%d:]+)") do
				calls[#calls+1] = {"set_lockband", {band_class=rat:upper(), lock_band=list:gsub(":", ",")}}
			end
		else
			local output = util.ubus("qmodem", "get_lockband", {config_section=ctx.section}) or {}
			for _,rat in ipairs({"NR","LTE"}) do
				local band = output.lockband and output.lockband[rat]
				if band and type(band.available_band) == "table" and #band.available_band > 0 then
					calls[#calls+1] = {"set_lockband", {band_class=rat, lock_band=table.concat(band.available_band, ",")}}
				end
			end
		end
	end
	local cells = req.earfcns or (req.earfcn and {req.earfcn}) or {}
	for _, item in ipairs(cells) do
		local active
		if req.earfcns then active = tostring(item.status) == "1"
		else active = tonumber(req.action) == 1 end
		local rat = type(item.MODE) == "string" and item.MODE:match("^NR") and 1 or 0
		local arfcn, pci, band = tostring(item.EARFCN or ""), tostring(item.PCI or ""), tostring(item.BAND or "")
		if active and (pci == "" or (rat == 1 and band == "")) then
			return nil, "QModem cell locking requires PCI and the 5G band"
		end
		local scs = ({["15"]=0,["30"]=1,["60"]=2,["120"]=3,["240"]=4})[tostring(item.SCS or item.scs)] or 1
		calls[#calls+1] = {"set_neighborcell", {rat=rat, arfcn=active and arfcn or "", pci=active and pci or "",
			band=active and band or "", scs=scs}}
	end
	if #calls == 0 then return nil, "No QModem radio setting to apply" end
	for _,call in ipairs(calls) do
		local output = util.ubus("qmodem", call[1], {config_section=ctx.section, params=call[2]})
		if type(output) ~= "table" then return nil, "QModem did not return a radio response" end
		local data = type(output.result) == "table" and output.result or output
		local reply = data[call[1] == "set_neighborcell" and "setlockcell" or call[1]]
		if type(reply) == "string" and reply:find("OK",1,true) and not reply:find("ERROR",1,true) then
			-- This backend returned the modem acknowledgement.
		elseif call[1] == "set_network_prefer" and reply == nil then
			local readback = util.ubus("qmodem", "get_network_prefer", {config_section=ctx.section})
			local current = type(readback) == "table" and readback.network_prefer
			if type(current) ~= "table" then return nil, "QModem network mode not confirmed" end
			local expected = {}
			for _,rat in ipairs(call[2]) do expected[rat] = true end
			for _,rat in ipairs({"4G","5G","3G"}) do
				if (rat ~= "3G" or current[rat] ~= nil) and
					(tostring(current[rat]) == "1") ~= (expected[rat] == true) then
					return nil, "QModem network mode readback differs"
				end
			end
		else
			return nil, "QModem could not apply " .. call[1]
		end
	end
	return true
end

function M.radio(req)
	local uci = uci_model.cursor()
	local ctx = context(uci, req)
	if not ctx then return failure("Cellular modem unavailable") end
	local action = tonumber(req.action) or 2
	local write = action ~= 2 or req.mode ~= nil or req.band ~= nil or req.earfcn ~= nil or
		req.earfcns ~= nil or req.adv ~= nil or req.earfreq_mode ~= nil
	local result
	if ctx.package == "qmodem" then
		if write then
			local ok, message = qmodem_radio_write(ctx, req)
			if not ok then return failure(message) end
		end
		result = qmodem_radio_state(ctx)
	else
		if write then
			if req.adv ~= nil then return failure("Advanced radio options are not available on this interface") end
			local settings = native_radio_settings(req)
			if not settings or not next(settings) then return failure("Invalid radio settings") end
			if not uci:get("cpecfg", ctx.section) then
				if not native_config("cpecfg") then return failure("Cellular configuration unavailable") end
				uci:set("cpecfg", ctx.section, "cpesim")
			end
			for key,value in pairs(settings) do uci:set("cpecfg", ctx.section, key, value) end
			if not uci:commit("cpecfg") then return failure("Failed to save radio settings") end
			if ctx.slot == ctx.current and not restart_data(ctx) then return failure("Radio settings saved but could not restart the modem") end
		end
		result = native_radio_state(uci, ctx)
	end
	if not result then return failure("Radio status unavailable") end
	result.index, result.action, result.sim = ctx.index, action, ctx.slot
	return success(result)
end

function M.password(req)
	local psw = req.password
	if type(psw) ~= "string" or #psw < 5 or #psw > 64 or psw:find("[%z\r\n]") then return {code=1} end
	local user = require("luci.sys").user
	if user.checkpasswd("root", psw) then return {code=2} end
	local rc = user.setpasswd("root", psw)
	return { code=(rc == 0 or rc == true) and 0 or 1 }
end

function M.lan(req)
	if type(req.ip) ~= "string" then return failure("Invalid LAN address") end
	local ok, addr = pcall(require("luci.ip").IPv4, req.ip)
	if not ok or not addr or addr:prefix() ~= 32 then return failure("Invalid LAN address") end
	local uci = uci_model.cursor()
	if uci:get("network", "lan", "ipaddr") == req.ip then return success() end
	uci:set("network", "lan", "ipaddr", req.ip)
	if not uci:commit("network") then return failure("Failed to save LAN address") end
	os.execute("(sleep 2; /etc/init.d/network reload; /etc/init.d/dnsmasq reload) >/dev/null 2>&1 &")
	return success()
end

function M.combo(req)
	local result = util.ubus("combo", "update", { trans_id=req.trans_id, from="app", combo=req.combo })
	return result or failure("Combo service unavailable")
end

return M
