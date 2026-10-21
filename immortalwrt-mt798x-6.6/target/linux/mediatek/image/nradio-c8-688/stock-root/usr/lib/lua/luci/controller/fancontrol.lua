module("luci.controller.fancontrol", package.seeall)
local http = require "luci.http"
local fs = require "nixio.fs"
local json = require("luci.jsonc")
uci = luci.model.uci.cursor()
local script_path="/usr/share/qmodem/"
local run_path="/tmp/run/qmodem/"
local modem_ctrl = "/usr/share/qmodem/modem_ctrl.sh "

function index()
    entry({"admin", "Outofnetwork"}, call("action_Outofnetwork"))
    entry({"admin", "system", "mwrtst"}, cbi("mwrt"), _("Mwrt选项"), 100)
    entry({"admin", "status", "fancontrolst"}, cbi("fancontrol"), _("风扇设置"), 100)
    entry({"admin", "status", "fancontrol"}, template("Airpifanctrl/fancontrol"), _("风扇控制"), 94)
    entry({"admin", "fancontrol", "fanstop"}, call("action_fanstop"))
    entry({"admin", "fancontrol", "fanst1"}, call("action_fanst1"))
    entry({"admin", "fancontrol", "fanst2"}, call("action_fanst2"))
    entry({"admin", "fancontrol", "devinfo"}, call("action_devinfo"))
    entry({"admin", "fancontrol", "darkmode_status"}, call("action_darkmode_status")).leaf = true


    local e_fanst3 = entry({"admin", "fancontrol", "fanst3"}, call("action_fanst3"))
    e_fanst3.sysauth = false
    e_fanst3.leaf = true
    local e_fanst4 = entry({"admin", "fancontrol", "fanst4"}, call("action_fanst4"))
    e_fanst4.sysauth = false
    e_fanst4.leaf = true

    entry({"admin", "fancontrol", "fansttp"}, call("action_fansttp"))
    entry({"admin", "fancontrol", "fanst"}, call("action_fanst"))
    entry({"admin", "fancontrol", "fansvm"}, call("action_fansvm"))
    entry({"admin", "fancontrol", "fansvc"}, call("action_fansvc"))
    entry({"admin", "fancontrol", "fanswj"}, call("action_fanswj"))
    entry({"admin", "fancontrol", "fanswj2"}, call("action_fanswj2"))
    entry({"admin", "fancontrol", "modem_ctrl"}, call("modemCtrl")).leaf = true

    local e_msg = entry({"admin", "fancontrol", "msg"}, call("action_msg"))
    e_msg.sysauth = false
    e_msg.leaf = true

    local e_msgct = entry({"admin", "fancontrol", "msgct"}, call("action_msgct"))
    e_msgct.sysauth = false
    e_msgct.leaf = true
end


local http = require "luci.http"
local json = require "luci.jsonc"
local nixio = require "nixio"


local uci = require "luci.model.uci".cursor()

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


local function get_wifi_ssid_uci()
    local wifi24g = "未知"
    local wifi5g  = "未知"

    -- 遍历所有 wifi-iface
    uci:foreach("wireless", "wifi-iface", function(s)
        if s.mode == "ap" and s.ssid then
            -- 通过 device 区分 2G/5G
            if s.device and s.device:match("MT7981_1_1") then
                wifi24g = s.ssid
            elseif s.device and s.device:match("MT7981_1_2") then
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

    -------- 设备型号 --------
    local boardinfo_path = "/etc/board.json"
    if nixio.fs.access(boardinfo_path) then
        local board_data = json.parse(fs.readfile(boardinfo_path) or "{}")
        rv["设备型号"] = board_data.model or board_data.board_name or "未知"
    else
        local cpuh = io.popen("cat /proc/device-tree/model 2>/dev/null")
        local model = cpuh:read("*a") or ""
        cpuh:close()
        model = model:gsub("%s+$", "")
        rv["设备型号"] = model ~= "" and model or "未知"
    end

    -------- 固件版本 --------
    local rel_path = "/etc/openwrt_release"
    rv["固件版本"] = "未知"
    if nixio.fs.access(rel_path) then
        for line in io.lines(rel_path) do
            local desc = line:match('DISTRIB_DESCRIPTION="?\'?(.-)"?\'?$')
            if desc and desc ~= "" then
                rv["固件版本"] = desc:gsub("^['\"]",""):gsub("['\"]$","")
                break
            end
            local rel = line:match('DISTRIB_RELEASE="?\'?(.-)"?\'?$')
            if rel and rel ~= "" then
                rv["固件版本"] = rel:gsub("^['\"]",""):gsub("['\"]$","")
                break
            end
        end
    end
    -- 如果没取到，再尝试 /etc/os-release
    if rv["固件版本"] == "未知" or rv["固件版本"] == "" then
        local rel_file = io.popen("grep '^VERSION=' /etc/os-release | cut -d= -f2")
        local ver = rel_file:read("*a") or ""
        rel_file:close()
        ver = ver:gsub("^['\"]",""):gsub("['\"]$",""):gsub("%s+$","")
        if ver ~= "" then rv["固件版本"] = ver end
    end

    -------- WiFi 名称 --------
    local wifi24g, wifi5g = get_wifi_ssid_uci()
    rv["2.4G无线信号名称"] = wifi24g
    rv["5G无线信号名称"] = wifi5g

    -------- 设备温度 --------
    local temp_file = "/sys/class/thermal/thermal_zone0/temp"
    if nixio.fs.access(temp_file) then
        local tf = io.open(temp_file, "r")
        if tf then
            local temp_val = tonumber(tf:read("*a"))
            tf:close()
            rv["设备温度"] = temp_val and string.format("%.0f℃", temp_val / 1000) or "未知"
        else
            rv["设备温度"] = "未知"
        end
    else
        rv["设备温度"] = "未知"
    end

    -------- 内核版本 --------
    local kfile = io.popen("uname -r 2>/dev/null")
    local kver = kfile:read("*a") or ""
    kfile:close()
    rv["内核版本"] = kver:gsub("%s+$", "")
    if rv["内核版本"] == "" then rv["内核版本"] = "未知" end

    -------- CPU占用 --------
    local cpuinfo = io.popen("top -bn1 | grep -i 'CPU' | head -n 1")
    local cpu_line = cpuinfo:read("*a") or ""
    cpuinfo:close()
    local idle_val = cpu_line:match("([%d%.]+)%s*%%?%s*id")
    if idle_val then
        local usage = 100 - tonumber(idle_val)
        --rv["CPU占用"] = string.format("%.1f%%", usage)
        rv["CPU占用"] = get_cpu_usage()
    else
        rv["CPU占用"] = "未知"
    end

    -------- 内存信息 --------
    local meminfo = io.popen("free | grep Mem")
    local mem_line = meminfo:read("*a") or ""
    meminfo:close()
    local total, used, free = mem_line:match("Mem:%s+(%d+)%s+(%d+)%s+(%d+)")
    if total and free then
        rv["剩余内存百分比"] = tostring(math.floor((free / total) * 100))
        rv["内存占用"] = string.format("%.1f", (used / total) * 100)
    else
        rv["剩余内存百分比"] = "0"
        rv["内存占用"] = "未知"
    end

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

function modemCtrl()
	local action = http.formvalue("action")
	-- local cfg_id = http.formvalue("cfg")   -- 删除前端获取
	local cfg_id = get_modem_device_id()     -- 从配置文件读取
	local params = http.formvalue("params")
	local translate = http.formvalue("translate")
	
	if not cfg_id then
		--luci.http.status(500, "No modem-device found")
		--luci.http.write('{"error":"No modem-device found"}')
		return
	end

	local result
	if params then
		result = shell(modem_ctrl..action.." "..cfg_id.." ".."\""..params.."\"")
	else 
		result = shell(modem_ctrl..action.." "..cfg_id)
	end

	if translate == "1" then
		modem_more_info = json.parse(result)
		modem_more_info = translate_modem_info(modem_more_info)
		result = json.stringify(modem_more_info)
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
    os.execute("echo "..port.." > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1")
    os.execute("echo "..port.." > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1")
    os.execute("echo "..port.." > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1")
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
    os.execute("echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1")
    os.execute("echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1")
    os.execute("echo 192 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1")
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
    os.execute("echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1")
    os.execute("echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1")
    os.execute("echo 128 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1")
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
    os.execute("echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1")
    os.execute("echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1")
    os.execute("echo 255 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1")
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
    os.execute("echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1")
    os.execute("echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon1/pwm1")
    os.execute("echo 64 > /sys/devices/platform/pwm-fan/hwmon/hwmon2/pwm1")
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