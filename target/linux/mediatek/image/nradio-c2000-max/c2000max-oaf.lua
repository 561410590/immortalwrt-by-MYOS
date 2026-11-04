-- Keep the OAF 7 UI while reading the installed appfilter 5.1 backend.
local util = require "luci.util"
local fs = require "nixio.fs"
local nixio = require "nixio"
local json = require "luci.jsonc"
local M = setmetatable({}, { __index = util })

local function call(object, method, data)
    local ok, result = pcall(util.ubus, object, method, data or {})
    if ok and type(result) == "table" then return result end
end

local function value(path)
    return (fs.readfile(path) or ""):match("^%s*(.-)%s*$")
end

local function client_traffic()
    local paths = call("luci.wrtbwmon", "get_db_path") or {}
    local result = {}
    -- The .46 merge drops speed fields; sum the two original databases instead.
    for _, path in ipairs({ paths.file_4 or "/tmp/usage.db", paths.file_6 or "/tmp/usage.6.db" }) do
        local data = fs.readfile(path) or ""
        local fresh = os.time() - (fs.stat(path, "mtime") or 0) <= 125
        for line in data:gmatch("[^\n]+") do
            local fields = {}
            for field in (line .. ","):gmatch("(.-),") do fields[#fields + 1] = field end
            local mac = fields[1] and fields[1]:lower()
            if mac and #mac == 17 and tonumber(fields[6]) and tonumber(fields[7]) then
                local item = result[mac] or { up_bytes = 0, down_bytes = 0, up_rate = 0, down_rate = 0,
                    first_date = tonumber(fields[9]) or 0, last_date = 0 }
                result[mac] = item
                item.up_bytes = item.up_bytes + tonumber(fields[7])
                item.down_bytes = item.down_bytes + tonumber(fields[6])
                if fresh then
                    item.up_rate = item.up_rate + (tonumber(fields[5]) or 0)
                    item.down_rate = item.down_rate + (tonumber(fields[4]) or 0)
                end
                item.first_date = math.min(item.first_date, tonumber(fields[9]) or 0)
                item.last_date = math.max(item.last_date, tonumber(fields[10]) or 0)
            end
        end
    end
    return result
end

local function wan_traffic()
    local status = call("network.interface.2_1", "status") or {}
    local iface = status.l3_device
    if not iface or not iface:match("^[%w_.-]+$") then return nil end
    local base = "/sys/class/net/" .. iface .. "/statistics/"
    local up, down = tonumber(value(base .. "tx_bytes")), tonumber(value(base .. "rx_bytes"))
    local now = tonumber(value("/proc/uptime"):match("^%S+"))
    if not up or not down or not now then return nil end
    local path = "/tmp/c2000max-oaf-traffic.json"
    local previous = json.parse(fs.readfile(path) or "") or {}
    local lock = nixio.open(path .. ".lock", "w", "600")
    if not lock or not lock:lock("tlock") then
        if lock then lock:close() end
        return previous.result
    end
    previous = json.parse(fs.readfile(path) or "") or {}
    local elapsed = now - (previous.time or now)
    if previous.iface == iface and elapsed >= 0 and elapsed < 1 then
        lock:close()
        return previous.result
    end
    local points = previous.iface == iface and previous.result and previous.result.traffic or {}
    local rate_up, rate_down = 0, 0
    local wall = os.time()
    local days = previous.days or {}
    local day_key, hour_key = os.date("%Y-%m-%d", wall), os.date("%H", wall)
    local day = days[day_key] or { up_bytes = 0, down_bytes = 0, hours = {} }
    days[day_key] = day
    local delta_up, delta_down = 0, 0
    local valid = previous.iface == iface and elapsed >= 1 and previous.up and previous.down
        and up >= previous.up and down >= previous.down
    if valid then
        delta_up, delta_down = up - previous.up, down - previous.down
        if previous.wall and wall >= previous.wall then
            local hour = day.hours[hour_key] or { up_bytes = 0, down_bytes = 0 }
            day.hours[hour_key] = hour
            hour.up_bytes, hour.down_bytes = hour.up_bytes + delta_up, hour.down_bytes + delta_down
            day.up_bytes, day.down_bytes = day.up_bytes + delta_up, day.down_bytes + delta_down
        end
    end
    if previous.iface == iface and elapsed >= 1 and elapsed <= 30
        and up >= (previous.up or up) and down >= (previous.down or down) then
        rate_up, rate_down = delta_up / elapsed, delta_down / elapsed
    else
        points = {}
    end
    points[#points + 1] = { up = rate_up, down = rate_down }
    while #points > 60 do table.remove(points, 1) end
    local dates = {}
    for date in pairs(days) do dates[#dates + 1] = date end
    table.sort(dates)
    for i = 1, #dates - 30 do days[dates[i]] = nil end
    local result = { interface = iface, traffic = points, up_bytes = up, down_bytes = down,
        up_rate = rate_up, down_rate = rate_down, counter_scope = "interface_since_reset",
        today_up_bytes = day.up_bytes, today_down_bytes = day.down_bytes,
        collection_start = previous.collection_start or wall }
    local state = { iface = iface, time = now, wall = wall, up = up, down = down, result = result,
        days = days, collection_start = result.collection_start }
    local file = nixio.open(path .. ".new", "w", "600")
    if file then
        file:writeall(json.stringify(state))
        file:close()
        fs.rename(path .. ".new", path)
    end
    lock:close()
    return result
end

M.sample = wan_traffic

local function users(data)
    data = data or {}
    -- Native flag selects detail fields (3 includes apps), not online state.
    local all = call("appfilter", "get_all_users", {
        flag = tonumber(data.flag) or 3, page = tonumber(data.page) or 1
    })
    if not all then return nil end
    local list = (all.data or {}).list or {}
    local by_mac = {}
    for _, user in ipairs(list) do by_mac[(user.mac or ""):lower()] = user end
    for line in (fs.readfile("/tmp/dhcp.leases") or ""):gmatch("[^\n]+") do
        local mac, ip, hostname = line:match("^%d+%s+(%S+)%s+(%S+)%s+(%S+)")
        local user = mac and by_mac[mac:lower()]
        if user then
            user.ip = user.ip or ip
            if not user.hostname or user.hostname == "" then
                user.hostname = hostname ~= "*" and hostname or ""
            end
        end
    end
    local counters = client_traffic()
    for _, user in ipairs(list) do
        local counter = counters[(user.mac or ""):lower()]
        if counter then
            user.up_bytes, user.down_bytes = counter.up_bytes, counter.down_bytes
            user.total_bytes = counter.up_bytes + counter.down_bytes
            user.up_rate = user.online == 1 and counter.up_rate or 0
            user.down_rate = user.online == 1 and counter.down_rate or 0
            user.traffic_sample_seconds = 60
            user.traffic_updated = counter.last_date
            user.traffic_scope = "wrtbwmon_since_first_seen"
            if os.date("%Y-%m-%d", counter.first_date) == os.date("%Y-%m-%d") then
                user.today_up_bytes, user.today_down_bytes = counter.up_bytes, counter.down_bytes
            end
        end
        -- Legacy dev_list contains historical top apps, not current visits.
        local visits = call("appfilter", "dev_visit_list", { mac = user.mac }) or {}
        user.recent_apps = {}
        local latest = 0
        for _, record in ipairs(visits.list or {}) do
            if user.online == 1 and (record.lt or 0) >= os.time() - 90 then
                user.recent_apps[#user.recent_apps + 1] = { id = record.id, name = record.name }
                if (record.lt or 0) > latest then
                    latest, user.app_id, user.app = record.lt, record.id, record.name
                end
            end
        end
        user.active = #user.recent_apps > 0 and 1 or 0
    end
    return list
end

local function paginate(list, data)
    local size = math.max(1, math.min(100, math.floor(tonumber(data.page_size) or 15)))
    local page = math.max(1, math.floor(tonumber(data.page) or 1))
    local result = {}
    for i = (page - 1) * size + 1, math.min(page * size, #list) do result[#result + 1] = list[i] end
    return { list = result, total_num = #list, total_page = math.max(1, math.ceil(#list / size)),
        page = page, page_size = size }
end

local function feature()
    local data = fs.readfile("/etc/appfilter/feature.cfg") or ""
    local info = { version = data:match("#version%s+(%S+)"),
        format = data:match("#format%s+(%S+)"), app_count = 0, class_count = 0 }
    for line in data:gmatch("[^\n]+") do
        if line:match("^%s*%d+") then info.app_count = info.app_count + 1 end
        if line:match("^#class%s") then info.class_count = info.class_count + 1 end
    end
    return info
end

local direct = {
    class_list = true, dev_visit_list = true, dev_visit_time = true,
    app_class_visit_time = true, get_oaf_status = true, get_app_filter = true,
    get_app_filter_base = true, get_app_filter_time = true, get_app_filter_user = true
}

function M.ubus(object, method, request)
    if object ~= "fwx" or method ~= "common" then return util.ubus(object, method, request) end
    local native = call(object, method, request)
    if native then return native end

    request = request or {}
    local api, data = request.api, request.data or {}
    local result
    if direct[api] then
        result = call("appfilter", api, data)
        result = result and (result.data or result)
    elseif api == "get_all_users" or api == "get_active_users" or api == "get_user_basic_info"
        or api == "get_daily_top_users" then
        local list = users(api == "get_all_users" and data or nil)
        if list then
            if api == "get_daily_top_users" then
                table.sort(list, function(a, b)
                    return (a.up_bytes or 0) + (a.down_bytes or 0) > (b.up_bytes or 0) + (b.down_bytes or 0)
                end)
                local today = {}
                for _, user in ipairs(list) do
                    if user.today_up_bytes and user.today_down_bytes then today[#today + 1] = user end
                end
                while #today > 8 do table.remove(today) end
                result = { users = today, total_count = #today, date = tonumber(os.date("%Y%m%d")),
                    traffic_scope = "wrtbwmon_current_day_only" }
            elseif api == "get_user_basic_info" then
                for _, user in ipairs(list) do
                    if (user.mac or ""):lower() == tostring(data.mac or ""):lower() then result = user end
                end
            elseif api == "get_active_users" then
                local active = {}
                for _, user in ipairs(list) do if user.online == 1 then active[#active + 1] = user end end
                local page = paginate(active, { page_size = tonumber(data.count) or 8 })
                result = { users = page.list, total_count = #active }
            else
                result = paginate(list, data)
            end
        end
    elseif api == "get_feature_info" then
        result = feature()
    elseif api == "get_global_app_type_stats" then
        local list = users()
        if list then
            local now = os.time()
            local midnight = os.date("*t", now)
            midnight.hour, midnight.min, midnight.sec = 0, 0, 0
            local cutoff = data.type == "hourly" and now - 3600 or os.time(midnight)
            local groups = {}
            for _, user in ipairs(list) do
                local classes = call("appfilter", "app_class_visit_time", { mac = user.mac }) or {}
                for _, class in ipairs(classes.class_list or {}) do
                    groups[class.type] = groups[class.type] or {
                        app_type = class.type, name = class.name, total_time = 0
                    }
                end
                local visits = call("appfilter", "dev_visit_list", { mac = user.mac }) or {}
                for _, visit in ipairs(visits.list or {}) do
                    local first, last = tonumber(visit.ft) or now, tonumber(visit.lt) or now
                    local duration = math.max(0, math.min(now, last) - math.max(cutoff, first))
                    local group = groups[math.floor((tonumber(visit.id) or 0) / 1000) - 1]
                    if group then group.total_time = group.total_time + duration end
                end
            end
            local types, total = {}, 0
            for _, group in pairs(groups) do
                if group.total_time > 0 then
                    types[#types + 1] = group
                    total = total + group.total_time
                end
            end
            table.sort(types, function(a, b) return a.total_time > b.total_time end)
            result = { types = types, total_time = total, total_count = #types,
                type = data.type or "daily", start_time = cutoff, end_time = now,
                duration_source = "appfilter_visit_intervals" }
        end
    elseif api == "get_global_traffic_stats" or api == "get_history_traffic_stats" then
        wan_traffic()
        local state = json.parse(fs.readfile("/tmp/c2000max-oaf-traffic.json") or "") or {}
        local days = state.days or {}
        if api == "get_global_traffic_stats" then
            local day = days[os.date("%Y-%m-%d")] or {}
            local hours = {}
            for hour, item in pairs(day.hours or {}) do
                hours[#hours + 1] = { hour = tonumber(hour), up_bytes = item.up_bytes,
                    down_bytes = item.down_bytes, total_bytes = item.up_bytes + item.down_bytes }
            end
            table.sort(hours, function(a, b) return a.hour < b.hour end)
            result = { date = tonumber(os.date("%Y%m%d")), is_today = 1, hourly_traffic = hours }
        else
            local list = {}
            for date, item in pairs(days) do
                list[#list + 1] = { date_str = date, up_bytes = item.up_bytes,
                    down_bytes = item.down_bytes, total_bytes = item.up_bytes + item.down_bytes }
            end
            table.sort(list, function(a, b) return a.date_str < b.date_str end)
            result = { days = math.min(30, math.max(1, tonumber(data.days) or 30)), list = list }
        end
        result.collection_start = state.collection_start
        result.history_scope = "collector_since_start"
    elseif api == "get_init_status" then
        result = { init_status = 1 }
    elseif api == "get_system_base_info" then
        result = { user_session_enable = 0, user_rssi_enable = 0 }
    elseif api == "get_system_info" then
        local adv = call("appfilter", "get_app_filter_adv")
        result = adv and { fwx = { lan_ifname = (adv.data or {}).lan_ifname, theme_mode = 0 } }
    elseif api == "get_work_mode" or api == "get_tcp_rst" or api == "get_advanced_settings" then
        local key = api == "get_work_mode" and "work_mode" or api == "get_tcp_rst" and "tcp_rst" or "disable_hnat"
        local response = call("appfilter", key == "work_mode" and "get_app_filter_base" or "get_app_filter_adv")
        result = response and { [key] = (response.data or {})[key] }
    elseif api == "get_app_filter_adv" then
        local response = call("appfilter", "get_app_filter_base")
        result = response and { enable = (response.data or {}).enable }
    elseif api == "get_dashboard_common" then
        local list = users()
        local info = call("system", "info")
        local board = call("system", "board")
        if list and info and board then
            local online, apps, app_list = 0, {}, {}
            for _, user in ipairs(list) do
                if user.online == 1 then online = online + 1 end
                for _, app in ipairs(user.recent_apps or {}) do
                    if not apps[app.id] then
                        apps[app.id] = true
                        app_list[#app_list + 1] = { id = app.id, name = app.name }
                    end
                end
            end
            local network = {}
            for name, iface in pairs({ lan = "lan", wan = "2_1" }) do
                local status = call("network.interface." .. iface, "status") or {}
                local addr = (status["ipv4-address"] or {})[1] or {}
                network[name] = { ip = addr.address, dns = status["dns-server"] }
                for _, route in ipairs(status.route or {}) do
                    if route.target == "0.0.0.0" and route.mask == 0 then network[name].gateway = route.nexthop end
                end
            end
            local mem = info.memory or {}
            local ok, traffic = pcall(wan_traffic)
            if not ok then traffic = nil end
            local base = (call("appfilter", "get_app_filter_base") or {}).data or {}
            local adv = (call("appfilter", "get_app_filter_adv") or {}).data or {}
            result = {
                system_status = { uptime = info.uptime, hostname = board.hostname,
                    openwrt_version = (board.release or {}).version,
                    total_mem = math.floor((mem.total or 0) / 1024),
                    used_mem = math.floor(((mem.total or 0) - (mem.available or mem.free or 0)) / 1024),
                    connections = tonumber(value("/proc/sys/net/netfilter/nf_conntrack_count")),
                    flow = traffic and { today_up = traffic.today_up_bytes / 1024,
                        today_down = traffic.today_down_bytes / 1024 },
                    cpu_temp = (tonumber(value("/sys/class/thermal/thermal_zone0/temp")) or 0) / 1000,
                    client_num = #list, online_user_count = online, offline_user_count = #list - online },
                network_status = network,
                interface_traffic = traffic,
                active_app = { total = #app_list, list = app_list },
                active_host = { total = 0, list = {}, supported = false },
                advanced = { disable_hnat = adv.disable_hnat, notice_status = 0 },
                oaf_status = { version = "7.0 UI / 5.1 backend", engine_version = value("/proc/sys/oaf/version"),
                    work_mode = base.work_mode, feature = feature() },
                backend = "appfilter", history_scope = "collector_since_start",
                multi_rule_supported = false, host_visits_supported = false
            }
        end
    elseif api == "get_active_app_records" or api == "get_app_history_records" then
        local list = users()
        if list then
            local records = {}
            for i, user in ipairs(list) do
                if i > 64 then break end
                if not data.mac or data.mac == "" or tostring(data.mac):lower() == (user.mac or ""):lower() then
                    local visits = call("appfilter", "dev_visit_list", { mac = user.mac }) or {}
                    for _, record in ipairs(visits.list or {}) do
                        local start_time, end_time = tonumber(data.start_time) or 0, tonumber(data.end_time) or 0
                        local appid = tonumber(data.appid) or 0
                        if (appid == 0 or record.id == appid) and (record.lt or 0) >= start_time
                            and (end_time == 0 or (record.ft or 0) <= end_time) then
                            record.mac, record.ip = user.mac, visits.ip or user.ip
                            record.hostname, record.nickname = visits.hostname or user.hostname, user.nickname
                            records[#records + 1] = record
                        end
                    end
                end
            end
            table.sort(records, function(a, b) return (a.lt or 0) > (b.lt or 0) end)
            result = paginate(records, data)
            result.history_scope = "daemon_memory"
        end
    end

    if result then return { code = 2000, data = result } end
    return { code = 4000, data = { error = "API unavailable with the installed appfilter 5.1 backend" },
        message = "API unavailable with the installed appfilter 5.1 backend" }
end

return M
