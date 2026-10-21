-- NBCPE APP bridge. The stock AK68 collector owns modem I/O.
-- This module reads its cache adapter and netifd interface data only.
local util = require "luci.util"
local ok_json, json = pcall(require, "cjson.safe")
if not ok_json then json = require "cjson" end
local M = {}

local function trim(value)
	return (tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function read1(path)
	local file = io.open(path, "r")
	if not file then return "" end
	local value = file:read("*l") or ""
	file:close()
	return trim(value)
end

function M.detect()
	local board = (read1("/tmp/sysinfo/board_name") .. " " ..
		read1("/tmp/sysinfo/model")):lower()
	return board:find("nbcpe", 1, true) ~= nil or
		board:find("nradiowt9111no2manpernb68", 1, true) ~= nil or
		board:find("nradio_wt9111_no2-manper_nb68", 1, true) ~= nil or
		board:find("hcmt7981-emmc-nb68", 1, true) ~= nil
end

local function decode(value)
	local ok, result = pcall(json.decode, value or "")
	return ok and type(result) == "table" and result or {}
end

function M.status()
	return decode(util.exec("/usr/libexec/myos-nbcpe-status status 2>/dev/null"))
end

local function network_mode(value)
	local mode = trim(value)
	local upper = mode:upper()
	if upper:find("NSA", 1, true) or upper:find("EN-DC", 1, true) or
	   upper:find("LTE-NR", 1, true) then return "NR NSA" end
	if upper:find("NR", 1, true) or upper:find("5G", 1, true) then
		return "NR SA"
	end
	if upper:find("LTE", 1, true) or upper:find("4G", 1, true) then return "LTE" end
	return mode
end

local function first_ip(status, key)
	local list = type(status[key]) == "table" and status[key] or {}
	local first = list[1]
	if type(first) ~= "table" or not first.address then return "", "" end
	local address = tostring(first.address)
	return address, address .. (first.mask and ("/" .. tostring(first.mask)) or "")
end

local function statistics(device)
	if not device:match("^[%w_.:%-]+$") then return 0, 0, 0, 0 end
	local base = "/sys/class/net/" .. device .. "/statistics/"
	return tonumber(read1(base .. "tx_bytes")) or 0,
		tonumber(read1(base .. "rx_bytes")) or 0,
		tonumber(read1(base .. "tx_packets")) or 0,
		tonumber(read1(base .. "rx_packets")) or 0
end

function M.summary()
	local status = M.status()
	local fields, stale, age = {}, true, nil
	for _, source in ipairs(status.sources or {}) do
		if source.kind == "cell" then
			stale, age = source.stale ~= false, source.age
			for _, entry in ipairs(source.entries or {}) do
				if entry.key then fields[entry.key] = trim(entry.value) end
			end
		end
	end
	local ipv4, ipv6 = {}, {}
	for _, interface in ipairs(status.interfaces or {}) do
		if type(interface["ipv4-address"]) == "table" and interface["ipv4-address"][1] then
			if not ipv4.up or interface.up then ipv4 = interface end
		end
		if type(interface["ipv6-address"]) == "table" and interface["ipv6-address"][1] then
			if not ipv6.up or interface.up then ipv6 = interface end
		end
	end
	local ip4, ip4p = first_ip(ipv4, "ipv4-address")
	local ip6, ip6p = first_ip(ipv6, "ipv6-address")
	local device = trim(ipv4.l3_device or ipv6.l3_device or
		ipv4.device or ipv6.device or "port5")
	local tx, rx, tx_packets, rx_packets = statistics(device)
	local plmn = (fields.MCC or "") .. (fields.MNC or "")
	local iccid = fields.ICCID or ""
	local simstatus = fields.sim_status or ""
	if simstatus == "" and iccid ~= "" then simstatus = "READY" end
	local slot = tonumber(status.sim_slot)
	if slot == nil then
		slot = ({ ["外置SIM卡"] = 0, ["内置SIM1"] = 1 })[status.sim_label]
	end
	local result = {
		section = "AK68", network = trim(ipv4.interface or ipv6.interface or "cpe_4"),
		dev = device, at_port = "", native_nros = true, native_nbcpe = true,
		up = status.connect_status == "Yes", connect_status = status.connect_status or "",
		uptime = tonumber(ipv4.uptime or ipv6.uptime) or 0,
		ip4 = ip4, ip4p = ip4p, ip6 = ip6, ip6p = ip6p,
		dns4 = ipv4["dns-server"] or {}, dns6 = ipv6["dns-server"] or {},
		tx = tx, rx = rx, tx_packets = tx_packets, rx_packets = rx_packets,
		mode = network_mode(fields.network_mode),
		isp = plmn ~= "" and plmn or (fields.ISP or ""),
		operator = fields.ISP or "", operatorName = fields.ISP or "",
		manufacturer = fields.manufacturer or "", model = fields.name or "",
		revision = fields.revision or "", imei = fields.IMEI or "",
		imsi = fields.IMSI or "", iccid = iccid,
		simno = slot and tostring(slot + 1) or "", sim_slot = slot,
		sim_label = status.sim_label or "", simstatus = simstatus,
		cell = fields.CID or "", cellid = fields.CID or "",
		lac = fields.LAC or "", tac = fields.TAC or "", pci = fields.PCI or "",
		earfcn = fields.ARFCN or "", band = (fields.Band or ""):gsub("^[BbNn]", ""),
		rsrp = fields.RSRP or "", rsrq = fields.RSRQ or "",
		sinr = fields.SINR or "", rssi = fields.RSSI or "",
		dlbw = "", ulbw = "", cqi = fields.QCI or "", ambr_dl = "", ambr_ul = "",
		temp = fields.temperature or "", apn = fields.APN or "",
		cache_only = true, cache_age = age, stale = stale,
		cache_state = status.cache_state or "missing",
	}
	result.signal = tonumber(result.rsrp) or tonumber(result.rssi) or -999
	result.signalStrength = tostring(result.signal)
	return result
end

function M.device_name()
	return "NBCPE-688"
end

-- The AK68 cache has no neighbour list. Keep unknown measurements empty.
function M.neighbour()
	return { {} }
end

return M
