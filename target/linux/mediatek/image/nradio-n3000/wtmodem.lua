module("luci.controller.wtmodem_n3000", package.seeall)

function index()
    entry({"admin", "modem", "wtmodem"}, call("action_status"), _("WTModem · 蜂窝状态"), 20)
    entry({"admin", "modem", "wtmodem", "refresh"}, post("action_refresh")).leaf = true
    entry({"admin", "modem", "smsc"}, template("zmode/smsc"), _("WTModem · 短信管理"), 21)
    for _, name in ipairs({"smslist", "smssend", "smsdel", "smscfg", "notice_ack"}) do
        entry({"admin", "modem", name}, post("action_" .. name)).leaf = true
    end
end

local function detected_modems()
    local devices, mt5700 = {}, nil
    require("luci.model.uci").cursor():foreach("qmodem", "modem-device", function(device)
        if device.state == "enabled" then
            devices[#devices+1] = device
            if (device.name or ""):lower():find("5700", 1, true) and device.at_port then
                mt5700 = mt5700 or device
            end
        end
    end)
    return devices, mt5700
end

function action_status()
    local devices, mt5700 = detected_modems()
    local fs, json = require "nixio.fs", require "luci.jsonc"
    local snapshot = mt5700 and json.parse(fs.readfile("/tmp/mwrt_cellinfo.json") or "") or nil
    require("luci.template").render("wtmodem/status", {devices=devices, mt5700=mt5700, snapshot=snapshot})
end

function action_refresh()
    local _, mt5700 = detected_modems()
    if mt5700 then
        require("luci.sys").call("/usr/bin/mwrt-cellinfo " .. require("luci.util").shellquote(mt5700.at_port))
    end
    require("luci.http").redirect(require("luci.dispatcher").build_url("admin", "modem", "wtmodem"))
end

function action_smsdel()
    local http, sys = require "luci.http", require "luci.sys"
    local ids = http.formvalue("idx") or ""
    local ok = false
    if ids == "all" then
        ok = sys.call("/usr/bin/sendat 1 'AT+CMGD=0,4' 30 | grep -qw OK") == 0
    elseif ids:match("^%d+[%d,]*$") then
        local slots = {}
        for slot in ids:gmatch("%d+") do slots[#slots+1] = tonumber(slot) end
        table.sort(slots, function(a,b) return a>b end)
        ok = true
        for _, slot in ipairs(slots) do
            if sys.call(("/usr/bin/sendat 1 'AT+CMGD=%d' 10 | grep -qw OK"):format(slot)) ~= 0 then
                ok = false
                break
            end
        end
    end
    http.prepare_content("application/json")
    http.write_json({ok=ok})
end

function action_notice_ack()
    local http, sys = require "luci.http", require "luci.sys"
    local upto = http.formvalue("upto")
    local ok = false
    if http.formvalue("scope") == "sms" and (not upto or upto:match("^%d+$") and #upto <= 10) then
        local cmd = "/usr/bin/ucode /usr/libexec/mwrt-notice-ack.uc scope sms"
        local reply = require("luci.jsonc").parse(sys.exec(cmd .. (upto and (" upto=" .. upto) or ""))) or {}
        ok = reply.ok == true
    end
    http.prepare_content("application/json")
    http.write_json({ok=ok})
end
