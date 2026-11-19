local m = Map("myos_c5800_sms", translate("短信转发"),
    "每分钟读取内置与副模组的短信。每张 SIM 首次读取只建立基线，后续新短信发送到配置的 Webhook。")
local s = m:section(NamedSection, "main", "service")
s:option(Flag, "enabled", translate("启用"))
local url = s:option(Value, "webhook", "Webhook URL")
url.password = true
url:depends("enabled", "1")
function url.validate(self, value)
    if #value <= 2048 and value:match("^https?://[^%s]+$") then return value end
    return nil, "请输入有效的 HTTP 或 HTTPS 地址。"
end

local status = s:option(DummyValue, "_status", "最近读取")
function status.cfgvalue()
    local fs, json = require "nixio.fs", require "luci.jsonc"
    local state = json.parse(fs.readfile("/tmp/myos-c5800-sms.status") or "") or {}
    local function describe(source)
        local result = state[source] or {}
        return result.error or (result.count and ("读到 " .. result.count .. " 条")) or "等待采样"
    end
    return "内置：" .. describe("cpe") .. "；副模组：" .. describe("cpe1")
end

function m.on_after_commit()
    require("luci.sys").call("/etc/init.d/myos-c5800-sms restart >/dev/null 2>&1")
end

return m
