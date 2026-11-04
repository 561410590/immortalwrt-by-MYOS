local uci = require("luci.model.uci").cursor()

local m = Map("Mwrt-Assurance", translate("MWRT WAN Failover"),
    translate("配置并实时查看多WAN自动切换状态（设置并实时查看多WAN自动切换的运行状态）"))

-- Tab 1: 配置
local s = m:section(NamedSection, "wan_failover", "settings", translate("Failover Settings"))
s.tab = "config"

local enabled = s:option(Flag, "enabled", translate("Enable Failover（启用功能）"))
enabled.default = "0"
enabled.rmempty = false

local primary_if = s:option(Value, "primary_if", translate("Primary WAN（主WAN口）"))
primary_if.default = "BLUE4"

local secondary_if = s:option(Value, "secondary_if", translate("Secondary WAN（备WAN口）"))
secondary_if.default = "eth1"

local ping_target = s:option(Value, "ping_target", translate("Ping Target（目标地址）"))
ping_target.default = "223.5.5.5"

local check_interval = s:option(Value, "check_interval", translate("Check Interval (sec)（间隔）"))
check_interval.datatype = "uinteger"
check_interval.default = "5"

local fail_threshold = s:option(Value, "fail_threshold", translate("Fail Threshold（失败次数）"))
fail_threshold.datatype = "uinteger"
fail_threshold.default = "2"

local success_threshold = s:option(Value, "success_threshold", translate("Success Threshold（成功判定次数）"))
success_threshold.datatype = "uinteger"
success_threshold.default = "2"

local force_primary = s:option(Flag, "force_primary", translate("Force Primary WAN(主可用时恢复)"))
force_primary.default = "1"
force_primary.rmempty = false

-- Tab 2: 状态页
local status = m:section(SimpleSection)
status.tab = "status"

return m
