local m, section, m2, s2
m = Map("modem", translate("巴龙MT5700M模组蜂窝-移动网络配置"))
section = m:section(TypedSection, "ndis", translate("巴龙MT5700M参数设置"))
section.anonymous = true
section.addremove = false
section:tab("general", translate("常规设置"))
section:tab("advanced", translate("高级设置"))

enable = section:taboption("general", Flag, "enable", translate("模块开关"))
enable.rmempty = false

simsel= section:taboption("general", ListValue, "simsel", translate("卡槽选择"))
simsel:value("0", translate("外置正面SIM卡1"))
simsel:value("6", translate("外置反面SIM卡2"))
simsel:value("1", translate("内置SIM卡"))
--simsel:value("2", translate("内置SIM2"))
simsel.rmempty = true


--pincode = section:taboption("general", Value, "pincode", translate("PIN-密码"))
--pincode.default=""
------
--apnconfig = section:taboption("general", Value, "apnconfig", translate("APN配置"))
--apnconfig.rmempty = true
------
local cmd = io.popen("cat /tmp/modconf.conf")
local content = cmd:read("*all") or ""
cmd:close()
if (content and string.find(content, "RM520")) or (content and string.find(content, "RM500U")) then
    -- RM520NSIN卡状态查询
    sim_card_stat = section:taboption("general", DummyValue, "sim_card_stat", translate("SIM状态"))
    sim_card_stat.value = luci.sys.exec("cat /tmp/simcardstat")
else
    -- MT5700-SIN卡状态查询
    sim_card_stat = section:taboption("general", DummyValue, "sim_card_stat", translate("SIM状态"))

    local sim_status_output =luci.sys.exec("cat /tmp/simcardstat")
    local sim_status_output = luci.sys.exec("sendat 1 'AT^SIMSQ?' | awk '/^\\^SIMSQ:/ {split($0, a, \",\"); print a[2]}'")
    if sim_status_output == "" then
        local sim_status_description = "未获取到值,请刷新。"
    else
        sim_status_output = sim_status_output:match("%S+")
    end
        local sim_status_description = "未获取到值,请刷新。"
        if sim_status_output == "0" then
            sim_status_description = "状态码:0 -SIM卡未插入"
        elseif sim_status_output == "1" then
            sim_status_description = "状态码:1 -SIM卡已插入"
        elseif sim_status_output == "2" then
            sim_status_description = "状态码:2 -SIM卡被锁"
        elseif sim_status_output == "3" then
            sim_status_description = "状态码:3-SIMLOCK 锁定(暂不支持上报)"
        elseif sim_status_output == "10" then
            sim_status_description = "状态码:10-卡文件正在初始化 SIM Initializing"
        elseif sim_status_output == "11" then
            sim_status_description = "状态码:11-SIN卡已经正常 （可接入网络）"
        elseif sim_status_output == "12" then
            sim_status_description = "状态码:12 -SIM卡正常工作"
        elseif sim_status_output == "98" then
            sim_status_description = "状态码:98 -卡物理失效 （PUK锁死或者卡物理失效）"
        elseif sim_status_output == "99" then
            sim_status_description = "状态码:99 -卡移除 SIM removed"
        elseif sim_status_output == "Note2" then
            sim_status_description = "状态码:Note2 -不支持虚拟SIM卡"
        elseif sim_status_output == "100" then
            sim_status_description = "状态码:100 -卡错误（初始化过程中，卡失败）"
        elseif sim_status_output == "" then
            sim_status_description = "未获取到值或未插入,请刷新。"
        else
            sim_status_description = "状态码:"..sim_status_output.."  请参考AT手册"
        end
        sim_card_stat.value = sim_status_description
    end
    -- MT5700SIN卡状态查询

current_mod = section:taboption("general", Value, "current_mod", translate("当前模组"))
current_mod.rmempty = true
current_mod.default = ""

local s = m:section(SimpleSection)
s.title = "USB 设备信息 /lsusb"
s.description = "<div style='background:#1a1a1a;color:#fffb00;padding:12px;border-radius:6px;font-family:monospace;font-size:12px;white-space:pre;'>" ..
    luci.sys.exec("lsusb 2>/dev/null") .. "</div>"

local s = m:section(SimpleSection)
s.title = "PCI 设备信息 /lspci"
s.description = "<div style='background:#1a1a1a;color:#fffb00;padding:12px;border-radius:6px;font-family:monospace;font-size:12px;white-space:pre;'>" ..
    luci.sys.exec("lspci 2>/dev/null") .. "</div>"

function current_mod.cfgvalue(self, section)
    if nixio.fs.access("/tmp/modconf.conf") then
        return luci.sys.exec("cat /tmp/modconf.conf")
    else
        return "未知模块或未接入模式"
    end
end
------------


smode = section:taboption("advanced", ListValue, "smode", translate("网络制式"))
smode.default = "0"
smode:value("0", translate("自动"))
smode:value("1", translate("4G网络"))
smode:value("2", translate("5G网络"))

if (content and string.find(content, "RM520")) or (content and string.find(content, "RM500U")) then
    nrmode = section:taboption("advanced", ListValue, "nrmode", translate("5G模式"))
    nrmode:value("0", translate("SA/NSA双模"))
    nrmode:value("1", translate("SA模式"))
    nrmode:value("2", translate("NSA模式"))
    nrmode:depends("smode","2")
else
    nrmode = section:taboption("advanced", ListValue, "nrmode", translate("5G-模式"))
    nrmode:value("1", translate("NR-5G"))
    nrmode:depends("smode","2")
end


bandlist_lte = section:taboption("advanced", ListValue, "bandlist_lte", translate("LTE频段"))
bandlist_lte.default = "0"
bandlist_lte:value("0", translate("自动"))
bandlist_lte:value("1", translate("BAND 1"))
bandlist_lte:value("3", translate("BAND 3"))
bandlist_lte:value("5", translate("BAND 5"))
bandlist_lte:value("8", translate("BAND 8"))
bandlist_lte:value("34", translate("BAND 34"))
bandlist_lte:value("38", translate("BAND 38"))
bandlist_lte:value("39", translate("BAND 39"))
bandlist_lte:value("40", translate("BAND 40"))
bandlist_lte:value("41", translate("BAND 41"))
bandlist_lte:depends("smode","1")

bandlist_sa = section:taboption("advanced", ListValue, "bandlist_sa", translate("5G-频段"))
bandlist_sa.default = "0"
bandlist_sa:value("0", translate("自动"))
bandlist_sa:value("1", translate("BAND 1"))
bandlist_sa:value("3", translate("BAND 3"))
bandlist_sa:value("5", translate("BAND 5"))
bandlist_sa:value("8", translate("BAND 8"))
bandlist_sa:value("28", translate("BAND 28"))
bandlist_sa:value("41", translate("BAND 41"))
bandlist_sa:value("78", translate("BAND 78"))
bandlist_sa:value("79", translate("BAND 79"))
bandlist_sa:depends("nrmode","1")


if (content and string.find(content, "RM520")) or (content and string.find(content, "RM500U")) then
bandlist_nsa = section:taboption("advanced", ListValue, "bandlist_nsa", translate("5G-NSA频段"))
bandlist_nsa.default = "0"
bandlist_nsa:value("0", translate("自动"))
bandlist_nsa:value("41", translate("BAND 41"))
bandlist_nsa:value("78", translate("BAND 78"))
bandlist_nsa:value("79", translate("BAND 79"))
bandlist_nsa:depends("nrmode","2")
end


earfcn = section:taboption("advanced", Value, "earfcn", translate("频点EARFCN"))
earfcn:depends("bandlist_lte","1")
earfcn:depends("bandlist_lte","3")
earfcn:depends("bandlist_lte","5")
earfcn:depends("bandlist_lte","8")
earfcn:depends("bandlist_lte","34")
earfcn:depends("bandlist_lte","38")
earfcn:depends("bandlist_lte","39")
earfcn:depends("bandlist_lte","40")
earfcn:depends("bandlist_lte","41")

earfcn:depends("bandlist_sa","1")
earfcn:depends("bandlist_sa","3")
earfcn:depends("bandlist_sa","5")
earfcn:depends("bandlist_sa","8")
earfcn:depends("bandlist_sa","28")
earfcn:depends("bandlist_sa","41")
earfcn:depends("bandlist_sa","78")
earfcn:depends("bandlist_sa","79")

earfcn:depends("bandlist_nsa","41")
earfcn:depends("bandlist_nsa","78")
earfcn:depends("bandlist_nsa","79")

earfcn.rmempty = true

cellid = section:taboption("advanced", Value, "cellid", translate("小区PCI"))
cellid:depends("bandlist_lte","1")
cellid:depends("bandlist_lte","3")
cellid:depends("bandlist_lte","5")
cellid:depends("bandlist_lte","8")
cellid:depends("bandlist_lte","34")
cellid:depends("bandlist_lte","38")
cellid:depends("bandlist_lte","39")
cellid:depends("bandlist_lte","40")
cellid:depends("bandlist_lte","41")

cellid:depends("bandlist_sa","1")
cellid:depends("bandlist_sa","3")
cellid:depends("bandlist_sa","5")
cellid:depends("bandlist_sa","8")
cellid:depends("bandlist_sa","28")
cellid:depends("bandlist_sa","41")
cellid:depends("bandlist_sa","78")
cellid:depends("bandlist_sa","79")

cellid:depends("bandlist_nsa","41")
cellid:depends("bandlist_nsa","78")
cellid:depends("bandlist_nsa","79")
------------------------------------------------------------------------------------
luci.sys.exec("/usr/bin/LQXH.sh > /tmp/LQXH.file || echo '-' > /tmp/LQXH.file")--如果没有值要搞个-进去存着，不然这二逼LUA会报错
enable_imei2 = section:taboption("advanced", Flag, "enable_imei2", translate("显示邻区"))
enable_imei2.default = false
enable_imei2.rmempty = true

local function createSignalBar(rsrp_value)
    local bars = 10  -- 最大条数表示信号
    local percentage = math.min(100, math.max(0, (rsrp_value + 140) * 2))  -- 转换公式，确保范围在0-100
    local filledBars = math.floor((percentage / 100) * bars)
    local barGraph = string.rep(">", filledBars) .. string.rep("-", bars - filledBars)
    return string.format("[%s] %d%% (%d dBm)", barGraph, percentage, rsrp_value)
end

local function render_signal(name, line_number)
    local raw_value = luci.sys.exec("sed -n '" .. line_number .. "p' /tmp/LQXH.file")
    local mode = raw_value:match("模式(%w+)")
    local earfcn = raw_value:match("频点:(%d+)")
    local pci = raw_value:match("小区:(%d+)")
    local rsrp_value = tonumber(raw_value:match("信号:(%-?%d+)")) or -140
    local signalDisplay = createSignalBar(rsrp_value)
    return string.format("%s | 模式 %s | 频点： %s | 小区PCI： %s |  信号： %s", name, mode or "N/A", earfcn or "N/A", pci or "N/A", signalDisplay)
end

modify_imei2 = section:taboption("advanced", DummyValue, "modify_imei2", translate("邻信号1"))
modify_imei2.cfgvalue = function()
    return render_signal("节点", 1)
end
modify_imei2:depends("enable_imei2", "1")

modify_imei3 = section:taboption("advanced", DummyValue, "modify_imei3", translate("邻信号2"))
modify_imei3.cfgvalue = function()
    return render_signal("节点", 2)
end
modify_imei3:depends("enable_imei2", "1")

modify_imei4 = section:taboption("advanced", DummyValue, "modify_imei4", translate("邻信号3"))
modify_imei4.cfgvalue = function()
    return render_signal("节点", 3)
end
modify_imei4:depends("enable_imei2", "1")

modify_imei5 = section:taboption("advanced", DummyValue, "modify_imei5", translate("邻信号4"))
modify_imei5.cfgvalue = function()
    return render_signal("节点", 4)
end
modify_imei5:depends("enable_imei2", "1")

modify_imei6 = section:taboption("advanced", DummyValue, "modify_imei6", translate("邻信号5"))
modify_imei6.cfgvalue = function()
    return render_signal("节点", 5)
end
modify_imei6:depends("enable_imei2", "1")

modify_imei7 = section:taboption("advanced", DummyValue, "modify_imei7", translate("邻信号6"))
modify_imei7.cfgvalue = function()
    return render_signal("节点", 6)
end
modify_imei7:depends("enable_imei2", "1")

modify_imei8 = section:taboption("advanced", DummyValue, "modify_imei8", translate("邻信号7"))
modify_imei8.cfgvalue = function()
    return render_signal("节点", 7)
end
modify_imei8:depends("enable_imei2", "1")

modify_imei9 = section:taboption("advanced", DummyValue, "modify_imei9", translate("邻信号8"))
modify_imei9.cfgvalue = function()
    return render_signal("节点", 8)
end
modify_imei9:depends("enable_imei2", "1")

modify_imei10 = section:taboption("advanced", DummyValue, "modify_imei10", translate("邻信号9"))
modify_imei10.cfgvalue = function()
    return render_signal("节点", 9)
end
modify_imei10:depends("enable_imei2", "1")

modify_imei11 = section:taboption("advanced", DummyValue, "modify_imei11", translate("邻信号10"))
modify_imei11.cfgvalue = function()
    return render_signal("节点", 10)
end
modify_imei11:depends("enable_imei2", "1")
-----------------------------------------------------------------------------------
freqlock = section:taboption("advanced", Flag, "freqlock", translate("频段/小区锁定开关"),"设置后需要勾选开关才可以锁小区基站")
freqlock:depends("bandlist_lte","1")
freqlock:depends("bandlist_lte","3")
freqlock:depends("bandlist_lte","5")
freqlock:depends("bandlist_lte","8")
freqlock:depends("bandlist_lte","34")
freqlock:depends("bandlist_lte","38")
freqlock:depends("bandlist_lte","39")
freqlock:depends("bandlist_lte","40")
freqlock:depends("bandlist_lte","41")

freqlock:depends("bandlist_sa","1")
freqlock:depends("bandlist_sa","3")
freqlock:depends("bandlist_sa","5")
freqlock:depends("bandlist_sa","8")
freqlock:depends("bandlist_sa","28")
freqlock:depends("bandlist_sa","41")
freqlock:depends("bandlist_sa","78")
freqlock:depends("bandlist_sa","79")

freqlock:depends("bandlist_nsa","41")
freqlock:depends("bandlist_nsa","78")
freqlock:depends("bandlist_nsa","79")
freqlock.rmempty = true
cellid.rmempty = true
-----------------------------------------------------------------------------------------------

--dataroaming = section:taboption("advanced", Flag, "datarroaming", translate("国际漫游"),"适用于行动网路漫游的数据体验，可能会产生高昂的费用。")
--dataroaming.rmempty = true

smode2 = section:taboption("advanced", ListValue, "smode2", translate("网络制式"))
smode2.default = "0"
smode2:value("0", translate("自动"))
smode2:value("1", translate("4G网络"))
smode2:value("2", translate("5G网络"))
smode2:depends("switchNetwork","1")

-- 屏幕保护勾选框
--rshowtime = section:taboption("general", Flag, "rshowtime", translate("屏幕保护"))
--rshowtime.rmempty = false
--rshowtime.default = 1

--showtime = section:taboption("general", ListValue, "showtime", translate("屏幕时间(秒)"))
--showtime:depends("rshowtime", "1")   
--local times = {5,10,20,30,40,50,60,70,80,90,100,120,300,600}
--for _,v in ipairs(times) do showtime:value(tostring(v), tostring(v)) end
--showtime.rmempty = true
--showtime.default = "10"
-----------------------------------
local cmd2 = io.popen("cat /tmp/modconf.conf")
local content = cmd2:read("*all") or ""
cmd2:close()

if (content and string.find(content, "MT5700")) or (content and string.find(content, "5700")) then
    enable_imei = section:taboption("advanced", Flag, "enable_imei", translate("ModifyI"))
    enable_imei.default = false
    enable_imei.rmempty = true
    --enable_imei:depends("simsel", "0")

    modify_imei = section:taboption("advanced", Value, "modify_imei", translate("ModifyI"), translate("Warning! Warning! Warning! This is an internal engineering testing program, which is limited to testing whether the module is properly mounted. It is strictly prohibited to use it for other purposes, and the user shall bear all consequences arising from using it for other purposes on their own!"))
    modify_imei.default = luci.sys.exec("sendat 1 AT+CGSN | sed -n '2p'")
    modify_imei:depends("enable_imei", "1")
    modify_imei.validate = function(self, value)
       if not value:match("^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$") then
           return nil, translate("IMEI必须是15位数字")
      end
      return value
end

end

if (content and string.find(content, "520")) or (content and string.find(content, "RM520")) then
    enable_imei = section:taboption("advanced", Flag, "enable_imei", translate("修改IMEI"))
    enable_imei.default = false
    enable_imei.rmempty = true
    --enable_imei:depends("simsel", "0")

    modify_imei = section:taboption("advanced", Value, "modify_imei", translate("IMEI"))
    modify_imei.default = luci.sys.exec("sendat 2 'AT+CGSN'| grep -oE '[0-9]+'")
    modify_imei:depends("enable_imei", "1")
    modify_imei.validate = function(self, value)
        if not value:match("^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$") then
            return nil, translate("IMEI必须是15位数字")
        end
        return value
    end           
end
---------------------------------------------
section:tab("smstool", translate("发送短信"),translate("温馨提示：如果发送短信失败，请检查常规设置短信开关是否打开。"))
local phone_number = section:taboption("smstool", Value, "phone_number", translate("接收号码"))
local message_content = section:taboption("smstool", Value, "message_content", translate("短信内容"))
local btn_send = section:taboption("smstool", Button, "sendsms", translate("发送短信"))

function is_valid_phone_number(number)
    return #number == 13
end

function get_length(str)
    local length = 0
    for i = 1, #str do
        local byte = str:byte(i)
        if byte >= 0 and byte <= 127 then
            length = length + 1
        else
            length = length + 2
        end
    end
    return length
end

function btn_send.write(self, section)
    local number1 = phone_number:formvalue(section)
    local number = "86" .. number1
    local message = message_content:formvalue(section)
    
    if number == "" or message == "" then
        luci.http.write("<script>alert('电话号码和短信内容不能为空！');</script>")
    --elseif not is_valid_phone_number(number) then
        --luci.http.write("<script>alert('电话号码长度不正确！应是11位数。');</script>")
    elseif get_length(message) > 140 then
        luci.http.write("<script>alert('无法发送短信，字符超限，请输入不超过140个占位符再发送。');</script>")
    elseif get_length(message) / 2 > 70 then
        luci.http.write("<script>alert('无法发送短信，字符超限，请输入不超过70个中文字符再发送。');</script>")
    else
        luci.sys.exec('sendat 1 AT+CMGF=0')
        local command1 = 'lua /usr/bin/pdul.lua "" "' .. number .. '" "' .. message .. '"'
        local pdu = luci.sys.exec(command1)
        local command = 'sendat 1 "' .. 'AT+CMGS=' .. pdu .. '"'
        local result = luci.sys.exec(command)
        if result and result:find("OK") then
            luci.http.write("<script>alert('短信发送成功！如未发出请检查短信功能开关是否关闭或其他原因');</script>")
        else
            luci.http.write("<script>alert('短信发送失败！请检查短信功能开关是否关闭或其他原因。');</script>")
        end
    end
    return nil
end
-----------------------------------------------------------------
section:tab("WEBUI", translate("WEBUI选项与转发"), translate("适用于MT5700M-CN模组WEBUI服务设置,更改WebSocket IP和端口后webui界面需要修改相应参数。"))
local websocket_status = section:taboption("WEBUI", DummyValue, "_websocket_status", translate("WEBUI 服务状态"))
websocket_status.rawhtml = true --允许原始HTML渲染
local handle = io.popen("ps w | grep websocket_server.py | grep -v grep")
local result = handle:read("*a")
handle:close()

webuishow = section:taboption("WEBUI", Button, "webuishow", translate("打开webui界面"))
webuishow.inputtitle = translate("打开webui界面")
webuishow.inputstyle = "apply"
webuishow.write = function()
    luci.http.redirect(luci.dispatcher.build_url("admin/modem/webui"))
end

webuishow2 = section:taboption("WEBUI", Button, "webuishow2", translate("网页样式显示"))
webuishow2.inputtitle = translate("打开webui链接")
webuishow2.inputstyle = "apply"
webuishow2.write = function()
    luci.http.redirect("/5700")
end

if result ~= "" then
    websocket_status.value = [[<span style="color: green; font-weight: bold;">✔ webui服务运行中</span>]]
else
    websocket_status.value = [[<span style="color: red; font-weight: bold;">✖ webui服务停止</span>]]
end
-- 选择连接WEBUI的方式
connection_type = section:taboption("WEBUI", ListValue, "connection_type", translate("连接方式"))
connection_type:value("SERIAL", translate("串口（通常是内部模组）"))
--connection_type:value("NETWORK", translate("网络（通常是AK68天线）"))
connection_type.default = "SERIAL"
connection_type.rmempty = false  -- 强制必须选择
-- 网络配置（仅当选择网络时必填）
host = section:taboption("WEBUI", Value, "host", translate("模组IP"))
host:depends("connection_type", "NETWORK")
host.rmempty = true  -- 允许为空但后端会填充默认值
host.default = "192.168.8.1"

port = section:taboption("WEBUI", Value, "port", translate("模组端口"))
port:depends("connection_type", "NETWORK")
port.rmempty = true
port.default = "20249"
port.datatype = "port"

timeout = section:taboption("WEBUI", Value, "timeout", translate("超时时间"))
timeout:depends("connection_type", "NETWORK")
timeout.rmempty = true
timeout.default = "10"
timeout.datatype = "uinteger"
-- 串口配置（用于SERIAL）
serial_port = section:taboption("WEBUI", ListValue, "serial_port", translate("串口端口"))
serial_port.rmempty = true
serial_port.default = "/dev/ttyUSB1"
serial_port:depends("connection_type", "SERIAL")  -- 仅在选择串口时可见
-- 获取系统所有的串口接口
local serial_ports = {}
local f = io.popen("ls /dev/ttyUSB*")
if f then
    for port in f:lines() do
        table.insert(serial_ports, port)
    end
    f:close()
end
for _, port in ipairs(serial_ports) do
    serial_port:value(port)
end

baudrate = section:taboption("WEBUI", Value, "baudrate", translate("波特率"))
baudrate:depends("connection_type", "SERIAL")
baudrate.rmempty = true
baudrate.default = "115200"
baudrate.datatype = "uinteger"

serial_timeout = section:taboption("WEBUI", Value, "serial_timeout", translate("串口超时"))
serial_timeout:depends("connection_type", "SERIAL")
serial_timeout.rmempty = true
serial_timeout.default = "10"
serial_timeout.datatype = "uinteger"
-- 微信Webhook URL
wechat_webhook = section:taboption("WEBUI", Value, "wechat_webhook", translate("PPS+平台转发Token"),translate("Token申请请微信搜索公众号“pushplus推送加”，然后根据教程开通即可获取。"))
wechat_webhook.rmempty = true
-- 日志文件路径
log_file = section:taboption("WEBUI", Value, "log_file", translate("日志文件"))
log_file.default = "notifications.log"
log_file.rmempty = false
-- 通知类型设置
notification_sms = section:taboption("WEBUI", Flag, "notification_sms", translate("短信转发"))
notification_sms.default = true

notification_call = section:taboption("WEBUI", Flag, "notification_call", translate("来电转发"))
notification_call.default = true

notification_memory_full = section:taboption("WEBUI", Flag, "notification_memory_full", translate("内存满通知转发"))
notification_memory_full.default = true

notification_signal = section:taboption("WEBUI", Flag, "notification_signal", translate("信号变化通知转发"),translate("以上信息勾选后会推送到PPS+公众号，请按需选择，短信转发需要在常规设置里面勾选短信开关。"))
notification_signal.default = false
-- 选择协议类型：IPv4 或 IPv6
protocol_type = section:taboption("WEBUI", ListValue, "protocol_type", translate("选择WebSocket协议类型"))
protocol_type:value("IPV4", translate("IPv4"))
protocol_type:value("IPV6", translate("IPv6"))
protocol_type.rmempty = false

-- WebSocket 主机地址和端口设置
websocket_host = section:taboption("WEBUI", Value, "websocket_host", translate("WebSocket 主机"))
websocket_host.default = "0.0.0.0"
websocket_host.rmempty = false
websocket_host:depends("protocol_type", "IPV4")

websocket_host_ipv6 = section:taboption("WEBUI", Value, "websocket_host_ipv6", translate("WebSocket 主机 (IPv6)"))
websocket_host_ipv6.default = "::"
websocket_host_ipv6.rmempty = false
websocket_host_ipv6:depends("protocol_type", "IPV6")

websocket_port = section:taboption("WEBUI", Value, "websocket_port", translate("WebSocket 端口") ,translate("提示：修改端口和IP后请在webui界面修改对应参数,0.0.0.0代表当前设备IP,通常不需要修改。"))
websocket_port.default = "8766"
websocket_port.rmempty = false

-- 执行按钮（更新和执行通知配置、WebSocket 配置）
apply_notifications_websocket = section:taboption("WEBUI", Button, "apply_notifications_websocket", translate("应用通知与WebSocket配置"))
apply_notifications_websocket.inputtitle = translate("应用并重启服务")
apply_notifications_websocket.inputstyle = "apply"
apply_notifications_websocket.write = function(self, section)
    local jsonc = require("luci.jsonc")
    local nixio = require("nixio")

    local config_file = "/usr/bin/config.json"

    -- 读取现有配置（如果文件存在）
    local content = nixio.fs.readfile(config_file)
    local data = content and jsonc.parse(content) or {}

    -- 确保各个字段存在
    data.AT_CONFIG = data.AT_CONFIG or {}
    data.AT_CONFIG.NETWORK = data.AT_CONFIG.NETWORK or {}
    data.AT_CONFIG.SERIAL = data.AT_CONFIG.SERIAL or {}
    data.NOTIFICATION_CONFIG = data.NOTIFICATION_CONFIG or {}
    data.NOTIFICATION_CONFIG.NOTIFICATION_TYPES = data.NOTIFICATION_CONFIG.NOTIFICATION_TYPES or {}
    data.WEBSOCKET_CONFIG = data.WEBSOCKET_CONFIG or {}
    data.WEBSOCKET_CONFIG.IPV4 = data.WEBSOCKET_CONFIG.IPV4 or {}
    data.WEBSOCKET_CONFIG.IPV6 = data.WEBSOCKET_CONFIG.IPV6 or {}

    -- **获取用户输入的值**
    local connection_type = self.map:get(section, "connection_type")
    if connection_type and connection_type ~= "" then
        data.AT_CONFIG.TYPE = connection_type
    end

    -- **更新网络模式配置**
    if connection_type == "NETWORK" then
        local host = self.map:get(section, "host")
        if host and host ~= "" then
            data.AT_CONFIG.NETWORK.HOST = host
        end
        
        local port = self.map:get(section, "port")
        if port and port ~= "" then
            data.AT_CONFIG.NETWORK.PORT = tonumber(port)
        end
        
        local timeout = self.map:get(section, "timeout")
        if timeout and timeout ~= "" then
            data.AT_CONFIG.NETWORK.TIMEOUT = tonumber(timeout)
        end
    end

    -- **更新串口模式配置**
    if connection_type == "SERIAL" then
        local serial_port = self.map:get(section, "serial_port")
        if serial_port and serial_port ~= "" then
            data.AT_CONFIG.SERIAL.PORT = serial_port
        end

        local baudrate = self.map:get(section, "baudrate")
        if baudrate and baudrate ~= "" then
            data.AT_CONFIG.SERIAL.BAUDRATE = tonumber(baudrate)
        end

        local serial_timeout = self.map:get(section, "serial_timeout")
        if serial_timeout and serial_timeout ~= "" then
            data.AT_CONFIG.SERIAL.TIMEOUT = tonumber(serial_timeout)
        end
    end

    -- **更新通知设置**
    local wechat_webhook = self.map:get(section, "wechat_webhook")
    if wechat_webhook and wechat_webhook ~= "" then
        data.NOTIFICATION_CONFIG.WECHAT_WEBHOOK = wechat_webhook
    end

    local log_file = self.map:get(section, "log_file")
    if log_file and log_file ~= "" then
        data.NOTIFICATION_CONFIG.LOG_FILE = log_file
    end

    -- 更新通知设置
    local notification_sms = self.map:get(section, "notification_sms")
    if notification_sms ~= nil then
        -- 转换为布尔值
        data.NOTIFICATION_CONFIG.NOTIFICATION_TYPES.SMS = notification_sms == "1" or notification_sms == true
    end

    local notification_call = self.map:get(section, "notification_call")
    if notification_call ~= nil then
        -- 转换为布尔值
        data.NOTIFICATION_CONFIG.NOTIFICATION_TYPES.CALL = notification_call == "1" or notification_call == true
    end

    local notification_memory_full = self.map:get(section, "notification_memory_full")
    if notification_memory_full ~= nil then
        -- 转换为布尔值
        data.NOTIFICATION_CONFIG.NOTIFICATION_TYPES.MEMORY_FULL = notification_memory_full == "1" or notification_memory_full == true
    end

    local notification_signal = self.map:get(section, "notification_signal")
    if notification_signal ~= nil then
        -- 转换为布尔值
        data.NOTIFICATION_CONFIG.NOTIFICATION_TYPES.SIGNAL = notification_signal == "1" or notification_signal == true
    end
    -- **更新 WebSocket 设置**
    local protocol_type = self.map:get(section, "protocol_type")
    if protocol_type and protocol_type ~= "" then
        local websocket_host = self.map:get(section, "websocket_host")
        local websocket_host_ipv6 = self.map:get(section, "websocket_host_ipv6")
        local websocket_port = self.map:get(section, "websocket_port")

        if protocol_type == "IPV4" and websocket_host and websocket_host ~= "" then
            data.WEBSOCKET_CONFIG.IPV4.HOST = websocket_host
        end
        
        if protocol_type == "IPV6" and websocket_host_ipv6 and websocket_host_ipv6 ~= "" then
            data.WEBSOCKET_CONFIG.IPV6.HOST = websocket_host_ipv6
        end
        
        if websocket_port and websocket_port ~= "" then
            data.WEBSOCKET_CONFIG.IPV4.PORT = tonumber(websocket_port)
            data.WEBSOCKET_CONFIG.IPV6.PORT = tonumber(websocket_port)
        end
    end

    -- **删除旧的 config.json 并重新创建**
    nixio.fs.remove(config_file)
    nixio.fs.writefile(config_file, jsonc.stringify(data, true))

    -- **重启 WebSocket 服务**
    os.execute("ps w | grep websocket_server.py | grep -v grep | awk '{print $1}' | xargs kill -9")
    local file88 = io.open("/tmp/modconf.conf", "r")
	if file88 then
		local content88 = file88:read("*all")
		file88:close()
		if content88 and string.find(content88, "RM520") then
            luci.http.write("<script>alert('RM520模组无法使用次WEBUI！')</script>")
		elseif content88 and string.find(content88, "RM500U") then
            luci.http.write("<script>alert('RM500模组无法使用次WEBUI！')</script>")
		elseif content88 and string.find(content88, "MT5700") then
            os.execute("rm -f /usr/bin/config.json")
            os.execute("python3 /usr/bin/websocket_server.py &")
			luci.http.write("<script>alert('已重新开启WEBUI！')</script>")
        else
            luci.http.write("<script>alert('ERR！未检测到支持的模组！无法开启。')</script>")
		end
	end
    return true
end


-----------------------------------------------------
function get_network_latency(test_url)
    local stdout = io.popen('ping -c 4 ' .. test_url .. ' | tail -1| awk \'{print $4}\' | cut -d \'/\' -f 2')
    local latency = stdout:read("*a")
    stdout:close()
    return latency:gsub("^%s*(.-)%s*$", "%1") 
end


section:tab("Networkchek", translate("网络检查"),translate("温馨提示：用于检查当前模组网络连接是否正常，不正常会自动重新拨号，如你的基站状态稳定建议不要打开此功能。"))
enable_weburl = section:taboption("Networkchek", Flag, "enable_weburl", translate("健康检查开关"))
enable_weburl.default = true
enable_weburl.rmempty = true
weburl = section:taboption("Networkchek", Value, "weburl", translate("网络检查地址"))
weburl.default = "www.baidu.com"
weburl.template = "cbi/value"
weburl.write = function(self, section, value)
    weburl.map:set(section, self.option, value)
    local latency = get_network_latency(value)
    weburl.map:set(section, "display_latency", latency)
end

display_latency = section:taboption("Networkchek", DummyValue, "display_latency", translate("检测延迟 (ms)"),translate("提示：URL输入后回车可测延迟"))
display_latency.rawhtml = true
display_latency:depends("enable_weburl", "1")

weburl:depends("enable_weburl", "1")
enable_reconnect = section:taboption("Networkchek", Flag, "enable_reconnect", translate("定时重拨开关"))
enable_reconnect.default = false
enable_reconnect.rmempty = true
check_time = section:taboption("Networkchek", ListValue, "check_time", translate("选择重拨时间"),translate("提示：该重拨选项会根据高级设置里面的参数进行重新拨号,比如之前是设置的锁定5G,会完全按照设置进行拨号。"))
-- 添加预设的时间选项
check_time:value("00:00", "00:00")
check_time:value("01:00", "01:00")
check_time:value("02:00", "02:00")
check_time:value("03:00", "03:00")
check_time:value("04:00", "04:00")
check_time:value("05:00", "05:00")
check_time:value("06:00", "06:00")
check_time:value("07:00", "07:00")
check_time:value("08:00", "08:00")
check_time:value("09:00", "09:00")
check_time:value("10:00", "10:00")
check_time:value("11:00", "11:00")
check_time:value("12:00", "12:00")
check_time:value("13:00", "13:00")
check_time:value("14:00", "14:00")
check_time:value("15:00", "15:00")
check_time:value("16:00", "16:00")
check_time:value("17:00", "17:00")
check_time:value("18:00", "18:00")
check_time:value("19:00", "19:00")
check_time:value("20:00", "20:00")
check_time:value("21:00", "21:00")
check_time:value("22:00", "22:00")
check_time:value("23:00", "23:00")

-- 设置默认时间
check_time.default = "01:00"
check_time.rmempty = true
check_time:depends("enable_reconnect", "1")

-----------------------------------------------------------------
local cmd2 = io.popen("cat /tmp/modconf.conf")
local content2 = cmd2:read("*all") or ""
cmd2:close()

if content2 ~= "" then
   if string.find(content2, "RM520") then
       section:tab("ipv6", translate("当前OP固件不适用于RM520N模组,请刷移远固件！"), translate("当前OP固件不适用于RM520N模组,请刷移远固件！"))
   else
      if string.find(content2, "MT5700") then
        section:tab("ipv6", translate("IPV6操作"), translate("温馨提示：请选择你需要的IPV6执行结果后点击按钮完成设置,该操作可能在非鲲鹏设备上无法完成。"))
        local ipv6mode = section:taboption("ipv6", ListValue, "ipv6mode", translate("IPV6模式选择"))
        ipv6mode:value("full", translate("打开IPV6通信"))
        ipv6mode:value("half", translate("仅限CPE开IPV6"))
        ipv6mode:value("off", translate("关闭IPV6通信"))
        local ipv6btn = section:taboption("ipv6", Button, "ipv6btn", translate("执行选择模式"), translate("执行选择的模式，点击按钮即可生效，重启不会重新执行。"))
        function ipv6btn.write(self, section)
            local mode = ipv6mode:formvalue(section)
            if not mode then
                luci.http.write("<script>alert('无效的模式选择！')</script>")
                return
            end
          
            if mode == "full" then
                command = '/usr/bin/full_ipv6.run'
                message = {
                    done = translate("已完成打开IPV6动作，为了确保IPV6地址下发，建议使你的设备重新连接CPE以便于获取IPV6地址！"),
                    fail = translate("执行失败：")
                }
            elseif mode == "half" then
                command = '/usr/bin/half_ipv6.run'
                message = {
                    done = translate("已完成仅限CPE开IPV6，此模式你的设备不接入IPV6，但CPE本身可以通过IPV6通信！"),
                    fail = translate("执行失败：")
                }
            elseif mode == "off" then
                command = '/usr/bin/turn_off_ipv6.run'
                message = {
                    done = translate("已完成关闭所有IPV6通信，此模式CPE和你的设备都不通过IPV6通信！"),
                    fail = translate("执行失败：")
                }
                luci.http.write("<script>alert('关闭IPV6需要等待数秒，请点击后等待完成，请不要刷新页面和操作其他！')</script>")
            end
        
            if command then
                local handle = io.popen(command, "r")
                local result = handle:read("*a")
                handle:close()
        
                if result:find("Done") then
                    luci.http.write("<script>alert('" .. message.done .. "')</script>")
                else
                    luci.http.write("<script>alert('" .. message.fail .. result .. "')</script>")
                end
            end
        end
      end
       --section:tab("ipv6"):addoption("note", translate("当前模组暂不支持IPV6设置"))
   end
else
   --section:tab("ipv6", translate("未检测到模组信息或不支持"), translate("当前未检测到模组信息，请不要操作以下参数。"))
end


-----------------------------------------------
smsen = section:taboption("advanced", Flag, "smsen", translate("短信开关"),translate("温馨提示：如果发送短信失败，请检查此处开关是否打开；某些SIM卡打开短信和语音功能可能导致自动网络无法驻网5G!"))
--smsen.rmempty = true
smsen.default = 0

-- 初始化5700按钮
initmodem = section:taboption("advanced", Button, "initmodem", translate("模组重置"))
initmodem.inputtitle = translate("初始化5700模组为转网口模式(点击立即执行)")
initmodem.inputstyle = "apply"

-- shell脚本内容
local script = [[
#!/bin/sh
echo "开始初始化模组...
1
#ff0000" > /tmp/lucimsg.file
sendat 1 'AT^TDPCIELANCFG=2'
sleep 1
sendat 1 'AT^TDPMCFG=1,0,0,0'
sleep 1
sendat 1 'AT+CGDCONT=8,"IPV4V6"'
sleep 1
sendat 1 'AT^SETAUTODIAL=1,2'
echo "切换完成，重启设备..." > /tmp/lucimsg.file
sleep 2
reboot
]]

if not nixio.fs.access("/usr/bin/init_5700.sh") then
    local f = io.open("/usr/bin/init_5700.sh", "w")
    f:write(script)
    f:close()
    luci.sys.call("chmod +x /usr/bin/init_5700.sh")
end

initmodem.write = function()
    local has_modem = (os.execute("lsusb | grep -q '5700'") == 0)
    if has_modem then
        io.popen("/usr/bin/init_5700.sh &")
    else
        luci.http.write("<script>alert('没有检测到MT5700M-CN模组，无法执行。')</script>")
    end
end


-- IP直通操作
initmodem2 = section:taboption("advanced", Button, "initmodem2", translate("打开IP直通"))
initmodem2.inputtitle = translate("点击打开IP直通功能(点击立即执行)")
initmodem2.inputstyle = "apply"

-- shell脚本内容
local script = [[
#!/bin/sh
echo "执行开启IP直通操作,模组将重启...
1
#ff0000" > /tmp/lucimsg.file
sendat 1 'AT^TDCFG="infcfg","mode",3'
sleep 1
sendat 1 'AT+CFUN=1,1'
echo "切换完成，重启设备..." > /tmp/lucimsg.file
sleep 2
reboot
]]

if not nixio.fs.access("/tmp/init_57002.sh") then
    local f = io.open("/tmp/init_57002.sh", "w")
    f:write(script)
    f:close()
    luci.sys.call("chmod +x /tmp/init_57002.sh")
end

local function js_escape(str)
    if not str then return "" end
    str = str:gsub("\\", "\\\\")
    str = str:gsub("'", "\\'")
    str = str:gsub('"', '\\"')
    str = str:gsub("\r", "\\r"):gsub("\n", "\\n")
    return str
end

initmodem2.write = function()
    local has_modem = (os.execute("lsusb | grep -q '5700'") == 0)
    if not has_modem then
        luci.http.write("<script>alert('没有检测到MT5700M-CN模组，无法执行。')</script>")
        return
    end

    local handle = io.popen("sendat 1 'ATI'")
    local output = handle:read("*a") or ""
    handle:close()

    local revision_line = output:match("Revision[^\n]*")
    if revision_line then
        -- 提取B后数字
        local bnum = revision_line:match("B(%d+)")
        local version_num = tonumber(bnum)

        if version_num and version_num >= 21 then
            io.popen("/tmp/init_57002.sh &")
        else
            local safe_version = js_escape(revision_line)
            luci.http.write(
                string.format(
                    "<script>alert('检测到你的模组版本低于B021，请升级后再尝试此功能。\\n当前模组固件版本：%s')</script>",
                    safe_version
                )
            )
        end
    else
        luci.http.write("<script>alert('无法读取模组版本信息，请检查设备。')</script>")
    end
end


-- IP直通操作
initmodem3 = section:taboption("advanced", Button, "initmodem3", translate("关闭IP直通"))
initmodem3.inputtitle = translate("点击关闭IP直通功能(点击立即执行)")
initmodem3.inputstyle = "apply"

-- shell脚本内容
local script = [[
#!/bin/sh
echo "执行关闭IP直通操作,模组将重启...
1
#ff0000" > /tmp/lucimsg.file
sendat 1 'AT^TDCFG="infcfg","mode",1'
sleep 1
sendat 1 'AT+CFUN=1,1'
echo "切换完成，重启设备..." > /tmp/lucimsg.file
sleep 2
reboot
]]

if not nixio.fs.access("/tmp/init_57003.sh") then
    local f = io.open("/tmp/init_57003.sh", "w")
    f:write(script)
    f:close()
    luci.sys.call("chmod +x /tmp/init_57003.sh")
end

initmodem3.write = function()
    local has_modem = (os.execute("lsusb | grep -q '5700'") == 0)
    if not has_modem then
        luci.http.write("<script>alert('没有检测到MT5700M-CN模组，无法执行。')</script>")
        return
    end

    local handle = io.popen("sendat 1 'ATI'")
    local output = handle:read("*a") or ""
    handle:close()

    local revision_line = output:match("Revision[^\n]*")
    if revision_line then
        -- 提取B后数字
        local bnum = revision_line:match("B(%d+)")
        local version_num = tonumber(bnum)

        if version_num and version_num >= 21 then
            io.popen("/tmp/init_57003.sh &")
        else
            local safe_version = js_escape(revision_line)
            luci.http.write(
                string.format(
                    "<script>alert('检测到你的模组版本低于B021，请升级后再尝试此功能。\\n当前模组固件版本：%s')</script>",
                    safe_version
                )
            )
        end
    else
        luci.http.write("<script>alert('无法读取模组版本信息，请检查设备。')</script>")
    end
end


local util  = require "luci.util"
local jsonc = require "luci.jsonc"

local function get_iface_ip(ifname)
    -- 调用 ubus 获取接口信息
    local cmd = string.format("ubus call network.interface.%s status 2>/dev/null", ifname)
    local raw = util.exec(cmd)

    if raw and #raw > 0 then
        local data = jsonc.parse(raw)
        if data and data['ipv4-address'] and #data['ipv4-address'] > 0 then
            return data['ipv4-address'][1].address
        end
    end
    return nil
end

local eth1_ip = get_iface_ip("wan")  -- 如果 eth1 是作为 wan 接口
-- 如果你要直接用设备名，也可以用下面方式：
-- local eth1_ip = get_iface_ip("eth1")

local ips_status = section:taboption(
    "advanced",
    DummyValue,
    "_ips_status",
    translate("IP直通状态"),
    translate("打开IP直通WAN口会获取到运营商下发的IP地址,关闭IP直通会获取到模组的管理地址。")
)
ips_status.rawhtml = true

if eth1_ip == "192.168.8.1" then
    ips_status.value = [[<span style="color: green; font-weight: bold;">✔ IP透传未打开</span>]]
elseif eth1_ip and eth1_ip ~= "" then
    ips_status.value = [[<span style="color: green; font-weight: bold;">✔ IP透传已打开</span>]]
else
    ips_status.value = [[<span style="color: yellow; font-weight: bold;">？ 未知状态，请刷新重试</span>]]
end


-----------------------------------------
--------------------AKct

-- 管理 AK68 开关
manage_ak68 = section:taboption("general", Flag, "manage_ak68", translate("管理AK68"))
manage_ak68.default = false
manage_ak68.rmempty = true


-- AKCT close
initmodem4 = section:taboption("general", Button, "initmodem4", translate("关闭AK控制模式"))
initmodem4.inputtitle = translate("点击关闭AK控制模式")
initmodem4.inputstyle = "apply"
initmodem4:depends("manage_ak68", "1")

local script_close = [[
#!/bin/sh
echo "已切关闭AK68模式。
1
#2b9ce7ff" > /tmp/lucimsg.file
rm -f /usr/bin/AK68EN.file
uci delete network.BLUE4POE
uci delete network.BLUE4POEV6
uci commit network
/etc/init.d/modeminit restart
ifdown eth1
ifup eth1
]]

if not nixio.fs.access("/tmp/init_57004.sh") then
    local f = io.open("/tmp/init_57004.sh", "w")
    f:write(script_close)
    f:close()
    luci.sys.call("chmod +x /tmp/init_57004.sh")
end

initmodem4.write = function()
    local has_modem = (os.execute("lsusb | grep -q '5700'") == 0)
    if not has_modem then
        luci.http.write("<script>alert('没有检测到MT5700M-CN模组，无法执行。')</script>")
        return
    end

    os.execute("ps w | grep websocket_server-AK68.py | grep -v grep | awk '{print $1}' | xargs kill -9")
    os.execute("pgrep -f devck-AK68.sh | xargs kill -9")
    os.execute("pgrep -f netmodeled-AK68.sh | xargs kill -9")
    os.execute("rm -f /usr/bin/config.json")
    io.popen("/tmp/init_57004.sh &")
end


-- AKCT EN
initmodem5 = section:taboption("general", Button, "initmodem5", translate("打开AK控制模式"))
initmodem5.inputtitle = translate("点击打开AK控制模式")
initmodem5.inputstyle = "apply"
initmodem5:depends("manage_ak68", "1")

local script_open = [[
#!/bin/sh
echo "已切换至AK68模式，请及时将AK68接入到WAN口，如不是鲲鹏设备，需手动新建WAN口。
1
#2392fbff" > /tmp/lucimsg.file
echo "1" > /usr/bin/AK68EN.file
echo "AK68-Mode-内置模组停止" > /tmp/modconf.conf
uci delete network.wan
uci delete network.wan6
uci commit network
/etc/init.d/modeminit-AK68 restart
ifdown BLUE4
ifup BLUE4
]]

if not nixio.fs.access("/tmp/init_57005.sh") then
    local f = io.open("/tmp/init_57005.sh", "w")
    f:write(script_open)
    f:close()
    luci.sys.call("chmod +x /tmp/init_57005.sh")
end

initmodem5.write = function()
    local has_modem = (os.execute("lsusb | grep -q '5700'") == 0)
    if not has_modem then
        luci.http.write("<script>alert('没有检测到MT5700M-CN模组，无法执行。')</script>")
        return
    end
    io.popen("/tmp/init_57005.sh &")
    os.execute("ps w | grep websocket_server.py | grep -v grep | awk '{print $1}' | xargs kill -9")
    os.execute("pgrep -f 5700netchek.sh | xargs kill -9")
    os.execute("pgrep -f netmodeled.sh | xargs kill -9")
end


-- AKCT 状态
local AKCT_status = section:taboption(
    "general",
    DummyValue,
    "_AKCT_status",
    translate("AKCT状态"),
    translate("AKCT打开代表当前设备网络来自于AK68，内置模组不工作，关闭则内置模组工作。")
)
AKCT_status.rawhtml = true
AKCT_status:depends("manage_ak68", "1")

local ls = io.popen("lsusb", "r"):read("*a") or ""
if ls:find("5700") then
    if nixio.fs.access("/usr/bin/AK68EN.file") then
        AKCT_status.value = [[<span style="color: green; font-weight: bold;">✔ AKCT已打开</span>]]
    else
        AKCT_status.value = [[<span style="color: red; font-weight: bold;">x AKCT未打开</span>]]
    end
else
    AKCT_status.value = [[<span style="color: yellow; font-weight: bold;">无内置5700模组，不可操作。</span>]]
end


----------------------------------
section:tab("led", translate("LED/风扇"),translate("说明：风扇控制仅限于新C8系列,该功能仅实用于鲲鹏设备,永久关闭灯光后,重启依然关闭,自定义LED行为不受本程序影响。"))
local btn_send2 = section:taboption("led", Button, "cled2", translate("临时关闭所有灯光"))
function btn_send2.write()
    os.execute("echo 0 > /sys/class/leds/hc:blue:cmode4/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:cmode5/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:wifi/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:status/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:sig1/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:sig2/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:sig3/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:int/brightness")
    os.execute('echo "0" > /tmp/ledflag.conf')
end

local btn_send3 = section:taboption("led", Button, "cledl3", translate("临时开启所有灯光"))
function btn_send3.write()
    os.execute("echo 1 > /sys/class/leds/hc:blue:cmode4/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:cmode5/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:wifi/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:status/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:sig1/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:sig2/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:sig3/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:int/brightness")
    os.execute('echo "1" > /tmp/ledflag.conf')
end

local btn_send5 = section:taboption("led", Button, "cledl5", translate("恢复自动灯光控制"))
function btn_send5.write()
    os.execute("echo 1 > /sys/class/leds/hc:blue:wifi/brightness")
    os.execute("echo 1 > /sys/class/leds/hc:blue:status/brightness")
    os.execute("rm -f /tmp/ledflag.conf")
    os.execute("rm -f /usr/bin/ledflagc.conf")
end

local btn_send4 = section:taboption("led", Button, "cledl4", translate("永久关闭所有灯光！"))
function btn_send4.write()
    os.execute("echo 0 > /sys/class/leds/hc:blue:cmode4/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:cmode5/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:wifi/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:status/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:sig1/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:sig2/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:sig3/brightness")
    os.execute("echo 0 > /sys/class/leds/hc:blue:int/brightness")
    os.execute('echo "0" > /tmp/ledflag.conf')
    os.execute('echo "0" > /usr/bin/ledflagc.conf')
end

local btn_send6 = section:taboption("led", Button, "cledl6", translate("关闭内置风扇"))
function btn_send6.write()
    os.execute("echo 0 > /sys/class/thermal/cooling_device0/cur_state")
end
local btn_send7 = section:taboption("led", Button, "cledl7", translate("内置风扇1档"))
function btn_send7.write()
    os.execute("echo 1 > /sys/class/thermal/cooling_device0/cur_state")
end
local btn_send8 = section:taboption("led", Button, "cledl8", translate("内置风扇2档"))
function btn_send8.write()
    os.execute("echo 2 > /sys/class/thermal/cooling_device0/cur_state")
end
local btn_send9 = section:taboption("led", Button, "cledl9", translate("内置风扇3档"))
function btn_send9.write()
    os.execute("echo 3 > /sys/class/thermal/cooling_device0/cur_state")
end
local btn_send10 = section:taboption("led", Button, "cledl10", translate("风扇智能控制"))
function btn_send10.write()
    os.execute("echo 3 > /sys/class/thermal/cooling_device0/cur_state")
end
-----------------------------------
section:tab("ATA", translate("Internal testing"),translate("提示：该功能需要将模组设置-模组参数设置-短信开关勾选打开并提交生效后才能使用;Warning! Warning! Warning! This is an internal engineering testing program, which is limited to testing whether the module is properly mounted. It is strictly prohibited to use it for other purposes, and the user shall bear all consequences arising from using it for other purposes on their own!"))
local number5 = section:taboption("ATA", Value, "number1", translate("NUM"))
local take_btn = section:taboption("ATA", Button, "take", translate("Apply"))
local takeof_btn = section:taboption("ATA", Button, "takeof", translate("OFF"))

function take_btn.write(self, section)
    local number6 = number5:formvalue(section)
    if number6 == ""  then
        luci.http.write("<script>alert('输入框不能为空！');</script>")
    else
        local command = 'sendat 1 "' .. 'ATD' .. number6 .. ';"'
        local result = luci.sys.exec(command)
        if result and result:find("OK") then
            luci.http.write("<script>alert('complete！');</script>")
        else
            luci.http.write("<script>alert('Busy or EER！');</script>")
        end
        return nil
    end
    return nil
end

function takeof_btn.write()
    local result = luci.sys.exec('sendat 1 "ATH"')
    if result and result:find("OK") then
        luci.http.write("<script>alert('complete！');</script>")
    else
        luci.http.write("<script>alert('EER！');</script>")
    end
    return nil
end
-----------------------------------
local apply = luci.http.formvalue("cbi.apply")
local sys = require "luci.sys"
local file = io.open("/tmp/modconf.conf", "r")
if apply then
    if file then
        local content = file:read("*all")
        file:close()
        if content and string.find(content, "RM520") then
            io.popen("/usr/share/modem/rm520n.sh &")
        elseif content and string.find(content, "RM500U") then
            io.popen("/usr/share/modem/500U.sh &")  
            sys.call("/sbin/ifup wan")
            sys.call("/sbin/ifup wan6")
        elseif content and string.find(content, "MT5700") then
			io.popen("/usr/share/modem/mt5700m.sh &")     
        end
    end
end
return m,m2


