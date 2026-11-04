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
	local identity = require("luci.jsonc").parse(fs.readfile("/tmp/infocd/cache/cpe_dev") or "")
	local parameter = type(identity) == "table" and identity.parameter
	local uci = require("luci.model.uci").cursor()
	local configured_mt5700 = uci:get("network", "globals", "nbcpe") == "1"
	   and (uci:get("network", "cpe", "odu_model") or ""):match("^MT5700")
	if uci:get("network", "cpe", "mode") == "odu"
	   and (configured_mt5700 or (type(parameter) == "table"
	   and parameter.driver == "odu" and parameter.alias == "mt5700")) then
		template_name = "zmode-AK68/net_status_MT5700-AK68"
		modem_cbi = "modem5700-AK68"
	end
	entry({"admin", "modem-AK68", "webui"}, template("zmode-AK68/webui-AK68"), _("WEB界面"), 92)
	entry({"admin", "status", "mwrtstyle2"}, template("zmode-AK68/mwrtstyle2-AK68"), _("界面颜色"), 93)
	entry({"admin", "modem-AK68", "Smstrun"}, template("zmode-AK68/sms-forward"), _("短信转发"), 94).dependent = true
	entry({"admin", "modem-AK68", "smsc"}, template("zmode-AK68/sms-messages"), _("设备短信"), 95)
	entry({"admin", "modem-AK68", "nets"}, template("zmode-AK68/nbcpe-cellular"), _("蜂窝状态"), 97)
	entry({"admin", "modem-AK68", "status"}, call("action_cellular_status")).leaf = true
	entry({"admin", "modem-AK68", "at"}, template("zmode-AK68/at-terminal"), _("网络调试"), 98)
	entry({"admin", "modem-AK68", "at_command"}, post("action_at_command")).leaf = true
	entry({"admin", "modem-AK68", "modem"}, cbi(modem_cbi), _("模组设置"), 98) 
	entry({"admin", "modem-AK68", "get_csq"}, call("action_get_csq"))
	entry({"admin", "modem-AK68", "smscs"}, post_on({ action = true }, "action_smscs"))
        entry({"admin", "modem-AK68", "Smstrun", "set_token"}, post("set_token"), nil).leaf = true
        entry({"admin", "modem-AK68", "Smstrun", "set_title"}, post("set_title"), nil).leaf = true
        entry({"admin", "modem-AK68", "Smstrun", "check_status"}, call("check_status"), nil).leaf = true
        entry({"admin", "modem-AK68", "Smstrun", "redhis"}, post("redhis"), nil).leaf = true
        entry({"admin", "modem-AK68", "mwrtdf"}, call("action_mwrtdf"))
	entry({"admin", "modem-AK68", "mwrtco"}, call("action_mwrtco"))
	entry({"admin", "modem-AK68", "mwrtpk"}, call("action_mwrtpk"))
end

function action_cellular_status()
    local json = require "luci.jsonc"
    local pipe = io.popen("/usr/libexec/myos-nbcpe-status 2>/dev/null", "r")
    local output = pipe and pipe:read("*a") or ""
    if pipe then pipe:close() end
    local data = #output <= 1048576 and json.parse(output) or nil
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control", "no-store")
    if type(data) ~= "table" or type(data.sources) ~= "table" then
        luci.http.status(503, "Status unavailable")
        luci.http.write_json({ sources = {}, cache = { state = "missing", stale = true } })
        return
    end
    luci.http.write_json(data)
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
    if token and #token <= 512 and not token:find("[%z\r\n]") then
        token = token:match("^%s*(.-)%s*$")
        if not fs.writefile("/usr/bin/smstrun-AK68.conf", token) then
            luci.http.status(500, "Forward configuration write failed")
            luci.http.prepare_content("application/json")
            luci.http.write_json({ result = false, output = "转发配置保存失败。" })
            return
        end
        -- The worker caches its token in memory. Stop the old process before
        -- restarting its procd owner, including workers started by older UIs.
        local pid = tonumber((fs.readfile("/tmp/smstrun-AK68.lock") or ""):match("^%s*(%d+)%s*$"))
        if pid and pid > 1 then
            local command = fs.readfile("/proc/" .. pid .. "/cmdline") or ""
            if command:find("/usr/bin/smstrun-AK68.py", 1, true) then
                require("nixio").kill(pid, 15)
            end
        end
        local result = luci.sys.call("/etc/init.d/myos-nbcpe-native restart >/dev/null 2>&1")
        luci.http.prepare_content("application/json")
        if result ~= 0 then luci.http.status(502, "Forward service restart failed") end
        luci.http.write_json({ result = result == 0, output = token == "" and "转发关闭请求已提交。" or "转发配置已保存，服务已重新启动。" })
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
    local token_file = "/usr/bin/smstrun-AK68.conf"
    local title_file = "/usr/bin/smstrun-title-AK68.conf"
    local pid = tonumber((fs.readfile("/tmp/smstrun-AK68.lock") or ""):match("^%s*(%d+)%s*$"))
    local command = pid and fs.readfile("/proc/" .. pid .. "/cmdline") or ""
    local token_content = fs.readfile(token_file) or ""
    local title_content = fs.readfile(title_file) or ""
    local is_running = token_content:match("%S") ~= nil and command:find("/usr/bin/smstrun-AK68.py", 1, true) ~= nil
    luci.http.prepare_content("application/json")
    if is_running then
        luci.http.write_json({ status = "running", configured = true, title = title_content })
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
    local modem = require "myos_nbcpe_modem"
    local action = luci.http.formvalue("action")
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control", "no-store")
    if action == "delete" or action == "delete_all" then
        if luci.http.getenv("REQUEST_METHOD") ~= "POST" then
            luci.http.status(405, "Method Not Allowed")
            luci.http.write_json({ result = false })
            return
        end
        local index = luci.http.formvalue("index") or ""
        if action == "delete" and not index:match("^%d+$") then
            luci.http.status(400, "Invalid SMS index")
            luci.http.write_json({ result = false })
            return
        end
        local output = modem.at(action == "delete_all" and "AT+CMGD=0,4" or "AT+CMGD=" .. index)
        local success = ("\n" .. output:gsub("\r", "") .. "\n"):find("\nOK\n", 1, true) ~= nil
        if not success then luci.http.status(502, "SMS delete failed") end
        luci.http.write_json({ result = success })
        return
    end
    local messages = modem.read_raw()
    if not messages then
        luci.http.status(502, "SMS unavailable")
        luci.http.write_json({ result = false })
        return
    end
    luci.http.write_json(messages)
end

function action_at_command()
    luci.http.prepare_content("application/json")
    if luci.http.getenv("REQUEST_METHOD") ~= "POST" then
        luci.http.status(405, "Method Not Allowed")
        luci.http.write_json({ output = "POST required" })
        return
    end
    local command = luci.http.formvalue("command") or ""
    if #command > 512 or not command:upper():match("^AT") or command:find("[%z\r\n]") then
        luci.http.status(400, "Invalid AT command")
        luci.http.write_json({ output = "Invalid AT command" })
        return
    end
    luci.http.write_json({ output = require("myos_nbcpe_modem").at(command) })
end
-----------------------------------------------------------------------------------
------------获取信号状态
function action_get_csq()
    -- myos-nbcpe-cache owns periodic modem refreshes. This request only reads
    -- its stock files, so polling cannot spawn competing AT collectors.
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
