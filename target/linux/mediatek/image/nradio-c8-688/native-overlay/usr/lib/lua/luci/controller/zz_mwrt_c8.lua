module("luci.controller.zz_mwrt_c8", package.seeall)

function index()
	local mwrt = require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") == "/luci-static/argon"
	entry({"admin"}, call("action_root_home"), _("Administration"), 10)
	if mwrt then
		local home = entry({"admin", "home"}, template("zmode/modem_info"), _("C8-688 首页"), 0)
		home.dependent, home.leaf = false, false
	else
		local home = entry({"admin", "home"}, view("c2000max/home-c8"), _("C8-688 首页"), 0)
		home.acl_depends = {"luci-theme-c2000max-ui"}
	end
	entry({"admin", "status", "c8"}, call("action_c8_home")).leaf = true
	entry({"admin", "status", "nbcpe"}, call("action_c8_home")).leaf = true
	entry({"admin", "modeminfo"}, call("action_c8_home")).leaf = false
end

function action_c8_home()
	require("luci.http").redirect(require("luci.dispatcher").build_url("admin", "home"))
end

function action_root_home()
	local http = require("luci.http")
	local path = http.getenv("PATH_INFO") or ""
	-- A missing admin child must remain 404 for SSH authorization fallback.
	if path ~= "" and path ~= "/" and path ~= "/admin" and path ~= "/admin/" then
		http.status(404, "Not Found")
		http.prepare_content("text/plain")
		http.write("Not Found\n")
		return
	end
	action_c8_home()
end
