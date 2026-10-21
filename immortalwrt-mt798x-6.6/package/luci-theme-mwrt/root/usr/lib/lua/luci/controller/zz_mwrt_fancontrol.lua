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

local function get_cpu_usage()
    local proc1 = io.open("/proc/stat", "r")
    local cpu_line1 = proc1:read("*l")
    proc1:close()

    local user1, nice1, system1, idle1, iowait1, irq1, softirq1, steal1 =
        cpu_line1:match("cpu%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)")

    local total1 = user1 + nice1 + system1 + idle1 + iowait1 + irq1 + softirq1 + steal1

    nixio.nanosleep(1) -- 等 1 秒再采样第二次

    local proc2 = io.open("/proc/stat", "r")
    local cpu_line2 = proc2:read("*l")
    proc2:close()

    local user2, nice2, system2, idle2, iowait2, irq2, softirq2, steal2 =
        cpu_line2:match("cpu%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)")

    local total2 = user2 + nice2 + system2 + idle2 + iowait2 + irq2 + softirq2 + steal2

    local total_diff = total2 - total1
    local idle_diff  = idle2 - idle1

    local usage = (total_diff - idle_diff) / total_diff * 100

    return string.format("%.1f%%", usage)
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
        local model = board_data.model
        rv["设备型号"] = type(model) == "table" and (model.name or model.id) or model or board_data.board_name or "未知"
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

    rv["CPU占用"] = get_cpu_usage()
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
    else rv["剩余内存百分比"] = "0"; rv["内存占用"] = "未知"; rv["内存总量"] = "未知" end

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
                if def_dev ~= "" and l3 == def_dev and iface.up and iface["ipv4-address"] and iface["ipv4-address"][1] then
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
    local fan_value = tonumber(fan_pwm) or 0
    rv["风扇PWM"] = tostring(fan_value)
    rv["风扇转速百分比"] = tostring(math.floor(fan_value / 255 * 100))
    local fmf = io.open("/etc/fanvall", "r")
    if fmf then
        local fm = fmf:read("*a"):gsub("%s+$", ""); fmf:close()
        if fm == "0" then rv["风扇模式"] = "低速" elseif fm == "1" then rv["风扇模式"] = "中速" elseif fm == "2" then rv["风扇模式"] = "高速" elseif fm == "3" then rv["风扇模式"] = "全速" elseif fm == "9" then rv["风扇模式"] = "智能" elseif fm == "999" then rv["风扇模式"] = "无极" else rv["风扇模式"] = "手动" end
    else rv["风扇模式"] = "未知" end

    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end


function action_Outofnetwork()
    local rv = {}
    local at = http.formvalue("set") or ""
    local p = http.formvalue("p") or ""
    local port = string.gsub(p, "\"", "~")
    rv["at"] = at
    rv["port"] = port

    local url = "https://openwrt-1gwhywjh138e0466-1252386204.ap-shanghai.app.tcloudbase.com/mwrt"
    local tmpfile = "/tmp/mwrt_resp.json"
    local ok = os.execute(string.format('wget -qO- "%s" > %s', url, tmpfile))

    if ok then
        local fp = io.open(tmpfile, "r")
        if fp then
            local content = fp:read("*a")
            fp:close()
            local dat = json.parse(content)
            if dat and dat.code == 0 and dat.txt then
                if at == dat.txt then
                    rv["resdd"] = "yes"
                    local conf = io.open("/etc/config/Outofnetwork", "w")
                    if conf then conf:write("ok\n") conf:close() end

                    http.prepare_content("application/json")
                    http.write_json(rv)
                    nixio.fork()
                    os.execute("(sleep 2; reboot) &")
                    --os.execute('echo "抱歉，此功能已取消使能，请联系固件作者。" > /tmp/lucimsg.file')
                    return
                else
                    rv["resdd"] = "no"
                end
            else
                rv["resdd"] = "nonet"
            end
        else
            rv["resdd"] = "nonet"
        end
        os.remove(tmpfile)
    else
        rv["resdd"] = "nonet"
    end

    http.prepare_content("application/json")
    http.write_json(rv)
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
