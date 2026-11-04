module("luci.controller.uuplugin", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/uuplugin") then
		return
	end

	entry({"admin", "services", "uuplugin"}, cbi("uuplugin"), ("网易UU游戏加速器"), 99).dependent = true
	entry({"admin", "services", "uuplugin", "status"}, call("act_status")).leaf = true
end

function act_status()
	local fs = require("nixio.fs")
	local uci = require("luci.model.uci").cursor()
	local e = { running = luci.sys.call("pidof uuplugin >/dev/null") == 0 }
	e.guardian = luci.sys.call("pidof xuplugin-guardian >/dev/null") == 0
	e.configured_platform = uci:get_first("uuplugin", "uuplugin", "platform") or "h3c-nx30pro"
	e.platform = (fs.readfile("/var/tmp/uu/.platform") or "h3c-nx30pro"):gsub("%s+$", "")
	local activation = fs.readfile("/var/tmp/uu/activate_status") or fs.readfile("/tmp/uu/activate_status") or ""
	e.uu_status = tonumber(activation:match("uu_status%s*=%s*[\"']?(%d+)")
		or activation:match("[\"']uu_status[\"']%s*:%s*[\"']?(%d+)")
		or activation:match("^%s*(%d+)%s*$"))
	e.activated = e.uu_status ~= nil and e.uu_status > 0
	e.rules_present = false
	local remove_rules = fs.readfile("/tmp/xu_nft_delete.txt") or ""
	for family, name in remove_rules:gmatch("table%s+(%w+)%s+([%w_.%-]+)") do
		if family == "ip" or family == "ip6" or family == "inet" then
			if luci.sys.call("nft list table " .. family .. " " .. name .. " >/dev/null 2>&1") == 0 then
				e.rules_present = true
			end
		end
	end
	e.dhcp_leases = 0
	for line in (fs.readfile("/tmp/dhcp.leases") or ""):gmatch("[^\n]+") do
		if line:match("^%d+%s+[%x:]+%s+[%d.]+%s+") then e.dhcp_leases = e.dhcp_leases + 1 end
	end
	luci.http.prepare_content("application/json")
	luci.http.write_json(e)
end
