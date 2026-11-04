-- AK ODU management. Discovery and dialing remain owned by the native services.
local M = {}
local fs = require "nixio.fs"
local json = require "luci.jsonc"
local util = require "luci.util"
local cursor = require("luci.model.uci").cursor

local function read(path)
    return json.parse(fs.readfile(path) or "") or {}
end

local function csv(value)
    local result = {}
    if value == nil or tostring(value) == "" then return result end
    for item in (tostring(value or "") .. ","):gmatch("([^,]*),") do
        result[#result + 1] = item
    end
    return result
end

function M.is_family()
    return cursor():get("network", "cpe1", "odu_model") == "NRFAMILY"
end

function M.status(refresh)
    local uci = cursor()
    local net = uci:get_all("network", "cpe1") or {}
    local id = tostring(net.kpcpe_id or "")
    for _, device in ipairs((read("/tmp/infocd/cache/kpcped").parameter or {}).list or {}) do
        if device.name == "cpe1" and device.host == net.auto_sniffer then
            id = tostring(device.device_id or "")
            break
        end
    end
    local info = {}
    if id:match("^[%w_-]+$") then
        info = read("/tmp/infocd/cache/kpcpe_" .. id .. "_info").parameter or {}
    end
    local read_error
    if refresh then
        local reply, err = M.request("info", {type="all"})
        if reply and type(reply.result) == "table" then info = reply.result
        else read_error = err end
    end
    -- A local APP cache is not an ODU identity or a source of its SIM slots.
    local local_mac = (fs.readfile("/sys/class/net/br-lan/address") or ""):gsub("[^%w]", ""):upper()
    local info_mac = tostring((info.basic or {}).mac or ""):gsub("[^%w]", ""):upper()
    if info_mac ~= "" and info_mac == local_mac then info = {} end
    local selector = (info.cpesel or {})[1] or {}
    local basic = info.basic or {}
    local current = tonumber(selector.cur) or tonumber(uci:get("cpesel", "sim1", "cur")) or 1
    local types, mapping = csv(selector.stype or selector.type), csv(selector.gval)
    local maximum = tonumber(selector.max) or math.max(#types, #mapping)
    local slots, external = {}, 0
    for index = 1, maximum do
        local embedded = types[index] == "4"
        if not embedded then external = external + 1 end
        slots[#slots + 1] = { value = index, type = types[index] or "0",
            label = embedded and "内置卡" or ("SIM" .. external), mapping = mapping[index] or "" }
    end
    local signal = {}
    for _, item in ipairs(read("/tmp/infocd/cache/cpestatus").result or {}) do
        if item.status and item.status.name == "cpe1" then signal = item.status end
    end
    if refresh then
        local reply = M.request("signal", {})
        if reply and type(reply.result) == "table" then signal = reply.result end
    end
    local profiles = (info.cpecfg or {})[1] or {}
    local profile = profiles[tostring(current)] or uci:get_all("cpecfg", "cpe1sim" .. current) or {}
    if refresh then
        -- The ODU info response can retain an older mode after a successful write.
        -- Read the current SIM's mode from its native configuration endpoint.
        local reply, err = M.request("earfcn", {index="1", action=0, sim=current})
        local mode = reply and type(reply.result) == "table" and reply.result.mode
        if type(mode) == "string" and mode ~= "" then
            profile.freqmode = mode
            selector.freqmode = mode
        else
            read_error = read_error or err or "副设备未返回当前网络制式"
        end
    end
    return { iface = "cpe1", id = id, name = basic.name or "副设备信息未读取",
        version = basic.version or "", transport = net.odu_model or "",
        host = net.auto_sniffer or "", connected = fs.readfile("/sys/class/net/port5/carrier") == "1\n",
        cur = current, max = maximum, mode = tonumber(selector.mode) or 1,
        stype = selector.stype or selector.type or "", gval = selector.gval or "",
        default = selector.default or current, slots = slots, signal = signal,
        profile = profile, profiles = profiles, apn = selector.apn or {}, capability = (info.cellular or {})[1] or {},
        cpesel = selector, read_error = read_error }
end

-- kpcpe-ctl performs the original authenticated/encrypted ODU request.
-- No local interface, modem, selector or network service is restarted here.
function M.request(action, payload)
    if action ~= "cpesel" and action ~= "earfcn" and action ~= "apn" and action ~= "info" and action ~= "signal" then
        return nil, "副设备操作无效"
    end
    local ok, status = pcall(util.ubus, "network.interface.cpe1_4", "status", {})
    local host = ""
    for _, route in ipairs(ok and type(status) == "table" and status.route or {}) do
        if route.target == "0.0.0.0" and route.nexthop then
            host = route.nexthop
            break
        end
    end
    if host == "" then host = cursor():get("network", "cpe1", "auto_sniffer") or "" end
    local params = action == "signal" and "{}" or json.stringify(payload)
    local cmd = "/usr/bin/kpcpe-ctl -H " .. util.shellquote(host) .. " -i cpe -a " .. action ..
        " -p " .. util.shellquote(params)
    local pipe = io.popen(cmd .. " 2>/dev/null")
    if not pipe then return nil, "无法调用原厂副设备控制程序" end
    local raw = pipe:read("*a") or ""
    pipe:close()
    local reply = json.parse(raw)
    if type(reply) ~= "table" then
        return nil, "副设备未返回有效结果，请刷新实际状态"
    end
    if action == "signal" and reply.code == nil then return { result = reply } end
    if tostring(reply.code) ~= "0" then
        return nil, type(reply.result) == "string" and reply.result or "副设备未接受设置"
    end
    return reply
end

function M.select(slot)
    local state = M.status(true)
    slot = tonumber(slot)
    if not slot or slot % 1 ~= 0 or slot < 1 or slot > state.max then
        return nil, "请选择副设备实际提供的卡槽"
    end
    local reply, err = M.request("cpesel", { cur = { slot }, mode = { 1 }, adv_sim = { 0 } })
    if not reply then return nil, err end
    local uci = cursor()
    uci:set("cpesel", "sim1", "cpesel")
    uci:set("cpesel", "sim1", "cur", tostring(slot))
    uci:set("cpesel", "sim1", "mode", "1")
    for _, key in ipairs({ "max", "stype", "gval", "default" }) do
        if state[key] ~= nil and tostring(state[key]) ~= "" then uci:set("cpesel", "sim1", key, tostring(state[key])) end
    end
    if not uci:commit("cpesel") then return nil, "副设备已接受切卡，但本地配置保存失败，请刷新实际状态" end
    if state.id:match("^[%w_-]+$") then
        local config = "kpcpe_" .. state.id .. "_cpesel"
        if uci:get(config, "sim") then
            uci:set(config, "sim", "cur", tostring(slot))
            uci:set(config, "sim", "mode", "1")
            if not uci:commit(config) then return nil, "副设备已接受切卡，但本地副设备配置保存失败，请刷新实际状态" end
        end
    end
    return { ok = true, cur = slot, message = "副设备已接受手动切卡，请刷新驻网状态" }
end

local function profile_adv(state)
    local adv = {}
    for key, value in pairs(cursor():get_all("cpecfg", "cpe1sim" .. state.cur) or {}) do
        if key:sub(1, 1) ~= "." then adv[key] = value end
    end
    local p = state.profile
    -- Preserve the ODU's other settings when sending its complete native adv object.
    for key, value in pairs(p) do
        if type(value) ~= "table" and key ~= "freqmode" then adv[key] = value end
    end
    if p.freqmode then adv.mode = p.freqmode end
    for _, key in ipairs({"earfreq_mode", "apn_cfg", "ippass", "mobility", "roaming"}) do
        if p[key] ~= nil then adv[key] = p[key] end
    end
    if type(p.band) == "table" then adv.custom_freq = p.band.enabled; adv.freq = p.band.freq end
    for _, n in ipairs({"4", "5"}) do
        local f = p["freq" .. n]
        if type(f) == "table" then
            adv["custom_earfcn" .. n] = f.enabled
            for _, k in ipairs({"earfcn", "pci", "band"}) do adv[k .. n] = f[k] end
        end
    end
    return adv
end

local function save_profile(state, adv)
    local uci = cursor()
    local targets = {{"cpecfg", "cpe1sim" .. state.cur}}
    if state.id:match("^[%w_-]+$") then targets[#targets + 1] = {"kpcpe_" .. state.id .. "_cpecfg", "cpesim" .. state.cur} end
    for _, target in ipairs(targets) do
        if not uci:get(target[1], target[2]) then uci:section(target[1], "cpesim", target[2], {}) end
        for key, value in pairs(adv) do uci:set(target[1], target[2], key, value) end
        if not uci:commit(target[1]) then return nil, "副设备已接受设置，但本地配置保存失败，请刷新实际状态" end
    end
    return true
end

function M.frequency(input, state)
    state = state or M.status(true)
    local adv = profile_adv(state)
    if input.mobility and input.mobility ~= "" then
        local policy = tostring(input.mobility)
        if not policy:match("^[012]$") then return nil, "锁频策略无效" end
        adv.mobility = policy
    end
    if input.mode and input.mode ~= "keep" then
        if input.mode ~= "auto" and input.mode ~= "sa_only" and input.mode ~= "lte" then return nil, "网络模式无效" end
        adv.mode = input.mode
    end
    for _, key in ipairs({"custom_freq", "freq", "earfreq_mode", "custom_earfcn5", "earfcn5", "pci5", "band5", "custom_earfcn4", "earfcn4", "pci4", "band4"}) do
        if input[key] ~= nil then
            local value = tostring(input[key])
            if #value > 512 or value:find("[%z\r\n]") then return nil, "频段或频点格式无效" end
            adv[key] = value
        end
    end
    local payload = {index="1",action=1,sim=state.cur,
        band={freq=adv.freq or "",enabled=adv.custom_freq or "0"},
        earfreq_mode=adv.earfreq_mode or "",earfcns={},adv=adv,mode=adv.mode or ""}
    for _, item in ipairs({{"5","NR"},{"4","LTE"}}) do
        local n = item[1]
        local entry = {enabled=adv["custom_earfcn" .. n] or "0",MODE=item[2],
            EARFCN=adv["earfcn" .. n] or "",PCI=adv["pci" .. n] or "",BAND=adv["band" .. n] or ""}
        entry["earfcn" .. n .. "_mode"] = adv["earfcn" .. n .. "_mode"] or ""
        payload.earfcns[#payload.earfcns + 1] = entry
    end
    local reply, err = M.request("earfcn", payload)
    if not reply then return nil, err end
    local actual = M.status(true)
    local requested_mode = adv.mode
    if not actual.read_error and actual.profile.freqmode then adv.mode = actual.profile.freqmode end
    local ok, message = save_profile(state, adv)
    if not ok then return nil, message end
    local read_error = actual.read_error
    if not read_error and actual.profile.freqmode ~= requested_mode then
        read_error = "网络制式回读不一致：请求 " .. tostring(requested_mode) .. "，实际 " .. tostring(actual.profile.freqmode)
    end
    return {ok=true, profile=actual.profile, read_error=read_error,
        message=read_error and ("设置已提交，" .. read_error) or "副设备网络制式已读回确认，请刷新驻网状态"}
end

function M.radio_profiles(profiles, mode, state)
    local input = profile_adv(state)
    local parts, band_lock = {}, false
    for _, profile in ipairs(profiles) do
        local key, n = profile.rat == "NR" and "nr" or "lte", profile.rat == "NR" and "5" or "4"
        input["custom_earfcn" .. n] = "0"
        if profile.kind == "3" and not profile.use_default then
            band_lock = true
            parts[#parts + 1] = key .. "-" .. profile.bands:gsub(",", ":")
        elseif profile.kind == "1" or profile.kind == "2" then
            input["custom_earfcn" .. n] = "1"
            input["band" .. n], input["earfcn" .. n] = profile.bands, profile.arfcn
            input["pci" .. n] = profile.kind == "2" and profile.pci or ""
        end
        input.mobility = profile.policy
    end
    input.custom_freq, input.earfreq_mode = band_lock and "1" or "0", band_lock and "band" or ""
    input.freq, input.mode = table.concat(parts, ","), mode
    return M.frequency(input, state)
end

function M.radio(profile, mode, state)
    local input = profile_adv(state)
    local key, n = profile.rat == "NR" and "nr" or "lte", profile.rat == "NR" and "5" or "4"
    local parts = {}
    for item in tostring(input.freq or ""):gmatch("[^,]+") do
        if item:match("^([^-]+)") ~= key then parts[#parts + 1] = item end
    end
    if profile.kind == "3" then
        parts[#parts + 1] = key .. "-" .. profile.bands:gsub(",", ":")
        input.custom_freq, input.earfreq_mode = "1", "band"
        input.custom_earfcn5, input.custom_earfcn4 = "0", "0"
    elseif profile.kind == "0" then
        input.custom_freq = #parts > 0 and "1" or "0"
        input["custom_earfcn" .. n] = "0"
        if #parts == 0 then input.earfreq_mode = "" end
    else
        input.custom_freq, input.earfreq_mode = "0", ""
        input["custom_earfcn" .. n] = "1"
        input["band" .. n], input["earfcn" .. n] = profile.bands, profile.arfcn
        input["pci" .. n] = profile.kind == "2" and profile.pci or ""
    end
    input.freq = table.concat(parts, ",")
    input.mobility, input.mode = profile.policy, mode
    return M.frequency(input, state)
end

function M.configure(input)
    local state = M.status(true)
    local mode = tostring(input.mode or state.profile.freqmode or state.profile.mode or "auto")
    if mode ~= "auto" and mode ~= "sa_only" and mode ~= "lte" then return nil, "网络模式无效" end
    local adv = profile_adv(state)
    adv.mode = mode
    if input.apn ~= nil then
        local name = tostring(input.apn)
        if #name > 100 or name:find("[%z\r\n]") then return nil, "APN 格式无效" end
        adv.custom_apn = name ~= "" and "1" or "0"
        adv.apn_cfg = ""
        adv.apn = name
        adv.username = tostring(input.username or "")
        adv.password = tostring(input.password or "")
        adv.auth = tostring(input.auth or "0")
        adv.pdptype = tostring(input.pdptype or "IPV4V6")
    end
    local reply, err = M.request("earfcn", { index = "1", action = 1, sim = state.cur, mode = mode, adv = adv })
    if not reply then return nil, err end
    local saved, message = save_profile(state, adv)
    if not saved then return nil, message end
    return { ok = true, message = "副设备已接受设置，请刷新连接状态" }
end

function M.app_info(refresh)
    local state = M.status(refresh ~= false)
    local info, iccid = {}, {}
    for i, slot in ipairs(state.slots) do
        iccid[i] = i == state.cur and (state.signal.iccid or "") or ""
        info[i] = {cpesel=tostring(i),cur=i,type=slot.type,iccid=iccid[i],
            operator=i==state.cur and (state.signal.operator or state.signal.isp or "") or "",name=slot.label}
    end
    return { cur = state.cur, mode = state.mode, max = tostring(state.max), type = state.stype,
        stype = state.stype, gval = state.gval, default = tostring(state.default),
        adv_sim = "0", info = info, iccid=iccid, device_id = state.id, name = state.name,
        freqmode=state.profile.freqmode, band=state.profile.band, freq5=state.profile.freq5,
        freq4=state.profile.freq4, apn=state.apn }
end

return M
