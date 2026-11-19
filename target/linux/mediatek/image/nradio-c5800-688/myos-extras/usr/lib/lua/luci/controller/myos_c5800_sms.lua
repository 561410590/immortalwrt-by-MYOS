module("luci.controller.myos_c5800_sms", package.seeall)

function index()
    local page = entry({"admin", "modem", "myos_c5800_sms"},
        cbi("myos_c5800_sms"), _("短信转发"), 91)
    page.acl_depends = {"luci-mod-system-config"}
end
