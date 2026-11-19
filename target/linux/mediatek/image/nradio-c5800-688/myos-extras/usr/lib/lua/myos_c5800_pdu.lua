-- GSM SMS-DELIVER decoding adapted from the MWRT C5800 SMS inbox.
local M = {}

local GSM_BASIC_STR =
    "@£$¥èéùìòÇ\nØø\rÅå" ..
    "Δ_ΦΓΛΩΠΨΣΘΞ\27ÆæßÉ" ..
    " !\"#¤%&'()*+,-./0123456789:;<=>?" ..
    "¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§" ..
    "¿abcdefghijklmnopqrstuvwxyzäöñüà"

local GSM_BASIC = {}
for ch in GSM_BASIC_STR:gmatch("[%z\1-\127\194-\244][\128-\191]*") do
    GSM_BASIC[#GSM_BASIC + 1] = ch
end

local GSM_EXT = {
    [0x0A] = "\f", [0x14] = "^", [0x28] = "{", [0x29] = "}", [0x2F] = "\\",
    [0x3C] = "[", [0x3D] = "~", [0x3E] = "]", [0x40] = "|", [0x65] = "\226\130\172",
}

local function utf8char(cp)
    if cp < 0x80 then
        return string.char(cp)
    elseif cp < 0x800 then
        return string.char(0xC0 + math.floor(cp / 64), 0x80 + cp % 64)
    elseif cp < 0x10000 then
        return string.char(0xE0 + math.floor(cp / 4096),
            0x80 + math.floor(cp / 64) % 64, 0x80 + cp % 64)
    end
    return string.char(0xF0 + math.floor(cp / 262144),
        0x80 + math.floor(cp / 4096) % 64,
        0x80 + math.floor(cp / 64) % 64, 0x80 + cp % 64)
end

local function hexbyte(s, pos)
    -- pos 从 0 开始(与 python 版切片下标一致)
    local v = tonumber(s:sub(pos + 1, pos + 2), 16)
    if not v or #s:sub(pos + 1, pos + 2) < 2 then error("bad pdu") end
    return v
end

local function swap_semi(h)
    local t = {}
    for i = 1, #h - 1, 2 do
        t[#t + 1] = h:sub(i + 1, i + 1) .. h:sub(i, i)
    end
    return table.concat(t)
end

local function decode_7bit(data, septets, skip)
    local bits, val = 0, 0
    local chars = {}
    for _, b in ipairs(data) do
        val = val + b * 2 ^ bits
        bits = bits + 8
        while bits >= 7 do
            chars[#chars + 1] = val % 128
            val = math.floor(val / 128)
            bits = bits - 7
            if #chars >= septets then break end
        end
        if #chars >= septets then break end
    end
    local out = {}
    local ext = false
    for i = (skip or 0) + 1, #chars do
        local c = chars[i]
        if ext then
            out[#out + 1] = GSM_EXT[c] or GSM_BASIC[c + 1] or "?"
            ext = false
        elseif c == 0x1B then
            ext = true
        else
            out[#out + 1] = GSM_BASIC[c + 1] or "?"
        end
    end
    return table.concat(out)
end

local function decode_address(pdu, pos)
    local addr_len = hexbyte(pdu, pos); pos = pos + 2
    local toa = hexbyte(pdu, pos); pos = pos + 2
    local n = math.floor((addr_len + 1) / 2)
    local digits = pdu:sub(pos + 1, pos + n * 2)
    local addr
    if math.floor(toa / 16) % 8 == 5 then
        local raw = {}
        for i = 0, #digits - 2, 2 do
            raw[#raw + 1] = tonumber(digits:sub(i + 1, i + 2), 16) or 0
        end
        addr = decode_7bit(raw, math.floor(addr_len * 4 / 7), 0)
    else
        addr = swap_semi(digits):sub(1, addr_len):gsub("[fF]", "")
        if math.floor(toa / 16) % 8 == 1 then addr = "+" .. addr end
    end
    return addr, pos + n * 2
end

local function decode_scts(pdu, pos)
    local ts = swap_semi(pdu:sub(pos + 1, pos + 14))
    local function f(a) return tonumber(ts:sub(a, a + 1), 10) end
    local yy, mo, dd, hh, mi, ss = f(1), f(3), f(5), f(7), f(9), f(11)
    if yy and mo and dd and hh and mi and ss and #ts >= 12
        and not ts:sub(1, 12):find("[^0-9]") then
        local year = yy < 90 and 2000 + yy or 1900 + yy
        return string.format("%04d-%02d-%02d %02d:%02d:%02d", year, mo, dd, hh, mi, ss), pos + 14
    end
    return "", pos + 14
end

local function ucs2_to_utf8(bytes, first)
    local out = {}
    local i = first
    local n = #bytes
    while i <= n do
        if i == n then
            out[#out + 1] = "\239\191\189"
            break
        end
        local u = bytes[i] * 256 + bytes[i + 1]
        i = i + 2
        if u >= 0xD800 and u <= 0xDBFF then
            local lo = (i + 1 <= n) and (bytes[i] * 256 + bytes[i + 1]) or nil
            if lo and lo >= 0xDC00 and lo <= 0xDFFF then
                i = i + 2
                out[#out + 1] = utf8char(0x10000 + (u - 0xD800) * 1024 + (lo - 0xDC00))
            else
                out[#out + 1] = "\239\191\189"
            end
        elseif u >= 0xDC00 and u <= 0xDFFF then
            out[#out + 1] = "\239\191\189"
        else
            out[#out + 1] = utf8char(u)
        end
    end
    return table.concat(out)
end

local function decode_pdu_inner(pdu)
    pdu = pdu:gsub("[^0-9a-fA-F]", ""):upper()
    if #pdu < 4 then return nil end
    local smsc_len = hexbyte(pdu, 0)
    local pos = 2 + smsc_len * 2
    local first = hexbyte(pdu, pos); pos = pos + 2
    if first % 4 ~= 0 then return nil end
    local udhi = math.floor(first / 64) % 2 == 1
    local sender
    sender, pos = decode_address(pdu, pos)
    pos = pos + 2
    local dcs = hexbyte(pdu, pos); pos = pos + 2
    local dt
    dt, pos = decode_scts(pdu, pos)
    local udl = hexbyte(pdu, pos); pos = pos + 2
    local ud_hex = pdu:sub(pos + 1)
    local ud = {}
    for i = 1, #ud_hex - 1, 2 do
        ud[#ud + 1] = tonumber(ud_hex:sub(i, i + 1), 16)
    end
    local ref, total, seq
    local udh_len = 0
    if udhi and #ud > 0 then
        udh_len = ud[1]
        local i = 0
        -- hdr = ud[1:1+udh_len](python 切片, 越界自动截断)
        local function h(k) -- k 从 0 开始
            if k < udh_len then return ud[2 + k] end
            return nil
        end
        while i + 1 < udh_len do
            local iei, ielen = h(i), h(i + 1)
            if not iei or not ielen then break end
            local d = {}
            for k = 0, ielen - 1 do
                local v = h(i + 2 + k)
                if v == nil then break end
                d[#d + 1] = v
            end
            if iei == 0x00 and #d >= 3 then
                ref, total, seq = d[1], d[2], d[3]
            elseif iei == 0x08 and #d >= 4 then
                ref, total, seq = d[1] * 256 + d[2], d[3], d[4]
            end
            i = i + 2 + ielen
        end
    end
    local coding = math.floor(dcs / 4) % 4
    local text
    if coding == 2 then
        local start = udhi and (2 + udh_len) or 1
        text = ucs2_to_utf8(ud, start)
    elseif coding == 1 then
        local start = udhi and (2 + udh_len) or 1
        local o = {}
        for k = start, #ud do o[#o + 1] = utf8char(ud[k]) end
        text = table.concat(o)
    else
        local skip = udhi and math.floor(((udh_len + 1) * 8 + 6) / 7) or 0
        text = decode_7bit(ud, udl, skip)
    end
    return { sender = sender, dt = dt, text = text, ref = ref, total = total, seq = seq }
end

local function decode_pdu(pdu)
    local ok, info = pcall(decode_pdu_inner, pdu)
    if ok then return info end
    return nil
end

local function trim(s) return (s:gsub("^%s+", ""):gsub("%s+$", "")) end

local function parse_cmgl(raw)
    local lines = {}
    raw = raw:gsub("\r", "")
    for l in (raw .. "\n"):gmatch("([^\n]*)\n") do lines[#lines + 1] = l end
    local out = {}
    local i = 1
    while i <= #lines do
        local line = trim(lines[i])
        if line:sub(1, 6) == "+CMGL:" then
            local rest = line:sub(7)
            local f = {}
            for fld in (rest .. ","):gmatch("([^,]*),") do f[#f + 1] = fld end
            local idx = tonumber(trim(f[1] or ""), 10)
            local stat = tonumber(trim(f[2] or ""), 10)
            local j = i + 1
            while j <= #lines do
                local cand = trim(lines[j])
                if cand ~= "" and not cand:find("[^0-9a-fA-F]") then
                    if idx and (stat == 0 or stat == 1) then out[#out + 1] = { idx, stat, cand } end
                    i = j
                    break
                end
                if cand:sub(1, 6) == "+CMGL:" then break end
                j = j + 1
            end
        end
        i = i + 1
    end
    return out
end

local function collect(raw)
    local parts = {}
    for _, e in ipairs(parse_cmgl(raw)) do
        local info = decode_pdu(e[3])
        if not info then return nil, "模组返回了无法解码的短信" end
        info.idx = e[1]
        info.stat = e[2]
        parts[#parts + 1] = info
    end
    local groups, by_ref = {}, {}
    for _, p in ipairs(parts) do
        if p.ref ~= nil and p.total and p.seq and p.seq >= 1 and p.seq <= p.total then
            local key = p.sender .. "\0" .. p.ref .. "\0" .. p.total
            local g = by_ref[key]
            if not g or #g.items >= p.total or g.sequences[p.seq] then
                g = { sender = p.sender, dt = p.dt, items = {}, indices = {}, sequences = {} }
                by_ref[key] = g
                groups[#groups + 1] = g
            end
            g.sequences[p.seq] = true
            g.items[#g.items + 1] = p
            g.indices[#g.indices + 1] = p.idx
        else
            groups[#groups + 1] = { sender = p.sender, dt = p.dt, items = { p }, indices = { p.idx } }
        end
    end
    for _, g in ipairs(groups) do
        local m = g.indices[1]
        for _, v in ipairs(g.indices) do if v < m then m = v end end
        g.min = m
    end
    table.sort(groups, function(a, b) return a.min < b.min end)
    return groups
end

local function merge(g)
    local items = {}
    for k, it in ipairs(g.items) do it.ord = k; items[k] = it end
    table.sort(items, function(a, b)
        local sa = (a.seq and a.seq ~= 0) and a.seq or 1
        local sb = (b.seq and b.seq ~= 0) and b.seq or 1
        if sa ~= sb then return sa < sb end
        return a.ord < b.ord
    end)
    local texts = {}
    for k, it in ipairs(items) do texts[k] = it.text end
    local dt = items[1].dt
    if dt == nil or dt == "" then dt = g.dt end
    local indices = {}
    for k, v in ipairs(g.indices) do indices[k] = v end
    table.sort(indices)
    return items, table.concat(texts), dt, indices
end

function M.decode(raw)
    local groups, message = collect(raw)
    if not groups then return nil, message end
    local messages = {}
    for _, group in ipairs(groups) do
        local items, body, time, indices = merge(group)
        local unread = false
        for _, item in ipairs(items) do
            if item.stat == 0 then unread = true end
        end
        local total = math.max(#items, items[1].total or 0)
        messages[#messages + 1] = {sender=group.sender, text=body, time=time,
            indices=indices, unread=unread, complete=#items == total}
    end
    return messages
end

return M
