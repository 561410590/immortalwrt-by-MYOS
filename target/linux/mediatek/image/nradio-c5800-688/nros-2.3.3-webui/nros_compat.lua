local base = assert(os.getenv("NROS_BASE"))
local fs = require "nixio.fs"
local uci = require "uci"
local methods = getmetatable(uci.cursor()).__index
local config = setmetatable({}, { __mode = "k" })
local get_all, foreach = methods.get_all, methods.foreach
local get = methods.get

-- Original diagnostics expect the original IP binding with string MACs.
package.preload["luci.ip"] = assert(package.loadlib(base .. "/lua/luci/ip.so", "luaopen_luci_ip"))

-- The original views read cloudd's projection of the active wireless settings.
local function wireless_projection(cursor)
    local group
    foreach(cursor, "cloudd", "cboard", function(board)
        group = tonumber(board.pos)
        return false
    end)
    if not group then return nil end

    local wlan, radio, profiles = {}, {}, {}
    foreach(cursor, "wireless", "wifi-iface", function(ap)
        if ap.mode ~= "ap" or ap.network ~= "lan" then return end
        local phy = get_all(cursor, "wireless", ap.device)
        local index = phy and ({["2g"]=0, ["5g"]=1})[phy.band]
        if not index or wlan["ssid_wlan" .. index] then return end
        profiles[index] = ap
        local device = "wireless." .. ap.device
        local iface = "wireless." .. ap[".name"]
        for _, name in ipairs({
            "ssid", "encryption", "key", "disabled", "hidden", "isolate",
            "ieee80211w", "macfilter", "auth_server", "auth_port", "auth_secret"
        }) do
            local value = ap[name]
            if value == nil and name == "disabled" then value = phy.disabled or "0" end
            if value ~= nil then wlan[name .. "_wlan" .. index] = iface .. "." .. name .. "=" .. value end
        end
        for _, name in ipairs({"channel", "htmode", "txpower", "band"}) do
            if phy[name] then radio[name .. "_radio" .. index] = device .. "." .. name .. "=" .. phy[name] end
        end
    end)
    local name = "g" .. (group + 2)
    local template = {}
    local primary = profiles[1] or profiles[0]
    if primary then
        for _, key in ipairs({"ssid", "encryption", "key", "disabled", "hidden", "isolate", "ieee80211w"}) do
            if primary[key] ~= nil then template[key .. "_wlan"] = "X" .. primary[key] end
        end
        local first, second = profiles[0], profiles[1]
        if first and second then
            local shared = first.ssid == second.ssid and first.encryption == second.encryption and first.key == second.key
            template.diff_2_4_wlan = shared and "X0" or "X1"
            if first.ssid then template.diff_2_4_ssid_wlan = "X" .. first.ssid end
        end
    end
    return { [name .. "wlan"]=wlan, [name .. "radio"]=radio, t0=template }
end
local path = (os.getenv("PATH_INFO") or ""):gsub("/+", "/")
local wireless_page = path == "/nradio/basic/wifi" or path == "/nradio/basic/wifi/advanced"
if wireless_page or path == "/" or path == "" or path == "/nradio" or path == "/nradio/status" or path == "/nradio/system/overview" then
    local mapped
    local function projected_section(cursor, section)
        if not mapped then mapped = wireless_projection(cursor) or {} end
        return mapped[section]
    end
    methods.get_all = function(cursor, package, section, ...)
        local raw = get_all(cursor, package, section, ...)
        if package ~= "cloudd" or not section or get(cursor, "cloudd", "t0") then return raw end
        return projected_section(cursor, section) or raw
    end
    if wireless_page then
        methods.get = function(cursor, package, section, option, ...)
            if package == "cloudd" and section and option and not get(cursor, "cloudd", "t0") then
                local active = projected_section(cursor, section)
                if active and active[option] ~= nil then return active[option] end
            end
            if option == nil then return get(cursor, package, section) end
            return get(cursor, package, section, option, ...)
        end
    end
end

-- Legacy LuCI has its own theme and language; all non-LuCI UCI packages
-- continue to use the router's existing cursor and data.
for _, name in ipairs({
    "load", "unload", "get", "get_all", "set", "add", "delete", "rename",
    "reorder", "foreach", "changes", "save", "revert", "commit"
}) do
    local action = methods[name]
    if action then
        methods[name] = function(self, package, ...)
            if package == "luci" then
                local cursor = config[self]
                if not cursor then
                    cursor = uci.cursor(base .. "/config", base .. "/uci")
                    config[self] = cursor
                end
                return action(cursor, package, ...)
            end
            return action(self, package, ...)
        end
    end
end

-- The stock login checks the original media path before rendering its video.
local access = fs.access
fs.access = function(path, ...)
    if path:sub(1, 5) == "/www/" then
        return access(base .. path, ...)
    elseif path:sub(1, 18) == "/usr/lib/lua/luci/" then
        return access(base .. "/lua/" .. path:sub(14), ...)
    end
    return access(path, ...)
end

local writefile = fs.writefile
fs.writefile = function(path, ...)
    if path == "/tmp/luci-indexpath" then
        path = base .. "/luci-indexpath"
    end
    return writefile(path, ...)
end

local cgi = require "luci.sgi.cgi"
local json = require "luci.jsonc"
local nradio = require "luci.nradio"
local util = require "luci.util"
local translate = require("luci.i18n").translate
local runtime_info = nradio.get_runtime_info
local stock_plmn = nradio.genarate_plmn_company
local stock_vht160 = nradio.support_vht160
local bus_call = util.ubus

-- infocd in MYOS reports PLMN codes without the stock PLMN lookup method.
local home_operators = {
    ["46001"]="China Unicom", ["46006"]="China Unicom",
}

function nradio.genarate_plmn_company(plmn, only_company)
    if type(plmn) ~= "string" or plmn == "" or plmn == "none" then return "" end
    if not home_operators[plmn] then return stock_plmn(plmn, only_company) end
    return only_company and home_operators[plmn] or translate(home_operators[plmn])
end

local function identify_operators(status)
    if type(status.sim_company) ~= "string" or status.sim_company == "" or status.sim_company == "none" then
        local imsi = tostring(status.imsi or "")
        local plmn = imsi:match("^460%d%d")
        status.sim_company = plmn and nradio.genarate_plmn_company(plmn, true) or ""
    end
    if status.sim_company ~= "" then
        status.sim_company_lng = translate(status.sim_company)
    else
        status.sim_company_lng = "-"
    end
    if not status.isp_company or status.isp_company == "" or status.isp_company == "none" then
        status.isp_company = nradio.genarate_plmn_company(status.isp, true)
    end
    if status.isp_company and status.isp_company ~= "" then
        status.isp_company_lng = translate(status.isp_company)
    end
end

local function cached_sim_name(iccid)
    if type(iccid) ~= "string" or #iccid < 18 or #iccid > 22 or not iccid:match("^%x+$") then return end
    local raw = fs.readfile("/tmp/myos-new-sim-name-" .. iccid)
    local entry = raw and json.parse(raw)
    if type(entry) == "table" and entry.iccid == iccid and type(entry.name) == "string" then
        local name = entry.name:match("^%s*(.-)%s*$")
        if name ~= "" and name ~= "unknown" then return util.pcdata(name) end
    end
end

if wireless_page then
    function nradio.support_vht160()
        local radios = bus_call("network.wireless", "status") or {}
        for _, radio in pairs(radios) do
            local cfg = radio.config or {}
            if cfg.band == "5g" and tostring(cfg.htmode):match("160$") then return true end
        end
        return stock_vht160()
    end
end

if path == "/nradio/basic/wan" then
    local stock_wans = nradio.get_wans_data
    function nradio.get_wans_data()
        return require("nros_wan").read(stock_wans())
    end
end

local function add_secondary_status(modem, signal)
    for key, value in pairs(signal) do
        if key ~= "name" and value ~= "" and (key ~= "rsrp" or tonumber(value) ~= 0) then
            modem[key] = value
        end
    end
    modem.odu = "1"
end

local function active_wireless()
    local networks = bus_call("network.wireless", "status") or {}
    local aps = {}
    for _, radio in pairs(networks) do
        local band = (radio.config or {}).band
        if radio.up and (band == "2g" or band == "5g") then
            for _, iface in ipairs(radio.interfaces or {}) do
                local cfg = iface.config or {}
                if iface.ifname and cfg.mode == "ap" and not cfg.disabled then
                    local info = bus_call("iwinfo", "info", {device=iface.ifname}) or {}
                    local assoc = bus_call("iwinfo", "assoclist", {device=iface.ifname}) or {}
                    aps[#aps+1] = {band=band, ssid=info.ssid or cfg.ssid, channel=info.channel,
                                   stations=assoc.results}
                end
            end
        end
    end
    return aps
end

local function classify_terminals(result, selection)
    local clients = result.client or {}
    local by_mac = {}
    for _, client in pairs(clients) do
        if type(client.mac) == "string" then by_mac[client.mac:upper()] = client end
    end
    for _, ap in ipairs(active_wireless()) do
        for _, station in ipairs(ap.stations or {}) do
            local client = station.mac and by_mac[station.mac:upper()]
            if client then
                client.type = "wireless"
                client.frequency = ap.band == "2g" and "2.4" or "5.0"
                client.ssid = ap.ssid
                client.rssi = station.signal
                client.assoctime = station.connected_time or client.assoctime
                local rx, tx = station.rx or {}, station.tx or {}
                client.rxrate = tonumber(rx.rate) or client.rxrate
                client.txrate = tonumber(tx.rate) or client.txrate
                if (tonumber(rx.bytes) or 0) > 0 or (tonumber(tx.bytes) or 0) > 0 then
                    client.rxbytes, client.txbytes = rx.bytes, tx.bytes
                end
            end
        end
    end
    if selection == "wired" or selection == "wireless" then
        local matching, count = {}, 0
        for key, client in pairs(clients) do
            if client.type == selection then
                matching[key] = client
                if client.exsit == 1 then count = count + 1 end
            end
        end
        result.client, result.count = matching, count
    end
end

local function link_counters(name)
    if name ~= "cpe" and name ~= "cpe1" and name ~= "wan" then return end
    local link = bus_call("network.interface." .. name, "status") or {}
    local device = link.l3_device
    if not link.up or type(device) ~= "string" or not device:match("^[%w_.%-]+$") then return end
    local directory = "/sys/class/net/" .. device .. "/statistics/"
    local tx = fs.readfile(directory .. "tx_bytes")
    local rx = fs.readfile(directory .. "rx_bytes")
    return tonumber(tx), tonumber(rx)
end

util.ubus = function(object, method, args, ...)
    local result = bus_call(object, method, args, ...)
    if type(result) ~= "table" then return result end
    if object == "infocd" and method == "terminal" then
        local selection = args and args.type
        if selection == "wired" or selection == "wireless" then
            result = bus_call(object, method) or result
        end
        classify_terminals(result, selection)
    elseif object == "infocd" and method == "speed_record" then
        for _, line in ipairs(result.list or {}) do
            local tx, rx = link_counters(line.name)
            line.upload, line.download = tx or line.upload, rx or line.download
        end
    elseif object == "infocd" and method == "cpestatus" then
        local secondary, stamp
        for _, entry in ipairs(result.result or {}) do
            local status = entry.status
            if status and status.name == "cpe1" then
                local plugin = require "c5800_secondary"
                secondary, stamp = plugin.cached_signal()
                if secondary and stamp and os.time()-stamp <= 30 then
                    add_secondary_status(status, secondary)
                end
            end
            if status then identify_operators(status) end
        end
    elseif path:match("^/nradio/cellular/cpedevice/") and object == "kpcped" and method == "status" and not args then
        local secondary = require "c5800_secondary"
        local state = secondary.status(false)
        local signal, stamp = secondary.cached_signal()
        local net = bus_call("network.interface.cpe1_4", "status") or {}
        if state.connected and state.host and signal and stamp and os.time()-stamp <= 30 and
            net.up and net.l3_device == "port5" then
            for _, item in ipairs(result.list or {}) do
                if item.host == state.host and item.kind ~= "LOCAL" then
                    item.link = 1
                    item.device_name = state.name
                    if tostring(state.name):match("^AK") then item.kind = "AK" end
                end
            end
        end
    end
    return result
end

function nradio.get_runtime_info(...)
    local data = runtime_info(...)
    local names = {}
    for _, modem in ipairs(data.cpe or {}) do
        names[modem.name] = true
    end
    for _, item in ipairs(data.wans or {}) do
        local name = tostring(item.name or ""):gsub("_[46]$", "")
        if not item.classname and (names[name] or name == "wan") then
            item.classname = name
        end
    end

    -- The local infocd cpe1 record only describes the Ethernet interface.
    -- Read the matched PoE device through the C5800 plugin used by MYOS.
    if names.cpe1 then
        local ok, secondary = pcall(require, "c5800_secondary")
        local signal = ok and secondary.cached_signal()
        if signal then
            for _, modem in ipairs(data.cpe) do
                if modem.name == "cpe1" then
                    add_secondary_status(modem, signal)
                    break
                end
            end
        end
    end
    for _, modem in ipairs(data.cpe or {}) do
        identify_operators(modem)
        if modem.name == "cpe" then
            if not modem.sim_name or modem.sim_name == "" or modem.sim_name == "unknown" then
                modem.sim_name = cached_sim_name(modem.iccid) or modem.sim_name
            end
            if (tonumber(modem.sim_flow) or 0) == 0 and (tonumber(modem.sim_mon) or 0) == 0 then
                local tx, rx = link_counters(modem.name)
                modem.sim_flow = tx and rx and (tx + rx) or nil
                modem.sim_mon = nil
                modem.flow_scope = "interface"
            end
        end
    end

    local wlan = data.wlan and data.wlan.radio
    if type(wlan) == "table" and #wlan > 0 then
        local counts, channels = {}, {}
        for _, ap in ipairs(active_wireless()) do
            local slot = ap.band == "2g" and "wlan0" or "wlan1"
            if ap.stations then counts[slot] = (counts[slot] or 0) + #ap.stations end
            channels[slot] = channels[slot] or ap.channel
        end
        for _, radio in ipairs(wlan) do
            if counts[radio.name] then
                radio.sta = counts[radio.name]
                radio.channel = channels[radio.name] or radio.channel
                radio.phyname = radio.phyname ~= "" and radio.phyname or "mt7981"
            end
        end
    end
    return data
end

local run = cgi.run
cgi.run = function(...)
    require("luci.dispatcher").indexcache = base .. "/luci-indexcache"
    return run(...)
end
