-- MT5700M-CN AT manual 13.12/13.13. Values are validated before any AT write.
local M = {}
local fs = require "nixio.fs"
local nixio = require "nixio"
local sys = require "luci.sys"
local json = require "luci.jsonc"
local util = require "luci.util"
local cursor = require("luci.model.uci").cursor
local names = { NR = "NRFREQLOCK", LTE = "LTEFREQLOCK" }
local mode_at = { auto = "080302", sa_only = "08", lte = "03" }

local function query(iface, cmd)
    return sys.exec("cpetools.sh -i " .. iface .. " -t 0 -c " .. util.shellquote(cmd) .. " 2>/dev/null")
end
local function ack(raw)
    return ("\n" .. raw:gsub("\r", "") .. "\n"):find("\nOK\n", 1, true) ~= nil and not raw:find("ERROR", 1, true)
end
local function context(iface)
    if iface ~= "cpe" and iface ~= "cpe1" then return nil, "模组参数无效" end
    local uci = cursor()
    if iface == "cpe1" then
        if uci:get("network", iface, "proto") ~= "wwan" or uci:get("network", iface, "odu_model") ~= "MT5700M-CN" then
            return nil, "扩展口当前不是 MT5700 副模组模式"
        end
        if fs.readfile("/sys/class/net/port5/carrier") ~= "1\n" then return nil, "副模组未连接" end
    else
        local dev = fs.readfile("/tmp/infocd/cache/cpe_dev") or ""
        if not dev:find("mt5700", 1, true) then return nil, "当前主模组不是 MT5700" end
    end
    local sim = uci:get("cpesel", iface == "cpe" and "sim" or "sim1", "cur")
    if sim ~= "1" and sim ~= "2" then return nil, "未读到当前 SIM 卡槽" end
    return { uci = uci, iface = iface, sim = sim, section = iface .. "sim" .. sim }
end
local function split(text)
    local result = {}
    for value in (text .. ","):gmatch("(.-),") do result[#result + 1] = value:match("^%s*(.-)%s*$") end
    return result
end
local function parse_lock(raw, rat)
    local body = raw:match("%^" .. names[rat] .. ":%s*([^\r\n]+[\r\n].*)")
    if not body or not ack(raw) then return nil end
    local lines = {}
    for line in body:gmatch("[^\r\n]+") do lines[#lines + 1] = line:match("^%s*(.-)%s*$") end
    local kind = tonumber(lines[1])
    if not kind or kind < 0 or kind > 3 then return nil end
    local result = { rat = rat, kind = tostring(kind), mobility = "1", bands = "", arfcn = "", scs = "", pci = "" }
    if kind == 0 then return result end
    local meta = split(lines[2] or "")
    local count = tonumber(meta[2])
    if not count or count < 1 or count > 20 or not meta[1]:match("^[012]$") then return nil end
    result.mobility = meta[1]
    local cols = { {}, {}, {}, {} }
    for index = 1, count do
        local row = split(lines[index + 2] or "")
        for col = 1, 4 do cols[col][index] = row[col] or "" end
    end
    result.bands = table.concat(cols[1], ",")
    if kind ~= 3 then
        result.arfcn = table.concat(cols[2], ",")
        if rat == "NR" then result.scs = table.concat(cols[3], ",") end
        if kind == 2 then result.pci = table.concat(cols[rat == "NR" and 4 or 3], ",") end
    end
    return result
end
local function list(value, maximum, label)
    if type(value) ~= "string" or #value > 240 or not value:match("^%d+[,%d%s]*$") then return nil, label .. "须填写十进制数字，多个值用英文逗号分隔" end
    local items = split(value)
    if #items < 1 or #items > 20 then return nil, "每次支持 1 至 20 组参数" end
    for index, item in ipairs(items) do
        local number = tonumber(item)
        if not item:match("^%d+$") or not number or number > maximum then return nil, label .. "超出范围 0–" .. maximum end
        items[index] = string.format("%.0f", number)
    end
    return items
end
local function default_bands(ctx, rat)
    local key = rat == "NR" and "nr" or "lte"
    local supported = (ctx.supported or ctx.uci:get("network", ctx.iface, "freq_val") or ""):match(key .. "%-([^,]+)") or ""
    local skip = ""
    if rat == "NR" and ctx.uci:get("network", ctx.iface, "compatibility") == "1"
        and ctx.uci:get("cpecfg", ctx.section, "compatibility") ~= "1" then
        skip = ":" .. (ctx.uci:get("network", ctx.iface, "blacklist_band") or "") .. ":"
    end
    local bands = {}
    for band in supported:gmatch("%d+") do
        if not skip:find(":" .. band .. ":", 1, true) then bands[#bands + 1] = band end
    end
    return table.concat(bands, ",")
end
local function validate(input, ctx)
    if type(input) ~= "table" or not names[input.rat] then return nil, "请选择 NR 5G 或 LTE 4G" end
    local kind = tostring(input.kind or "")
    if kind ~= "0" and kind ~= "1" and kind ~= "2" and kind ~= "3" then return nil, "锁定类型无效" end
    local policy = tostring(input.policy or input.mobility or "1")
    if not policy:match("^[012]$") then return nil, "锁频策略无效" end
    local result = { rat = input.rat, kind = kind, policy = policy, mobility = policy, bands = "", arfcn = "", scs = "", pci = "" }
    if kind == "0" then return result end
    local selected = input.bands or ""
    if kind == "3" and selected:match("^%s*$") then
        if policy == "2" then result.kind = "0"; return result end
        result.use_default = ctx.native == true
        selected = default_bands(ctx, input.rat)
    end
    local bands, err = list(selected, 65535, "频段")
    if not bands then return nil, err end
    local allowed = {}
    local supported = (ctx.supported or ctx.uci:get("network", ctx.iface, "freq_val") or ""):match((input.rat == "NR" and "nr" or "lte") .. "%-([^,]+)")
    if not supported then return nil, "未读到此模组支持的频段" end
    for band in supported:gmatch("%d+") do allowed[tonumber(band)] = true end
    for _, band in ipairs(bands) do if not allowed[tonumber(band)] then return nil, "此模组不支持频段 " .. band end end
    if kind == "3" then
        local selected_set, ordered = {}, {}
        for _, band in ipairs(bands) do selected_set[tonumber(band)] = true end
        for band in supported:gmatch("%d+") do
            if selected_set[tonumber(band)] then ordered[#ordered + 1] = band end
        end
        bands = ordered
    end
    -- Factory 2.3.3 uses strict locking for one band and for frequency/cell locks.
    if kind ~= "3" or #bands == 1 then result.mobility = "0" end
    result.bands = table.concat(bands, ",")
    if kind == "3" then return result end
    if ctx.native and #bands ~= 1 then return nil, "副设备原厂接口每次设置一个小区或频点" end
    local columns = { { "arfcn", 4294967295, input.rat == "NR" and "SSB ARFCN" or "EARFCN" } }
    if input.rat == "NR" and not ctx.native then columns[#columns + 1] = { "scs", 4, "SCS Type" } end
    if kind == "2" then columns[#columns + 1] = { "pci", input.rat == "NR" and 1007 or 503, "PCI" } end
    for _, column in ipairs(columns) do
        local items, message = list(input[column[1]], column[2], column[3])
        if not items then return nil, message end
        if #items ~= #bands then return nil, "频段、频点、SCS、PCI 的组数须一一对应" end
        result[column[1]] = table.concat(items, ",")
    end
    return result
end
local function command(profile)
    local cmd = "AT^" .. names[profile.rat] .. "=" .. profile.kind
    if profile.kind == "0" then return cmd end
    cmd = cmd .. "," .. profile.mobility .. "," .. #split(profile.bands) .. ',"' .. profile.bands .. '"'
    if profile.kind ~= "3" then
        cmd = cmd .. ',"' .. profile.arfcn .. '"'
        if profile.rat == "NR" then cmd = cmd .. ',"' .. profile.scs .. '"' end
        if profile.kind == "2" then cmd = cmd .. ',"' .. profile.pci .. '"' end
    end
    return cmd
end
local function same(a, b)
    return a and b and command(a) == command(b)
end
local function mode_command(iface, mode)
    local raw = query(iface, "AT^SYSCFGEX?")
    local current, tail = raw:match('%^SYSCFGEX:%s*"([%d]+)"(,[%x,]+)')
    if not current or not ack(raw) then return nil, "未读到网络制式，未发送设置" end
    if mode == "keep" or mode_at[mode] == current then return false end
    return 'AT^SYSCFGEX="' .. mode_at[mode] .. '"' .. tail .. ",,"
end
local function save(ctx, profile, mode)
    -- Lock tuples live in the modem's NV. The host only relinquishes ownership.
    local values = { ["radio_policy_" .. profile.rat:lower()] = profile.policy }
    if mode ~= "keep" then values.mode = mode end
    local targets = { { "cpecfg", ctx.section } }
    local id = ctx.uci:get("network", ctx.iface, "kpcpe_id") or ""
    if ctx.iface == "cpe1" and id:match("^%d+$") then targets[#targets + 1] = { "kpcpe_" .. id .. "_cpecfg", "cpesim" .. ctx.sim } end
    for _, target in ipairs(targets) do
        if not ctx.uci:get(target[1], target[2]) then ctx.uci:section(target[1], "cpesim", target[2], {}) end
        for key, value in pairs(values) do ctx.uci:set(target[1], target[2], key, value) end
        if ctx.uci:get(target[1], target[2], "radio_lock_managed") == "1" then
            for _, key in ipairs({ "freq", "custom_freq", "custom_earfcn5", "earfcn5", "pci5", "band5", "custom_earfcn4", "earfcn4", "pci4", "band4", "earfreq_mode" }) do
                ctx.uci:delete(target[1], target[2], key)
            end
        end
        for _, key in ipairs({ "radio_lock_nr", "radio_lock_lte", "radio_lock_managed" }) do ctx.uci:delete(target[1], target[2], key) end
        if not ctx.uci:commit(target[1]) then return nil, "模组设置已下发，但持久化失败" end
    end
    ctx.uci:set("network", ctx.iface, "radio_lock_owner", "modem")
    if not ctx.uci:commit("network") then return nil, "模组设置已下发，但锁定归属保存失败" end
    return true
end
local function cycle(iface)
    local stopped = query(iface, "AT+CFUN=0")
    -- Manual 6.1.2: CFUN=0 and CFUN=1 must be separated by more than 1 s.
    require("nixio").nanosleep(2)
    local off_state = query(iface, "AT+CFUN?")
    local inactive = off_state:match("%+CFUN:%s*0") ~= nil
    -- Always try to return to full functionality, including a missing CFUN=0 ACK.
    local started = query(iface, "AT+CFUN=1")
    require("nixio").nanosleep(2)
    local active = query(iface, "AT+CFUN?")
    for attempt = 1, 3 do
        if active:match("%+CFUN:%s*1") then break end
        require("nixio").nanosleep(2)
        started = query(iface, "AT+CFUN=1")
        require("nixio").nanosleep(2)
        active = query(iface, "AT+CFUN?")
    end
    local trace = { off = stopped, off_state = off_state, on = started, on_state = active }
    -- A read-back CFUN=1 is stronger evidence than a possibly lost transport ACK.
    return (ack(stopped) or inactive) and active:match("%+CFUN:%s*1") ~= nil, trace
end

function M.status(iface)
    if iface == "cpe1" and require("c5800_secondary").is_family() then
        local state = require("c5800_secondary").status(true)
        local result = {iface=iface,native=true,sim=state.cur,mode=state.profile.freqmode,
            supported=state.capability.freq_val or "",raw="",profile=state.profile,read_error=state.read_error,default_bands={}}
        local cell_lock = tostring((state.profile.freq5 or {}).enabled) == "1" or tostring((state.profile.freq4 or {}).enabled) == "1"
        for rat, n in pairs({NR="5",LTE="4"}) do
            local key = rat == "NR" and "nr" or "lte"
            local cell, band = state.profile["freq" .. n] or {}, state.profile.band or {}
            local policy = tostring(state.profile.mobility or "")
            local profile = {rat=rat,kind="0",mobility=policy,policy=policy,bands="",arfcn="",pci="",scs=""}
            if tostring(cell.enabled) == "1" then
                profile.kind = tostring(cell.pci or "") ~= "" and "2" or "1"
                profile.bands, profile.arfcn, profile.pci = cell.band or "", cell.earfcn or "", cell.pci or ""
            elseif tostring(band.enabled) == "1" then
                local bands = tostring(band.freq or ""):match(key .. "%-([^,]+)")
                if bands then profile.kind, profile.bands = "3", bands:gsub(":", ",") end
            end
            local defaults = {}
            local skip = ":" .. tostring(state.capability.blacklist_band or ""):gsub(",", ":") .. ":"
            for value in (result.supported:match(key .. "%-([^,]+)") or ""):gmatch("%d+") do
                if not (key == "nr" and tostring(state.capability.compatibility) == "1"
                    and tostring(state.profile.compatibility or "0") == "0" and skip:find(":" .. value .. ":", 1, true)) then
                    defaults[#defaults + 1] = value
                end
            end
            result.default_bands[key] = table.concat(defaults, ",")
            if not cell_lock and profile.kind == "0" and (policy == "0" or policy == "1") and #defaults > 0 then
                profile.kind, profile.bands, profile.factory_default = "3", result.default_bands[key], true
            end
            profile.source = "factory_config"
            result[key] = profile
        end
        return result
    end
    local ctx, err = context(iface)
    if not ctx then return { error = err } end
    local nr = query(iface, "AT^NRFREQLOCK?")
    local lte = query(iface, "AT^LTEFREQLOCK?")
    local nr_lock, lte_lock = parse_lock(nr, "NR"), parse_lock(lte, "LTE")
    for rat, profile in pairs({nr=nr_lock, lte=lte_lock}) do
        profile.policy = ctx.uci:get("cpecfg", ctx.section, "radio_policy_" .. rat) or profile.mobility
    end
    return { iface = iface, sim = ctx.sim, nr = nr_lock, lte = lte_lock,
        default_bands = {nr=default_bands(ctx, "NR"), lte=default_bands(ctx, "LTE")},
        supported = ctx.uci:get("network", iface, "freq_val"), mode = ctx.uci:get("cpecfg", ctx.section, "mode") or "auto", raw = nr .. lte }
end

function M.apply(iface, input, defer_cycle)
    local secondary = require("c5800_secondary")
    if iface == "cpe1" and secondary.is_family() then
        local state = secondary.status(true)
        local ctx = {uci=cursor(),iface=iface,sim=tostring(state.cur),section=iface.."sim"..state.cur,
            native=true,supported=state.capability.freq_val or ""}
        local profile, err = validate(input, ctx)
        if not profile then return {error=err} end
        local result, message = secondary.radio(profile, input.mode or "keep", state)
        return result or {error=message}
    end
    local ctx, err = context(iface)
    if not ctx then return { error = err } end
    local profile, message = validate(input, ctx)
    if not profile then return { error = message } end
    local mode = input.mode or "keep"
    if mode ~= "keep" and not mode_at[mode] then return { error = "网络制式参数无效" } end
    local mutex = nixio.open("/tmp/c5800-radio-" .. iface .. ".lck", "w", "600")
    if not mutex then return { error = "无法打开模组操作锁" } end
    if not mutex:lock("tlock") then
        mutex:close()
        return { error = "此模组正在操作，请稍后再试" }
    end
    local ok, result = pcall(function()
        local cfg, cfg_error = mode_command(iface, mode)
        if cfg == nil then return { error = cfg_error } end
        local current = parse_lock(query(iface, "AT^" .. names[profile.rat] .. "?"), profile.rat)
        if not current then return { error = "未取得当前锁定状态，未发送设置" } end
        local changed = not same(current, profile) or cfg ~= false
        if cfg and not ack(query(iface, cfg)) then return { error = "网络制式设置未获模组确认" } end
        if cfg and mode_command(iface, mode) ~= false then return { error = "网络制式读回不一致，请刷新实际状态" } end
        if not same(current, profile) then
            local response = query(iface, command(profile))
            if not ack(response) then
                local detail = response:match("%+CME ERROR:%s*[^\r\n]+") or response:match("ERROR") or "未响应"
                return { error = "模组未接受锁频设置：" .. detail, response = response, command = command(profile) }
            end
            if not same(parse_lock(query(iface, "AT^" .. names[profile.rat] .. "?"), profile.rat), profile) then
                return { error = "锁定设置读回不一致，请刷新实际状态" }
            end
        end
        local saved, save_error = save(ctx, profile, mode)
        -- Complete reselection even if saving failed after the radio accepted a change.
        local resumed, trace = true, nil
        if changed and not defer_cycle then resumed, trace = cycle(iface) end
        fs.unlink("/tmp/c5800-radio-" .. iface .. ".json")
        if not resumed then return { error = "设置已下发，模组恢复射频未获确认，请读取实际状态", radio = trace } end
        if not saved then return { error = save_error } end
        return { ok = true, changed = changed, profile = profile, command = command(profile),
            message = changed and "模组锁定设置已读回确认，正在重新驻网；请刷新驻留与连接状态" or "与模组当前设置一致，无需重新驻网" }
    end)
    mutex:close()
    if not ok then return { error = "锁定操作失败，请读取实际状态" } end
    return result
end

function M.apply_form(iface, input)
    local secondary = require("c5800_secondary")
    local native = iface == "cpe1" and secondary.is_family()
    local state = native and secondary.status(true) or nil
    local ctx, err
    if state then
        ctx = {uci=cursor(),iface=iface,sim=tostring(state.cur),section=iface.."sim"..state.cur,native=true,supported=state.capability.freq_val or ""}
    else ctx, err = context(iface) end
    if not ctx then return {error=err} end
    local profiles, requests = {}, {}
    for _, rat in ipairs({"NR", "LTE"}) do
        local prefix = rat:lower() .. "_"
        local request = {rat=rat,kind=input[prefix .. "enabled"] == "1" and input.kind or "0",policy=input.policy,
            bands=input[prefix .. "bands"] or "",arfcn=input[prefix .. "arfcn"] or "",scs=input[prefix .. "scs"] or "",pci=input[prefix .. "pci"] or "",mode="keep"}
        local profile, message = validate(request, ctx)
        if not profile then return {error=(rat == "NR" and "5G：" or "4G：") .. message} end
        profiles[#profiles + 1], requests[#requests + 1] = profile, request
    end
    if native then
        local result, message = secondary.radio_profiles(profiles, input.mode or "keep", state)
        return result or {error=message}
    end
    requests[1].mode = input.mode or "keep"
    local results, errors, changed = {}, {}, false
    for _, request in ipairs(requests) do
        local result = M.apply(iface, request, true)
        results[request.rat] = result
        changed = changed or result.changed == true
        if result.error then errors[#errors + 1] = request.rat .. "：" .. result.error end
    end
    if changed then
        local resumed = cycle(iface)
        if not resumed then errors[#errors + 1] = "设置已下发，射频恢复请以实际驻网状态为准" end
    end
    if #errors > 0 then return {error=table.concat(errors, "；"),results=results} end
    return {ok=true,changed=changed,results=results,message=changed and "5G / 4G 设置已读回，模组正在重新驻网" or "与当前设置一致，无需重新驻网"}
end

return M
