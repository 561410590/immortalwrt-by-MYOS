module("luci.controller.modem", package.seeall)

function index()
	entry({"admin", "modem"}, firstchild(), _("内置蜂窝"), 25).dependent = false
	entry({"admin", "modem", "nets"}, template("myos_c8_modem/status"), _("模组状态"), 10)
	entry({"admin", "modem", "modem"}, template("myos_c8_modem/settings"), _("模组设置"), 20)
	entry({"admin", "modem", "smsc"}, template("myos_c8_modem/sms"), _("短信"), 30)
	entry({"admin", "modem", "at"}, template("myos_c8_modem/diagnostics"), _("网络调试"), 40)
	entry({"admin", "modem", "status"}, call("action_status")).leaf = true
	entry({"admin", "modem", "cpe_status"}, call("action_dashboard")).leaf = true
	entry({"admin", "modem", "get_csq"}, call("action_legacy")).leaf = true
	entry({"admin", "modem", "atinfo"}, call("action_legacy")).leaf = true
	entry({"admin", "modem", "settings"}, call("action_settings")).leaf = true
	entry({"admin", "modem", "request"}, post("action_request")).leaf = true
end

local function reply(result)
	local http = require "luci.http"
	http.prepare_content("application/json")
	http.write_json(result)
end

function action_status()
	reply(require("myos_c8_modem").status())
end

function action_dashboard()
	reply(require("myos_c8_modem").dashboard())
end

function action_legacy()
	reply(require("myos_c8_modem").legacy_status())
end

function action_settings()
	local cursor = require("luci.model.uci").cursor()
	local settings = cursor:get_all("myos_c8_modem", "main") or {}
	for key in pairs(settings) do if key:sub(1, 1) == "." then settings[key] = nil end end
	reply({ ok = true, settings = settings })
end

function action_request()
	local http = require "luci.http"
	local raw = http.formvalue("request") or ""
	local request = #raw <= 65536 and require("luci.jsonc").parse(raw) or nil
	if type(request) ~= "table" then
		http.status(400, "Bad Request")
		reply({ ok = false, error = "Invalid modem request" })
		return
	end
	local result = require("myos_c8_modem").request(request)
	if result.ok == false then http.status(400, "Bad Request") end
	reply(result)
end
