local M = {}
function M.status()
    local file = io.open("/tmp/myos-c8-fan.json", "r")
    local raw = file and file:read(65536) or ""
    if file then file:close() end
    local data = require("cjson.safe").decode(raw) or {}
    local fresh = os.time() - (tonumber(data.sampled_at) or 0) <= 90
    return { available = fresh and data.available == true,
        ["设备温度"] = fresh and data.cpu_temperature or nil,
        ["风扇PWM"] = fresh and data.pwm or nil,
        ["风扇转速百分比"] = fresh and data.percent or nil,
        ["风扇模式"] = fresh and data.mode_label or "不可用", source = "cpu" }
end
return M
