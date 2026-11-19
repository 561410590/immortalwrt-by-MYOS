local m = Map("myos_c8_sms", translate("短信转发"),
    translate("每张 SIM 首次读取只记录现有短信，随后将新收到的完整短信发送到指定 Webhook。"))
local s = m:section(NamedSection, "main", "service")
local enabled = s:option(Flag, "enabled", translate("启用转发"))
enabled.rmempty = false
local url = s:option(Value, "webhook", "Webhook URL")
url.password = true
url.rmempty = false
url:depends("enabled", "1")
function url.validate(self, value)
    if #value <= 2048 and value:match("^https?://[^%s]+$") then return value end
    return nil, translate("请输入有效的 HTTP 或 HTTPS 地址。")
end
local status = s:option(DummyValue, "_status", translate("最近转发状态"))
function status.cfgvalue()
    local fs = require "nixio.fs"
    if m.uci:get("myos_c8_sms", "main", "enabled") ~= "1" then return translate("已关闭") end
    return fs.readfile("/tmp/myos-c8-sms.status") or translate("等待读取")
end

function m.on_after_commit()
    require("luci.sys").call("/etc/init.d/myos-c8-sms restart >/dev/null 2>&1")
end

return m
