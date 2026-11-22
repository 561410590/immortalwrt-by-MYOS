local base = assert(os.getenv("NROS_BASE"))
local fs = require "nixio.fs"
local uci = require "uci"
local methods = getmetatable(uci.cursor()).__index
local config = setmetatable({}, { __mode = "k" })
local get_all, foreach = methods.get_all, methods.foreach
local get = methods.get

-- Original diagnostics expect the original IP binding with string MACs.
package.preload["luci.ip"] = assert(package.loadlib(base .. "/lua/luci/ip.so", "luaopen_luci_ip"))

-- A fresh MYOS install has no cloudd discovery records for the local radios.
local local_board
local function fallback_board(cursor)
    if local_board ~= nil then return local_board or nil end
    local_board = false
    local id = get(cursor, "oem", "board", "id")
    if not id then return end
    id = id:gsub(":", "")
    local found
    foreach(cursor, "cloudd", "cboard", function(board)
        if board.id == id then found = true; return false end
    end)
    if found then return end
    local counts = {["2g"]=0, ["5g"]=0}
    foreach(cursor, "wireless", "wifi-device", function(radio)
        if counts[radio.band] then counts[radio.band] = counts[radio.band] + 1 end
    end)
    if counts["2g"] + counts["5g"] == 0 then return end
    local_board = {
        d0={[".name"]="d0", [".type"]="device", id=id, next_cb_idx="1"},
        d0cboard0={[".name"]="d0cboard0", [".type"]="cboard", id=id, pos="0",
                    r2cnt=tostring(counts["2g"]), p2cnt=tostring(counts["2g"]),
                    r5cnt=tostring(counts["5g"]), p5cnt=tostring(counts["5g"])},
    }
    return local_board
end

-- The original views read cloudd's projection of the active wireless settings.
local function wireless_projection(cursor)
    local group
    foreach(cursor, "cloudd", "cboard", function(board)
        group = tonumber(board.pos)
        return false
    end)
    if not group then
        local board = fallback_board(cursor)
        if not board then return nil end
        group = tonumber(board.d0cboard0.pos)
    end

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
    template[".type"] = "template"
    return {
        [name]={[".type"]="group", template={"t0","t0"}},
        [name .. "wlan"]=wlan, [name .. "radio"]=radio, t0=template
    }
end
local mapped
local function projected_section(cursor, section)
    local board = fallback_board(cursor)
    if board and board[section] then return board[section] end
    if get(cursor, "cloudd", "t0") then return end
    if not mapped then mapped = wireless_projection(cursor) or {} end
    return mapped[section]
end
methods.get_all = function(cursor, package, section, ...)
    if package == "cloudd" and section then
        local current = projected_section(cursor, section)
        if current then return current end
    end
    return get_all(cursor, package, section, ...)
end
methods.get = function(cursor, package, section, option, ...)
    if package == "cloudd" and section then
        local current = projected_section(cursor, section)
        if current then
            local value = current[option or ".type"]
            if value ~= nil then return value end
        end
    end
    if option == nil then return get(cursor, package, section) end
    return get(cursor, package, section, option, ...)
end
methods.foreach = function(cursor, package, kind, fn, ...)
    if package == "cloudd" and (kind == "device" or kind == "cboard") then
        local board = fallback_board(cursor)
        if board then
            fn(kind == "device" and board.d0 or board.d0cboard0)
            return true
        end
    end
    return foreach(cursor, package, kind, fn, ...)
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
local nradio = require "luci.nradio"
local util = require "luci.util"
local bus = util.ubus
local runtime

local function snapshot()
    if not runtime then runtime = bus("infocd", "runtime") or {} end
    return runtime
end

local function addresses(...)
    local result = {}
    for _, net in ipairs({...}) do
        for _, family in ipairs({"ipv4-address", "ipv6-address"}) do
            for _, item in ipairs(net[family] or {}) do
                if item.address and item.mask then
                    local row = {ipaddr=item.address .. "/" .. item.mask}
                    if family == "ipv4-address" then
                        local octets = {}
                        for i = 0, 3 do
                            local bits = math.max(0, math.min(8, item.mask - i * 8))
                            octets[i+1] = 256 - 2 ^ (8 - bits)
                        end
                        row.netmask = table.concat(octets, ".")
                    end
                    result[#result+1] = row
                end
            end
        end
    end
    return result
end

local function wan_status()
    local net = bus("network.interface.wan", "status") or {}
    local net6 = bus("network.interface.wan6", "status") or {}
    local device = net.l3_device or net6.l3_device or net.device or net6.device
    local tx, rx
    if device and device:match("^[%w_.%-]+$") then
        local root = "/sys/class/net/" .. device .. "/statistics/"
        tx, rx = tonumber(fs.readfile(root .. "tx_bytes")), tonumber(fs.readfile(root .. "rx_bytes"))
    end
    local gateway
    for _, route in ipairs(net.route or {}) do
        if route.target == "0.0.0.0" and route.mask == 0 then
            gateway = route.nexthop
            break
        end
    end
    return {
        name="wan", classname="wan", type="wired", proto=net.proto, device=device,
        status=(net.up or net6.up) and 0 or 1, uptime=net.uptime or net6.uptime,
        ipaddrs=addresses(net, net6), gateway=gateway, upload=tx, download=rx
    }
end

local function terminals()
    local source = snapshot().terminal_all or {}
    local rows = {}
    for _, row in pairs(source.client or {}) do
        if type(row.mac) == "string" then
            row.exsit = 1
            row.frequency = row.frequency or ""
            row.type = row.type or "wired"
            row.qos = row.qos or "0"
            rows[row.mac:upper()] = row
        end
    end
    local radios = bus("network.wireless", "status") or {}
    local bands = {}
    for _, device in pairs(radios) do
        local band = (device.config or {}).band
        if device.up and (band == "2g" or band == "5g") then
            for _, interface in ipairs(device.interfaces or {}) do
                if interface.ifname and (interface.config or {}).mode == "ap" then
                    local request = {device=interface.ifname}
                    local info = bus("iwinfo", "info", request) or {}
                    local stations = bus("iwinfo", "assoclist", request) or {}
                    local stats = bands[band] or {count=0, channel=info.channel, phyname=info.phyname}
                    bands[band] = stats
                    for _, station in ipairs(stations.results or {}) do
                        if station.mac then
                            local mac = station.mac:upper()
                            local row = rows[mac] or {mac=mac,exsit=1,qos="0"}
                            local rx, tx = station.rx or {}, station.tx or {}
                            row.type = "wireless"
                            row.frequency = band == "2g" and "2.4" or "5.0"
                            row.ssid, row.rssi, row.assoctime = info.ssid, station.signal, station.connected_time
                            row.rxrate, row.txrate = rx.rate, tx.rate
                            if (tonumber(rx.bytes) or 0) > 0 or (tonumber(tx.bytes) or 0) > 0 then
                                row.rxbytes, row.txbytes = rx.bytes, tx.bytes
                            end
                            rows[mac] = row
                            stats.count = stats.count + 1
                        end
                    end
                end
            end
        end
    end
    return rows, bands
end

local function present_terminals(mode)
    local rows = terminals()
    local result = {client={},count=0}
    for mac,row in pairs(rows) do
        if not mode or mode == "all" or row.type == mode then
            result.client[mac] = row
            result.count = result.count + 1
        end
    end
    return result
end

util.ubus = function(object, method, args, ...)
    local result = bus(object, method, args, ...)
    if object ~= "infocd" or type(result) == "table" then return result end
    if method == "terminal" then return present_terminals(args and args.type) end
    if method == "speed_record" then
        local wan = wan_status()
        return {list=wan.upload and wan.download and {
            {name=wan.name, upload=wan.upload, download=wan.download}
        } or {}}
    end
end

local stock_wan_port = nradio.has_wan_port
local stock_has_wan = nradio.has_wan
local stock_switch_info = nradio.get_nrswitch_info
local stock_wans_data = nradio.get_wans_data

function nradio.has_wan_port()
    return stock_wan_port() or uci.cursor():get("network", "wan", "device") == "eth1"
end

function nradio.has_wan()
    return stock_has_wan() or uci.cursor():get("network", "wan", "device") == "eth1"
end

function nradio.get_nrswitch_info()
    local info = stock_switch_info()
    if info.count > 0 then return info end
    return {
        count=2, last_wan_port=1,
        port={
            ["1"]={port=0, index=1, label="eth0", type="L"},
            ["2"]={port=1, index=2, label="eth1", type="W"}
        }
    }
end

function nradio.get_wans_data()
    local result = stock_wans_data()
    local cursor = uci.cursor()
    for _, wan in ipairs(result.wans) do
        local section = cursor:get_all("network", wan.name) or {}
        wan.weight = wan.weight or "1"
        wan.disabled = (section.auto == "0" or section.disabled == "1") and "1" or "0"
    end
    return result
end

local link_speed = {[10]=1,[100]=2,[1000]=3,[2500]=4}
local function port_link(device, is_wan)
    local path = "/sys/class/net/" .. device .. "/"
    local carrier = tonumber(fs.readfile(path .. "carrier"))
    local speed = carrier == 1 and tonumber(fs.readfile(path .. "speed"))
    return {
        label=device, mode=is_wan and 1 or 0, belong=0,
        status=carrier == 1 and 0 or 1, speed=link_speed[speed] or 0
    }
end

function nradio.get_runtime_info()
    local result = snapshot()
    result.wans = {wan_status()}
    result.link = {port_link("eth0", false), port_link("eth1", true)}
    result.lans = {{ipaddrs=addresses(bus("network.interface.lan", "status") or {})}}
    local rows,bands = terminals()
    local count = 0
    for _ in pairs(rows) do count = count + 1 end
    result.terminal_all = {client=rows, count=count}
    local wifi = nradio.get_wifi_info()
    result.wlan = {radio={}}
    for _, board in ipairs(wifi.wifi or {}) do
        for _, radio in ipairs(board.radio or {}) do
            local band = radio.section == "wlan0" and "2g" or "5g"
            local live = bands[band] or {}
            radio.name, radio.device_id = radio.section, board.id
            radio.channel = live.channel or radio.channel
            radio.sta = live.count or 0
            radio.phyname = live.phyname or "mt7992"
            result.wlan.radio[#result.wlan.radio+1] = radio
        end
    end
    return result
end

local run = cgi.run
cgi.run = function(...)
    require("luci.dispatcher").indexcache = base .. "/luci-indexcache"
    return run(...)
end
