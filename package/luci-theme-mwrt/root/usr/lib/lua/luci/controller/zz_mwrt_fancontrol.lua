module("luci.controller.zz_mwrt_fancontrol", package.seeall)
local http = require "luci.http"
local fs = require "nixio.fs"
local json = require("luci.jsonc")
uci = luci.model.uci.cursor()
local script_path="/usr/share/qmodem/"
local run_path="/tmp/run/qmodem/"
local modem_ctrl = "/usr/share/mwrt-qmodem/modem_ctrl.sh "

function index()
    if require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") ~= "/luci-static/argon" then return end
    entry({"admin", "Outofnetwork"}, post("action_Outofnetwork")).leaf = true
    entry({"admin", "fancontrol", "devpop"}, call("action_devpop")).leaf = true
    entry({"admin", "fancontrol", "memfree"}, post("action_memfree")).leaf = true
    entry({"admin", "fancontrol", "devinfo"}, call("action_devinfo")).leaf = true
    entry({"admin", "fancontrol", "darkmode_status"}, call("action_darkmode_status")).leaf = true
    entry({"admin", "fancontrol", "modem_ctrl"}, call("modemCtrl")).leaf = true
    entry({"admin", "fancontrol", "msg"}, call("action_msg")).leaf = true
    entry({"admin", "fancontrol", "msgct"}, call("action_msgct")).leaf = true
    entry({"admin", "fancontrol", "devimg_upload"}, call("action_devimg_upload")).leaf = true
    entry({"admin", "fancontrol", "devimg_reset"}, call("action_devimg_reset")).leaf = true
end


local http = require "luci.http"
local json = require "luci.jsonc"
local nixio = require "nixio"


local uci = require "luci.model.uci".cursor()

-- ============================================================================
-- 设备总览自定义整机图
--   上传:POST 到 devimg_upload/<机型>,写 /www/luci-static/router3d/custom_<机型>.png
--   恢复:POST 到 devimg_reset/<机型>,删除对应 custom_<机型>.png
-- 机型串仅取字母数字(最长16),按机型隔离,换机型互不影响。仅限 PNG、1MB 以内。
-- 写在 overlay(可写、重启保留);热路径无 AT,纯文件操作。
-- ============================================================================
local DEVIMG_DIR = "/www/luci-static/router3d"

local function devimg_model(m)
    m = tostring(m or ""):gsub("[^%w_-]", "")
    if #m == 0 or #m > 16 then return nil end
    return m
end

function action_devimg_upload(model)
    local hp = require "luci.http"
    local nx = require "nixio"
    model = devimg_model(model)
    if not model then
        hp.status(400, "Bad Request")
        hp.prepare_content("application/json")
        hp.write_json({ ok = false, err = "bad model" })
        return
    end
    local dst = DEVIMG_DIR .. "/custom_" .. model .. ".png"
    local tmp = dst .. ".upl"
    local PNG_SIG = "\137PNG\r\n\26\n"   -- PNG 魔数(前8字节)
    local fp, total, toobig, badfmt, head = nil, 0, false, false, ""

    hp.setfilehandler(function(meta, chunk, eof)
        if toobig or badfmt then return end
        if chunk then
            -- 累积前8字节校验 PNG 魔数;不是 PNG 就丢弃,不落盘
            if #head < 8 then
                head = head .. chunk
                if #head >= 8 and head:sub(1, 8) ~= PNG_SIG then
                    badfmt = true
                    if fp then fp:close(); fp = nil end
                    os.remove(tmp)
                    return
                end
            end
            total = total + #chunk
            if total > 1024 * 1024 then   -- 限 1MB
                toobig = true
                if fp then fp:close(); fp = nil end
                os.remove(tmp)
                return
            end
            if not fp then fp = io.open(tmp, "w") end
            if fp then fp:write(chunk) end
        end
        if eof and fp then fp:close(); fp = nil end
    end)

    -- 触发 multipart 解析,让上面的 filehandler 收到分片
    local _ = hp.formvalue("image")

    hp.prepare_content("application/json")
    if badfmt then
        hp.write_json({ ok = false, err = "只支持 PNG 格式，请重新上传" })
        return
    end
    if toobig then
        hp.write_json({ ok = false, err = "图片过大(限1MB),请重新上传" })
        return
    end
    local st = nx.fs.stat(tmp)
    if not st or st.size < 8 then
        os.remove(tmp)
        hp.write_json({ ok = false, err = "空文件或非法图片" })
        return
    end
    os.remove(dst)
    if not os.rename(tmp, dst) then
        os.remove(tmp)
        hp.write_json({ ok = false, err = "写入失败" })
        return
    end
    nx.fs.chmod(dst, "644")  -- 世界可读(nixio 需传字符串,传数字会被当八进制字面量)
    hp.write_json({ ok = true, url = "/luci-static/router3d/custom_" .. model .. ".png" })
end

function action_devimg_reset(model)
    local hp = require "luci.http"
    model = devimg_model(model)
    hp.prepare_content("application/json")
    if not model then
        hp.write_json({ ok = false })
        return
    end
    os.remove(DEVIMG_DIR .. "/custom_" .. model .. ".png")
    hp.write_json({ ok = true })
end

-- 读取 mwrt 配置文件中的深色模式状态
function action_darkmode_status()
    local rv = {}
    local uci = require "luci.model.uci".cursor()
    local darkval = nil

    -- 遍历 mwrt 配置里的第一个 ndis 节点
    uci:foreach("mwrt", "ndis", function(s)
        darkval = s.darkmode
        return false -- 只取第一个
    end)

    if darkval == "1" then
        rv["darkmode"] = "深色开启"
    else
        rv["darkmode"] = "深色关闭"
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


-- 判断一个 wifi-device 属于哪个频段。
-- 原先这里直接匹配 "MT7992_1_1" / "MT7992_1_2" 这两个写死的段名，于是换一块
-- 主控就全部落空：C2000MAX 的段名是 MT7993_1_1 / MT7993_1_2，两个分支都不命中，
-- 返回两个"未知"，前端据此判定"当前设备未检测到无线网卡硬件"。
-- 改成读频段本身，与芯片型号无关。
local function band_of(dev)
    if not dev then return nil end

    -- 首选 band：现行 wireless 配置就有 band='2g' / '5g' / '6g'
    local b = dev.band and dev.band:lower()
    if b == "2g" or b == "5g" or b == "6g" then return b end

    -- 老配置没有 band，只有 hwmode：11g/11ng/11axg 是 2.4G，11a/11na/11ac/11axa 是 5G
    local hw = dev.hwmode and dev.hwmode:lower()
    if hw then
        if hw:match("g$") then return "2g" end
        if hw:match("a$") or hw:match("ac$") then return "5g" end
    end

    -- 两者都没有就按信道推断（1-14 是 2.4G，36 以上是 5G）
    local ch = tonumber(dev.channel)
    if ch then
        if ch >= 1 and ch <= 14 then return "2g" end
        if ch >= 36 then return "5g" end
    end

    return nil
end

local function get_wifi_ssid_uci()
    local wifi24g = "未知"
    local wifi5g  = "未知"

    -- 先建 段名 -> 频段 的映射
    local band = {}
    uci:foreach("wireless", "wifi-device", function(s)
        band[s[".name"]] = band_of(s)
    end)

    -- 再遍历 AP，按频段归位；同频段多个 SSID 时取第一个（主 SSID）
    uci:foreach("wireless", "wifi-iface", function(s)
        if s.mode == "ap" and s.ssid and s.ssid ~= "" and s.device then
            local b = band[s.device]
            if b == "2g" and wifi24g == "未知" then
                wifi24g = s.ssid
            elseif b == "5g" and wifi5g == "未知" then
                wifi5g = s.ssid
            end
        end
    end)

    return wifi24g, wifi5g
end

-- 读一次 /proc/stat 开头的 cpu 行: "cpu" 是合计, "cpu0".."cpuN" 是各核(离线的核没有行)。
-- 每行只取前 8 列 user..steal 求和, idle 只算第 4 列(和原先合计的算法一致)。
local function read_cpu_stat()
    local t, order = {}, {}
    local f = io.open("/proc/stat", "r")
    if not f then return t, order end
    for line in f:lines() do
        local name, rest = line:match("^(cpu%d*)%s+(.*)$")
        if not name then break end
        local n, total, idle = 0, 0, 0
        for v in rest:gmatch("%d+") do
            n = n + 1
            if n > 8 then break end
            v = tonumber(v)
            total = total + v
            if n == 4 then idle = v end
        end
        t[name] = { total = total, idle = idle }
        order[#order + 1] = name
    end
    f:close()
    return t, order
end

-- 返回合计占用("12.3%")和各核明细 { {id=0, usage=12.3, mhz=2000}, ... }。
-- 各核和合计使用同一对计数；一秒内的并发页面共享结果，避免短间隔读数抖动。
local function get_cpu_usage()
    local cache = json.parse(fs.readfile("/tmp/mwrt_cpustat") or "")
    local sec, usec = nixio.gettimeofday()
    local now = sec + usec / 1000000
    if type(cache) == "table" and cache.at and cache.usage and cache.cores
        and now >= cache.at and now - cache.at < 1 then
        return cache.usage, cache.cores
    end
    local s2, order = read_cpu_stat()
    -- 用相邻请求的实际计数差，不让每次页面刷新等待一秒。
    local s1 = type(cache) == "table" and cache.stat
    if type(s1) ~= "table" or not s1.cpu or not s2.cpu or s1.cpu.total >= s2.cpu.total then
        s1 = s2
        nixio.nanosleep(0, 50000000)
        s2, order = read_cpu_stat()
    end

    local function pct(a, b)
        if not a or not b then return nil end
        local dt = b.total - a.total
        if dt <= 0 then return 0 end
        return (dt - (b.idle - a.idle)) / dt * 100
    end

    local cores = {}
    for _, name in ipairs(order) do
        local id = tonumber(name:match("^cpu(%d+)$"))
        local p = id and pct(s1[name], s2[name])
        if p then
            local c = { id = id, usage = math.floor(p * 10 + 0.5) / 10 }
            local ff = io.open("/sys/devices/system/cpu/" .. name .. "/cpufreq/scaling_cur_freq", "r")
            if ff then
                local khz = tonumber(ff:read("*l") or "")
                ff:close()
                if khz then c.mhz = math.floor(khz / 1000 + 0.5) end
            end
            cores[#cores + 1] = c
        end
    end

    local usage = string.format("%.1f%%", pct(s1.cpu, s2.cpu) or 0)
    fs.writefile("/tmp/mwrt_cpustat", json.stringify({ at = now, stat = s2, usage = usage, cores = cores }))
    return usage, cores
end


-- 首页温度圆环默认显示 WiFi 芯片温度(s54)。MTK 闭源驱动只在 iwpriv <if> stat 里给 CurrentTemperature,
-- devinfo 2 秒一轮, 每轮都 fork 一次 iwpriv 不值当: 结果连同读不到的情况一起在 /tmp 缓存 10 秒。
-- 同一颗芯片几个频段读数相同, 取第一个已 up 的 ra0/rai0/rax0 就够; 读不到返回 nil, 前端回落 CPU 温度。
local WIFI_TEMP_CACHE = "/tmp/mwrt_wifitemp"
local function wifi_temp()
    local now = os.time()
    local ts, val = (fs.readfile(WIFI_TEMP_CACHE) or ""):match("^(%d+) (%S*)")
    ts = tonumber(ts)
    if ts and now >= ts and now - ts < 10 then return tonumber(val) end
    local t
    for _, ifn in ipairs({ "ra0", "rai0", "rax0" }) do
        local fl = tonumber(((fs.readfile("/sys/class/net/" .. ifn .. "/flags") or ""):match("0x(%x+)")) or "", 16)
        if fl and fl % 2 == 1 then
            local h = io.popen("iwpriv " .. ifn .. " stat 2>/dev/null")
            local out = h and h:read("*a") or ""
            if h then h:close() end
            t = tonumber(out:match("CurrentTemperature%s*=%s*(%-?%d+)"))
            if t then break end
        end
    end
    fs.writefile(WIFI_TEMP_CACHE, now .. " " .. (t or "") .. "\n")
    return t
end

function action_devinfo()
    local rv = {}
    local nixio = require "nixio"
    local fs = require "nixio.fs"
    local json = require "luci.jsonc"
    local uci = require "luci.model.uci".cursor()

    local ubus = require("luci.util").ubus
    local sysinfo = ubus("system", "info")

    local boardinfo_path = "/etc/board.json"
    if nixio.fs.access(boardinfo_path) then
        local board_data = json.parse(fs.readfile(boardinfo_path) or "{}")
        rv["设备型号"] = type(board_data.model) == "table" and (board_data.model.name or board_data.model.id) or board_data.model or "未知"
    else
        local cpuh = io.popen("cat /proc/device-tree/model 2>/dev/null")
        local model = cpuh:read("*a") or ""
        cpuh:close()
        rv["设备型号"] = model:gsub("%s+$", "") ~= "" and model:gsub("%s+$", "") or "未知"
    end

    rv["主机名"] = uci:get("system", "@system[0]", "hostname") or ""
    if rv["主机名"] == "" then rv["主机名"] = "OpenWrt" end

    rv["固件版本"] = "未知"
    if nixio.fs.access("/etc/openwrt_release") then
        for line in io.lines("/etc/openwrt_release") do
            local desc = line:match("DISTRIB_DESCRIPTION=['\"]?(.-)['\"]?$")
            if desc and desc ~= "" then
                rv["固件版本"] = desc:gsub("^['\"]",""):gsub("['\"]$","")
                break
            end
        end
    end

    local wifi24g, wifi5g = get_wifi_ssid_uci()
    rv["2.4G无线信号名称"] = wifi24g
    rv["5G无线信号名称"] = wifi5g

    local temp_file = "/sys/class/thermal/thermal_zone0/temp"
    if nixio.fs.access(temp_file) then
        local tf = io.open(temp_file, "r")
        if tf then
            local temp_val = tonumber(tf:read("*a"))
            tf:close()
            rv["设备温度"] = temp_val and string.format("%.0f", temp_val / 1000) or "未知"
        else rv["设备温度"] = "未知" end
    else rv["设备温度"] = "未知" end
    -- 设备温度仍是 CPU(风扇卡和风扇策略都看它); 圆环优先用这个, 没有这个键就回落设备温度
    local wt = wifi_temp()
    if wt then rv["WiFi温度"] = tostring(wt) end

    rv["内核版本"] = nixio.uname().release

    local cpu_cores
    rv["CPU占用"], cpu_cores = get_cpu_usage()
    -- 空表 write_json 会出成 {}, 前端按数组用, 所以读不到就干脆不给这个键
    if #cpu_cores > 0 then rv["CPU各核"] = cpu_cores end
    local cpucount, cpumodel = 0, ""
    for line in io.lines("/proc/cpuinfo") do
        if line:match("^processor") then cpucount = cpucount + 1 end
        if cpumodel == "" then local m = line:match("^model name%s*:%s*(.+)"); if m then cpumodel = m end end
    end
    rv["CPU型号"] = cpumodel ~= "" and cpumodel or "未知"
    rv["CPU核心数"] = tostring(cpucount)

    if sysinfo and sysinfo.memory then
        local mem = sysinfo.memory
        local total, avail = mem.total or 0, mem.available or mem.free or 0
        rv["内存总量"] = string.format("%.0f MB", total / 1048576)
        rv["剩余内存百分比"] = tostring(math.floor(avail / total * 100))
        rv["内存占用"] = string.format("%.1f", (total - avail) / total * 100)
        -- 内存圆环悬停浮层要"已用 / 总量"的绝对值, 单位和下面虚拟内存一样用 KiB
        rv["物理内存"] = { total = math.floor(total / 1024), avail = math.floor(avail / 1024) }
    else rv["剩余内存百分比"] = "0"; rv["内存占用"] = "未知"; rv["内存总量"] = "未知" end

    -- 虚拟内存(swap), modeminfo 内存圆环卡的内环用。按 /proc/swaps 分两档:
    -- zram 压缩内存(先用, 快)和 SD 上的 swapfile(zram 满了才溢过去, 慢),
    -- 前端要分开显示, 所以不用 ubus system info 里那个合计的 swap。
    -- zram_mem 是压缩池实际占的物理内存(mm_stat 第 3 列, 字节)。单位一律 KiB;
    -- 没挂任何 swap 就不给这个键, 前端据此不显示内环。只读文件不 fork, 2 秒一轮也不贵。
    local swf = io.open("/proc/swaps", "r")
    if swf then
        local sw = { total = 0, used = 0, zram_total = 0, zram_used = 0, zram_mem = 0, disk_total = 0, disk_used = 0 }
        for line in swf:lines() do
            local name, size, used = line:match("^(/%S+)%s+%S+%s+(%d+)%s+(%d+)")
            if name then
                size, used = tonumber(size), tonumber(used)
                sw.total, sw.used = sw.total + size, sw.used + used
                local zdev = name:match("^/dev/(zram%d+)$")
                if zdev then
                    sw.zram_total, sw.zram_used = sw.zram_total + size, sw.zram_used + used
                    local mf = io.open("/sys/block/" .. zdev .. "/mm_stat", "r")
                    if mf then
                        local mem_used = (mf:read("*l") or ""):match("^%s*%d+%s+%d+%s+(%d+)")
                        mf:close()
                        sw.zram_mem = sw.zram_mem + math.floor((tonumber(mem_used) or 0) / 1024)
                    end
                else
                    sw.disk_total, sw.disk_used = sw.disk_total + size, sw.disk_used + used
                end
            end
        end
        swf:close()
        if sw.total > 0 then rv["虚拟内存"] = sw end
    end

    local uptime_sec = sysinfo and sysinfo.uptime or 0
    if uptime_sec > 0 then
        local d, h, m = math.floor(uptime_sec/86400), math.floor((uptime_sec%86400)/3600), math.floor((uptime_sec%3600)/60)
        if d > 0 then rv["运行时间"] = string.format("%d天 %d小时 %d分钟", d, h, m)
        elseif h > 0 then rv["运行时间"] = string.format("%d小时 %d分钟", h, m)
        else rv["运行时间"] = string.format("%d分钟", m) end
    else rv["运行时间"] = "未知" end

    if sysinfo and sysinfo.load then
        local l = sysinfo.load
        rv["系统负载"] = string.format("%.2f / %.2f / %.2f", (l[1] or 0)/65536, (l[2] or 0)/65536, (l[3] or 0)/65536)
    else rv["系统负载"] = "未知" end

    if sysinfo and sysinfo.root then
        local r = sysinfo.root
        rv["存储总量"] = string.format("%.1f GB", (r.total or 0) / 1048576)
        rv["存储已用"] = string.format("%.1f MB", (r.used or 0) / 1024)
        rv["存储使用率"] = tostring(r.total > 0 and math.floor(r.used / r.total * 100) or 0)
    else rv["存储总量"] = "未知"; rv["存储使用率"] = "未知" end

    -- Count distinct hosts on the LAN bridge, not ARP rows.
    --
    -- Counting rows overcounted three ways: a host holding several IPs was
    -- counted once per IP, the upstream gateway seen on the WAN port counted as
    -- a LAN client, and incomplete entries (flags 0x0, address never resolved)
    -- counted as well. Fields in /proc/net/arp are:
    --   IP address / HW type / Flags / HW address / Mask / Device
    -- so key on the MAC, require the complete bit (0x2) in Flags, and accept
    -- only the LAN bridge.
    local lan_dev = uci:get("network", "lan", "device") or "br-lan"

    local seen, count = {}, 0
    local arp_f = io.open("/proc/net/arp", "r")
    if arp_f then
        for line in arp_f:lines() do
            local flags, hw, iface = line:match("^%S+%s+%S+%s+(0x%x+)%s+(%S+)%s+%S+%s+(%S+)%s*$")
            if flags and hw and iface then
                local fl = tonumber(flags) or 0
                -- 0x2 = ATF_COM, the entry actually resolved to a real host.
                if iface == lan_dev and (fl % 4) >= 2
                   and hw ~= "00:00:00:00:00:00" and not seen[hw] then
                    seen[hw] = true
                    count = count + 1
                end
            end
        end
        arp_f:close()
    end
    rv["在线设备数"] = tostring(count)

    -- Find the logical interface that is ACTUALLY the upstream, instead of
    -- hardcoding "wan" (= wired eth1). A USB 5G modem's uplink is a different
    -- logical interface (e.g. modem/wwan/cellular), so the old code reported
    -- 未连接 even when the router was online through it. Strategy: take the
    -- default-route egress device, then find the logical interface whose
    -- l3_device matches it. Fall back to "wan" if there is no default route.
    local def_h = io.popen("ip route show default 2>/dev/null | awk '{print $5; exit}'")
    local def_dev = (def_h:read("*a") or ""):gsub("%s+$", "")
    def_h:close()

    local wan_ifname = "wan"
    local dump = ubus("network.interface", "dump")
    if dump and dump.interface then
        local best
        for _, iface in ipairs(dump.interface) do
            local l3 = iface.l3_device or iface.device
            -- skip loopback / lan; we want the upstream
            if iface.interface ~= "loopback" and iface.interface ~= "lan" then
                if def_dev ~= "" and l3 == def_dev and iface.up and
                    iface["ipv4-address"] and iface["ipv4-address"][1] then
                    best = iface.interface; break          -- exact egress match wins
                elseif iface.up and (iface["ipv4-address"] and iface["ipv4-address"][1]) and not best then
                    best = iface.interface                  -- else first up iface with an IP
                end
            end
        end
        if best then wan_ifname = best end
    end

    local wan_data
    for _, iface in ipairs(dump and dump.interface or {}) do
        if iface.interface == wan_ifname then wan_data = iface; break end
    end
    if not wan_data then wan_data = ubus("network.interface." .. wan_ifname, "status") end
    if wan_data then
        rv["WAN状态"] = wan_data.up and "已连接" or "未连接"
        rv["WAN协议"] = wan_data.proto or "未知"
        rv["WAN接口"] = wan_data.l3_device or wan_data.device or "未知"
        if wan_data.uptime then
            local wu = wan_data.uptime
            local wd, wh, wm = math.floor(wu/86400), math.floor((wu%86400)/3600), math.floor((wu%3600)/60)
            rv["WAN在线时长"] = wd > 0 and string.format("%d天%d时%d分", wd, wh, wm) or string.format("%d时%d分", wh, wm)
        end
        if wan_data["ipv4-address"] and wan_data["ipv4-address"][1] then
            rv["WAN_IP"] = wan_data["ipv4-address"][1]["address"] or "未知"
        end
        if wan_data["dns-server"] and wan_data["dns-server"][1] then
            rv["DNS服务器"] = table.concat(wan_data["dns-server"], ", ")
        end
    else rv["WAN状态"] = "未知" end

    local gw_h = io.popen("ip -4 route get 8.8.8.8 2>/dev/null")
    local route = gw_h:read("*a") or ""
    rv["默认网关"] = route:match(" via ([%d%.]+)") or ""
    rv["WAN_IP"] = route:match(" src ([%d%.]+)") or rv["WAN_IP"]
    gw_h:close()
    if rv["默认网关"] == "" then rv["默认网关"] = "未知" end

    rv["LAN_IP"] = uci:get("network", "lan", "ipaddr") or ""
    if rv["LAN_IP"] == "" then rv["LAN_IP"] = "192.168.88.1" end

    -- Count every active uplink, including defaults used by policy routing.
    -- IPv4/IPv6 logical interfaces sharing a device must only count once.
    local uplinks = {}
    for _, iface in ipairs(dump and dump.interface or {}) do
        local dev = iface.l3_device or iface.device
        if iface.up and dev and dev ~= "lo" and dev ~= "br-lan" and
            not fs.access("/sys/class/net/" .. dev .. "/tun_flags") then
            for _, r in ipairs(iface.route or {}) do
                if tonumber(r.mask) == 0 and (r.target == "0.0.0.0" or r.target == "::") then
                    uplinks[dev] = true
                    break
                end
            end
        end
    end
    if not next(uplinks) and def_dev ~= "" and def_dev ~= "lo" and def_dev ~= "br-lan" and
        not fs.access("/sys/class/net/" .. def_dev .. "/tun_flags") then
        uplinks[def_dev] = true
    end

    local rx, tx, counters, devices = 0, 0, {}, {}
    local nd = io.open("/proc/net/dev", "r")
    if nd then
        for line in nd:lines() do
            local nm, r, t = line:match("^%s*(%S+):%s*(%d+)%s+%d+%s+%d+%s+%d+%s+%d+%s+%d+%s+%d+%s+%d+%s+(%d+)")
            if uplinks[nm] then
                r, t = tonumber(r), tonumber(t)
                counters[nm] = { rx = r, tx = t }
                devices[#devices + 1] = nm
                rx, tx = rx + r, tx + t
            end
        end
        nd:close()
        table.sort(devices)
        rv["流量接口计数"] = counters
        rv["流量采样时间"] = tonumber((fs.readfile("/proc/uptime") or ""):match("^(%S+)"))
        local function fmt(b) b = tonumber(b); if b > 1073741824 then return string.format("%.2f GB", b/1073741824) elseif b > 1048576 then return string.format("%.1f MB", b/1048576) else return string.format("%.0f KB", b/1024) end end
        rv["WAN下行流量"] = fmt(rx)
        rv["WAN上行流量"] = fmt(tx)
        rv["WAN下行字节"] = rx
        rv["WAN上行字节"] = tx
        rv["流量来源接口"] = table.concat(devices, ", ")
    end

    local fan_pwm = "0"
    local fan_file = io.popen("cat $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1) 2>/dev/null </dev/null")
    if fan_file then fan_pwm = fan_file:read("*a"):gsub("%s+$", ""); fan_file:close() end
    local fan_value = tonumber(fan_pwm)
    if fan_value then
        rv["风扇PWM"] = tostring(fan_value)
        rv["风扇转速百分比"] = tostring(math.floor(fan_value / 255 * 100))
    end
    local fmf = io.open("/etc/fanvall", "r")
    if fmf then
        local fm = fmf:read("*a"):gsub("%s+$", ""); fmf:close()
        if fm == "0" then rv["风扇模式"] = "低速" elseif fm == "1" then rv["风扇模式"] = "中速" elseif fm == "2" then rv["风扇模式"] = "高速" elseif fm == "3" then rv["风扇模式"] = "全速" elseif fm == "9" then rv["风扇模式"] = "智能" elseif fm == "999" then rv["风扇模式"] = "无极" else rv["风扇模式"] = "手动" end
    else rv["风扇模式"] = "未知" end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


-- 首页(modeminfo)温度圆环 / "在线设备"格子的悬停浮层, 浮层开着时才 3 秒拉一次:
--   k=tmp      各温区(thermal_zone*) + WiFi 芯片温度。MTK 闭源驱动不报 hwmon/thermal,
--              只能从 iwpriv <if> stat 里抠 CurrentTemperature, 每个频段一次 fork,
--              所以不放进 2 秒一轮的 devinfo。
--   k=clients  在线设备明细, 口径和设备总览"在线设备"那格(router3d-data)一致:
--              LAN 桥上 ARP 完成态(flags 0x2)、按 MAC 去重。名字取 DHCP 租约, 其次静态租约;
--              接入方式看桥转发表里这个 MAC 落在哪个桥口(ra*=2.4G, rai*=5G, rax*=6G, 其余算有线)。
-- 列表一律在有内容时才放进 rv —— 空表 write_json 出成 {}, 前端按数组用。
local function wifi_band_of(ifn)
    if ifn:match("^rax%d+$") then return "6G" end
    if ifn:match("^rai%d+$") then return "5G" end
    if ifn:match("^ra%d+$") then return "2.4G" end
    return nil
end

local function devpop_temps(rv)
    local zones = {}
    for p in (fs.glob("/sys/class/thermal/thermal_zone*") or function() end) do
        local t = tonumber((fs.readfile(p .. "/temp") or ""):match("%-?%d+"))
        if t then
            local ty = (fs.readfile(p .. "/type") or ""):gsub("%s+$", "")
            zones[#zones + 1] = { name = (ty == "" or ty:lower():match("cpu") or ty:lower():match("soc")) and "CPU" or ty,
                                  temp = math.floor(t / 1000 + 0.5) }
        end
    end
    if #zones > 0 then rv.zones = zones end

    -- 每个频段取编号最小、已 up 的那个口(ra0/rai0/rax0); 同一颗芯片的几个频段读数相同,
    -- 合并成一行由前端处理, 这里如实给每个频段一条。
    local pick = {}
    for ifn in (fs.dir("/sys/class/net") or function() end) do
        local band = wifi_band_of(ifn)
        local fl = band and tonumber(((fs.readfile("/sys/class/net/" .. ifn .. "/flags") or ""):match("0x(%x+)")) or "", 16)
        if fl and fl % 2 == 1 then
            local n = tonumber(ifn:match("(%d+)$"))
            if not pick[band] or n < pick[band].n then pick[band] = { n = n, ifn = ifn } end
        end
    end
    local wifi = {}
    for _, band in ipairs({ "2.4G", "5G", "6G" }) do
        if pick[band] then
            local h = io.popen("iwpriv " .. pick[band].ifn .. " stat 2>/dev/null")
            local out = h and h:read("*a") or ""
            if h then h:close() end
            local t = tonumber(out:match("CurrentTemperature%s*=%s*(%-?%d+)"))
            if t then wifi[#wifi + 1] = { band = band, temp = t } end
        end
    end
    if #wifi > 0 then rv.wifi = wifi end
end

local function devpop_clients(rv)
    local uci = require "luci.model.uci".cursor()
    local lan_dev = uci:get("network", "lan", "device") or "br-lan"

    local list, by_mac = {}, {}
    local arp_f = io.open("/proc/net/arp", "r")
    if arp_f then
        for line in arp_f:lines() do
            local ip, flags, hw, iface = line:match("^(%S+)%s+%S+%s+(0x%x+)%s+(%S+)%s+%S+%s+(%S+)%s*$")
            if ip and iface == lan_dev and flags == "0x2" and hw ~= "00:00:00:00:00:00" then
                hw = hw:lower()
                local c = by_mac[hw]
                if not c then
                    c = { mac = hw, ip = ip }
                    by_mac[hw] = c
                    list[#list + 1] = c
                end
            end
        end
        arp_f:close()
    end
    rv.lan = lan_dev
    if #list == 0 then return end

    local names = {}
    local lf = io.open("/tmp/dhcp.leases", "r")
    if lf then
        for line in lf:lines() do
            local mac, host = line:match("^%S+%s+(%S+)%s+%S+%s+(%S+)")
            if mac and host and host ~= "*" then names[mac:lower()] = host end
        end
        lf:close()
    end
    uci:foreach("dhcp", "host", function(s)
        local macs = type(s.mac) == "table" and table.concat(s.mac, " ") or (s.mac or "")
        if s.name and s.name ~= "" then
            for m in macs:gmatch("%S+") do
                m = m:lower()
                if not names[m] then names[m] = s.name end
            end
        end
    end)

    -- 桥转发表 brforward 是二进制 struct __fdb_entry 数组, 每条 16 字节:
    -- mac[6] port_no is_local ageing_timer(4) port_hi pad unused(2)。直接读, 不 fork brctl。
    local ports = {}
    local brif = "/sys/class/net/" .. lan_dev .. "/brif"
    for ifn in (fs.dir(brif) or function() end) do
        local no = tonumber(((fs.readfile(brif .. "/" .. ifn .. "/port_no") or ""):match("0x(%x+)")) or "", 16)
        if no then ports[no] = ifn end
    end
    local port_of = {}
    local bf = io.open("/sys/class/net/" .. lan_dev .. "/brforward", "rb")
    if bf then
        local data = bf:read("*a") or ""
        bf:close()
        for i = 1, #data - 15, 16 do
            local b1, b2, b3, b4, b5, b6, pno, is_local = data:byte(i, i + 7)
            if is_local == 0 then
                local mac = string.format("%02x:%02x:%02x:%02x:%02x:%02x", b1, b2, b3, b4, b5, b6)
                port_of[mac] = ports[data:byte(i + 12) * 256 + pno]
            end
        end
    end

    for _, c in ipairs(list) do
        c.name = names[c.mac]
        local ifn = port_of[c.mac]
        if ifn then
            c.port = ifn
            c.band = wifi_band_of(ifn) or "wired"
        end
    end
    local function ipnum(ip)
        local a, b, c, d = ip:match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$")
        return a and ((tonumber(a) * 256 + tonumber(b)) * 256 + tonumber(c)) * 256 + tonumber(d) or 0
    end
    table.sort(list, function(x, y) return ipnum(x.ip) < ipnum(y.ip) end)
    rv.clients = list
end

-- s63 第4项:占用 CPU 的 TOP 进程(首页 CPU 负载卡悬停/点击的浮层数据)。
-- 用 busybox top 跑两轮采样(-d 1 -n 2)取第二轮的瞬时 %CPU —— 单轮 -n1 的 %CPU
-- 是进程自启动以来的累计均值,几乎全是 0,看不出"此刻谁在吃 CPU";跑两轮、读末轮
-- 才是两次采样之间的真实占用。两轮间隔 1 秒,只在浮层打开时拉(3 秒一次),不常驻。
--
-- busybox top -b 的列顺序:PID PPID USER STAT VSZ %VSZ [%CPU] COMMAND。
-- %CPU 列在多核上要开 CONFIG,这块固件实测有(前面板上 top -bn1 验证过带 %CPU)。
-- COMMAND 可能带路径/参数,取可执行名(去目录、去 {jail} 包装、截断过长参数)。
local function devpop_cputop(rv)
    local th = io.popen("top -bn2 -d 1 2>/dev/null")
    if not th then return end
    local raw = th:read("*a") or ""
    th:close()

    -- top -n2 会输出两屏,用最后一屏(第二次采样)。按出现的最后一处表头切分。
    local last = raw
    local hp = nil
    for p in raw:gmatch("()%s*PID%s+PPID") do hp = p end
    if hp then last = raw:sub(hp) end

    -- 采集 Load average 作为浮层副标题(和首页 hero-sys 的 load1 同源)
    local la = raw:match("Load average:%s*([%d%.]+%s+[%d%.]+%s+[%d%.]+)")
    if la then rv.loadavg = (la:gsub("%s+", " ")) end

    local procs = {}
    local header_seen = false
    for line in last:gmatch("[^\r\n]+") do
        if not header_seen then
            if line:match("PID%s+PPID") then header_seen = true end
        else
            -- PID PPID USER STAT VSZ %VSZ %CPU COMMAND...
            -- %VSZ / %CPU 两列都带尾随的百分号(busybox 输出 "0%  3%"),%CPU 要吃掉那个 %
            local pid, cpu, cmd = line:match("^%s*(%d+)%s+%d+%s+%S+%s+%S+%s+%d+%s+%d+%%%s+(%d+)%%%s+(.+)$")
            if pid and cpu and cmd then
                cmd = cmd:gsub("%s+$", "")
                local disp
                -- {name} 是 procd 给 ujail/服务起的包装名,[name] 是内核线程,都取括号里的
                local jail = cmd:match("^{([^}]+)}") or cmd:match("^%[([^%]]+)%]")
                if jail then
                    disp = jail
                else
                    -- 取第一个 token 的基名(去掉路径和参数)
                    local first = cmd:match("^(%S+)")
                    disp = first and first:match("([^/]+)$") or cmd
                end
                procs[#procs + 1] = { pid = tonumber(pid), cpu = tonumber(cpu), cmd = disp }
            end
        end
    end
    table.sort(procs, function(a, b) return a.cpu > b.cpu end)
    -- 只留前 8 个,且过滤掉 0% 的(全 0 时至少给前 3 个,免得空列表)
    local top = {}
    for _, p in ipairs(procs) do
        if #top >= 8 then break end
        if p.cpu > 0 or #top < 3 then top[#top + 1] = p end
    end
    rv.procs = top
end

-- s63: 存储占用磁贴的悬停浮层 —— 各挂载分区的容量/已用/占用%。
-- 用 df -k(1KiB 块, busybox 自带), 过滤掉 tmpfs/devtmpfs/proc/sysfs 这类伪文件系统
-- (它们是内存, 不是"存储"), 只留真正落盘的和 overlay。把每个挂载点算成一条。
local function devpop_storage(rv)
    local th = io.popen("df -k 2>/dev/null")
    if not th then return end
    local raw = th:read("*a") or ""
    th:close()

    -- 只认这些有意义的挂载点 + overlay/根; 伪文件系统一律跳过
    local SKIP = {
        ["tmpfs"] = true, ["devtmpfs"] = true, ["proc"] = true, ["sysfs"] = true,
        ["debugfs"] = true, ["none"] = true, ["ramfs"] = true,
    }
    local parts = {}
    local first = true
    for line in raw:gmatch("[^\r\n]+") do
        if first then first = false   -- 跳过表头 Filesystem 1K-blocks Used ...
        else
            -- Filesystem  1K-blocks  Used  Available  Use%  Mounted on
            local fs, total, used, avail, usep, mnt =
                line:match("^(%S+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%%%s+(%S.*)$")
            if fs and total and tonumber(total) > 0 then
                local fsl = fs:lower()
                -- 跳过伪文件系统, 以及 /tmp、/dev 这类内存挂载
                local m = (mnt or ""):gsub("%s+$", "")
                local skip = SKIP[fsl] or m == "/tmp" or m == "/dev"
                    or m:match("^/tmp/") or m:match("^/dev/")
                    or m:match("^/sys") or m:match("^/proc")
                if not skip then
                    parts[#parts + 1] = {
                        fs = fs, mount = m,
                        total = tonumber(total), used = tonumber(used),
                        avail = tonumber(avail), pct = tonumber(usep),
                    }
                end
            end
        end
    end
    -- 按已用容量从大到小, 大头排前面
    table.sort(parts, function(a, b) return a.used > b.used end)
    rv.parts = parts
end

-- s63: 活动连接磁贴的悬停浮层 —— 按"对端(外网)IP"聚合的 TOP 连接。
-- 数据源 /proc/net/nf_conntrack(没有就退回 /proc/net/ip_conntrack)。每条连接取它的
-- 目的地址作为"对端", 跳过本机内网网段(192.168./10./172.16-31./127.),这样列出来的
-- 就是"本机正在跟哪些外部地址通信、各占多少条", 回答用户"哪个 IP 占用 TOP"。
-- 反查不了 URL(内核只有 IP), 所以给 IP + 连接数 + 端口提示, 不臆造域名。
local function devpop_conns(rv)
    local path = "/proc/net/nf_conntrack"
    local f = io.open(path, "r")
    if not f then path = "/proc/net/ip_conntrack"; f = io.open(path, "r") end
    if not f then return end

    -- 本机/内网地址不算"对端外部流量", 过滤掉
    local function is_local(ip)
        if not ip then return true end
        if ip:match("^127%.") or ip:match("^192%.168%.") or ip:match("^10%.")
            or ip:match("^169%.254%.") or ip == "0.0.0.0" then return true end
        local n = tonumber(ip:match("^172%.(%d+)%."))
        if n and n >= 16 and n <= 31 then return true end
        -- IPv6 本地: 回环 ::1、链路本地 fe80::、唯一本地 fc00::/fd00::、未指定 ::
        if ip:find(":", 1, true) then
            local lip = ip:lower()
            if lip == "::1" or lip == "::" or lip:match("^fe80:")
                or lip:match("^fc") or lip:match("^fd") then return true end
        end
        return false
    end

    local agg = {}      -- ip -> { count, proto, ports={} }
    local total = 0
    for line in f:lines() do
        total = total + 1
        local proto = line:match("^%w+%s+%d+%s+(%w+)") or line:match("^(%w+)%s")
        -- 取第一个 dst=(通常是本机发往对端的目的地址)。两个方向都有 dst=,
        -- 原始方向的 dst 就是真正的远端; 回复方向的 dst 是本机。各取第一个即可。
        -- dst 可能是 IPv4(1.2.3.4)或 IPv6(2400:3200::1), IPv6 带冒号和十六进制,
        -- 所以匹配集要含 : 和 a-f, 不能只用 [%d%.](那会把 2400:... 截成 "2400")。
        local dst = line:match("dst=([%x%.:]+)")
        local dport = line:match("dport=(%d+)")
        if dst and not is_local(dst) then
            local e = agg[dst]
            if not e then e = { count = 0, proto = proto, ports = {} }; agg[dst] = e end
            e.count = e.count + 1
            if dport then e.ports[dport] = (e.ports[dport] or 0) + 1 end
        end
    end
    f:close()

    local list = {}
    for ip, e in pairs(agg) do
        -- 取该对端最常用的那个端口做提示
        local topport, topn = nil, -1
        for p, n in pairs(e.ports) do if n > topn then topn = n; topport = p end end
        list[#list + 1] = { ip = ip, count = e.count, proto = e.proto, port = topport }
    end
    table.sort(list, function(a, b) return a.count > b.count end)
    local top = {}
    for i = 1, math.min(5, #list) do top[i] = list[i] end
    rv.top = top
    rv.total = total
end

-- s64: 硬件加速 HNAT 卡 —— 读 turboacc 的 PPE 表 + Full Cone / FastPath 类型。
-- 数据源就是 admin/network/turboacc 页用的那三个 ubus 方法(luci.turboacc), 本地调用、
-- 不占串口不联网。PPE 的 BIND/ALL 折算占比(已绑定表项 / 总表项)。
local function devpop_hnat(rv)
    local function ubus_call(method)
        local p = require("luci.util").ubus("luci.turboacc", method)
        return type(p) == "table" and p or {}
    end
    local ppe = ubus_call("getMTKPPEStat")
    local fc  = ubus_call("getFullConeStat")
    local fp  = ubus_call("getFastPathStat")

    rv.fastpath = fp.type or ""       -- 例: "MediaTek HNAT"
    rv.fullcone = fc.type or ""       -- 例: "Boardcom Fullcone"

    local num = tonumber(ppe.PPE_NUM) or 0
    local list = {}
    local bind_sum, all_sum = 0, 0
    for i = 0, num - 1 do
        local bind = tonumber(ppe["BIND_PPE" .. i]) or 0
        local all  = tonumber(ppe["ALL_PPE" .. i]) or 0
        local pct = (all > 0) and math.floor(bind * 100 / all + 0.5) or 0
        list[#list + 1] = { n = i, bind = bind, all = all, pct = pct }
        bind_sum = bind_sum + bind
        all_sum = all_sum + all
    end
    rv.ppe = list
    rv.ppe_num = num
    rv.bind_total = bind_sum
    rv.all_total = all_sum
    rv.pct_total = (all_sum > 0) and math.floor(bind_sum * 100 / all_sum + 0.5) or 0
end

function action_devpop()
    local k = luci.http.formvalue("k")
    local rv = {}
    if k == "tmp" then devpop_temps(rv)
    elseif k == "clients" then devpop_clients(rv)
    elseif k == "cputop" then devpop_cputop(rv)
    elseif k == "storage" then devpop_storage(rv)
    elseif k == "conns" then devpop_conns(rv)
    elseif k == "hnat" then devpop_hnat(rv) end
    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end

-- s63 第3项:内存清理(首页内存圆环双击触发)。
-- 等价于"状态 - 释放内存":sync + echo 3 > drop_caches,回收 pagecache/dentry/inode。
-- 返回释放前后的内存占用百分比 + 回收的 KiB,前端播"占用→0→新占用"的清理动画。
--
-- 内存口径和 action_devinfo 完全一致(ubus system info 的 total / available),
-- 这样动画起点和圆环平时显示的值对得上,不会一点开就跳一下。
--
-- 安全:入口需登录鉴权(index 里没设 sysauth=false),匿名访问拿不到会话直接被
-- dispatcher 挡在登录页。drop_caches 本身是无损操作(只回收可重建的 pagecache/
-- dentry/inode 缓存,不丢数据),与同文件其它登录态写接口(wifi_set 等)口径一致。
local function mem_used_pct()
    local h = io.popen("ubus call system info 2>/dev/null")
    local s = h and h:read("*a") or ""
    if h then h:close() end
    local info = json.parse(s or "")
    if not (info and info.memory and info.memory.total and info.memory.total > 0) then
        return nil
    end
    local total = info.memory.total
    local avail = info.memory.available or info.memory.free or 0
    return math.floor((total - avail) / total * 100 + 0.5), total, avail
end

function action_memfree()
    luci.http.prepare_content("application/json")

    local before_pct, total, before_avail = mem_used_pct()
    luci.sys.call("sync; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null")
    local after_pct, _, after_avail = mem_used_pct()

    -- 回收量:释放后可用 - 释放前可用(KiB),负数归零(偶发统计抖动)
    local freed = 0
    if before_avail and after_avail then
        freed = math.floor((after_avail - before_avail) / 1024 + 0.5)
        if freed < 0 then freed = 0 end
    end

    luci.http.write_json({
        ok = true,
        before = before_pct or 0,   -- 释放前内存占用 %
        after = after_pct or 0,     -- 释放后内存占用 %
        freed_kib = freed,          -- 回收的 KiB
        total_kib = total and math.floor(total / 1024) or 0,
    })
end

function action_Outofnetwork()
    local code = http.formvalue("set")
    local ok = code == "88888888" or code == "666666"
    if ok then
        ok = fs.writefile("/etc/config/Outofnetwork", "ok\n") ~= nil
        if ok then
            for name in fs.glob("/tmp/luci-indexcache*") do fs.unlink(name) end
            luci.sys.call("rm -rf /tmp/luci-modulecache")
        end
    end
    http.prepare_content("application/json")
    http.write_json({ resdd = ok and "yes" or "no" })
end

function shell(command)
	local odpall = io.popen(command)
	local odp = odpall:read("*a")
	odpall:close()
	return odp
end

function get_modem_device_id()
	local file = io.open("/etc/config/qmodem", "r")
	if not file then return nil end
	for line in file:lines() do
		local id = line:match("^config%s+modem%-device%s+'([^']+)'")
		if id then
			file:close()
			return id
		end
	end
	file:close()
	return nil
end

function translate_modem_info(result)
	modem_info = result["modem_info"]
	response = {}
	for k,entry in pairs(modem_info) do
		if type(entry) == "table" then
			key = entry["key"]
			full_name = entry["full_name"]
			if full_name then
				full_name = luci.i18n.translate(full_name)
			elseif key then
				full_name = luci.i18n.translate(key)
			end
			entry["full_name"] = full_name
			if entry["class"] then
				entry["class"] = luci.i18n.translate(entry["class"])
			end
			table.insert(response, entry)
		end
	end
	return response
end

-- ===== 蜂窝信息粘滞缓存(2026-09-20) =====
-- modem_ctrl.sh info 靠 AT 口取数,ttyUSB1 被别的进程占着时整段 "Cell Information"
-- (网络模式/小区ID/RSRP...)会直接从 JSON 里消失,只剩 Base+SIM。原样透传的话,前端
-- 每隔几秒就跳一次"等待小区注册 / 未连接 / 无信号",而实际信号一直是好的。
-- 这里:1) 把最近一次取全的字段缓存到 /tmp,缺项/坏值时补回并标 stale 秒数;
--       2) INFO_CACHE_FRESH 秒内的重复请求直接吃缓存,页面+屏保+手机端不再同时抢
--          AT 口(tom_modem 少开,见 tom_modem PID 暴涨那次教训)。
local INFO_CACHE_DIR = "/tmp/run/mwrt"
local INFO_CACHE_MAX_AGE = 120   -- 缓存最多顶这么久,再旧就认账、显示真实状态
local INFO_CACHE_FRESH = 2       -- 这么新的缓存直接复用,不再跑 AT

local function info_cache_path(cfg_id)
	local safe = tostring(cfg_id):gsub("[^%w_%-]", "_")
	return INFO_CACHE_DIR .. "/modem_info_" .. safe .. ".json"
end

-- 空值和模组的报错回显都不算有效值("+CME ERROR: 10" 被当成运营商名的坑见 heroUp)
local function info_is_bad_value(v)
	if v == nil then return true end
	local s = tostring(v)
	if s == "" or s == "--" then return true end
	if s:find("CME ERROR", 1, true) or s:find("CMS ERROR", 1, true) or s:find("ERROR", 1, true) == 1 then
		return true
	end
	return false
end

-- 这次算不算"取全了": 蜂窝段的关键字段(小区/制式/信号)至少有一个有效
local function info_is_complete(list)
	local keys = {}
	for _, e in ipairs(list) do
		if type(e) == "table" and e["key"] and not info_is_bad_value(e["value"]) then
			keys[tostring(e["key"]):upper()] = true
		end
	end
	if keys["RSRP"] or keys["CELL ID"] or keys["NETWORK_MODE"] then
		return true
	end
	return false
end

local function info_read_cache(path)
	local f = io.open(path, "r")
	if not f then return nil end
	local raw = f:read("*a")
	f:close()
	if not raw or raw == "" then return nil end
	local c = json.parse(raw)
	if type(c) ~= "table" or type(c["modem_info"]) ~= "table" then return nil end
	local age = os.time() - (tonumber(c["time"]) or 0)
	if age < 0 or age > INFO_CACHE_MAX_AGE then return nil end
	c["age"] = age
	return c
end

local function info_write_cache(path, list)
	fs.mkdirr(INFO_CACHE_DIR)
	local f = io.open(path, "w")
	if not f then return end
	f:write(json.stringify({ time = os.time(), modem_info = list }))
	f:close()
end

-- 缺的字段用缓存补回,补进来的条目带 stale(秒),前端可据此决定要不要提示
local function info_merge(list, cached)
	local present, out = {}, {}
	for _, e in ipairs(list) do
		if type(e) == "table" then
			-- 坏值直接丢掉,下面让缓存里的好值顶上
			if not (e["key"] and info_is_bad_value(e["value"])) then
				if e["key"] then present[tostring(e["key"])] = true end
				table.insert(out, e)
			end
		end
	end
	for _, e in ipairs(cached["modem_info"]) do
		if type(e) == "table" and e["key"] and not present[tostring(e["key"])] then
			e["stale"] = cached["age"]
			table.insert(out, e)
		end
	end
	return out
end

local function info_from_cache(cached, mark_stale)
	local out = {}
	for _, e in ipairs(cached["modem_info"]) do
		if type(e) == "table" then
			if mark_stale then e["stale"] = cached["age"] end
			table.insert(out, e)
		end
	end
	return { result = {}, modem_info = out }
end

local function modem_info_sticky(cfg_id, path, raw)
	local parsed = json.parse(raw or "")
	if type(parsed) ~= "table" or type(parsed["modem_info"]) ~= "table" then
		-- 连 JSON 都没拿到(AT 口彻底占死 / 脚本超时): 有近期缓存就先顶上
		local cached = info_read_cache(path)
		if cached then return info_from_cache(cached, true) end
		return nil
	end
	if info_is_complete(parsed["modem_info"]) then
		info_write_cache(path, parsed["modem_info"])
		return parsed
	end
	local cached = info_read_cache(path)
	if cached then
		parsed["modem_info"] = info_merge(parsed["modem_info"], cached)
	end
	return parsed
end

function modemCtrl()
	local action = http.formvalue("action")
	if action ~= "info" or http.formvalue("params") then
		http.status(400, "Bad Request")
		return
	end
	local shared = require "myos_nradio_data"
	if shared.is_native_c5800() then
		local requested = http.formvalue("cfg")
		local m = shared.summary({ network = requested == "cpe1" and "cpe1" or "cpe" })
		local info = { result = {}, modem_info = {} }
		local function add(class, key, value)
			if value == nil or tostring(value) == "" then return end
			info.modem_info[#info.modem_info + 1] = {
				class = class, key = key, full_name = key, value = tostring(value)
			}
		end
		add("Base Information", "NAME", m.model)
		add("Base Information", "MANUFACTURER", m.manufacturer)
		add("Base Information", "REVISION", m.revision)
		add("Base Information", "NETWORK_MODE", m.mode)
		add("Base Information", "CONNECT_STATUS", m.up and "Yes" or "No")
		add("Base Information", "ISP", m.operator)
		add("Base Information", "TEMPERATURE", m.temp)
		add("SIM Information", "SIM_STATUS", m.simstatus)
		add("SIM Information", "SIM_SLOT", m.simno)
		add("SIM Information", "ICCID", m.iccid)
		add("Cell Information", "CELL ID", m.cell)
		local band = m.band or ""
		if band:match("^%d+$") then
			band = (tostring(m.mode):upper():find("NR", 1, true) and "n" or "B") .. band
		end
		add("Cell Information", "BAND", band)
		add("Cell Information", "EARFCN", m.earfcn)
		add("Cell Information", "PCI", m.pci)
		add("Cell Information", "TAC", m.tac)
		for _, key in ipairs({ "rsrp", "rsrq", "sinr", "rssi" }) do
			add("Cell Information", key:upper(), m[key])
		end
		http.prepare_content("application/json")
		http.write_json(http.formvalue("translate") == "1" and info.modem_info or info)
		return
	end
	-- local cfg_id = http.formvalue("cfg")   -- 删除前端获取
	local cfg_id = get_modem_device_id()     -- 从配置文件读取
	local params = http.formvalue("params")
	local translate = http.formvalue("translate")

	if not cfg_id then
		--luci.http.status(500, "No modem-device found")
		--luci.http.write('{"error":"No modem-device found"}')
		return
	end

	local raw, info
	if action == "info" and not params then
		local path = info_cache_path(cfg_id)
		local fresh = info_read_cache(path)
		if fresh and fresh["age"] <= INFO_CACHE_FRESH then
			info = info_from_cache(fresh, false)
		else
			raw = shell(modem_ctrl.."info "..cfg_id)
			info = modem_info_sticky(cfg_id, path, raw)
		end
	elseif params then
		raw = shell(modem_ctrl..action.." "..cfg_id.." ".."\""..params.."\"")
	else
		raw = shell(modem_ctrl..action.." "..cfg_id)
	end

	local result
	if info then
		if translate == "1" then
			result = json.stringify(translate_modem_info(info))
		else
			result = json.stringify(info)
		end
	else
		result = raw
		if translate == "1" then
			-- 解析失败时别再硬塞给 translate_modem_info(pairs(nil) 会 500),原样透传
			local parsed = json.parse(result or "")
			if type(parsed) == "table" and type(parsed["modem_info"]) == "table" then
				result = json.stringify(translate_modem_info(parsed))
			end
		end
	end

	luci.http.prepare_content("application/json")
	luci.http.write(result)
end


function action_msgct()
    local rv = {}
    local set = luci.http.formvalue("set")
    local fixed = set
    local p = luci.http.formvalue("p") or ""
    local port = string.gsub(p, "\"", "~")
    rv["at"] = fixed
    rv["port"] = port

    local uci = require "luci.model.uci".cursor()
    local rshowtime, showtime

    uci:foreach("mwrt", "ndis", function(s)
        rshowtime = s.rshowtime
        showtime = s.showtime
        return false -- 只取第一个 ndis
    end)

    if rshowtime == "1" then
        rv["showtime"] = showtime
    end

    if nixio.fs.access("/etc/config/mwrtinfo.file") then
        rv["osv"] = "常规版系统"
    else
        rv["osv"] = "纯净版系统"
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end




function action_msg()
    local rv = {}
    local p = luci.http.formvalue("p")
    local set = luci.http.formvalue("set")
    local fixed = set
    local port = string.gsub(p, "\"", "~")
    rv["at"] = fixed 
    rv["port"] = port

    local filepath = "/tmp/lucimsg.file"
    local file = io.open(filepath, "r")
    if file then
        local line1 = file:read("*l") or ""  -- 第一行
        local line2 = file:read("*l") or ""  -- 第二行
        local line3 = file:read("*l") or ""  -- 第三行
        file:close()
        rv["lucimsg"] = line1
        rv["sec"] = line2
        rv["color"] = line3
        os.remove(filepath)
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end

function action_fanswj()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end
	os.execute("echo 999 > /etc/fanvall")
    os.execute("echo "..port.." > $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1)")
	os.execute("echo "..port.." > /sys/kernel/duty_cycle")
	os.execute("echo "..port.." > /usr/bin/fanspeed.conf")
	rv["result"] = "fanswj"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fanswj2()
    local rv = {}
    local file
    local p = luci.http.formvalue("p")
    local set = luci.http.formvalue("set")
    fixed = set
    port = string.gsub(p, "\"", "~")
    rv["at"] = fixed 
    rv["port"] = port
    local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end

    os.execute("echo 999 > /etc/fanvall")
    local conf_handle = io.open("/usr/bin/fanspeed.conf", "r")
    if conf_handle then
        local conf_content = conf_handle:read("*a")
        conf_handle:close()
        rv["result"] = conf_content
    else
        rv["result"] = "无法读取 /usr/bin/fanspeed.conf"
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


function action_fanst()
    local rv = {}
    local file_path = "/usr/bin/fanspeed.conf"
    local file = io.open(file_path, "r")
    if file then
        local content = file:read("*a")
        file:close()
        rv["fanspd"] = content
    else
        rv["fanspd"] = "未获取到转速"
    end
    
    local process_check = io.popen("pgrep -f fancts.sh")
    local process_result = process_check:read("*a")
    process_check:close()

    local fanvall_file = "/etc/fanvall"
    local fanvall_check = io.open(fanvall_file, "r")
    local fanvall_content = nil
    if fanvall_check then
        fanvall_content = fanvall_check:read("*a")
        fanvall_check:close()
    end
    
    if fanvall_content and fanvall_content:match("^%s*999%s*$") then
        rv["fancts"] = "无极"
    elseif process_result ~= "" then
        rv["fancts"] = "智能"
    else
        rv["fancts"] = "手动"
    end

    local p = luci.http.formvalue("p")
    local set = luci.http.formvalue("set")
    local fixed = set
    local port = string.gsub(p, "\"", "~")
    rv["at"] = fixed 
    rv["port"] = port
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


function action_fansttp()
    local rv = {}
    local p = luci.http.formvalue("p")
    local set = luci.http.formvalue("set")
    local fixed = set
    local port = string.gsub(p, "\"", "~")

    local fansv="温度类型获取中..."
	local temperature = 0
    local conf_file = io.open("/etc/fanvallv.conf", "r")

    if conf_file then
        local config = conf_file:read("*a")
        conf_file:close()

        if config:match("模组温度") then
			fansv="模组温度"
            local sendat_command = io.popen("sendat 1 'AT^CHIPTEMP?' | grep 'CHIPTEMP' | sed -n '1p' | cut -d, -f9 | sed '/^$/d'")
            local temp_output = sendat_command:read("*a")
            sendat_command:close()
            local temp_value = tonumber(temp_output)
            if temp_value then
                temperature = temp_value / 10
            else
                temperature = "null"
            end
        else
            fansv="CPU温度"
            local file = io.open("/sys/class/thermal/thermal_zone0/temp", "r")
            if file then
                temperature = file:read("*n")
                file:close()
                temperature = temperature / 1000
            else
                temperature = "null"
            end
        end
    else
		fansv="CPU温度"
        local file = io.open("/sys/class/thermal/thermal_zone0/temp", "r")
        if file then
            temperature = file:read("*n")
            file:close()
            temperature = temperature / 1000
        else
            temperature = "null"
        end
    end

    rv["at"] = fixed 
    rv["port"] = port
    rv["fansttp"] = temperature
	rv["fansv"] = fansv

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end



function action_fanst2()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end
	os.execute("echo 2 > /etc/fanvall")
    os.execute("echo 192 > $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1)")
	os.execute("echo 192 > /sys/kernel/duty_cycle")
	os.execute("echo 192 > /usr/bin/fanspeed.conf")
	rv["result"] = "fanst2"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fanst1()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end
	os.execute("echo 1 > /etc/fanvall")
    os.execute("echo 128 > $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1)")
	os.execute("echo 128 > /sys/kernel/duty_cycle")
	os.execute("echo 128 > /usr/bin/fanspeed.conf")
	rv["result"] = "fanst2"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fanst3()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end
	os.execute("echo 3 > /etc/fanvall")
    os.execute("echo 255 > $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1)")
	os.execute("echo 255 > /sys/kernel/duty_cycle")
	os.execute("echo 255 > /usr/bin/fanspeed.conf")
	rv["result"] = "fanst3"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fanst4()
    local rv = {}
    local p = luci.http.formvalue("p")
    local set = luci.http.formvalue("set")
    local fixed = set
    local port = string.gsub(p, "\"", "~")

    rv["at"] = fixed 
    rv["port"] = port
    local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end
	os.execute("echo 9 > /etc/fanvall")
    os.execute("/usr/bin/fancts.sh &")
    rv["result"] = "fanst4"
    
    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


function action_fanstop()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local handle = io.popen("pgrep -f fancts.sh")
    local pid = handle:read("*a")
    handle:close()

    if pid then
        pid = pid:match("%d+") 
        if pid then
            os.execute("kill -9 " .. pid)
        end
    end
	os.execute("echo 0 > /etc/fanvall")
    os.execute("echo 64 > $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1)")
	os.execute("echo 64 > /sys/kernel/duty_cycle")
	os.execute("echo 64 > /usr/bin/fanspeed.conf")
	rv["result"] = "fanstop"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fansvm()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	os.execute("echo 9 > /etc/fanvall")
	os.execute("echo 模组温度 > /etc/fanvallv.conf")
	rv["result"] = "fansvm"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fansvc()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	os.execute("echo 9 > /etc/fanvall")
	os.execute("echo CPU温度 > /etc/fanvallv.conf")
	rv["result"] = "fansvc"
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end
--=====================================================================
-- 风扇定时任务(夜间静音)
--
-- 状态一律回读引擎的 status JSON,不走本模块顶部那个 uci cursor:它是跨请求
-- 单例,引擎(cron/APP)在别处 commit 过之后,这里再 get 还会拿到旧值。
--=====================================================================

local FANSCHED = "/usr/bin/mwrt-fansched"

local function sched_status()
	if not fs.access(FANSCHED) then return nil end
	local f = io.popen(FANSCHED .. " status 2>/dev/null")
	if not f then return nil end
	local out = f:read("*a")
	f:close()
	if not out or out == "" then return nil end
	return json.parse(out)
end

local function sched_reply()
	local st = sched_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json(st or { supported = false })
end

local function hhmm_ok(v)
	local h, m = tostring(v or ""):match("^(%d%d):(%d%d)$")
	if not h then return false end
	return tonumber(h) <= 23 and tonumber(m) <= 59
end

function action_sched_get()
	sched_reply()
end

function action_sched_set()
	-- 每个值都在这里先过一遍白名单再拼进命令行,引擎侧还会再校验一次。
	-- 星期用逗号传(引擎自己转空格),这样没有一个参数会含空格,不必加引号。
	local args = {}
	local function put(k, v) args[#args + 1] = k .. "=" .. v end

	local v = luci.http.formvalue("enabled")
	if v == "0" or v == "1" then put("enabled", v) end

	v = luci.http.formvalue("starttime")
	if hhmm_ok(v) then put("starttime", v) end

	v = luci.http.formvalue("endtime")
	if hhmm_ok(v) then put("endtime", v) end

	v = luci.http.formvalue("gear")
	if v == "0" or v == "1" or v == "2" or v == "3" or v == "9" or v == "999" then
		put("gear", v)
	end

	v = luci.http.formvalue("pwm")
	if v and v:match("^%d+$") and tonumber(v) <= 255 then put("pwm", v) end

	v = luci.http.formvalue("tempmax")
	if v and v:match("^%d+$") then
		local n = tonumber(v)
		if n == 0 or (n >= 50 and n <= 95) then put("tempmax", v) end
	end

	v = luci.http.formvalue("weekdays")
	if v ~= nil and (v == "" or v:match("^[0-6][0-6,]*$")) then put("weekdays", v) end

	if #args > 0 and fs.access(FANSCHED) then
		os.execute(FANSCHED .. " config " .. table.concat(args, " ") .. " >/dev/null 2>&1")
	end

	sched_reply()
end

-- 定时段内用户手动点了挡位:本段剩余时间交还给用户,但窗口结束时照样恢复
-- 到开窗前记录的模式。mode=resume 则把控制权还给定时任务。
function action_sched_bypass()
	if fs.access(FANSCHED) then
		local mode = luci.http.formvalue("mode")
		if mode == "resume" then
			os.execute(FANSCHED .. " resume >/dev/null 2>&1")
		else
			os.execute(FANSCHED .. " bypass >/dev/null 2>&1")
		end
	end
	sched_reply()
end
