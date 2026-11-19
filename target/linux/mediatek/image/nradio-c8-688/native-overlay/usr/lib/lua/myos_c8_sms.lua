local modem = require "myos_c8_modem"
local md5 = require "md5"
local util = require "luci.util"
local M = {}

local function seconds(value)
    local stamp = tonumber(value)
    if stamp and stamp >= 1000000000000 then stamp = math.floor(stamp / 1000) end
    return stamp
end

local function timestamp(value)
    local stamp = seconds(value)
    if stamp and stamp >= 1000000000 and stamp < 5000000000 then
        return os.date("%Y-%m-%d %H:%M:%S", stamp)
    end
    return value
end

function M.sim_identity()
    local reply = util.exec("/usr/sbin/atsd_cli -i cpe -c 'AT^ICCID?' -w 4000 2>/dev/null") or ""
    local iccid = reply:match('%^ICCID:%s*"?([%x]+)')
    if iccid and #iccid >= 18 and #iccid <= 22 then return md5.sumhexa(iccid:upper()) end
end

function M.inbox(reply)
    reply = reply or modem.sms({ action = "read" })
    local payload = reply.result
    if tonumber(reply.code) ~= 0 or type(payload) ~= "table" or tonumber(payload.ready) == 0 or
            (payload.code ~= nil and tonumber(payload.code) ~= 0) or type(payload.smslist) ~= "table" then
        return nil, "原生短信服务暂未返回收件箱"
    end
    local messages, groups = {}, {}
    for _, thread in ipairs(payload.smslist) do
        for _, item in ipairs(thread.list or { thread }) do
            if tonumber(item.role or 0) == 0 and type(item.content) == "string" then
                local sender = tostring(item.contact or thread.contact or "")
                local time = tostring(item.timestamp or thread.timestamp or "")
                local seq, total = tonumber(item.multi_sms_index), tonumber(item.multi_sms_max)
                local reference = tonumber(item.multi_sms_id)
                if reference and seq and total and total > 1 and total <= 255 and
                        seq % 1 == 0 and total % 1 == 0 and seq >= 1 and seq <= total then
                    local key = table.concat({sender, reference, total}, "\0")
                    local group = groups[key]
                    local current, first = seconds(time), group and seconds(group.time)
                    if not group or group.parts[seq] or (current and first and math.abs(current - first) > 300) then
                        group = { sender = sender, time = time, total = total, parts = {}, count = 0 }
                        groups[key] = group
                        messages[#messages + 1] = group
                    end
                    group.parts[seq], group.count = item.content, group.count + 1
                    if seq == 1 then group.time = time end
                else
                    messages[#messages + 1] = { sender = sender, time = time, text = item.content, complete = true }
                end
            end
        end
    end
    for _, message in ipairs(messages) do
        if message.parts then
            local ordered = {}
            for index = 1, message.total do
                if message.parts[index] then ordered[#ordered + 1] = message.parts[index] end
            end
            message.text, message.complete = table.concat(ordered), message.count == message.total
            message.parts, message.count, message.total = nil, nil, nil
        end
        message.key = md5.sumhexa(table.concat({message.sender, message.time, message.text}, "\0"))
        message.time = timestamp(message.time)
    end
    return messages, nil, { used = tonumber(payload.used), total = tonumber(payload.total) }
end

return M
