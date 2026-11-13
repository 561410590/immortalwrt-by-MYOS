local M = {}
local nixio = require "nixio"
local perf = require "luci.model.myos_liquid"
local window = 60

local function uptime()
    return tonumber(perf.read("/proc/uptime"):match("^[%d.]+"))
end

local function trim(points, oldest)
    while points[1] and points[1].time < oldest do table.remove(points, 1) end
end

local function traffic_counters(uplinks)
    local stats, counters = perf.net_counters(), {}
    for _, link in ipairs(uplinks) do
        local base = "/sys/class/net/" .. link.name .. "/statistics/"
        local current = stats[link.name] or {
            rx=tonumber(perf.read(base .. "rx_bytes")),
            tx=tonumber(perf.read(base .. "tx_bytes"))
        }
        if not current or not current.rx or not current.tx then return end
        counters[link.name] = current
    end
    return counters
end

local function traffic_rate(previous, current, elapsed)
    if not previous or not current or elapsed <= 0 or elapsed > 3 then return end
    local rx, tx, count = 0, 0, 0
    for name, value in pairs(current) do
        local old = previous[name]
        if not old or value.rx < old.rx or value.tx < old.tx then return end
        rx, tx, count = rx + value.rx - old.rx, tx + value.tx - old.tx, count + 1
    end
    for name in pairs(previous) do if not current[name] then return end end
    if count > 0 then return rx / elapsed, tx / elapsed end
end

local function read_uplinks()
    local bus = require("ubus").connect()
    if not bus then return end
    local ok, data = pcall(bus.call, bus, "network.interface", "dump", {})
    bus:close()
    if not ok or not data or not data.interface then return end
    local uci = require("luci.model.uci").cursor()
    local devices = perf.interface_devices(data.interface, uci)
    local links = {}
    for name in pairs(perf.default_uplinks(data.interface, devices)) do
        if name:match("^[%w_.:-]+$") then links[#links+1] = {name=name} end
    end
    return links
end

function M.run(kind)
    local controller = kind == "quality" and require "luci.controller.myos_new"
    local boot = perf.read("/proc/sys/kernel/random/boot_id"):gsub("%s+$", "")
    local key = "history-" .. kind
    local saved = perf.cached(key)
    local points = saved.boot == boot and saved.points or {}
    local previous, last_time, gap, uplinks, target
    if saved.boot == boot then target = saved.target end
    local next_routes = 0
    while true do
        local started = uptime()
        if started >= next_routes then
            uplinks = read_uplinks()
            next_routes = started + 5
        end
        if uplinks then
            if kind == "traffic" then
                local current = traffic_counters(uplinks)
                local down, up = traffic_rate(previous, current, started - (last_time or started))
                if down then
                    points[#points+1] = {time=started, down=down, up=up, gap=gap or nil}
                end
                previous, last_time, gap = current, started, not down
            else
                local quality = controller.link_quality_data(nil, {uplinks=uplinks})
                if target ~= quality.ping.target then
                    points, target = {}, quality.ping.target
                end
                local samples = quality.samples
                if next(samples) then points[#points+1] = {time=uptime(), samples=samples} end
            end
            local now = uptime()
            trim(points, now - window)
            perf.save(key, {boot=boot, time=now, points=points, target=target})
        else
            previous, last_time, gap = nil, nil, true
        end
        local delay = math.max(0.1, 1 - (uptime() - started))
        nixio.nanosleep(math.floor(delay), math.floor(delay % 1 * 1000000000))
    end
end

local function read(kind, boot, now, since, target)
    local cache = perf.cached("history-" .. kind)
    if cache.boot ~= boot or not cache.time or now - cache.time > 5 then return end
    local points = {}
    if target and cache.target ~= target then return {points=points} end
    for _, point in ipairs(cache.points) do
        if point.time >= now - window and point.time > since then points[#points+1] = point end
    end
    return {points=points}
end

function M.attach(data, traffic_since, quality_since)
    if traffic_since == nil or quality_since == nil then return data end
    data.ping = perf.ping_settings()
    data.traffic_history = read("traffic", data.boot, data.sample, tonumber(traffic_since) or 0)
    data.quality_history = read("quality", data.boot, data.sample, tonumber(quality_since) or 0, data.ping.target)
    return data
end

return M
