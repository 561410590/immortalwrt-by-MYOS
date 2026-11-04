module("luci.controller.myos_c8_fan", package.seeall)
function index()
    entry({"admin", "status", "c8_fan"}, cbi("myos_c8_fan"), _("C8 风扇"), 85).dependent = false
    entry({"admin", "myos_c8_fan", "set"}, call("set_mode")).leaf = true
    entry({"admin", "myos_c8_fan", "status"}, call("status")).leaf = true
end
function status()
    local http = require("luci.http")
    http.prepare_content("application/json")
    http.write_json(require("myos_c8_fan").status())
end
function set_mode()
    local http = require("luci.http")
    if http.getenv("REQUEST_METHOD") ~= "POST" then http.status(405, "Method Not Allowed"); return end
    local dispatcher = require("luci.dispatcher")
    if type(dispatcher.test_post_security) ~= "function" or not dispatcher.test_post_security() then return end
    local modes = { fanst4 = 0, fanstop = 1, fanst1 = 2, fanst2 = 3, fanst3 = 4, fanswj = 5 }
    local mode = modes[http.formvalue("set")]
    local pwm = tonumber(http.formvalue("p"))
    if mode == nil or (mode == 5 and (not pwm or pwm % 1 ~= 0 or pwm < 0 or pwm > 255)) then
        http.status(400, "Invalid fan mode"); return
    end
    local uci = require("luci.model.uci").cursor()
    if not uci:get("fanctrl", "fanctrl") then uci:section("fanctrl", "fanctrl", "fanctrl", {}) end
    uci:set("fanctrl", "fanctrl", "mode", tostring(mode))
    if mode == 5 then uci:set("fanctrl", "fanctrl", "manual_pwm", tostring(pwm)) end
    if not uci:commit("fanctrl") then http.status(500, "Cannot save fan settings"); return end
    os.execute("/usr/libexec/myos-c8-fan apply >/dev/null 2>&1")
    status()
end
