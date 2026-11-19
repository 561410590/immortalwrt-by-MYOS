-- Read-only 3GPP TS 27.007 response parsers. Rates are always in kbit/s.
local M={}
local function fields(body)
    local out={}
    for value in (body..','):gmatch('(.-),') do
        out[#out+1]=(value:match('^%s*(.-)%s*$'):gsub('^"(.*)"$','%1'))
    end
    return out
end
local function number(value)
    if type(value)~='string' or not value:match('^%d+$') then return nil end
    local n=tonumber(value)
    return n and n<=9007199254740991 and n or nil
end
function M.parse(raw, kind, cid)
    local prefix=kind=='5gs' and '+C5GQOSRDP:' or '+CGEQOSRDP:'
    local flows={}
    for line in (raw or ''):gmatch('[^\r\n]+') do
        line=line:match('^%s*(.-)%s*$')
        if line:sub(1,#prefix)==prefix then
            local f=fields(line:sub(#prefix+1))
            local class=number(f[2])
            if number(f[1])==cid and class and class<=255 then
                flows[#flows+1]={class=class,dl_gbr_kbps=number(f[3]),ul_gbr_kbps=number(f[4]),
                    dl_mbr_kbps=number(f[5]),ul_mbr_kbps=number(f[6]),
                    dl_ambr_kbps=number(f[7]),ul_ambr_kbps=number(f[8])}
            end
        end
    end
    return flows
end
function M.radio(raw,endc)
    local body=(raw or ''):match('%^SYSINFOEX:%s*([^\r\n]+)')
    if not body then return {access='未知',session_kind='数据会话'} end
    local f=fields(body)
    local system,submode=f[7] or '',f[9] or ''
    if system=='NR-5GC' or submode=='NR-5GC' then return {access='5G SA',session_kind='PDU 会话',system=system} end
    if system:find('LTE',1,true) or submode:find('LTE',1,true) then
        local dc=(endc or ''):match('%^LENDC:%s*([^\r\n]+)')
        local connected=dc and fields(dc) or {}
        local nsa=system:find('NR',1,true) or submode:find('NR',1,true) or submode:find('ENDC',1,true) or connected[5]=='1'
        return {access=nsa and '5G NSA' or '4G LTE',session_kind='PDN 连接',system=system}
    end
    return {access=submode~='' and submode or system,session_kind='PDP 上下文',system=system}
end
local function classes(flows)
    local out,seen={},{}
    for _,flow in ipairs(flows) do if not seen[flow.class] then out[#out+1]=flow.class; seen[flow.class]=true end end
    return out
end
local function common(flows,key)
    local value
    for _,flow in ipairs(flows) do
        if flow[key]==nil or (value~=nil and flow[key]~=value) then return nil end
        value=flow[key]
    end
    return value
end
function M.combine(five,eps,radio)
    local result={fiveqi=classes(five),qci=classes(eps)}
    -- A compatibility EPS response on SA is not a separately negotiated 5GS
    -- QoS class. Keep both identifiers, with their actual command sources.
    local is5g=radio.session_kind=='PDU 会话'
    local selected=is5g and five or eps
    result.rate_kind=is5g and 'Session-AMBR' or 'APN-AMBR'
    result.dl_ambr_kbps=common(selected,'dl_ambr_kbps')
    result.ul_ambr_kbps=common(selected,'ul_ambr_kbps')
    result.state=(#five>0 or #eps>0) and 'ok' or 'unavailable'
    result.source_5g=#five>0 and 'AT+C5GQOSRDP' or nil
    result.source_eps=#eps>0 and 'AT+CGEQOSRDP' or nil
    if result.state~='ok' then result.message='模组暂未返回 QoS 参数' end
    return result
end
return M
