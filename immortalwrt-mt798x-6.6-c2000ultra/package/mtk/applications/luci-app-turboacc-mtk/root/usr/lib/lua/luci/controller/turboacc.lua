module("luci.controller.turboacc", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/turboacc") then
		return
	end
	local page
	page = entry({"admin", "network", "turboacc"}, cbi("turboacc"), _("Turbo ACC Center"), 101)
	page.i18n = "turboacc"
	page.dependent = true
	
	entry({"admin", "network", "turboacc", "status"}, call("action_status"))
end

local function fastpath_status()
	return luci.sys.call("/etc/init.d/turboacc check_status fastpath 0") == 0
end

local function bbr_status()
	return luci.sys.call("/etc/init.d/turboacc check_status bbr") == 0
end

local function fullconenat_status()
	return luci.sys.call("/etc/init.d/turboacc check_status fullconenat") == 0
end

function action_status()
	local i18n = require "luci.i18n"
	luci.http.prepare_content("application/json")
	luci.http.write_json({
		fastpath_state = fastpath_status(),
		fastpath_name = i18n.translate((luci.sys.exec("/etc/init.d/turboacc check_status fastpath"):gsub("%s+$", ""))),
		wed_state = nixio.fs.access("/sys/module/mtk_wed") and true or false,
		fullconenat_state = fullconenat_status(),
		bbr_state = bbr_status()
    })
end
