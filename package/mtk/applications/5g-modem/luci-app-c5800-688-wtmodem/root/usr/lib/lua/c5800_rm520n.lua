-- Read-only AT support for RM520N-equipped ODU devices on the PoE uplink.
local M = {}
local fs = require "nixio.fs"
local json = require "luci.jsonc"
local nixio = require "nixio"
local cache_path = "/tmp/myos-c5800-rm520n.json"

local function query(peer, command)
    local socket = nixio.socket("inet", "stream")
    if not socket then return nil end
    if not socket:setopt("socket", "bindtodevice", "port5") or
        not socket:setopt("socket", "sndtimeo", 1, 0) or
        not socket:setopt("socket", "rcvtimeo", 1, 0) or
        not socket:bind(peer.address, 0) or not socket:connect(peer.host, 1555) or
        socket:send(command .. "\r") ~= #command + 1 then
        socket:close()
        return nil
    end

    local response, deadline = "\n", os.time() + 2
    while #response < 4096 and os.time() < deadline do
        local chunk = socket:recv(1024)
        if not chunk or chunk == "" then break end
        response = (response .. chunk):gsub("\r", "\n")
        if response:match("\nOK\n") then
            socket:close()
            return response
        end
        if response:match("\nERROR\n") or response:match("\n%+CM[ES] ERROR:") then break end
    end
    socket:close()
end

local function sample(peer)
    local identity = query(peer, "ATI")
    local model = identity and identity:match("\n%s*(RM520N[%w%-]*)[ \t]*\n")
    if not model then return nil end
    local signal = {
        name = "cpe1", device = "port5", model = model,
        revision = identity:match("\nRevision:%s*([^\n]+)")
    }
    local response = query(peer, 'AT+QENG="servingcell"')
    if not response then return signal end
    local lte, nr
    for line in response:gmatch("\n%+QENG:%s*([^\n]+)") do
        local f = {}
        for field in (line .. ","):gmatch("(.-),") do
            f[#f + 1] = field:match('^%s*"?(.-)"?%s*$')
        end
        local i = 1
        if f[1] == "servingcell" then
            local state = f[2]
            if state == "CONNECT" or state == "NOCONN" or state == "SEARCH" or state == "LIMSRV" then
                signal.registration = state
                i = 3
            else
                i = 2
            end
        end
        local rat = f[i]
        local function fields(mcc, mnc, band, pci, arfcn, rsrp, rsrq, sinr, cell)
            local strength = tonumber(f[i + rsrp])
            if not strength or strength < -200 or strength >= 0 then return nil end
            local plmn = (f[i + mcc] or "") .. (f[i + mnc] or "")
            return { isp = plmn:match("^%d%d%d%d%d%d?$"), band = f[i + band], pci = f[i + pci],
                earfcn = f[i + arfcn], rsrp = strength, rsrq = tonumber(f[i + rsrq]),
                sinr = tonumber(f[i + sinr]), cell = cell and f[i + cell] }
        end
        if rat == "LTE" then
            lte = fields(2, 3, 7, 5, 6, 11, 12, 14, 4)
            if lte then lte.mode = "LTE" end
        elseif rat == "NR5G-SA" then
            nr = fields(2, 3, 8, 5, 7, 10, 11, 12, 4)
            if nr then nr.mode = "NR SA" end
        elseif rat == "NR5G-NSA" then
            nr = fields(1, 2, 8, 3, 7, 4, 6, 5)
            if nr then nr.mode = "NR NSA" end
        end
    end
    for key, value in pairs(nr or lte or {}) do signal[key] = value end
    return signal
end

local function cached(peer)
    if not peer then return nil end
    local entry = json.parse(fs.readfile(cache_path) or "")
    if type(entry) == "table" and entry.host == peer.host and entry.address == peer.address
        and type(entry.time) == "number" and os.time() >= entry.time then
        return entry
    end
end

function M.cached(peer)
    local entry = cached(peer)
    if entry and os.time() - entry.time < 15 then return entry.signal or nil, entry.time end
end

function M.read(peer)
    if not peer then return nil end
    local entry = cached(peer)
    if entry and os.time() - entry.time < (entry.signal and 5 or 30) then return entry.signal or nil end
    local signal = sample(peer)
    local tmp = cache_path .. "." .. nixio.getpid()
    if fs.writefile(tmp, json.stringify({
        host = peer.host, address = peer.address, time = os.time(), signal = signal or false
    })) then
        if not fs.rename(tmp, cache_path) then fs.unlink(tmp) end
    end
    return signal
end

return M
