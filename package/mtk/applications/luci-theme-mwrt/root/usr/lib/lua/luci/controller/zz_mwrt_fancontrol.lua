module("luci.controller.zz_mwrt_fancontrol", package.seeall)
local http = require "luci.http"
local fs = require "nixio.fs"
local json = require("luci.jsonc")
uci = luci.model.uci.cursor()

function index()
    entry({"admin", "status", "mwrt_fan"}, template("Airpifanctrl/fancontrol"), _("MWRT 风扇"), 91).dependent = false
    entry({"admin", "Outofnetwork"}, call("action_Outofnetwork")).leaf = true
    for _, action in ipairs({"fanstop", "fanst1", "fanst2", "fanst3", "fanst4", "fanswj", "fanswj2", "fanst", "fansttp", "fansvm", "fansvc", "sched_get", "sched_set", "sched_bypass"}) do
        entry({"admin", "fancontrol", action}, call("action_" .. action)).leaf = true
    end
    entry({"admin", "fancontrol", "devinfo"}, call("action_devinfo")).leaf = true
    for _, action in ipairs({"devpop", "memfree", "hqos_get", "hqos_set"}) do
        entry({"admin", "fancontrol", action}, call("action_" .. action)).leaf = true
    end
    if require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") ~= "/luci-static/argon" then return end
    local home = entry({"admin", "modeminfo"}, template("zmode/modem_info"), _("系统首页"), 2)
    home.dependent = false
    local data = entry({"admin", "modeminfo", "router3d"}, call("action_router3d"))
    data.leaf = true
    data.sysauth = false
    entry({"admin", "fancontrol", "darkmode_status"}, call("action_darkmode_status")).leaf = true
    entry({"admin", "fancontrol", "msg"}, call("action_msg")).leaf = true
    entry({"admin", "fancontrol", "msgct"}, call("action_msgct")).leaf = true
    entry({"admin", "fancontrol", "devimg_upload"}, call("action_devimg_upload")).leaf = true
    entry({"admin", "fancontrol", "devimg_reset"}, call("action_devimg_reset")).leaf = true
end

function action_router3d()
    local out = luci.sys.exec("/www/cgi-bin/router3d-data") or ""
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control", "no-store")
    luci.http.write(out:match("\r\n\r\n(.*)") or out:match("\n\n(.*)") or "{}")
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
    m = tostring(m or ""):gsub("[^%w]", "")
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
-- 各核和合计是同一对采样算出来的, 不多睡也不多读; 首页 CPU 圆环的悬停浮层用各核。
local function get_cpu_usage()
    local s1 = read_cpu_stat()
    nixio.nanosleep(1) -- 等 1 秒再采样第二次
    local s2, order = read_cpu_stat()

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

    return string.format("%.1f%%", pct(s1.cpu, s2.cpu) or 0), cores
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

    local sys_h = io.popen("ubus call system info 2>/dev/null")
    local sys_json = sys_h:read("*a") or ""
    sys_h:close()
    local sysinfo = json.parse(sys_json)

    local boardinfo_path = "/etc/board.json"
    if nixio.fs.access(boardinfo_path) then
        local board_data = json.parse(fs.readfile(boardinfo_path) or "{}")
        rv["设备型号"] = board_data.model or board_data.board_name or "未知"
    else
        local cpuh = io.popen("cat /proc/device-tree/model 2>/dev/null")
        local model = cpuh:read("*a") or ""
        cpuh:close()
        rv["设备型号"] = model:gsub("%s+$", "") ~= "" and model:gsub("%s+$", "") or "未知"
    end

    local hn = io.popen("uci get system.@system[0].hostname 2>/dev/null")
    rv["主机名"] = (hn:read("*a") or ""):gsub("%s+$", "")
    hn:close()
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

    local kfile = io.popen("uname -r 2>/dev/null")
    rv["内核版本"] = (kfile:read("*a") or ""):gsub("%s+$", "")
    kfile:close()

    local cores
    rv["CPU占用"], cores = get_cpu_usage()
    if #cores > 0 then rv["CPU各核"] = cores end
    local wt = wifi_temp()
    if wt then rv["WiFi温度"] = tostring(wt) end
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
        rv["物理内存"] = { total = math.floor(total / 1024), avail = math.floor(avail / 1024) }
    else rv["剩余内存百分比"] = "0"; rv["内存占用"] = "未知"; rv["内存总量"] = "未知" end

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
    local dump_h = io.popen("ubus call network.interface dump 2>/dev/null")
    local dump = json.parse(dump_h:read("*a") or "")
    dump_h:close()
    if dump and dump.interface then
        local best
        for _, iface in ipairs(dump.interface) do
            local l3 = iface.l3_device or iface.device
            -- skip loopback / lan; we want the upstream
            if iface.interface ~= "loopback" and iface.interface ~= "lan" then
                if def_dev ~= "" and l3 == def_dev then
                    best = iface.interface; break          -- exact egress match wins
                elseif iface.up and (iface["ipv4-address"] and iface["ipv4-address"][1]) and not best then
                    best = iface.interface                  -- else first up iface with an IP
                end
            end
        end
        if best then wan_ifname = best end
    end

    local wan_h = io.popen("ubus call network.interface." .. wan_ifname .. " status 2>/dev/null")
    local wan_data = json.parse(wan_h:read("*a") or "")
    wan_h:close()
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

    local gw_h = io.popen("ip route show default 2>/dev/null | awk '{print $3}'")
    rv["默认网关"] = (gw_h:read("*a") or ""):gsub("%s+$", "")
    gw_h:close()
    if rv["默认网关"] == "" then rv["默认网关"] = "未知" end

    local lan_h = io.popen("uci get network.lan.ipaddr 2>/dev/null")
    rv["LAN_IP"] = (lan_h:read("*a") or ""):gsub("%s+$", "")
    lan_h:close()
    if rv["LAN_IP"] == "" then rv["LAN_IP"] = "192.168.88.1" end

    -- Pick the real upstream egress device from the default route. This auto-
    -- adapts to a wired WAN (eth1) OR a 5G/cellular uplink (wwan0/rmnet/usb0/…),
    -- whose logical interface is NOT named "wan" — so the old hardcoded "wan"
    -- lookup never read it and the traffic chart stayed flat. Fall back to the
    -- ubus WAN device, then to br-lan (LAN side) if there is no default route.
    local egress_h = io.popen("ip route show default 2>/dev/null | awk '{print $5; exit}'")
    local egress_dev = (egress_h:read("*a") or ""):gsub("%s+$", "")
    egress_h:close()

    local wan_dev, lan_fallback
    if egress_dev ~= "" then
        wan_dev = egress_dev
    elseif rv["WAN接口"] and rv["WAN接口"] ~= "未知" then
        wan_dev = rv["WAN接口"]
    else
        wan_dev = "br-lan"; lan_fallback = true
    end

    -- Read that device's counters from /proc/net/dev with an EXACT name match
    -- (the old `grep eth1` would also match eth10, br-eth1, etc.).
    local rx, tx
    local nd = io.open("/proc/net/dev", "r")
    if nd then
        for line in nd:lines() do
            local nm, r, t = line:match("^%s*(%S+):%s*(%d+)%s+%d+%s+%d+%s+%d+%s+%d+%s+%d+%s+%d+%s+%d+%s+(%d+)")
            if nm == wan_dev then rx, tx = r, t; break end
        end
        nd:close()
    end

    -- br-lan counters are mirrored vs. an upstream device: its rx is what the
    -- clients SENT (upload) and its tx is what was delivered to them (download).
    -- Swap on the LAN-fallback path so 下行/上行 stay correct.
    if lan_fallback and rx and tx then rx, tx = tx, rx end

    if rx and tx then
        local function fmt(b) b = tonumber(b); if b > 1073741824 then return string.format("%.2f GB", b/1073741824) elseif b > 1048576 then return string.format("%.1f MB", b/1048576) else return string.format("%.0f KB", b/1024) end end
        rv["WAN下行流量"] = fmt(rx)
        rv["WAN上行流量"] = fmt(tx)
        rv["WAN下行字节"] = rx
        rv["WAN上行字节"] = tx
        rv["流量来源接口"] = wan_dev
    end

    local fan_pwm = "0"
    local fan_file = io.popen("cat $(ls /sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1 2>/dev/null|head -1) 2>/dev/null </dev/null")
    if fan_file then fan_pwm = fan_file:read("*a"):gsub("%s+$", ""); fan_file:close() end
    rv["风扇PWM"] = fan_pwm
    rv["风扇转速百分比"] = tostring(math.floor((tonumber(fan_pwm) or 0) / 255 * 100))
    local fmf = io.open("/etc/fanvall", "r")
    if fmf then
        local fm = fmf:read("*a"):gsub("%s+$", ""); fmf:close()
        if fm == "0" then rv["风扇模式"] = "低速" elseif fm == "1" then rv["风扇模式"] = "中速" elseif fm == "2" then rv["风扇模式"] = "高速" elseif fm == "3" then rv["风扇模式"] = "全速" elseif fm == "9" then rv["风扇模式"] = "智能" elseif fm == "999" then rv["风扇模式"] = "无极" else rv["风扇模式"] = "手动" end
    else rv["风扇模式"] = "未知" end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


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
            local compact = lip:gsub(":", "")
            if compact:match("^0*$") or compact:match("^0*1$") then return true end
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
        local out = luci.sys.exec("ubus call luci.turboacc " .. method .. " 2>/dev/null") or ""
        local p = json.parse(out)
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
    if not require("luci.dispatcher").test_post_security() then return end
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

-- s64: 智能 HQoS(硬件 QoS)—— 首页 HNAT 卡详情里的开关。
-- 底层就是 luci-app-eqos-mtk: enabled=1 + smarthqos=1 时, /etc/init.d/eqos 把上下行速率
-- 直接写进 MT7987 的 QDMA 硬件队列(qdma_txq1~31 上行 / 33~63 下行), CPU 零负担。
-- 这里只做"智能整形"这一档(不碰 eqos 的按-IP 限速 device 列表)。
-- 判据:读 eqos uci + 探一个下行队列(txq33)的 EN 位确认硬件是否真的在整形。
local function hqos_active()
    -- qdma_txq33 里 "max   <EN>  <RATE> ..." 那行的 EN=1 即硬件整形生效
    local t = fs.readfile("/sys/kernel/debug/hnat/qdma_txq33") or ""
    local en = t:match("max%s+(%d+)")
    return en == "1"
end

function action_hqos_get()
    http.prepare_content("application/json")
    local enabled = (uci:get("eqos", "config", "enabled") == "1")
    local smart   = (uci:get("eqos", "config", "smarthqos") == "1")
    http.write_json({
        enabled  = enabled and smart,          -- 对首页而言"开"= 启用且智能档
        download = tonumber(uci:get("eqos", "config", "download")) or 0,
        upload   = tonumber(uci:get("eqos", "config", "upload")) or 0,
        active   = hqos_active(),              -- 硬件队列确有整形(真生效)
    })
end

function action_hqos_set()
    http.prepare_content("application/json")
    local ctx = luci.dispatcher.context
    if http.getenv("REQUEST_METHOD") ~= "POST" or
       not ctx or not ctx.authtoken or http.formvalue("token") ~= ctx.authtoken then
        http.status(403, "Forbidden")
        return http.write_json({ ok = false, error = "会话已过期" })
    end

    local enable = (http.formvalue("enable") == "1")
    if enable then
        local dl = tonumber(http.formvalue("download")) or 0
        local ul = tonumber(http.formvalue("upload")) or 0
        -- 合理区间:1~10000 Mbps(万兆上限足够, 0 或负数不接受 —— 智能档要真实带宽才整形得准)
        if dl < 1 or dl > 10000 or ul < 1 or ul > 10000 then
            return http.write_json({ ok = false, error = "请填写 1 - 10000 之间的带宽(Mbps)" })
        end
        uci:set("eqos", "config", "enabled", "1")
        uci:set("eqos", "config", "smarthqos", "1")
        uci:set("eqos", "config", "download", tostring(math.floor(dl)))
        uci:set("eqos", "config", "upload", tostring(math.floor(ul)))
    else
        uci:set("eqos", "config", "enabled", "0")
        uci:set("eqos", "config", "smarthqos", "0")
    end
    uci:commit("eqos")
    -- 后台重启 eqos(写硬件队列 + 分类打标 + 重启 dnsmasq), 不阻塞本响应。
    os.execute("( /etc/init.d/eqos restart ) >/dev/null 2>&1 &")

    http.write_json({ ok = true, enabled = enable })
end


-- 插件认证(后台连续键入 mwrtmwrt 弹出的那个框, 前端是全局注入的 mwrt-unlock.js)。
--
-- 口令交给更新服务器核验(POST /api/v1/activate), 设备本地不存、不比对。
-- 成功后写 /etc/config/Outofnetwork="ok" 并清 LuCI 菜单缓存 —— openclash/passwall
-- 的 index() 按这个文件的内容决定挂不挂菜单, 缓存一清, 前端刷新页面菜单就出来,
-- 不再需要重启。
--
-- 安全:
--  * 只收 POST, 且必须带当前会话的 token(防 CSRF)。
--  * 口令经 0600 临时文件交给 curl(--data-urlencode code@file), 不拼进命令行。
--  * 只走 https + 内置 CA, 不用 -k。
--  * 应答里不回显口令。
function action_Outofnetwork()
    local function reply(value)
        http.prepare_content("application/json")
        http.write_json({resdd=value})
    end
    if not require("luci.dispatcher").test_post_security() then return end
    if not require("luci.model.mwrt_gate").theme() then
        http.status(403, "Forbidden")
        return reply("denied")
    end
    if http.formvalue("set") ~= "666666" then return reply("no") end
    local config = require("luci.model.uci").cursor()
    config:set("mwrt", "access", "access")
    config:set("mwrt", "access", "unlocked", "1")
    if not config:commit("mwrt") then return reply("failed") end
    os.execute("rm -f /tmp/luci-indexcache* 2>/dev/null; rm -rf /tmp/luci-modulecache 2>/dev/null")
    return reply("yes")
end

function shell(command)
	local odpall = io.popen(command)
	local odp = odpall:read("*a")
	odpall:close()
	return odp
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
	local handle = io.popen("pidof fancontrol")
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
    local handle = io.popen("pidof fancontrol")
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
    local pwm
    for path in fs.glob("/sys/devices/platform/pwm-fan/hwmon/hwmon*/pwm1") do
        pwm = tonumber(fs.readfile(path))
        if pwm then break end
    end
    local mode = (fs.readfile("/etc/fanvall") or ""):match("%d+")
    local smart = luci.sys.call("pidof fancontrol >/dev/null 2>&1") == 0
    http.prepare_content("application/json")
    http.write_json({fanspd=pwm or 0, fancts=smart and "智能" or (mode == "999" and "无极" or "手动")})
end

function action_fansttp()
    local value = tonumber(fs.readfile("/sys/class/thermal/thermal_zone0/temp"))
    http.prepare_content("application/json")
    http.write_json({fansttp=value and value / 1000 or "null", fansv="CPU温度"})
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
	local handle = io.popen("pidof fancontrol")
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
	local handle = io.popen("pidof fancontrol")
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
	local handle = io.popen("pidof fancontrol")
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
    local handle = io.popen("pidof fancontrol")
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
	local handle = io.popen("pidof fancontrol")
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
    if not require("luci.dispatcher").test_post_security() then return end
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
    if not require("luci.dispatcher").test_post_security() then return end
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

-- C2000MAX manual controls share the scheduler and native procd fan driver.
local function max_fan_set(gear)
    if not require("luci.dispatcher").test_post_security() then return end
    local pwm = tonumber(http.formvalue("p")) or 64
    if pwm < 0 or pwm > 255 or pwm ~= math.floor(pwm) then
        http.status(400, "Invalid PWM"); return
    end
    local out = luci.sys.exec("/usr/bin/mwrt-fansched manual " .. gear .. " " .. pwm .. " 2>/dev/null")
    http.prepare_content("application/json")
    http.write(out ~= "" and out or '{"supported":false}')
end
function action_fanstop() max_fan_set(0) end
function action_fanst1() max_fan_set(1) end
function action_fanst2() max_fan_set(2) end
function action_fanst3() max_fan_set(3) end
function action_fanst4() max_fan_set(9) end
function action_fanswj() max_fan_set(999) end
function action_fansvm()
    http.prepare_content("application/json")
    http.write_json({error="MAX 智能调速使用 CPU 温度，请选择 CPU 温度"})
end
function action_fansvc() max_fan_set(9) end

function action_fanswj2()
    http.prepare_content("application/json")
    http.write_json({result=fs.readfile("/usr/bin/fanspeed.conf") or "128"})
end
