module("luci.controller.myos_c8_sms", package.seeall)

function index()
    local page = entry({"admin", "modem", "smsforward"}, cbi("myos_c8_sms_forward"), _("短信转发"), 31)
    page.acl_depends = {"luci-mod-system-config"}
end
