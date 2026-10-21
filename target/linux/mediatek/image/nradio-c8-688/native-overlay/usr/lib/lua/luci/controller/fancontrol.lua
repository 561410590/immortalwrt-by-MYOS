module("luci.controller.fancontrol", package.seeall)

function index()
	entry({"admin", "fancontrol", "modem_ctrl"}, call("action_modem_info")).leaf = true
	entry({"admin", "fancontrol", "devinfo"}, call("action_devinfo")).leaf = true
end

local function reply(data)
	local http = require "luci.http"
	http.prepare_content("application/json")
	http.write_json(data)
end

function action_modem_info()
	reply(require("myos_c8_modem").mwrt_info())
end

function action_devinfo()
	local util = require "luci.util"
	local uci = require("luci.model.uci").cursor()
	local data = { ["设备型号"] = "C8-688", ["主机名"] = uci:get("system", "@system[0]", "hostname") or "MYOS-C8-688",
		["CPU型号"] = "MediaTek MT7981", ["CPU核心数"] = "2" }
	local f = io.open("/proc/sys/kernel/osrelease", "r")
	data["内核版本"] = f and f:read("*l") or ""
	if f then f:close() end
	local board = util.ubus("system", "board", {}) or {}
	data["固件版本"] = (board.release or {}).description or "MYOS"
	local info = util.ubus("system", "info", {}) or {}
	data["内存总量"] = tostring(math.floor(((info.memory or {}).total or 0) / 1048576)) .. " MB"
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.disabled ~= "1" then
			local key = s.device == "radio1" and "5G无线信号名称" or "2.4G无线信号名称"
			data[key] = s.ssid
		end
	end)
	local network = util.ubus("network.interface", "dump", {}) or {}
	for _, item in ipairs(network.interface or {}) do
		if item.interface == "lan" then data["LAN_IP"] = ((item["ipv4-address"] or {})[1] or {}).address end
		if item.up == true and #(item["ipv4-address"] or {}) > 0 then
			for _, route in ipairs(item.route or {}) do
				if route.target == "0.0.0.0" and tonumber(route.mask) == 0 then
					data["WAN状态"], data["WAN接口"], data["WAN协议"] = "已连接", item.l3_device, item.proto
					data["WAN_IP"], data["默认网关"] = item["ipv4-address"][1].address, route.nexthop
					data["DNS服务器"] = table.concat(item["dns-server"] or {}, " ")
				end
			end
		end
	end
	local ok, fan = pcall(require, "myos_c8_fan")
	if ok then for key, value in pairs(fan.status() or {}) do data[key] = value end end
	reply(data)
end
