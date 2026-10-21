module("luci.controller.zz_mwrt_mwrt_assurance", package.seeall)

local fs = require "nixio.fs"

local CONF = "Mwrt-Assurance"
local SECT = "wan_failover"
local LOG  = "/tmp/mwrt_failover.log"
local NET  = "/sys/class/net/"

function index()
    if require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") ~= "/luci-static/argon" then return end
    if not nixio.fs.access("/etc/config/Mwrt-Assurance") then
        return
    end

    -- 配置页面
    entry({"admin", "network", "mwrt_assurance"},
        cbi("zmode/mwrt_wan_failover"),
        _("网关存活"), 80).dependent = true

    -- 状态展示页面
    entry({"admin", "network", "mwrt_Failover_status"},
        template("zmode/mwrt_wan_failover_status"),
        _("存活状态"), 96)

    -- modeminfo 页(WTModem 和 luci-app-modem 两份 modem_info.htm 的 gw())用的接口,
    -- 按中文键取值、按"在线"判断颜色, 改键名要两边一起改。
    -- 存活状态页改用下面的 mwrt_assurance_state。
    entry({"admin", "network", "mwrt_assurance_status"},
        call("get_status")).leaf = true

    -- 存活状态页: 状态
    entry({"admin", "network", "mwrt_assurance_state"},
        call("get_state")).leaf = true

    -- 存活状态页: 日志(JSON, 新的在前)
    entry({"admin", "network", "mwrt_assurance_logs"},
        call("get_logs")).leaf = true

    -- 原来还注册了 mwrt_assurance_switch -> call("switch_wan"), 但 switch_wan 从来
    -- 没实现过, 访问就是 500, 删掉。
end

local function rd(path)
    local s = fs.readfile(path)
    return s and (s:gsub("%s+$", "")) or nil
end

-- 网卡名会拼进 shell 命令和 sysfs 路径, 只放行正常的名字
local function safe_if(n)
    return type(n) == "string" and n:match("^%w[%w%._%-]*$") ~= nil
end

-- 网口实况, 全从 sysfs 读(状态页 5 秒问一次, 不为它 fork)
--   bridge: 是网桥的成员口时为网桥名(比如 br-lan), 即"作 LAN 口用"
local function ifinfo(name)
    if not safe_if(name) or not fs.access(NET .. name) then
        return { exists = false }
    end
    local d = NET .. name
    local br
    if fs.access(d .. "/brport") then
        br = fs.basename(fs.readlink(d .. "/master") or "") or "bridge"
    end
    return {
        exists = true,
        -- 口没起来时读 carrier 会报错(nil), 也算没连上
        up = rd(d .. "/carrier") == "1" and rd(d .. "/operstate") ~= "down",
        bridge = br,
    }
end

-- /proc/net/route 里的网关是小端十六进制, 比如 0100A8C0 = 192.168.0.1
local function hex2ip(h)
    local a, b, c, d = h:match("^(%x%x)(%x%x)(%x%x)(%x%x)$")
    if not a or h == "00000000" then return "" end
    return string.format("%d.%d.%d.%d",
        tonumber(d, 16), tonumber(c, 16), tonumber(b, 16), tonumber(a, 16))
end

-- 全部 IPv4 默认路由, 按 metric 从小到大(第一条就是实际出口);
-- via[口] = true 表示这个口自己有一条默认路由
local function default_routes()
    local list, via = {}, {}
    local f = io.open("/proc/net/route", "r")
    if f then
        for line in f:lines() do
            local dev, dst, gw, metric, mask = line:match(
                "^(%S+)%s+(%x+)%s+(%x+)%s+%x+%s+%d+%s+%d+%s+(%d+)%s+(%x+)")
            if dev and dst == "00000000" and mask == "00000000" then
                list[#list + 1] = { dev = dev, gw = hex2ip(gw), metric = tonumber(metric) }
                via[dev] = true
            end
        end
        f:close()
    end
    table.sort(list, function(x, y) return x.metric < y.metric end)
    list.via = via
    return list
end

-- 并行 ping: 一次 popen 里每个口一条后台 ping, 一共最多等 1 秒。
-- 原来是一个口一个 os.execute 串行跑, 三个口都不通就要 3 秒, 而 modeminfo 页
-- 每 2 秒就问一次。返回 ping 通了的口的集合。
local function ping_many(list, target)
    local ok, parts, seen = {}, {}, {}
    if type(target) ~= "string" or not target:match("^[%w%.:%-]+$") then
        return ok
    end
    for _, n in ipairs(list) do
        if safe_if(n) and not seen[n] then
            seen[n] = true
            parts[#parts + 1] = string.format(
                "(ping -I %s -c 1 -W 1 %s >/dev/null 2>&1 && echo %s) &", n, target, n)
        end
    end
    if #parts == 0 then return ok end
    local p = io.popen(table.concat(parts, " ") .. " wait")
    if p then
        for l in p:lines() do ok[l] = true end
        p:close()
    end
    return ok
end

local function daemon_running()
    for pid in fs.dir("/proc") or function() end do
        if pid:match("^%d+$") then
            local c = fs.readfile("/proc/" .. pid .. "/cmdline")
            if c and c:find("mwrt_wan_failover.sh", 1, true) then
                return true
            end
        end
    end
    return false
end

-- 本机的物理网口: 带 phydev 的网络设备(DSA 板子上就是 lan1/wan 这些用户口)。
-- C2000MAX 只有 eth1 一个, 它的 eth0 是 GMAC 留下的空壳, 没有 PHY。
local function phy_ports()
    local t = {}
    for n in fs.dir(NET) or function() end do
        if fs.access(NET .. n .. "/phydev") then
            t[#t + 1] = n
        end
    end
    table.sort(t)
    return t
end

-- 状态页和设置页共用的提示, 返回 { {level="info"|"warn", text=...}, ... }。
-- c 是 uci cursor: 设置页传 m.uci, 刚点"保存并应用"时提示按新值算。
--
-- 单网口机型(C2000MAX)出厂这个口在 br-lan 里作 LAN 口, 所以网关存活出厂关闭;
-- 用户把它改成 WAN 口(不在网桥里了)才有意义, 这时提示可以开启。
function hints(c)
    local out = {}
    local function add(level, fmt, ...)
        out[#out + 1] = { level = level, text = string.format(fmt, ...) }
    end
    local en  = (c:get(CONF, SECT, "enabled") or "1") == "1"
    local pif = c:get(CONF, SECT, "primary_if") or ""
    local ports = phy_ports()

    if #ports == 1 then
        local p = ports[1]
        if ifinfo(p).bridge then
            if en then
                add("warn", "本机只有一个网口（%s），现在作 LAN 口用，没有接宽带，网关存活起不到作用，建议关闭。把网口改成 WAN 口接宽带后再开启。", p)
            else
                add("info", "本机只有一个网口（%s），默认作 LAN 口用，所以网关存活默认关闭。如果把这个网口改成 WAN 口接宽带，就可以开启网关存活：宽带断网时自动切到备用口（蜂窝），恢复后再切回宽带。", p)
            end
        elseif not en then
            add("info", "检测到网口（%s）已改成 WAN 口，可以开启网关存活：宽带断网时自动切到备用口（蜂窝），恢复后再切回宽带。%s", p,
                pif ~= p and string.format("开启时主 WAN 口请填 %s。", p) or "")
        elseif pif ~= p and pif:match("^eth") then
            -- PPPoE 时主口填 pppoe-wan 是对的, 只拦另一个 ethX(空壳 eth0 / 模组的 eth2)
            add("warn", "主 WAN 口填的是 %s，但本机唯一的网口是 %s，请把主 WAN 口改成 %s。", pif, p, p)
        end
    elseif en then
        local i = ifinfo(pif)
        if not i.exists then
            add("warn", "主 WAN 口 %s 不存在，网关存活无法工作，请在设置里改成实际的网口。", pif ~= "" and pif or "（未填写）")
        elseif i.bridge then
            add("warn", "主 WAN 口 %s 现在在 LAN 网桥里（作 LAN 口用），网关存活没法用它判断宽带，请换成 WAN 口。", pif)
        end
    end

    -- 守护进程由 modeminit 拉起, 模组被禁用时它直接退出, 不会启动监测
    if en and not daemon_running() then
        add("warn", "网关存活已开启，但后台监测服务没有运行，不会自动切换线路（模组被禁用时服务不启动）。")
    end
    return out
end

-- 存活状态页的数据
function get_state()
    local c = require("luci.model.uci").cursor()
    local function g(k, d) return c:get(CONF, SECT, k) or d end
    local pif, sif = g("primary_if", ""), g("secondary_if", "")
    local target = g("ping_target", "223.5.5.5")
    local routes = default_routes()
    local cur = routes[1]
    local pi, si = ifinfo(pif), ifinfo(sif)

    -- 只 ping 能得出结论的口: 连着、不在网桥里、而且自己有默认路由。
    -- ping -I 是绑口发包, 没有经这个口的路由就必然失败 —— 那不叫"不通"。
    local list = {}
    local function want(n, i)
        if i.exists and i.up and not i.bridge and routes.via[n] then
            list[#list + 1] = n
        end
    end
    want(pif, pi)
    want(sif, si)
    if cur then list[#list + 1] = cur.dev end
    local ok = ping_many(list, target)

    local function st(n, i)
        if not i.exists then return "missing" end
        if i.bridge then return "lan" end
        if not i.up then return "nolink" end
        if not routes.via[n] then return "idle" end
        return ok[n] and "online" or "offline"
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json({
        enabled   = g("enabled", "1") == "1",
        running   = daemon_running(),
        force     = g("force_primary", "1") == "1",
        target    = target,
        interval  = g("check_interval", "5"),
        fail      = g("fail_threshold", "2"),
        primary   = { name = pif, state = st(pif, pi) },
        secondary = { name = sif, state = st(sif, si) },
        current   = cur and cur.dev or "",
        gateway   = cur and cur.gw or "",
        online    = cur and ok[cur.dev] or false,
        hints     = hints(c),
        time      = os.date("%H:%M:%S"),
    })
end

-- modeminfo 页用, 见 index() 里的说明。原来键和值都是中英双语("在线 / Online"),
-- 页面上直接显示出来, 改成只用中文。
function get_status()
    local c = require("luci.model.uci").cursor()
    local primary_if    = c:get(CONF, SECT, "primary_if") or "eth1"
    local secondary_if  = c:get(CONF, SECT, "secondary_if") or "eth2"
    local target        = c:get(CONF, SECT, "ping_target") or "8.8.8.8"
    local force_primary = c:get(CONF, SECT, "force_primary") or "0"

    local cur = default_routes()[1]
    local current_if = cur and cur.dev or ""
    local ok = ping_many({ current_if, primary_if, secondary_if }, target)

    local status_data = {
        ["当前物理接口"]   = current_if,
        ["当前网关"]       = cur and cur.gw or "",
        ["当前是否联网"]   = ok[current_if] and "在线" or "离线",
        ["宽带接口状态"]   = ok[primary_if] and "在线" or "离线",
        ["蜂窝接口状态"]   = ok[secondary_if] and "在线" or "离线",
        ["主接口有网切换"] = (force_primary == "1") and "已启用" or "已关闭"
    }

    luci.http.prepare_content("application/json")
    luci.http.write_json(status_data)
end

-- 升级后 /tmp 里那份日志要到重启才清掉, 旧版守护进程写的这几类周期性噪声不给用户看
local LEGACY_NOISE = {
    "的网关(内核/netifd/配置都没有)",   -- 每 5 分钟一条"取不到 ethX 的网关"
    "] LED: ",                          -- 点灯动作
    "WAN 状态变化: ",                   -- 内部状态名
}

local function noisy(line)
    for _, s in ipairs(LEGACY_NOISE) do
        if line:find(s, 1, true) then return true end
    end
    return false
end

-- 日志: 只读文件末尾 16KB, 新的在前, 最多 60 条。
-- { lines = { {t="10-14 11:56:38", l="INFO", m="..."}, ... }, total = 末尾这段里的条数 }
function get_logs()
    local all = {}
    local f = io.open(LOG, "r")
    if f then
        local size = f:seek("end") or 0
        local from = size > 16384 and size - 16384 or 0
        f:seek("set", from)
        if from > 0 then f:read("*l") end   -- 截断处的半行丢掉
        for line in f:lines() do
            local d, t, lv, msg = line:match("^%d%d%d%d%-(%d%d%-%d%d) (%d%d:%d%d:%d%d) %[(%u+)%] (.*)$")
            if d and not noisy(line) then
                all[#all + 1] = { t = d .. " " .. t, l = lv, m = msg }
            end
        end
        f:close()
    end

    local lines = {}
    for i = #all, math.max(1, #all - 59), -1 do
        lines[#lines + 1] = all[i]
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json({ lines = lines, total = #all })
end
