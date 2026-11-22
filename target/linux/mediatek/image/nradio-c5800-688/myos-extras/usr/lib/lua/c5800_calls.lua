-- MT5700 incoming calls, following MWRT's CLCC ringing/answered/idle transitions.
local M = {}

function M.parse(raw)
    local calls, complete = {}, false
    for line in (raw or ""):gmatch("[^\r\n]+") do
        line = line:match("^%s*(.-)%s*$")
        if line == "OK" then
            complete = true
        elseif line:match("^%+CLCC:") then
            local id, dir, state, mode, tail = line:match("^%+CLCC:%s*(%d+),%s*(%d+),%s*(%d+),%s*(%d+),%s*%d+(.*)$")
            if not id then return nil end
            if dir == "1" and mode == "0" then
                local number = tail:match('^,%s*"([%d+*#]*)"') or ""
                calls[id] = {state=tonumber(state), number=number}
            end
        elseif line ~= "AT+CLCC" and line ~= "" then
            return nil
        end
    end
    if complete then return calls end
end

function M.update(tracked, calls, now, new_id)
    local missed = {}
    for slot, call in pairs(calls) do
        local entry = tracked[slot]
        if call.state == 4 or call.state == 5 then
            if not entry or entry.answered then
                entry = {id=new_id(), time=now, number=call.number}
                tracked[slot] = entry
            end
            if call.number ~= "" then entry.number = call.number end
        elseif entry and (call.state == 0 or call.state == 1) then
            entry.answered = true
        end
        if entry then entry.absent = 0 end
    end
    for slot, entry in pairs(tracked) do
        if not calls[slot] then
            entry.absent = (entry.absent or 0) + 1
            if entry.absent >= 2 then
                if not entry.answered then
                    missed[#missed+1] = {id=entry.id, time=entry.time, number=entry.number}
                end
                tracked[slot] = nil
            end
        end
    end
    return missed
end

return M
