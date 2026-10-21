local m = Map("Mwrt-Assurance", translate("网关存活"),
    translate("宽带（主 WAN 口）断网时自动把默认路由切到备用口（蜂窝），宽带恢复后再切回来。运行情况见“存活状态”页。"))

-- 网口角色提示(单网口机型默认作 LAN 口用等, 见 controller 的 hints)。
-- 放在 render 里算: model 在 POST 解析之前就执行了, 那时 m.uci 里还是旧值;
-- render 在解析之后, 刚点"保存并应用"时提示也是按新值算的。
local hint = m:section(SimpleSection)
function hint.render(self)
    local ctl = require "luci.controller.mwrt-assurance"
    local color = { info = "37,99,235", warn = "217,119,6" }
    for _, h in ipairs(ctl.hints(m.uci)) do
        local rgb = color[h.level] or color.info
        luci.http.write(string.format(
            '<div style="border:1px solid rgba(%s,.4);background:rgba(%s,.08);border-radius:8px;padding:10px 14px;margin:0 0 12px;font-size:13px;line-height:1.7">%s</div>',
            rgb, rgb, luci.util.pcdata(h.text)))
    end
end

local s = m:section(NamedSection, "wan_failover", "settings", translate("设置"))

local enabled = s:option(Flag, "enabled", translate("启用网关存活"),
    translate("关闭后不再自动切换线路，指示灯照常显示联网状态。"))
enabled.default = "1"
enabled.rmempty = false

local primary_if = s:option(Value, "primary_if", translate("主 WAN 口"),
    translate("接宽带的网卡名，例如 eth1。"))
primary_if.default = "eth0"

local secondary_if = s:option(Value, "secondary_if", translate("备用口"),
    translate("蜂窝模组的网卡名，例如 eth2。"))
secondary_if.default = "eth2"

local ping_target = s:option(Value, "ping_target", translate("检测地址"),
    translate("定时探测这个地址，判断线路是否可用。"))
ping_target.default = "223.5.5.5"

local check_interval = s:option(Value, "check_interval", translate("检测间隔（秒）"))
check_interval.datatype = "uinteger"
check_interval.default = "5"

local fail_threshold = s:option(Value, "fail_threshold", translate("切换前连续失败次数"))
fail_threshold.datatype = "uinteger"
fail_threshold.default = "2"

local success_threshold = s:option(Value, "success_threshold", translate("恢复判定次数"))
success_threshold.datatype = "uinteger"
success_threshold.default = "2"

local force_primary = s:option(Flag, "force_primary", translate("主口恢复后自动切回"))
force_primary.default = "1"
force_primary.rmempty = false

return m
