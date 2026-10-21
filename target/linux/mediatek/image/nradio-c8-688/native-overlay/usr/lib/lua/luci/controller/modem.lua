module("luci.controller.modem", package.seeall)

function index()
	entry({"admin", "modem"}, firstchild(), _("蜂窝网络"), 25).dependent = false
	entry({"admin", "modem", "nets"}, template("myos_c8_modem/status"), _("蜂窝状态"), 10)
	entry({"admin", "modem", "settings"}, form("myos_c8_modem"), _("蜂窝设置"), 20)
	entry({"admin", "modem", "modem"}, alias("admin", "modem", "settings")).leaf = true
	entry({"admin", "modem", "smsc"}, form("myos_c8_sms"), _("短信"), 30)
	entry({"admin", "modem", "at"}, template("myos_c8_modem/diagnostics"), _("网络诊断"), 40)
	entry({"admin", "network", "c8_modem"}, alias("admin", "modem", "settings")).leaf = true
	entry({"admin", "network", "mwrt_assurance_status"}, call("action_gateway")).leaf = true
	entry({"admin", "modem", "status"}, call("action_status")).leaf = true
	entry({"admin", "modem", "cpe_status"}, call("action_dashboard")).leaf = true
	entry({"admin", "modem", "get_csq"}, call("action_legacy")).leaf = true
	entry({"admin", "modem", "atinfo"}, call("action_legacy")).leaf = true
	entry({"admin", "modem", "simstat"}, call("action_simstat")).leaf = true
	entry({"admin", "modem", "qmodem", "get_modem_cfg"}, call("action_cfg")).leaf = true
	entry({"admin", "modem", "request"}, post("action_request")).leaf = true
	entry({"admin", "modem-AK68", "get_csq"}, call("action_legacy")).leaf = true
	entry({"admin", "modem-AK68", "nets"}, alias("admin", "modem", "nets")).leaf = true
	entry({"admin", "modem-AK68", "webui"}, alias("admin", "modem", "settings")).leaf = true
	entry({"admin", "modeminfo", "cellx"}, call("action_cellx")).leaf = true
end

local function reply(data)
	local http = require "luci.http"
	http.prepare_content("application/json")
	http.header("Cache-Control", "no-store")
	http.write_json(data)
end

function read_status()
	local json = require "luci.jsonc"
	local fs = require "nixio.fs"
	local path = "/tmp/myos-cellular.json"
	local function unavailable(state)
		return { schema = 1, cache_state = state, stale = true, registered = json.null,
			data_connected = json.null, modem = {}, sim = {}, signal = {}, cell = {},
			network = {}, settings = {}, registration = { registered = json.null },
			session = { connected = json.null } }
	end
	local info, code = fs.stat(path)
	if not info then return unavailable(tonumber(code) == 2 and "missing" or "read_error") end
	local file = io.open(path, "r")
	if not file then return unavailable("read_error") end
	local raw = file:read(1048577)
	file:close()
	if not raw then return unavailable("read_error") end
	local data = #raw <= 1048576 and json.parse(raw) or nil
	if type(data) ~= "table" or data.schema ~= 1 or not tonumber(data.sampled_at) then
		return unavailable("invalid")
	end
	for _, key in ipairs({ "modem", "sim", "signal", "cell", "network", "settings", "registration", "session" }) do
		if type(data[key]) ~= "table" then return unavailable("invalid") end
	end
	local now, sampled, source_age = os.time(), tonumber(data.sampled_at), tonumber(data.cache_age)
	local missing_age = data.cache_age == nil or data.cache_age == json.null
	if sampled <= 0 or sampled > now or (source_age and source_age < 0)
		or (not source_age and not (missing_age and data.stale == true)) then return unavailable("invalid") end
	data.age = now - sampled
	data.stale = data.stale ~= false or data.age > 90 or not source_age or source_age + data.age > 90
	data.cache_state = data.stale and "stale" or "fresh"
	if type(data.registered) ~= "boolean" then data.registered = json.null end
	-- A missing registration sample cannot prove that the data session is down.
	if data.stale or type(data.registered) ~= "boolean" then
		data.registered, data.data_connected = json.null, json.null
	elseif type(data.data_connected) ~= "boolean" then
		data.data_connected = json.null
	end
	data.registration.registered, data.session.connected = data.registered, data.data_connected
	return data
end

function action_status() reply(read_status()) end
function action_dashboard() reply(require("myos_c8_modem").dashboard()) end
function action_legacy() reply(require("myos_c8_modem").legacy_status()) end
function action_gateway() reply(require("myos_c8_modem").gateway_status()) end
function action_cfg()
	local data = require("myos_c8_modem").status()
	reply({ cfgs = { { cfg = "cpe", name = (data.modem or {}).model or "C8-688" } } })
end
function action_simstat()
	local d = require("myos_c8_modem").status()
	local sim = d.sim or {}
	reply({ slot = sim.slot ~= "" and sim.slot or "unknown", sim_label = sim.label or "", carrier = d.operator,
		current_slot = ({ ["1"] = "external1", ["2"] = "external2", ["3"] = "internal_unicom", ["4"] = "internal_mobile" })[sim.slot] or "unknown", count = 4, stale = d.stale })
end
function action_cellx()
	local d = require("myos_c8_modem").status()
	reply({ supported = false, stale = d.stale, source = "official infocd", sampled_at = d.sampled_at })
end
function action_request()
	local http = require "luci.http"
	local raw = http.formvalue("request") or ""
	local request = #raw <= 4096 and require("luci.jsonc").parse(raw) or nil
	if type(request) ~= "table" or not ({ select_sim = true, reconnect = true, settings = true, ip_passthrough = true })[request.action] then
		http.status(400, "Bad Request")
		reply({ ok = false, error = "Invalid module-settings request" })
		return
	end
	if request.action == "select_sim" then request.origin = "module_settings" end
	local result = require("myos_c8_modem").request(request)
	if result.ok == false then http.status(400, "Bad Request") end
	reply(result)
end
