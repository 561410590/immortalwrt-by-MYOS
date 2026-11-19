local M = {}

local function minutes(value)
    local hour, minute = (value or ""):match("^(%d%d?):(%d%d?)$")
    hour, minute = tonumber(hour), tonumber(minute)
    if hour and hour < 24 and minute < 60 then return hour * 60 + minute end
end

function M.valid_time(value)
    return (value or ""):match("^%d%d:%d%d$") and minutes(value) ~= nil
end

function M.active(start_time, end_time, weekdays)
    local first, last = minutes(start_time), minutes(end_time)
    if not first or not last or first == last then return false end
    local now = os.date("*t")
    local current, day = now.hour * 60 + now.min, now.wday - 1
    if first < last then
        if current < first or current >= last then return false end
    elseif current < last then
        day = (day + 6) % 7
    elseif current < first then
        return false
    end
    if not weekdays or weekdays == "" then return true end
    for value in weekdays:gmatch("%S+") do
        if tonumber(value) == day then return true end
    end
    return false
end

return M
