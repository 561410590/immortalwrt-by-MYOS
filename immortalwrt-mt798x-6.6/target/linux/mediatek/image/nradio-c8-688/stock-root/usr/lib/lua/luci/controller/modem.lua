module("luci.controller.modem", package.seeall)

function index()
	entry({"admin", "modem"}, firstchild(), _("内置蜂窝"), 25).dependent=false
	entry({"admin", "modeminfo"}, firstchild(), _("设备总览"), 2).dependent = false
    entry({"admin", "modeminfo", "cellular_data"}, template("zmode/modem_info"), _("系统数据"), 1)
    entry({"admin", "modeminfo", "device_data"}, template("zmode/modem_info2"), _("设备数据"), 2)
	if nixio.fs.access("/etc/config/mwrtinfo.file") then
		--entry({"admin", "modeminfo", "routerdog"}, template("routerdog/index"), _("路由桌面"), 3)
	end
	local file = io.open("/tmp/modconf.conf", "r")
	local template_name = "zmode/net_status"
	if file then
		local content = file:read("*all")
		file:close()
		if content and string.find(content, "RM520") then
			template_name = "zmode/net_status_RM520"
		elseif content and string.find(content, "RM500U") then
			template_name = "zmode/net_status_RM500U"
		elseif content and string.find(content, "MT5700") then
			template_name = "zmode/net_status_MT5700M"	
		end
	end
	--entry({"admin", "status", "fancontrol"}, template("zmode/fancontrol"), _("风扇控制"), 94)
	entry({"admin", "status", "mwrtstyle"}, template("zmode/mwrtstyle2"), _("界面修改"), 93)
	entry({"admin", "modem", "smsc"}, template("zmode/smsc"), _("短信管理"), 95)
	entry({"admin", "modem", "webui"}, template("zmode/webui"), _("WEB界面"), 92)
	entry({"admin", "modem", "nets"}, template(template_name), _("模组状态"), 96)
	entry({"admin", "modem", "at"}, template("zmode/at"), _("串口调试"), 97)
	--entry({"admin", "modem", "at2"}, template("zmode/at2"), _("网络调试"), 98)
	entry({"admin", "modem", "modem"}, cbi("modem"), _("模组设定"), 100) 
	entry({"admin", "modem", "get_csq"}, call("action_get_csq"))
	entry({"admin", "modem", "get_if"}, call("action_get_if"))
	entry({"admin", "modem", "send_atcmd"}, call("action_send_atcmd"))
	entry({"admin", "modem", "fanstop"}, call("action_fanstop"))
	entry({"admin", "modem", "fanst1"}, call("action_fanst1"))
	entry({"admin", "modem", "fanst2"}, call("action_fanst2"))
	entry({"admin", "modem", "fanst3"}, call("action_fanst3"))
	entry({"admin", "modem", "fanst4"}, call("action_fanst4"))
	entry({"admin", "modem", "fansttp"}, call("action_fansttp"))
	entry({"admin", "modem", "fansvm"}, call("action_fansvm"))
	entry({"admin", "modem", "fansvc"}, call("action_fansvc"))
	entry({"admin", "modem", "fanst"}, call("action_fanst"))
	entry({"admin", "modem", "smscs"}, call("action_smscs"))
    entry({"admin", "modem", "mwrtdf"}, call("action_mwrtdf"))
    entry({"admin", "modem", "mwrtco"}, call("action_mwrtco"))
    entry({"admin", "modem", "mwrtpk"}, call("action_mwrtpk"))
	entry({"admin", "modem", "mwrtgr"}, call("action_mwrtgr"))
	entry({"admin", "modem", "mwrtor"}, call("action_mwrtor"))
	entry({"admin", "modem", "mwrtinit"}, call("action_mwrtinit"), nil).leaf = true
	entry({"admin", "modem", "mwrtinit"}, call("action_mwrtinit"), nil).sysauth = false
end

function action_mwrtinit()
	local rv ={}
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	local fixed = "txt"
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port
	local file_exist = nixio.fs.access("/usr/bin/modemflag.conf")
    if file_exist then
        rv["rv"] = "OK"
    else
        rv["rv"] = "NO"
    end
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_mwrtco()
	local source_file = "/usr/share/modem/cascade.cssdf"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command4 = string.format("cp %s %s", source_file, target_file)
	--os.execute(command4)
	local p = luci.http.formvalue("p")
	port= string.gsub(p, "\"", "~")
	local S = port
	local command = string.format("sed -i 's/--primary:[^;]*/--primary:%s;/' /www/luci-static/argon/css/cascade.css", S)
	os.execute(command)
	local command2 = string.format("sed -i 's/--bar-bg:[^;]*/--bar-bg:%s;/' /www/luci-static/argon/css/cascade.css", S)
	os.execute(command2)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtdf()
	local source_file = "/usr/share/modem/cascade.cssdf"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtpk()
	local source_file = "/usr/share/modem/cascade.csspk"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtgr()
	local source_file = "/usr/share/modem/cascade.cssgr"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtor()
	local source_file = "/usr/share/modem/cascade.cssor"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_smscs()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port

	os.execute("/usr/share/modem/smsc.sh \'" .. port .. "\' \'" .. fixed .. "\'")
	smsc = "/tmp/smsc.at"
	file = io.open(smsc, "r")
	if file ~= nil then
		rv["smsc"] = file:read("*all")
		file:close()
	else
		rv["smsc"] = " "
	end
	os.execute("/usr/share/modem/rsmsc.sh")
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_get_if()
    local fs = require "nixio.fs"
    local uci = require "luci.model.uci".cursor()
    local http = require "luci.http"
    local rv = {}

    -- 获取无线 SSID
    local ssid_24g, ssid_5g

    uci:foreach("wireless", "wifi-iface", function(s)
        if s.device == "MT7981_1_1" and not ssid_24g then
            ssid_24g = s.ssid
        elseif s.device == "MT7981_1_2" and not ssid_5g then
            ssid_5g = s.ssid
        end
    end)

    rv["2.4G"] = ssid_24g or "未知"
    rv["5G"] = ssid_5g or "未知"

    -- CPU温度
    local cpu_temp_path = "/sys/class/thermal/thermal_zone0/temp"
    local temp = fs.readfile(cpu_temp_path)
    if temp then
        temp = tonumber(temp) / 1000
        rv["tmp"] = string.format("%.1f", temp)
    else
        rv["tmp"] = "未知"
    end

    -- 风扇转速
    local fanspeed = fs.readfile("/usr/bin/fanspeed.conf")
    rv["fansp"] = fanspeed and fanspeed:gsub("\n", "") or "未知"

    -- 运行时间
    local uptime = fs.readfile("/proc/uptime")
    if uptime then
        local sec = tonumber(uptime:match("^(%d+%.?%d*)"))
        if sec then
            local h = math.floor(sec / 3600)
            local m = math.floor((sec % 3600) / 60)
            local s = math.floor(sec % 60)
            rv["uptime"] = string.format("%02d:%02d:%02d", h, m, s)
        else
            rv["uptime"] = "未知"
        end
    else
        rv["uptime"] = "未知"
    end

    http.prepare_content("application/json")
    http.write_json(rv)
end

function action_send_atcmd()
	local rv ={}
	local file
	local p = luci.http.formvalue("p")
	local set = luci.http.formvalue("set")
	--fixed = string.gsub(set, "\"", "~")
	fixed = set
	port= string.gsub(p, "\"", "~")
	rv["at"] = fixed 
	rv["port"] = port

	os.execute("/usr/share/modem/atcmd.sh \'" .. port .. "\' \'" .. fixed .. "\'")
	result = "/tmp/result.at"
	file = io.open(result, "r")
	if file ~= nil then
		rv["result"] = file:read("*all")
		file:close()
	else
		rv["result"] = " "
	end
	os.execute("/usr/share/modem/delatcmd.sh")
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function action_fanst()
    local rv = {}
    local file_path = "/sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm1"
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
    
    if process_result ~= "" then
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
			fansv="MT5700M-CN模组温度"
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
	os.execute("echo 1 > /etc/fanvall")
	os.execute("echo 2 > /sys/class/thermal/cooling_device0/cur_state")
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
	os.execute("echo 1 > /sys/class/thermal/cooling_device0/cur_state")
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
	os.execute("echo 3 > /sys/class/thermal/cooling_device0/cur_state")
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
	os.execute("echo 0 > /sys/class/thermal/cooling_device0/cur_state")
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

function action_get_csq()
	local file = io.open("/tmp/modconf.conf", "r")
	if file then
		local content = file:read("*all")
		file:close()
		if content and string.find(content, "RM520") then
			rm520_rm500U()			
		elseif content and string.find(content, "RM500U") then
			rm520_rm500U()	
		elseif content and string.find(content, "MT5700") then	
			MT5700()
		end
	end
end

function rm520_rm500U()
	os.execute("/usr/share/modem/zinfo.sh")
	local file
	stat = "/tmp/cpe_cell.file"
	file = io.open(stat, "r")
	local rv ={}
	rv["stsss"] = file:read("*line")
	rv["modem"] = file:read("*line")
	rv["conntype"] = file:read("*line")
	rv["firmware"] = file:read("*line")
	rv["temper"] = file:read("*line")
	rv["date"] = file:read("*line")
	--------------------------------
	rv["simsel"] = file:read("*line")
	rv["cops"] = file:read("*line")
	rv["imei"] = file:read("*line")
	rv["imsi"] = file:read("*line")
	rv["iccid"] = file:read("*line")
	rv["phone"] = file:read("*line")
	--------------------------------
	rv["mode"] = file:read("*line")
	rv["per"] = file:read("*line")
	rv["rssi"] = file:read("*line")
	rv["rsrq"] = file:read("*line")
	rv["rscp"] = file:read("*line")
	rv["sinr"] = file:read("*line")
	-------------------------------
	rv["mcc"] = file:read("*line")
	rv["lac"] = file:read("*line")
	rv["cid"] = file:read("*line")
	rv["band"] = file:read("*line")
	rv["rfcn"] = file:read("*line")
	rv["pci"] = file:read("*line")
	rv["apn"] = file:read("*line")
	rv["down"] = file:read("*line")
	rv["up"] = file:read("*line")
	--------------------------------
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

function MT5700()
	os.execute("/usr/share/modem/zinfo_mt5700.sh")
    local file, file2
    local stat = "/tmp/cpe_cell.file"
    local stat2 = "/tmp/stsss_u.file"
    file = io.open(stat, "r")
    file2 = io.open(stat2, "r")
	local rv ={}
	rv["stsss"] = file:read("*line")
	rv["modem"] = file:read("*line")
	rv["conntype"] = file:read("*line")
	rv["firmware"] = file:read("*line")
	rv["temper"] = file:read("*line")
	rv["date"] = file:read("*line")
	--------------------------------
	rv["simsel"] = file:read("*line")
	rv["cops"] = file:read("*line")
	rv["imei"] = file:read("*line")
	rv["imsi"] = file:read("*line")
	rv["iccid"] = file:read("*line")
	rv["phone"] = file:read("*line")
	--------------------------------
	rv["mode"] = file:read("*line")
	rv["per"] = file:read("*line")
	rv["rssi"] = file:read("*line")
	rv["rsrq"] = file:read("*line")
	rv["rscp"] = file:read("*line")
	rv["sinr"] = file:read("*line")
	-------------------------------
	rv["mcc"] = file:read("*line")
	rv["lac"] = file:read("*line")
	rv["cid"] = file:read("*line")
	rv["band"] = file:read("*line")
	rv["rfcn"] = file:read("*line")
	rv["pci"] = file:read("*line")
	rv["apn"] = file:read("*line")
	rv["down"] = file:read("*line")
	rv["up"] = file:read("*line")
	rv["qci"] = file:read("*line")
	rv["zbjh"] = file:read("*line")
	rv["r2cc"] = file:read("*line")
	rv["r3cc"] = file:read("*line")
	--------------------------------
	rv["cell1"] = file:read("*line")
	rv["cell2"] = file:read("*line")
	rv["cell3"] = file:read("*line")
	rv["cell4"] = file:read("*line")
	rv["cell5"] = file:read("*line")
-------------------------------
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end

