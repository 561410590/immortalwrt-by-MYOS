local m = Map("fanctrl", translate("C8 风扇"))
local s = m:section(NamedSection, "fanctrl", "fanctrl")
s.addremove = false
local source = s:option(DummyValue, "_temperature_source", translate("温度来源"))
source.cfgvalue = function() return "CPU" end
local mode = s:option(ListValue, "mode", translate("模式"))
mode:value("0", translate("自动"))
mode:value("1", translate("低速"))
mode:value("2", translate("中速"))
mode:value("3", translate("高速"))
mode:value("4", translate("全速"))
mode:value("5", translate("手动 PWM"))
mode.default = "0"
local pwm = s:option(Value, "manual_pwm", translate("PWM"))
pwm.datatype = "range(0,255)"
pwm:depends("mode", "5")
local protect = s:option(Value, "protecttemp", translate("保护温度"))
protect.datatype = "range(60,100)"
protect.default = "85"
local minimum = s:option(ListValue, "smartmin", translate("最低转速"))
for _, value in ipairs({ 0, 30, 50 }) do minimum:value(tostring(value), value .. "%") end
minimum:depends("mode", "0")
minimum.default = "30"
for index, default in ipairs({ 50, 60, 70, 80 }) do
    local threshold = s:option(Value, "temp" .. index, translate("温度阈值") .. " " .. index)
    threshold.datatype = "range(30,90)"
    threshold.default = tostring(default)
    threshold:depends("mode", "0")
end
m.on_after_commit = function() os.execute("/usr/libexec/myos-c8-fan apply >/dev/null 2>&1") end
return m
