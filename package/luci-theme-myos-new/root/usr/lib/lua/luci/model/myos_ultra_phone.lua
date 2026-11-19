local json = require("luci.jsonc")
local fs = require("nixio.fs")
local quote = require("luci.util").shellquote
local statefile = "/tmp/myos-ultra-phone/state.json"
local M = {}

function M.port()
    local at
    require("luci.model.uci").cursor():foreach("qmodem", "modem-device", function(s)
        if (s.name or ""):lower():find("mt5700", 1, true) and
            s.at_port and s.at_port:match("^/dev/tty[%w_%-]+$") then
            at = s.state == "enabled" and s.at_port or at or s.at_port
            if s.state == "enabled" then return false end
        end
    end)
    return at
end

function M.at(port, command)
    local pipe = io.popen("/usr/libexec/mt5700-web/qmodem-at " ..
        quote(port) .. " 4 " .. quote(command) .. " 2>&1")
    local response = pipe and pipe:read("*a") or ""
    if pipe then pipe:close() end
    return response, ("\n" .. response .. "\n"):match("[\r\n][ \t]*OK[ \t]*[\r\n]") ~= nil
end

local labels = { [0]="active", [1]="held", [2]="dialing", [3]="alerting", [4]="ringing", [5]="waiting" }

function M.calls(port)
    local response, confirmed = M.at(port, "AT+CLCC")
    local found = {}
    for line in response:gmatch("[^\r\n]+") do
        local id, dir, state, mode, tail = line:match("^%s*%+CLCC:%s*(%d+),%s*(%d+),%s*(%d+),%s*(%d+),%s*%d+(.*)$")
        if mode == "0" and labels[tonumber(state)] then
            found[#found + 1] = {
                id = tonumber(id), incoming = dir == "1", state = labels[tonumber(state)],
                number = tail:match('^%s*,%s*"([+%d*#]+)"')
            }
        end
    end
    if confirmed or #found > 0 then return found end
    local status = response:match("UBUS AT command failed with status:%s*(%d+)")
    if status then return nil, "AT 服务读取通话状态失败（" .. status .. "）" end
    local reason = response:match("(%+CME ERROR:%s*%d+)") or response:match("(ERROR)") or
        response:match("(NO CARRIER)")
    return nil, reason and ("模组返回 " .. reason) or "模组未返回通话状态"
end

function M.cached()
    local content = fs.readfile(statefile)
    return content and json.parse(content)
end

local function update(state, calls, now)
    local previous = {}
    for _, call in ipairs(state.calls) do previous[call.id] = call end
    for _, call in ipairs(calls) do
        local prior = previous[call.id]
        if prior and prior.incoming == call.incoming and
            (not prior.number or not call.number or prior.number == call.number) and
            (not prior.connected or call.state == "active" or call.state == "held") then
            call.number = call.number or prior.number
            call.started = prior.started
            call.connected = prior.connected
            previous[call.id] = nil
        end
        call.started = call.started or now
        if call.state == "active" or call.state == "held" then call.connected = call.connected or now end
        call.duration = call.connected and math.max(0, now - call.connected) or 0
    end
    for _, call in pairs(previous) do
        call.ended = now
        call.state = call.connected and "ended" or (call.incoming and "missed" or "unanswered")
        call.duration = call.connected and math.max(0, now - call.connected) or 0
        table.insert(state.recent, 1, call)
    end
    while #state.recent > 12 do table.remove(state.recent) end
    state.calls = calls
end

if ... == "watch" then
    local nixio = require("nixio")
    local state = M.cached() or {calls={}, recent={}}
    while true do
        local port = M.port()
        state.sampled = os.time()
        state.supported = port ~= nil
        state.error = nil
        if port then
            local calls
            calls, state.error = M.calls(port)
            if calls then update(state, calls, os.time()) end
        else
            state.calls = {}
        end
        if fs.writefile(statefile .. ".new", json.stringify(state)) then
            fs.rename(statefile .. ".new", statefile)
        end
        nixio.nanosleep(port and #state.calls > 0 and 1 or 5)
    end
end

return M
