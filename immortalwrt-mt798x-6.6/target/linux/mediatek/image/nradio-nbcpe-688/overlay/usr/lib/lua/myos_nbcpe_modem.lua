-- NBCPE operations use the original WWAN/cpetools owner. Status stays cached.
local util = require "luci.util"
local ok_fs, fs = pcall(require, "nixio.fs")
local ok_json, json = pcall(require, "cjson.safe")
if not ok_json then json = require "cjson" end
local M = {}
local CLI = "/usr/bin/cpetools.sh"
local DECODER = "/usr/bin/pdu_decoder-AK68"

local function quote(value)
	return "'" .. tostring(value or ""):gsub("'", "'\\''") .. "'"
end

local function read(path)
	local file = io.open(path, "r")
	if not file then return "" end
	local text = file:read("*a") or ""
	file:close()
	return text
end

local function ok(response)
	return type(response) == "string" and
		("\n" .. response:gsub("\r", "") .. "\n"):find("\nOK\n", 1, true) ~= nil
end

function M.supported()
	return ok_fs and fs.access(CLI) and fs.access(DECODER) and true or false
end

function M.at(command, seconds)
	if type(command) ~= "string" or #command > 4096 or command:find("%z") then return "" end
	return util.exec("/usr/bin/timeout " .. tostring(tonumber(seconds) or 25) ..
		" " .. CLI .. " -i cpe -t 0 -c " .. quote(command) .. " 2>/dev/null") or ""
end

-- Decode the same native output used by smsc-AK68.sh, retaining physical indexes.
local function decode(pdu, index, stat)
	local text = util.exec("printf '%s\\n' " .. quote(pdu) .. " | " .. DECODER .. " 2>/dev/null") or ""
	text = text:gsub("\r", "")
	local sender = text:match("From:([^\n]+)")
	local date, body = text:match("Date/Time:([^\n]+)\n(.*)")
	if not sender or not body then return nil end
	local month, day, year, hour, minute, second = date:match("(%d+)/(%d+)/(%d+)%s+(%d+):(%d+):(%d+)")
	local timestamp = 0
	if year then
		year = tonumber(year)
		if year < 100 then year = year + (year < 70 and 2000 or 1900) end
		timestamp = os.time({ year = year, month = tonumber(month), day = tonumber(day),
			hour = tonumber(hour), min = tonumber(minute), sec = tonumber(second) }) or 0
	end
	local reference = text:match("Reference number:%s*(%d+)")
	local part, total = text:match("SMS segment%s+(%d+)%s+of%s+(%d+)")
	local content = {}
	for line in (body .. "\n"):gmatch("([^\n]*)\n") do
		if not line:match("^Textlen=") and not line:match("^Reference number:") and
		   not line:match("^SMS segment ") then content[#content + 1] = line end
	end
	while content[#content] == "" do table.remove(content) end
	return { index = index, sim_indices = { index }, contact = sender,
		content = table.concat(content, "\n"), timestamp = timestamp, stat = stat == 0 and 0 or 1,
		multi_sms_id = tonumber(reference) or 0, multi_sms_index = tonumber(part) or 0,
		multi_sms_max = tonumber(total) or 0, merged = true }
end

function M.read_raw()
	if not M.supported() then return nil end
	local response = M.at("AT+CMGL=4", 25)
	if not ok(response) then return nil end
	local messages, pending, count = {}, nil, 0
	for line in (response:gsub("\r", "") .. "\n"):gmatch("([^\n]*)\n") do
		local index, stat = line:match("^%+CMGL:%s*(%d+),%s*(%d+)")
		if index then
			pending = { tonumber(index), tonumber(stat) }
		elseif pending and line:match("^%x+$") and #line % 2 == 0 then
			local message = decode(line, pending[1], pending[2])
			-- Never report a successful empty mailbox when a PDU failed to decode.
			if not message then return nil end
			messages[#messages + 1], pending, count = message, nil, count + 1
		end
	end
	if pending then return nil end
	local groups, combined = {}, {}
	for _, message in ipairs(messages) do
		if message.multi_sms_max > 1 and message.multi_sms_index > 0 then
			local key = table.concat({ message.contact, message.multi_sms_id, message.multi_sms_max }, ":")
			groups[key] = groups[key] or {}
			groups[key][#groups[key] + 1] = message
		else combined[#combined + 1] = message end
	end
	for _, group in pairs(groups) do
		table.sort(group, function(left, right) return left.multi_sms_index < right.multi_sms_index end)
		local complete = #group == group[1].multi_sms_max
		for part, message in ipairs(group) do if message.multi_sms_index ~= part then complete = false end end
		if complete then
			local merged, content = group[1], {}
			merged.sim_indices = {}
			for _, message in ipairs(group) do
				content[#content + 1] = message.content
				merged.sim_indices[#merged.sim_indices + 1] = message.index
				merged.index = math.min(merged.index, message.index)
				merged.stat = math.min(merged.stat, message.stat)
			end
			merged.content, merged.multi_sms_index = table.concat(content), 0
			combined[#combined + 1] = merged
		else for _, message in ipairs(group) do combined[#combined + 1] = message end end
	end
	local used, total = M.at("AT+CPMS?", 8):match('%+CPMS:%s*"[^"]*",(%d+),(%d+)')
	return { msg = combined, _myos_direct = true, sms_capabilities = { mem1 = "SM",
		SM = { used = tonumber(used) or count, total = tonumber(total) or count } } }
end

-- Original pdul-AK68 format: decimal TPDU length, separator, hexadecimal PDU.
-- Segment independently; the original UI concatenates multiple output lines.
local function encode(phone, message)
	local digits = phone:gsub("^%+", "")
	if #digits == 11 and digits:sub(1, 1) == "1" then digits = "86" .. digits end
	if not digits:match("^%d+$") or #digits > 20 or #digits == 0 then return nil end
	local encoded, units, index = {}, {}, 1
	while index <= #message do
		local first = message:byte(index)
		local length = first < 128 and 1 or first >= 194 and first < 224 and 2 or
			first < 240 and first >= 224 and 3 or first >= 240 and first <= 244 and 4 or 0
		if length == 0 or index + length - 1 > #message then return nil end
		local value = first % (length == 1 and 128 or 2 ^ (7 - length))
		for offset = 1, length - 1 do
			local byte = message:byte(index + offset)
			if byte < 128 or byte > 191 then return nil end
			value = value * 64 + byte - 128
		end
		if (length == 2 and value < 128) or (length == 3 and value < 2048) or
		   (length == 4 and value < 65536) or value > 1114111 or
		   (value >= 55296 and value <= 57343) or value == 0 then return nil end
		if value > 65535 then
			value = value - 65536
			units[#units + 1] = string.format("%04X%04X", 55296 + math.floor(value / 1024), 56320 + value % 1024)
		else units[#units + 1] = string.format("%04X", value) end
		index = index + length
	end
	if #units == 0 then return nil end
	local segments, current, size = {}, {}, 0
	for _, unit in ipairs(units) do
		if size + #unit > 240 then segments[#segments + 1] = table.concat(current); current, size = {}, 0 end
		current[#current + 1], size = unit, size + #unit
	end
	segments[#segments + 1] = table.concat(current)
	if #segments > 255 then return nil end
	local padded = digits .. (#digits % 2 == 1 and "F" or "")
	local address = padded:gsub("(.)(.)", "%2%1")
	local reference = os.time() % 256
	for part, segment in ipairs(segments) do
		local header = #segments > 1 and string.format("050003%02X%02X%02X", reference, #segments, part) or ""
		local tpdu = (#segments > 1 and "41" or "01") .. "00" .. string.format("%02X91", #digits) ..
			address .. "0008" .. string.format("%02X", (#header + #segment) / 2) .. header .. segment
		encoded[#encoded + 1] = tostring(#tpdu / 2) .. "\\r00" .. tpdu
	end
	return encoded
end

function M.send(phone, message)
	if not M.supported() or type(phone) ~= "string" or type(message) ~= "string" or #message > 2048 then return false end
	local segments = encode(phone, message)
	if not segments then return false end
	for _, pdu in ipairs(segments) do
		local length, payload = pdu:match("^(%d+)\\r(%x+)$")
		if not length then return false end
		-- Official sms_send chooses the firmware separator and performs CMGW/CMSS.
		local raw = util.exec("/usr/bin/timeout 90 " .. CLI ..
			" -i cpe -c sms_send " .. quote(length) .. " " .. quote(payload) .. " 2>/dev/null") or ""
		local decoded, result = pcall(json.decode, raw)
		if not decoded or type(result) ~= "table" or tonumber(result.code) ~= 0 then return false end
	end
	return true
end

local function settings()
	return require("luci.model.uci").cursor()
end

local function is_rm520(config)
	local model = config:get("network", "cpe", "odu_model") or read("/tmp/modconf-AK68.conf")
	return model:find("RM520", 1, true) ~= nil
end

local function current_slot(config)
	local slot = tonumber(config:get("cpesel", "sim", "cur")) or 1
	return slot == 2 and 2 or 1
end

local function execute(command)
	local result = os.execute(command .. " >/tmp/myos-nbcpe-modem.log 2>&1")
	return result == true or result == 0
end

function M.sms_enabled()
	-- RM520's native UI has no IMS/SMS switch; APP access is its only gate.
	local config = settings()
	if is_rm520(config) then return true end
	local slot = current_slot(config)
	-- Official force_ims applies to physical USIM only; embedded SIM uses APP gate.
	if slot == 2 then return true end
	return config:get("cpecfg", "cpesim" .. slot, "force_ims") == "1"
end

function M.set_sms_enabled(value)
	if value ~= "0" and value ~= "1" then return false end
	local config = settings()
	if is_rm520(config) then return true end
	local slot = current_slot(config)
	if slot == 2 then return true end
	local section = "cpesim" .. slot
	if (config:get("cpecfg", section, "force_ims") or "0") == value then return true end
	config:set("cpecfg", section, "cpesim")
	if not config:set("cpecfg", section, "force_ims", value) or not config:commit("cpecfg") then return false end
	return execute("/sbin/ifup cpe")
end

function M.apply_sim(slot, mode)
	if slot ~= 1 and slot ~= 2 then return false end
	if mode ~= 1 then return false end
	if not M.supported() then return false end
	local config = settings()
	local changed = current_slot(config) ~= slot
	config:set("cpesel", "sim", "cpesel")
	config:set("cpesel", "sim", "max", "2")
	config:set("cpesel", "sim", "cur", tostring(slot))
	config:set("cpesel", "sim", "mode", tostring(mode))
	if not config:commit("cpesel") then return false end
	if not changed then return true end
	return execute(CLI .. " -i cpe -s " .. tostring(slot))
end

-- Read the same settings that the active official WWAN owner uses. A single
-- band control cannot represent a multi-band list, so an unchanged control
-- must preserve that list when another setting is submitted.
function M.ui_values()
	local config = settings()
	local slot = current_slot(config)
	local section = "cpesim" .. slot
	local mode = config:get("cpecfg", section, "mode") or "auto"
	local values = {
		simsel = tostring(slot - 1), enable = config:get("network", "cpe", "disabled") == "1" and "0" or "1",
		smode = mode == "lte" and "1" or (mode == "auto" and "0" or "2"),
		nrmode = mode:find("nsa", 1, true) and "2" or "1",
		datarroaming = config:get("cpecfg", section, "roaming") or "0",
		apnconfig = config:get("cpecfg", section, "custom_apn") == "1" and
			(config:get("cpecfg", section, "apn") or "") or "",
		bandlist_lte = "0", bandlist_sa = "0", bandlist_nsa = "0",
		mobility = config:get("cpecfg", section, "mobility") or "2",
		freqlock = "0", earfcn = "", cellid = "", pincode = "",
		smsen = config:get("cpecfg", section, "force_ims") == "1" and "1" or "0",
	}
	if config:get("cpecfg", section, "custom_freq") == "1" then
		for kind, band in (config:get("cpecfg", section, "freq") or ""):gmatch("([%a]+)%-([^,]+)") do
			if band:match("^%d+$") then
				local key = kind == "lte" and "lte" or kind == "nsa" and "nsa" or "sa"
				values["bandlist_" .. key] = band
			end
		end
	end
	local suffix = values.smode == "1" and "4" or "5"
	local lock_mode = config:get("cpecfg", section, "earfreq_mode")
	if config:get("cpecfg", section, "custom_earfcn" .. suffix) == "1" and
	   (not lock_mode or lock_mode == "" or lock_mode == "earfcn" .. suffix) then
		values.freqlock = "1"
		values.earfcn = config:get("cpecfg", section, "earfcn" .. suffix) or ""
		values.cellid = config:get("cpecfg", section, "pci" .. suffix) or ""
		values[values.smode == "1" and "bandlist_lte" or "bandlist_sa"] =
			config:get("cpecfg", section, "band" .. suffix) or "0"
	end
	return values
end

-- Keep the native AK68 form, but persist operations for the official owner.
-- Invoked after CBI commits its form; never launch the old whole-modem scripts.
function M.apply_ui(baseline)
	local config = settings()
	local function old(name) return config:get("modem-AK68", "@ndis[0]", name) end
	local function edited(name, value) return not baseline or tostring(value or "") ~= tostring(baseline[name] or "") end
	local slot = tonumber(old("simsel") or tostring(current_slot(config) - 1))
	if slot ~= 0 and slot ~= 1 then return false, "Invalid SIM slot" end
	slot = slot + 1
	local rm = is_rm520(config)
	local mode = old("smode") or "0"
	local nrmode = old("nrmode") or (baseline and baseline.nrmode) or "0"
	if mode ~= "0" and mode ~= "1" and mode ~= "2" then return false, "Invalid network mode" end
	if nrmode ~= "0" and nrmode ~= "1" and nrmode ~= "2" then return false, "Invalid NR mode" end
	local native_mode = mode == "1" and "lte" or mode == "2" and "sa_only" or "auto"
	if rm and mode == "2" and nrmode == "2" then native_mode = "nsa_only" end
	local enabled, roaming = old("enable") or "1", old("datarroaming") or "0"
	if (enabled ~= "0" and enabled ~= "1") or (roaming ~= "0" and roaming ~= "1") then
		return false, "Invalid modem flag"
	end
	local apn, pin = old("apnconfig") or "", old("pincode") or ""
	if #apn > 100 or apn:find("[%c]") then return false, "Invalid APN" end
	if pin ~= "" and (not pin:match("^%d+$") or #pin < 4 or #pin > 8) then return false, "Invalid PIN" end
	local bands = {}
	for _, name in ipairs({ "lte", "sa", "nsa" }) do
		local value = old("bandlist_" .. name) or (baseline and baseline["bandlist_" .. name]) or "0"
		if not value:match("^%d+$") or tonumber(value) > 512 then return false, "Invalid band" end
		bands[name] = value
	end
	local lock = old("freqlock") == "1"
	local mobility = old("mobility") or (baseline and baseline.mobility) or "2"
	if mobility ~= "0" and mobility ~= "1" and mobility ~= "2" then return false, "Invalid frequency preference" end
	local hidden_lock = baseline and baseline.smode == "0" and mode == "0" and old("freqlock") == nil
	if hidden_lock then lock = baseline.freqlock == "1" end
	local earfcn = old("earfcn") or (hidden_lock and baseline.earfcn) or ""
	local pci = old("cellid") or (hidden_lock and baseline.cellid) or ""
	local band = mode == "1" and bands.lte or rm and nrmode == "2" and bands.nsa or bands.sa
	local lock_changed = edited("freqlock", lock and "1" or "0") or edited("earfcn", earfcn) or
		edited("cellid", pci) or edited("smode", mode) or (rm and edited("nrmode", nrmode))
	for _, name in ipairs(rm and { "sa", "nsa", "lte" } or { "sa", "lte" }) do
		lock_changed = lock_changed or edited("bandlist_" .. name, bands[name])
	end
	-- Existing official auto-mode locks may coexist with an APN or strategy edit.
	-- Validate the native form's lock inputs only when those inputs are changed.
	if lock and lock_changed and (mode == "0" or band == "0" or not earfcn:match("^%d+$") or
		not pci:match("^%d+$") or tonumber(pci) > (mode == "1" and 503 or 1007)) then
		return false, "A frequency lock requires a band, EARFCN and PCI"
	end
	local imei = old("enable_imei") == "1" and (old("modify_imei") or "") or nil
	if imei and not imei:match("^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$") then return false, "Invalid IMEI" end
	if imei and rm then return false, "This native IMEI operation is not supported by the official RM520 backend" end
	local iccid
	if pin ~= "" then
		if slot ~= current_slot(config) then return false, "Apply SIM selection before setting its PIN" end
		iccid = (util.exec("/usr/bin/timeout 15 " .. CLI .. " -i cpe -c iccid 2>/dev/null") or ""):match("^%s*([%w]+)%s*$")
		if not iccid or #iccid < 19 or #iccid > 22 then return false, "Current SIM ICCID unavailable" end
	end
	local changed, restart_needed, dirty = false, false, {}
	local function put(package, section, option, value, radio)
		value = tostring(value)
		if config:get(package, section, option) ~= value then
			config:set(package, section, option, value)
			changed = true
			dirty[package] = true
			if not radio then restart_needed = true end
		end
	end
	local section = "cpesim" .. slot
	if not config:get("cpecfg", section) then
		config:set("cpecfg", section, "cpesim")
		dirty.cpecfg = true
	end
	if edited("mobility", mobility) then put("cpecfg", section, "mobility", mobility, true) end
	if edited("smode", mode) or edited("nrmode", nrmode) then put("cpecfg", section, "mode", native_mode, true) end
	if edited("datarroaming", roaming) then put("cpecfg", section, "roaming", roaming) end
	if edited("apnconfig", apn) then
		put("cpecfg", section, "custom_apn", apn ~= "" and "1" or "0")
		put("cpecfg", section, "apn", apn)
	end
	local freq, band_changed = {}, false
	local kinds = rm and { "sa", "nsa", "lte" } or { "nr", "lte" }
	for _, kind in ipairs(kinds) do
		local name = kind == "nr" and "sa" or kind
		local changed_band = edited("bandlist_" .. name, bands[name])
		band_changed = band_changed or changed_band
		local value = bands[name]
		if baseline and not changed_band and config:get("cpecfg", section, "custom_freq") == "1" then
			for key, existing in (config:get("cpecfg", section, "freq") or ""):gmatch("([%a]+)%-([^,]+)") do
				if key == kind then value = existing end
			end
		end
		if value ~= "0" then freq[#freq + 1] = kind .. "-" .. value end
	end
	if band_changed then
		put("cpecfg", section, "custom_freq", #freq > 0 and "1" or "0", true)
		put("cpecfg", section, "freq", table.concat(freq, ","), true)
	end
	if lock_changed then
		local lock_mode = "band"
		if lock then
			lock_mode = config:get("network", "cpe", "earfreq_mode") == "one" and
				(mode == "1" and "earfcn4" or "earfcn5") or ""
		end
		put("cpecfg", section, "earfreq_mode", lock_mode, true)
		put("cpecfg", section, "custom_earfcn4", lock and mode == "1" and "1" or "0", true)
		put("cpecfg", section, "custom_earfcn5", lock and mode == "2" and "1" or "0", true)
	end
	if lock and lock_changed then
		local suffix = mode == "1" and "4" or "5"
		put("cpecfg", section, "earfcn" .. suffix, earfcn, true)
		put("cpecfg", section, "pci" .. suffix, pci, true)
		put("cpecfg", section, "band" .. suffix, band, true)
	end
	if not rm and slot == 1 and edited("smsen", old("smsen") or "0") then
		put("cpecfg", section, "force_ims", old("smsen") == "1" and "1" or "0")
	end
	if iccid then
		if not config:get("cpecfg", iccid) then config:set("cpecfg", iccid, "iccid") end
		put("cpecfg", iccid, "pin", pin)
	end
	if edited("enable", enabled) then put("network", "cpe", "disabled", enabled == "1" and "0" or "1") end
	local sim_changed = edited("simsel", tostring(slot - 1)) and current_slot(config) ~= slot
	if not config:get("cpesel", "sim") then
		config:set("cpesel", "sim", "cpesel")
		dirty.cpesel = true
	end
	put("cpesel", "sim", "mode", "1")
	if sim_changed then
		put("cpesel", "sim", "cur", tostring(slot))
		put("cpesel", "sim", "max", "2")
	end
	for package in pairs(dirty) do
		if not config:commit(package) then return false, "Cannot save official modem settings" end
	end
	if imei then
		local current_imei = M.at("AT+CGSN", 10):match("(%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d)")
		if current_imei ~= imei then
			if not ok(M.at("AT^PHYNUM=IMEI," .. imei, 25)) then return false, "IMEI operation failed" end
			changed = true
			restart_needed = true
		end
	end
	if sim_changed and not execute(CLI .. " -i cpe -s " .. tostring(slot)) then return false, "Manual SIM selection failed" end
	if (changed or sim_changed) and not (sim_changed and enabled == "1") then
		local live = util.ubus("network.interface.cpe", "status", {}) or {}
		local apply = enabled == "1" and "/sbin/ifup cpe" or "/sbin/ifdown cpe"
		if changed and not restart_needed and not sim_changed and enabled == "1" and
		   slot == current_slot(config) and live.up then
			-- Preserve DHCP/data sessions when only radio settings change. The
			-- WWAN owner compares AT state and batches any required RF restart.
			apply = CLI .. " -i cpe -u -m " .. quote(native_mode)
		end
		if not execute(apply) then return false, "WWAN apply failed" end
	end
	return true
end

return M
