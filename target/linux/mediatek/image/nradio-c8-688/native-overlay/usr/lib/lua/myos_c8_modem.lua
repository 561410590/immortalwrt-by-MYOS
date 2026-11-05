local util = require "luci.util"
local json = require "luci.jsonc"
local M = {}

local function val(data, key)
	return data and data[key] ~= nil and data[key] ~= json.null and tostring(data[key]) or ""
end

local function quote(value)
	return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

function M.request(data)
	local raw = util.exec("/usr/libexec/myos-c8-native request " .. quote(json.stringify(data)) .. " 2>/dev/null")
	return json.parse(raw or "") or { ok = false, error = "Native modem adapter unavailable" }
end

function M.status()
	local file = io.open("/tmp/myos-cellular.json", "r")
	local raw = file and file:read("*a") or ""
	if file then file:close() end
	local data = json.parse(raw) or {}
	data.age = math.max(0, os.time() - (tonumber(data.sampled_at) or 0))
	data.stale = data.stale ~= false or data.age > 90 or (tonumber(data.cache_age) or 999) + data.age > 90
	return data
end

function M.summary()
	local d = M.status()
	local m, s, c, n, sig = d.modem or {}, d.sim or {}, d.cell or {}, d.network or {}, d.signal or {}
	local device = val(n, "device")
	if device == "" then device = "eth1" end
	local function counter(key)
		local f = io.open("/sys/class/net/" .. device .. "/statistics/" .. key, "r")
		local value = f and tonumber(f:read("*l")) or 0
		if f then f:close() end
		return value or 0
	end
	return { section = "cpe", model = val(m, "model"), manufacturer = val(m, "manufacturer"), revision = val(m, "revision"),
		imei = val(m, "imei"), imsi = val(m, "imsi"), iccid = val(m, "iccid"), temp = val(m, "temperature"),
		simno = val(s, "slot"), simstatus = val(s, "status"), dev = device, at_port = val(m, "endpoint"),
		up = not d.stale and d.data_connected == true, registered = not d.stale and d.registered == true,
		mode = val(c, "mode"), operator = val(d, "operator"), isp = val(c, "mcc") .. val(c, "mnc"),
		band = val(c, "band"), cell = val(c, "cell"), cellid = val(c, "cell"), pci = val(c, "pci"),
		tac = val(c, "tac"), lac = val(c, "tac"), earfcn = val(c, "arfcn"),
		rsrp = val(sig, "rsrp"), rsrq = val(sig, "rsrq"), sinr = val(sig, "sinr"), rssi = val(sig, "rssi"),
		signal = tonumber(sig.rsrp) or -999, signalStrength = tostring(tonumber(sig.rsrp) or -999),
		ip4p = (n.ipv4 or {})[1] or "", ip6p = (n.ipv6 or {})[1] or "", dns4 = n.dns or {}, dns6 = n.dns or {},
		tx = counter("tx_bytes"), rx = counter("rx_bytes"), tx_packets = counter("tx_packets"), rx_packets = counter("rx_packets"),
                qos = d.qos or {}, stale = d.stale, sampled_at = d.sampled_at, age = d.age }
end

function M.cpe_item()
	local d, s = M.status(), M.summary()
	local item = {}
	for key, value in pairs(d.native_status or {}) do item[key] = value end
	for key, value in pairs(s) do item[key] = value end
	item.name, item.cpeno, item.using, item.real_name = "cpe", 1, true, s.model
	item.up, item.online, item.netlink = s.up and 1 or 0, s.up and 1 or 0, s.up and 1 or 0
	item.status, item.gateway_if = s.up and 0 or -1, s.up and 1 or -1
	item.model_temp, item.control, item.interface = tonumber(s.temp) or 0, s.at_port, s.dev
	item.simcount, item.switchmode, item.simmode, item.simtype, item.odu = "4", "1", "1", "0,0,4,4", "0"
	item.CPIN = s.simstatus == "READY" and "0" or s.simstatus
	item.RSRP, item.RSRQ, item.SINR, item.RSSI = s.rsrp, s.rsrq, s.sinr, s.rssi
	item.traffic, item.device_traffic = tostring(s.tx + s.rx), tostring(s.tx + s.rx)
	item.upload, item.download = s.tx, s.rx
	item.operatorName, item.sim_isp = s.operator, s.operator
	item.V4 = { ipaddrs = (d.network or {}).ipv4 or {}, dns = s.dns4 }
	item.V6 = { ipaddrs = (d.network or {}).ipv6 or {}, dns = s.dns6 }
	return item
end

function M.dashboard()
	local d, s = M.status(), M.summary()
	local entries = {}
	local values = { name = s.model, manufacturer = s.manufacturer, revision = s.revision, temperature = s.temp,
		network_mode = s.mode, ISP = s.operator, RSRP = s.rsrp, RSRQ = s.rsrq, SINR = s.sinr, RSSI = s.rssi,
		Band = s.band, ARFCN = s.earfcn, PCI = s.pci, MCC = (d.cell or {}).mcc, MNC = (d.cell or {}).mnc,
		IMEI = s.imei, ICCID = s.iccid, signal_percent = (d.signal or {}).percent }
	for key, value in pairs(values) do
		if value ~= nil and value ~= json.null and value ~= "" then entries[#entries + 1] = { key = key, value = value } end
	end
	local primary, index = false, 0
	local nr = s.mode:find("NR") ~= nil
	for _, carrier in ipairs((d.cell or {}).carrier_details or {}) do
		local band = val(carrier, "band")
		if (nr and band:sub(1, 1) == "n") or (not nr and band:sub(1, 1) == "B") then
			if primary then
				index = index + 1
				entries[#entries + 1] = { key = "Band " .. index, value = band, extra_info = nr and "NR CA" or "LTE CA" }
			else primary = true end
		end
	end
	return { section = "cpe", cache_only = true, registration = d.stale and { registered = json.null } or d.registration or {},
		dial = d.stale and { connected = json.null } or d.session or {},
		network = d.network or {}, passthrough = d.passthrough or {}, connect_status = d.stale and "" or (s.up and "Yes" or "No"),
		cache = { age = d.age + (tonumber(d.cache_age) or 999), stale = d.stale },
		sources = { { kind = "cell", age = d.age + (tonumber(d.cache_age) or 999), stale = d.stale, entries = entries } },
		summary = s, serving_cell = d.cell or {}, sim_slot = s.simno, sim_label = (d.sim or {}).label,
		stale = d.stale, error = d.error, sampled_at = d.sampled_at }
end

function M.legacy_status()
	local d, s = M.status(), M.summary()
	return { stsss = d.stale and "数据过期" or "C8 原厂缓存", present = not d.stale and (d.modem or {}).supported == true,
		modem = s.manufacturer, conntype = s.model, firmware = s.revision, temper = s.temp,
		date = d.sampled_at and os.date("%Y-%m-%d %H:%M:%S", d.sampled_at) or "", simsel = (d.sim or {}).label,
		simno = s.simno, pin = s.simstatus, cops = s.operator, imei = s.imei, imsi = s.imsi, iccid = s.iccid,
		mode = s.mode, per = (d.signal or {}).percent or "", rssi = s.rssi, rsrq = s.rsrq, rscp = s.rsrp,
		sinr = s.sinr, mcc = s.isp, lac = s.tac, tac = s.tac, cid = s.cell, band = s.band, rfcn = s.earfcn,
		pci = s.pci, apn = (d.session or {}).apn, iface = "cpe", ifdev = s.dev, connected = s.up,
		ipv4 = s.ip4p, ipv6 = s.ip6p, stale = d.stale, registration = d.registration or {}, network = d.network or {} }
end

function M.mwrt_info()
	local d, s = M.status(), M.summary()
	local result = {}
	local groups = {
		{ "基本信息", { name = s.model, manufacturer = s.manufacturer, revision = s.revision, temperature = s.temp,
			at_port = s.at_port, network_mode = s.mode, connect_status = d.stale and "Unknown" or (s.up and "Yes" or "No"), ISP = s.operator } },
		{ "SIM卡信息", { IMEI = s.imei, IMSI = s.imsi, ICCID = s.iccid, sim_label = (d.sim or {}).label or "", sim_status = s.simstatus } },
		{ "基站信息", { RSRP = s.rsrp, RSRQ = s.rsrq, SINR = s.sinr, RSSI = s.rssi, Band = s.band,
			MCC = (d.cell or {}).mcc, MNC = (d.cell or {}).mnc, CID = s.cell, TAC = s.tac, PCI = s.pci, ARFCN = s.earfcn } }
	}
	for _, group in ipairs(groups) do
		for key, value in pairs(group[2]) do
			if value ~= nil and value ~= json.null and value ~= "" then
				result[#result + 1] = { key = key, full_name = key, value = tostring(value), class = group[1],
					class_origin = group[1], stale = d.stale and math.max(91, d.age + (tonumber(d.cache_age) or 999)) or 0 }
			end
		end
	end
	return result
end

function M.gateway_status()
	local dump = util.ubus("network.interface", "dump", {}) or {}
	local primary, gateway, device, metric = false, "--", "--", math.huge
	for _, iface in ipairs(dump.interface or {}) do
		if iface.interface == "wan" or iface.interface == "wan6" then
			for _, route in ipairs(iface.route or {}) do
				if iface.up == true and tonumber(route.mask) == 0 then primary = true end
			end
		end
	end
	local file = io.open("/proc/net/route", "r")
	if file then
		for line in file:lines() do
			local dev, destination, next_hop, flags, _, _, cost = line:match("^(%S+)%s+(%x+)%s+(%x+)%s+(%x+)%s+(%d+)%s+(%d+)%s+(%d+)")
			if destination == "00000000" and tonumber(flags, 16) % 2 == 1 and tonumber(cost) < metric then
				metric, device = tonumber(cost), dev
				local octets = {}
				for i = 7, 1, -2 do octets[#octets + 1] = tonumber(next_hop:sub(i, i + 1), 16) end
				gateway = table.concat(octets, ".")
			end
		end
		file:close()
	end
	local data = M.status()
	return { ["宽带接口状态 / Primary Status"] = primary and "在线 / Online" or "离线 / Offline",
		["蜂窝接口状态 / Secondary Status"] = not data.stale and data.data_connected == true and "在线 / Online" or "离线 / Offline",
		["当前网关 / Gateway"] = gateway, ["当前物理接口 / Current Interface"] = device }
end

-- The generic APP/remote path may carry automatic SIM requests. Only the module
-- settings controller can originate the explicit local operation.
function M.apply_native_cpesel() return false end
function M.sim_swap() return false end
function M.invalidate() return true end
function M.neighbour() return { code = -4, message = "Neighbour scan unavailable", list = {} } end
function M.sms_supported()
	local d = M.status()
	return not d.stale and (d.modem or {}).supported == true
end

function M.sms(data)
	data = type(data) == "table" and data or {}
	local action = data.action or "get"
	local uci = require("luci.model.uci").cursor()
	if action == "get" or action == "modify" then
		if action == "modify" then
			local value = tostring(data.enabled or "")
			if value ~= "0" and value ~= "1" then return { code = 2, message = "Invalid SMS setting" } end
			uci:set("myos_nros", "main", "sms_enabled", value)
			uci:commit("myos_nros")
		end
		return { code = 0, result = { index = "1", sim = tonumber(M.summary().simno),
			enabled = uci:get("myos_nros", "main", "sms_enabled") == "0" and "0" or "1" } }
	end
	if not M.sms_supported() or uci:get("myos_nros", "main", "sms_enabled") == "0" then
		return { code = -4, message = "SMS unavailable" }
	end
	if action == "read" then
		local result = util.ubus("smsd", "list", { type = tostring(data.type or "") })
		return result and { code = 0, result = result } or { code = -4, message = "Official SMS service unavailable" }
	elseif action == "del" then
		local ids = tostring(data.ids or data.index or "")
		if not ids:match("^%d[%d, ]*$") then return { code = 2, message = "Invalid SMS indices" } end
		return util.ubus("smsd", "del", { ids = ids, type = tostring(data.type or "") }) or { code = -4 }
	elseif action == "send" then
		local phone = tostring(data.phone_num or data.phone_number or "")
		local text = data.msg or data.message_content
		if not phone:match("^%+?%d+$") or #phone > 20 or type(text) ~= "string" or #text == 0 or #text > 2048 then
			return { code = 2, message = "Invalid SMS recipient or content" }
		end
		if phone:match("^1[3-9]%d%d%d%d%d%d%d%d%d$") then phone = "+86" .. phone end
		local ok, pdu = pcall(require, "luci.luapdu")
		if not ok then return { code = -4, message = "Official SMS encoder unavailable" } end
		local message = pdu.newTx()
		message.recipient.num, message.msg.content, message.smsc = phone, text, { num = "" }
		local encoded_ok, parts = pcall(message.encode, message)
		if not encoded_ok or type(parts) ~= "table" then return { code = -1, message = "SMS encoding failed" } end
		local result = { code = 0, item = {} }
		for _, encoded in ipairs(parts) do
			local item = util.ubus("smsd", "record", { len = #encoded / 2 - tonumber(encoded:sub(1, 2), 16) - 1, msg = encoded }) or { code = -1 }
			result.item[#result.item + 1] = item
			if tonumber(item.code) ~= 0 then result.code = -1 end
		end
		return result
	end
	return { code = -4, message = "Unsupported SMS action" }
end

return M
