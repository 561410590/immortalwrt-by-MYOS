module("luci.controller.modem-AK68", package.seeall)
local fs = require "nixio.fs"
function index()
	entry({"admin", "modem-AK68"}, firstchild(), _("ATSD蜂窝"), 25).dependent = false
	local file = io.open("/tmp/modconf-AK68.conf", "r")
	local template_name = "zmode-AK68/net_status-AK68"
	local modem_cbi = "modem5700-AK68"  -- 默认设置为 modem5700-AK68
	if file then
		local content = file:read("*all")
		file:close()
		if content and string.find(content, "RM520") then
			template_name = "zmode-AK68/net_status_RM520-AK68"
			modem_cbi = "modem-AK68"
		elseif content and string.find(content, "MT5700") then
			template_name = "zmode-AK68/net_status_MT5700-AK68"
			modem_cbi = "modem5700-AK68" 
		end
	end
	entry({"admin", "modem-AK68", "webui"}, template("zmode-AK68/webui-AK68"), _("WEB界面"), 92)
	--entry({"admin", "status", "mwrtstyle2"}, template("zmode-AK68/mwrtstyle2-AK68"), _("界面颜色"), 93)
	entry({"admin", "modem-AK68", "Smstrun"}, template("zmode-AK68/settings"), _("短信转发"), 94).dependent = true
	entry({"admin", "modem-AK68", "smsc"}, template("zmode-AK68/smsc-AK68"), _("设备短信"), 95)
	entry({"admin", "modem-AK68", "nets"}, template(template_name), _("模组状态"), 97)
	entry({"admin", "modem-AK68", "at"}, template("zmode-AK68/at-AK68"), _("网络调试"), 98)
	entry({"admin", "modem-AK68", "modem"}, cbi(modem_cbi), _("模组设置"), 98) 
	entry({"admin", "modem-AK68", "get_csq"}, call("action_get_csq"))
	entry({"admin", "modem-AK68", "smscs"}, call("action_smscs"))
        entry({"admin", "modem-AK68", "Smstrun", "set_token"}, call("set_token"), nil).leaf = true
        entry({"admin", "modem-AK68", "Smstrun", "set_title"}, call("set_title"), nil).leaf = true
        entry({"admin", "modem-AK68", "Smstrun", "check_status"}, call("check_status"), nil).leaf = true
        entry({"admin", "modem-AK68", "Smstrun", "redhis"}, call("redhis"), nil).leaf = true
        entry({"admin", "modem-AK68", "mwrtdf"}, call("action_mwrtdf"))
	entry({"admin", "modem-AK68", "mwrtco"}, call("action_mwrtco"))
	entry({"admin", "modem-AK68", "mwrtpk"}, call("action_mwrtpk"))
end


function action_mwrtco()
	local source_file = "/usr/share/modem-AK68/cascade.cssdf"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command4 = string.format("cp %s %s", source_file, target_file)
	os.execute(command4)
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
	local source_file = "/usr/share/modem-AK68/cascade.cssdf"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end

function action_mwrtpk()
	local source_file = "/usr/share/modem-AK68/cascade.csspink"
	local target_file = "/www/luci-static/argon/css/cascade.css"
	local command = string.format("cp %s %s", source_file, target_file)
	os.execute(command)
	os.execute("/etc/init.d/uhttpd restart")
end
----------------短信转发
------------------------------------------------------------------------------------------------------------
function set_token()
    local token = luci.http.formvalue("ppsToken")
    if token then
        fs.writefile("/usr/bin/smstrun-AK68.conf", token)
        local output = luci.sys.exec("/usr/bin/setppstoken-AK68.sh")
        luci.http.prepare_content("application/json")
        luci.http.write_json({ result = true, output = output })
        luci.sys.exec("python3 /usr/bin/smstrun-AK68.py")
    else
        luci.http.status(400, "Bad Request")
    end
end

function set_title()
    local title = luci.http.formvalue("smsTitle")
    if title then
        fs.writefile("/usr/bin/smstrun-title-AK68.conf", title)
        local output = luci.sys.exec("/usr/bin/setsmstitle-AK68.sh")
        luci.http.prepare_content("application/json")
        luci.http.write_json({ result = true, output = output })
    else
        luci.http.status(400, "Bad Request")
    end
end


function check_status()
    local script = "/usr/bin/smstrun-AK68.py"
    local token_file = "/usr/bin/smstrun-AK68.conf"
    local title_file = "/usr/bin/smstrun-title-AK68.conf"
    local is_running = luci.sys.exec("pgrep -f " .. script) ~= ""
    local token_content = luci.sys.exec("cat " .. token_file) or ""
    local title_content = luci.sys.exec("cat " .. title_file) or ""
    luci.http.prepare_content("application/json")
    if is_running then
        luci.http.write_json({ status = "running", token = token_content, title = title_content })
    else
        luci.http.write_json({ status = "stopped" })
    end
end

function redhis()
    local output = luci.sys.exec("cat /tmp/smstrunsum-AK68.conf") or "" 
    if output == "" then
        output = "未发现转发记录，请核对。"
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json({ result = true, output = output })
end
-------------------------------------------------------------------------------
-----发短信
function action_smscs()
    local rv = {}
    local set = luci.http.formvalue("set")
    rv["at"] = set 
    os.execute(set)
    local handle = io.popen("/usr/share/modem-AK68/smsc-AK68.sh 2>&1")
    local result = handle:read("*a")
    local exit_code = handle:close()

    if exit_code ~= 0 then
        rv["smsc"] = "Error executing SMSC script: " .. result
    else
        rv["smsc"] = result
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end
-----------------------------------------------------------------------------------
------------获取信号状态
function action_get_csq()
    local conf_file_path = "/tmp/modconf-AK68.conf"
    local conf_file = io.open(conf_file_path, "r")
    local modem_type = nil

    if conf_file then
        modem_type = conf_file:read("*line")
        conf_file:close()
    else
        error("Unable to open configuration file: " .. conf_file_path)
    end

    if modem_type:find("RM520") then
        io.popen("/usr/share/modem-AK68/zinfo-AK68.sh")
    elseif modem_type:find("MT5700") then
        io.popen("/usr/share/modem-AK68/zinfo5700-AK68.sh")
    else
        error("Unsupported modem type")
    end

    local file, file2
    local stat = "/tmp/cpe_cell-AK68.file"
    local stat2 = "/tmp/stsss.file"

    file = io.open(stat, "r")
    file2 = io.open(stat2, "r")

    if not file or not file2 then
        error("Error opening status files.")
    end

    local rv = {}
    rv["stsss"] = file2:read("*line")
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
	rv["cell1"] = file:read("*line")
	rv["cell2"] = file:read("*line")
	rv["cell3"] = file:read("*line")
	rv["cell4"] = file:read("*line")
	rv["cell5"] = file:read("*line")
	--------------------------------
	file:close()
    	file2:close()
	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end


