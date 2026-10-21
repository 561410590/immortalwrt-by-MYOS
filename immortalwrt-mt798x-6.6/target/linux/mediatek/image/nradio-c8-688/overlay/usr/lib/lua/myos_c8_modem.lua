local util = require "luci.util"
local json = require "luci.jsonc"
local M = {}

local function value(data, key)
	return data and data[key] ~= nil and tostring(data[key]) or ""
end

local function quote(text)
	return "'" .. tostring(text):gsub("'", "'\\''") .. "'"
end

function M.request(request)
	local encoded = json.stringify(request)
	local raw = util.exec("/usr/bin/python3 /usr/sbin/myos-c8-modemctl " .. quote(encoded) .. " 2>/dev/null")
	return json.parse(raw or "") or { ok = false, error = "Modem owner is unavailable" }
end

function M.status()
	local file = io.open("/tmp/myos-c8-modem.json", "r")
	local data = file and file:read("*a") or ""
	if file then file:close() end
	local result = json.parse(data) or {}
	result.age = math.max(0, os.time() - (tonumber(result.sampled_at) or 0))
	result.stale = result.stale ~= false or result.age > 90
	return result
end

function M.summary()
	local data = M.status()
	local modem, sim, signal = data.modem or {}, data.sim or {}, data.signal or {}
	local network, cell, reg = data.network or {}, data.cell or {}, data.registration or {}
	local slot = ({ [0] = 1, [6] = 2, [1] = 3, [2] = 4 })[tonumber(sim.slot)]
	local device = network.device or ""
	local function counter(name)
		if not device:match("^[%w_.%-]+$") then return 0 end
		local file = io.open("/sys/class/net/" .. device .. "/statistics/" .. name, "r")
		local result = file and tonumber(file:read("*l")) or 0
		if file then file:close() end
		return result or 0
	end
	return {
		section = "c8modem", model = value(modem, "model"), manufacturer = value(modem, "manufacturer"),
		revision = value(modem, "revision"), imei = value(modem, "imei"), iccid = value(modem, "iccid"),
		imsi = value(modem, "imsi"), temp = value(modem, "temperature"), simno = slot and tostring(slot) or "",
		simstatus = value(sim, "status"), up = not data.stale and network.online == true,
		registered = not data.stale and reg.registered, dev = device, at_port = value(modem, "endpoint"),
		mode = value(cell, "mode") ~= "" and cell.mode or value(reg, "rat"),
		operator = value(data, "operator"), isp = value(cell, "mcc") .. value(cell, "mnc"),
		band = value(cell, "band"), cell = value(cell, "cell"), cellid = value(cell, "cell"),
		pci = value(cell, "pci"), tac = value(cell, "tac"), lac = value(cell, "tac"),
		earfcn = value(cell, "arfcn"), band_count = #(cell.carriers or {}),
		rsrp = value(signal, "rsrp"), rsrq = value(signal, "rsrq"), sinr = value(signal, "sinr"),
		rssi = value(signal, "rssi"), signal = signal.rsrp or signal.rssi or -999,
		signalStrength = tostring(signal.rsrp or signal.rssi or -999),
		ip4p = (network.ipv4 or {})[1] or "", ip6p = (network.ipv6 or {})[1] or "",
		dns4 = network.dns or {}, dns6 = network.dns or {},
		tx = counter("tx_bytes"), rx = counter("rx_bytes"), tx_packets = counter("tx_packets"), rx_packets = counter("rx_packets"),
		stale = data.stale, sampled_at = data.sampled_at, age = data.age
	}
end

function M.cpe_item()
	local summary = M.summary()
	local item = {}
	for key, field in pairs(summary) do item[key] = field end
	item.name, item.cpeno, item.using = "cpe", 1, true
	item.up, item.online, item.netlink = summary.up and 1 or 0, summary.up and 1 or 0, summary.up and 1 or 0
	item.status, item.gateway_if = summary.up and 0 or -1, summary.up and 1 or -1
	item.model_temp = tonumber(summary.temp) or 0
	item.real_name, item.hwid, item.control = summary.model, summary.model, summary.at_port
	item.driver, item.interface = summary.manufacturer, summary.dev
	item.RSRP, item.RSRQ, item.SINR, item.RSSI = summary.rsrp, summary.rsrq, summary.sinr, summary.rssi
	item.CPIN = summary.simstatus == "READY" and "0" or summary.simstatus
	item.simtype = tonumber(summary.simno) and tonumber(summary.simno) >= 3 and "4" or "0"
	item.simcount, item.switchmode, item.simmode, item.odu = "4", "1", "1", "0"
	item.traffic, item.device_traffic = tostring(summary.tx + summary.rx), tostring(summary.tx + summary.rx)
	item.upload, item.download = summary.tx, summary.rx
	item.operatorName, item.sim_isp = summary.operator, summary.operator
	item.nrcap = summary.mode:find("NR", 1, true) and "1" or "0"
	if summary.ip4p ~= "" then item.V4 = { ipaddrs = { summary.ip4p }, dns = summary.dns4 } end
	if summary.ip6p ~= "" then item.V6 = { ipaddrs = { summary.ip6p }, dns = summary.dns6 } end
	return item
end

function M.dashboard()
	local data, summary = M.status(), M.summary()
	local entries = {}
	local names = { name = summary.model, manufacturer = summary.manufacturer, revision = summary.revision,
		temperature = summary.temp, network_mode = summary.mode, ISP = summary.operator,
		RSRP = summary.rsrp, RSRQ = summary.rsrq, SINR = summary.sinr, RSSI = summary.rssi,
		MCC = (data.cell or {}).mcc, MNC = (data.cell or {}).mnc, Band = summary.band,
		ARFCN = summary.earfcn, PCI = summary.pci, IMEI = summary.imei, ICCID = summary.iccid,
		signal_percent = (data.signal or {}).percent }
	for key, field in pairs(names) do
		if field ~= nil and field ~= "" then entries[#entries + 1] = { key = key, value = field } end
	end
	for index, carrier in ipairs((data.cell or {}).carriers or {}) do
		if index > 1 then entries[#entries + 1] = { key = "Band " .. (index - 1), value = carrier.band, extra_info = carrier.rat .. " CA" } end
	end
	return { section = "c8modem", registration = data.stale and { registered = json.null } or data.registration,
		dial = data.session or {}, network = data.network or {}, connect_status = data.stale and "" or (summary.up and "Yes" or "No"),
		cache = { age = data.age }, sources = { { kind = "cell", age = data.age, entries = entries } },
		stale = data.stale, error = data.error, sampled_at = data.sampled_at }
end

function M.legacy_status()
	local data, summary = M.status(), M.summary()
	return { stsss = data.stale and "数据过期" or "C8实时采集", present = not data.stale and (data.modem or {}).supported == true,
		modem = summary.manufacturer, conntype = summary.model, firmware = summary.revision,
		temper = summary.temp, date = data.sampled_at and os.date("%Y-%m-%d %H:%M:%S", data.sampled_at) or "",
		simsel = (data.sim or {}).label or "", simno = summary.simno, pin = summary.simstatus,
		cops = summary.operator, imei = summary.imei, imsi = summary.imsi, iccid = summary.iccid,
		mode = summary.mode, per = (data.signal or {}).percent or "", rssi = summary.rssi,
		rsrq = summary.rsrq, rscp = summary.rsrp, sinr = summary.sinr, mcc = summary.isp,
		lac = summary.tac, tac = summary.tac, cid = summary.cell, band = summary.band,
		rfcn = summary.earfcn, pci = summary.pci, apn = (data.session or {}).apn or "",
		iface = "c8modem", ifdev = summary.dev, connected = summary.up, ipv4 = summary.ip4p, ipv6 = summary.ip6p,
		stale = data.stale, registration = data.registration or {}, network = data.network or {} }
end

function M.apply_native_cpesel(slot)
	local logical = ({ [1] = 0, [2] = 6, [3] = 1, [4] = 2 })[tonumber(slot)]
	if logical == nil then return false end
	return M.request({ action = "select_sim", slot = logical }).ok == true
end

function M.sim_swap(slot)
	return M.apply_native_cpesel(slot)
end

function M.sms_supported()
	local data = M.status()
	return not data.stale and (data.modem or {}).supported == true
end

function M.sms(data)
	data = type(data) == "table" and data or {}
	local uci = require("luci.model.uci").cursor()
	if data.action == nil or data.action == "modify" then
		if data.action == "modify" then
			local enabled = tostring(data.enabled or "")
			if enabled ~= "0" and enabled ~= "1" then return { code = 2, message = "Invalid SMS setting" } end
			if not uci:set("myos_nros", "main", "sms_enabled", enabled) or not uci:commit("myos_nros") then
				return { code = 2, message = "Cannot save SMS setting" }
			end
		end
		return { code = 0, result = { index = tostring(data.index or "1"), sim = tonumber(M.summary().simno),
			enabled = uci:get("myos_nros", "main", "sms_enabled") == "0" and "0" or "1" } }
	end
	if uci:get("myos_nros", "main", "sms_enabled") == "0" then return { code = 3, message = "APP SMS is disabled" } end
	if data.action == "send" then
		local result = M.request({ action = "sms_send", number = data.phone_num or data.phone_number,
			text = data.msg or data.message_content })
		return { code = result.ok and 0 or -1, message = result.error,
			item = { { code = result.ok and 0 or -1, index = "", timestamp = os.time() } } }
	end
	local snapshot = M.request({ action = "sms_list" })
	if not snapshot.ok then return { code = -4, message = snapshot.error } end
	if data.action == "del" then
		local allowed, deleted = {}, {}
		for _, message in ipairs(snapshot.messages or {}) do allowed[tostring(message.index)] = true end
		for id in tostring(data.ids or data.index or ""):gmatch("%d+") do
			if not allowed[id] then return { code = -4, message = "Refresh SMS before deleting" } end
			local result = data.type == "new" and { ok = true } or M.request({ action = "sms_delete", index = tonumber(id) })
			deleted[#deleted + 1] = { index = tonumber(id), code = result.ok and 0 or 1 }
		end
		return { code = 0, smsdel = deleted }
	elseif data.action ~= "read" then
		return { code = -4, message = "Invalid SMS action" }
	end
	local grouped, list, count = {}, {}, 0
	for _, message in ipairs(snapshot.messages or {}) do
		local multipart = message.concat or {}
		local item = { index = message.index, contact = message.number, content = message.text,
			timestamp = 0, date = message.date, stat = tonumber(message.status) or 0, role = 0,
			multi_sms_id = multipart.reference or 0, multi_sms_index = multipart.part or 0,
			multi_sms_max = multipart.total or 0 }
		count = count + 1
		if data.type == "new" then
			if item.stat == 0 then list[#list + 1] = item end
		else
			local contact = item.contact or ""
			if not grouped[contact] then
				grouped[contact] = { contact = contact, undeal = 0, list = {} }
				list[#list + 1] = grouped[contact]
			end
			local group = grouped[contact]
			group.list[#group.list + 1] = item
			if item.stat == 0 then group.undeal = group.undeal + 1 end
		end
	end
	return { code = 0, result = { code = 0, ready = 1, total = count, count = count, used = count,
		type = "ME", smslist = #list > 0 and list or { {} } } }
end

function M.neighbour()
	return { code = -4, message = "Neighbour-cell collection is unavailable", list = {} }
end

function M.invalidate()
	return true
end

return M
