module("luci.controller.zz_mwrt_mwrt_assurance", package.seeall)

function index()
    if require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") ~= "/luci-static/argon" then return end
    entry({"admin", "network", "mwrt_assurance_status"},
        call("get_status")).leaf = true
end

-- 获取指定接口网关
function get_gateway(ifname)
    local cmd = string.format("ip route show dev %s | grep '^default ' | awk '{print $3}'", ifname)
    local pipe = io.popen(cmd)
    if pipe then
        local gw = pipe:read("*l") or ""
        pipe:close()
        return gw
    end
    return ""
end

-- 检查接口是否可访问目标
function check_online(ifname, target)
    local ping_cmd = string.format("ping -I %s -c 1 -W 1 %s >/dev/null 2>&1", ifname, target)
    return (os.execute(ping_cmd) == 0)
end

function check_online_temp(ifname, target)
    -- 开启接口，不影响路由
    os.execute(string.format("ip link set %s up", ifname))

    -- 获取接口 IP
    local pipe = io.popen(string.format("ip addr show dev %s | grep 'inet ' | awk '{print $2}'", ifname))
    local ip_cidr = pipe:read("*l") or ""
    pipe:close()

    if ip_cidr == "" then
        -- 接口没有 IP 就直接失败
        return false
    end

    local source_ip = ip_cidr:match("^(%d+%.%d+%.%d+%.%d+)")
    local ping_cmd = string.format("ping -c 1 -W 1 -I %s %s >/dev/null 2>&1", source_ip, target)

    return (os.execute(ping_cmd) == 0)
end


-- 获取当前状态信息（返回 JSON）
function get_status()
    local uci = require("luci.model.uci").cursor()
    local ok, native = pcall(require, "myos_nradio_data")
    if ok and native.is_native_c5800() then
        local bus = require("ubus").connect()
        local cellular, wired, current_if, gateway = false, false, "", ""
        local dump = bus and bus:call("network.interface", "dump", {}) or {}
        for _, iface in ipairs(dump.interface or {}) do
            if iface.up then
                for _, route in ipairs(iface.route or {}) do
                    if route.target == "0.0.0.0" and tonumber(route.mask) == 0 then
                        local name = iface.interface or ""
                        if name:match("^cpe[01]?[_]?%d*$") then
                            cellular = true
                        elseif name ~= "lan" and name ~= "loopback" then
                            wired = true
                        end
                        if current_if == "" or name:match("^cpe") then
                            current_if = iface.l3_device or iface.device or ""
                            gateway = route.nexthop or ""
                        end
                    end
                end
            end
        end
        if bus then bus:close() end
        local function label(up)
            return up and "在线 / Online" or "离线 / Offline"
        end
        luci.http.prepare_content("application/json")
        luci.http.write_json({
            ["当前物理接口 / Current Interface"] = current_if,
            ["当前网关 / Gateway"] = gateway,
            ["当前是否联网 / Current Status"] = label(cellular or wired),
            ["宽带接口状态 / Primary Status"] = label(wired),
            ["蜂窝接口状态 / Secondary Status"] = label(cellular),
            ["主接口有网切换 / Force Primary"] = "已关闭 / Disabled"
        })
        return
    end
    local primary_if    = uci:get("Mwrt-Assurance", "wan_failover", "primary_if") or "eth1"
    local secondary_if  = uci:get("Mwrt-Assurance", "wan_failover", "secondary_if") or "eth2"
    local target        = uci:get("Mwrt-Assurance", "wan_failover", "ping_target") or "8.8.8.8"
    local force_primary = uci:get("Mwrt-Assurance", "wan_failover", "force_primary") or "0"

    -- 获取当前默认路由接口
    local pipe = io.popen("ip route | grep '^default ' | awk '{print $5}'")
    local current_if = pipe:read("*l") or ""
    pipe:close()

    local status_online  = "在线 / Online"
    local status_offline = "离线 / Offline"
    local status_enable  = "已启用 / Enabled"
    local status_disable = "已关闭 / Disabled"

    local status_data = {
        ["当前物理接口 / Current Interface"]       = current_if,
        ["当前网关 / Gateway"]                      = get_gateway(current_if),
        ["当前是否联网 / Current Status"]           = check_online(current_if, target) and status_online or status_offline,
        ["宽带接口状态 / Primary Status"]         = check_online(primary_if, target) and status_online or status_offline,
        ["蜂窝接口状态 / Secondary Status"]     = check_online(secondary_if, target) and status_online or status_offline,
        ["主接口有网切换 / Force Primary"]           = (force_primary == "1") and status_enable or status_disable
    }

    luci.http.prepare_content("application/json")
    luci.http.write_json(status_data)
end



-- 获取 failover 日志
function get_logs()
    local fs = require("nixio.fs")
    local log = fs.readfile("/tmp/mwrt_failover.log") or "暂无日志 / No logs"
    luci.http.prepare_content("text/plain; charset=utf-8")
    luci.http.write(log)
end
