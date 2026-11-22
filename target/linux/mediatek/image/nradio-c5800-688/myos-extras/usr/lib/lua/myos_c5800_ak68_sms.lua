-- Read the AK68 MT5700 inbox through its PoE AT socket.
local M = {}
local nixio = require "nixio"
local decoder = require "myos_c5800_pdu"

local function connect(peer, host)
    local sock = nixio.socket("inet", "stream")
    if not sock then return nil end
    if sock:setopt("socket", "bindtodevice", "port5") and
        sock:setopt("socket", "sndtimeo", 1, 0) and
        sock:setopt("socket", "rcvtimeo", 2, 0) and
        sock:bind(peer.address, 0) and sock:connect(host, 20249) then return sock end
    sock:close()
end

local function read_at(sock, command, deadline)
    if sock:send(command .. "\r") ~= #command + 1 then return nil end
    local body = "\n"
    while #body < 262144 and os.time() < deadline do
        local part = sock:recv(4096)
        if not part or part == "" then break end
        body = body .. part:gsub("\r", "\n")
        local tail = body:sub(-128)
        if tail:match("\nOK%s*$") then return body, true end
        if tail:match("\nERROR%s*$") or tail:match("\n%+CM[ES] ERROR:[^\n]*%s*$") then
            return body, false
        end
    end
end

local function inbox(sock, peer)
    local deadline = os.time() + 10
    local identity, confirmed = read_at(sock, "AT^ICCID?", deadline)
    if not confirmed or identity:match("%^ICCID:%s*(%d+)") ~= peer.iccid then
        return nil, "AK68 AT 通道未确认当前 SIM"
    end
    local mode = read_at(sock, "AT+CMGF?", deadline)
    if not mode or tonumber(mode:match("%+CMGF:%s*(%d+)")) ~= 0 then
        return nil, "AK68 副模组当前未提供 PDU 短信读取"
    end
    local memory, storage
    local info = read_at(sock, "AT+CPMS?", deadline)
    if info then
        local count, capacity
        memory, count, capacity = info:match('%+CPMS:%s*"([^"]+)"%s*,%s*(%d+)%s*,%s*(%d+)')
        if memory then storage = {name=memory, used=tonumber(count), total=tonumber(capacity)} end
    end
    if storage and storage.used == 0 then return {}, nil, storage end

    local raw, listed = read_at(sock, "AT+CMGL=4", deadline)
    if listed and raw:find("+CMGL:", 1, true) then return decoder.decode(raw), nil, storage end
    if not raw or not storage or storage.total < storage.used then
        return nil, "AK68 未返回完整短信列表", storage
    end

    -- Some AT proxies omit CMGL rows. Read the occupied slots individually.
    local rows = {}
    for index = 0, storage.total do
        if os.time() >= deadline then break end
        local reply, ok = read_at(sock, "AT+CMGR=" .. index, deadline)
        if not reply then break end
        if ok then
            local header, pdu = reply:match("\n%+CMGR:%s*([^\n]+)\n+([%x]+)%s*\n")
            if header and pdu then
                rows[#rows + 1] = "+CMGL: " .. index .. "," .. header .. "\n" .. pdu
                if #rows == storage.used then return decoder.decode(table.concat(rows, "\n")), nil, storage end
            end
        end
    end
    return nil, "AK68 短信列表未能完整补读", storage
end

-- Diagnostics share the same port-bound AT transport as the inbox.
-- An ODU can expose its AT socket at its management IP or at its modem IP.
function M.identity(peer)
    local sock = connect(peer, peer.host)
    if not sock and peer.host ~= "192.168.8.1" then sock = connect(peer, "192.168.8.1") end
    if not sock then return nil end
    local data, deadline = {}, os.time() + 8
    for _, field in ipairs({{"model","CGMM"},{"vendor","CGMI"},{"fw","CGMR"},{"imei","CGSN"}}) do
        local command = "AT+" .. field[2]
        local raw, ok = read_at(sock, command, deadline)
        if not raw then break end
        if ok then
            for line in raw:gmatch("[^\r\n]+") do
                line = line:match("^%s*(.-)%s*$")
                if line ~= "" and line ~= command and line ~= "OK" then
                    data[field[1]] = line:gsub("^%+%u+:%s*", ""):gsub('^"(.-)"$', "%1")
                    break
                end
            end
        end
    end
    sock:close()
    if data.model then return data end
end

function M.read(peer)
    if not peer then return nil, "副模组短信列表暂不可读" end
    local sock = connect(peer, peer.host)
    if not sock and peer.host ~= "192.168.8.1" then sock = connect(peer, "192.168.8.1") end
    if not sock then return nil, "AK68 的 AT 短信通道不可达" end
    local messages, err, storage = inbox(sock, peer)
    sock:close()
    return messages, err, storage
end

return M
