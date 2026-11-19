local fan = require("myos_c8_fan")
local m = Map("fanctrl", translate("C8 风扇"))
local status = m:section(SimpleSection)
status.template = "myos_c8_fan/status"
status.state = fan.status()
local s = m:section(NamedSection, "fanctrl", "fanctrl")
s.addremove = false
local mode = s:option(ListValue, "mode", translate("模式"))
mode:value("0", translate("自动"))
for index, label in ipairs({translate("低速"), translate("中速"), translate("高速"), translate("全速")}) do
    mode:value(tostring(index), string.format("%s · %d%%", label, fan.percent(fan.duties[index])))
end
mode:value("5", translate("手动 PWM"))
mode.default = "0"
local pwm = s:option(Value, "manual_pwm", translate("PWM"))
pwm.datatype = "range(0,255)"
pwm:depends("mode", "5")
local protect = s:option(Value, "protecttemp", translate("保护温度"))
protect.datatype = "range(60,100)"
protect.default = "85"
protect.description = translate("达到保护温度时使用 100% 占空比。")
local minimum = s:option(ListValue, "smartmin", translate("最低转速"))
for _, value in ipairs({ 0, 30, 50 }) do minimum:value(tostring(value), value .. "%") end
minimum:depends("mode", "0")
minimum.default = "30"
for index, default in ipairs(fan.thresholds) do
    local threshold = s:option(Value, "temp" .. index,
        string.format("%s %d · %d%%", translate("温度阈值"), index, fan.percent(fan.duties[index])))
    threshold.datatype = index == 4 and "range(30,95)" or "range(30,90)"
    threshold.default = tostring(default)
    threshold:depends("mode", "0")
    threshold.description = translate("升档按设定温度，降档需低 2 °C。最低转速设置优先。")
end
local quiet = s:option(Flag, "quiet", translate("夜间静音"))
quiet.default = "0"
quiet:depends("mode", "0")
local function quiet_time(key, label, default)
    local option = s:option(Value, key, translate(label))
    option.default = default
    option.rmempty = false
    option:depends("quiet", "1")
    function option.validate(self, value)
        if require("myos_c8_schedule").valid_time(value) then return value end
        return nil, translate("请输入 24 小时制时间，例如 23:00。")
    end
end
quiet_time("quiet_start", "静音开始", "23:00")
quiet_time("quiet_end", "静音结束", "07:00")
local days = s:option(MultiValue, "quiet_days", translate("静音星期"))
for index, label in ipairs({"周日", "周一", "周二", "周三", "周四", "周五", "周六"}) do
    days:value(tostring(index - 1), translate(label))
end
days.description = translate("留空表示每天。跨午夜按开始时间所在的星期计算。")
days:depends("quiet", "1")
local gear = s:option(ListValue, "quiet_gear", translate("静音最高档位"))
gear:value("0", translate("停转"))
gear:value("1", translate("低速"))
gear:value("2", translate("中速"))
gear.default = "1"
gear:depends("quiet", "1")
local ceiling = s:option(Value, "quiet_temp", translate("恢复自动散热温度（°C）"))
ceiling.datatype, ceiling.default = "range(40,80)", "65"
ceiling:depends("quiet", "1")
m.on_after_commit = function() os.execute("/usr/libexec/myos-c8-fan apply >/dev/null 2>&1") end
return m
