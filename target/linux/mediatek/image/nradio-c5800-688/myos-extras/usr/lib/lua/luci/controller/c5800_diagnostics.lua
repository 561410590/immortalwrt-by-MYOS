module("luci.controller.c5800_diagnostics", package.seeall)

function index()
    entry({"admin","modem","incoming_calls"}, call("calls_page"), _("来电与未接记录"), 94)
    entry({"admin","modem","incoming_state"}, call("calls_state")).leaf = true
    entry({"admin","modem","ak68_diagnostics"}, call("secondary_page"), _("AK68 副模组诊断"), 103)
end

local function snapshot(full)
    local fs = require "nixio.fs"
    local json = require "luci.jsonc"
    local state = json.parse(fs.readfile("/tmp/myos-c5800-calls.json") or "") or {}
    if not state.updated or math.abs(os.time() - state.updated) > 25 then
        state.available, state.ringing, state.ims, state.registered = false, {}, nil, nil
    end
    local history = json.parse(fs.readfile("/etc/myos-c5800-missed.json") or "") or {}
    state.latest = history[1]
    if full then state.history = history end
    return state
end

function calls_state()
    local http = require "luci.http"
    http.header("Cache-Control", "no-store")
    http.prepare_content("application/json")
    http.write_json(snapshot(http.formvalue("history") == "1"))
end

function calls_page()
    require("luci.template").render("zmode/c5800_diagnostics", {calls=snapshot(true)})
end

function secondary_page()
    require("luci.template").render("zmode/c5800_diagnostics", {odu=require("c5800_secondary").diagnostics()})
end
