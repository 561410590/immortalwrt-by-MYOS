local m, section, m2, s2
-- 检查配置文件内容
local function is_module_connected()
    local file = io.open("/tmp/modconf-AK68.conf", "r")
    if file then
        local content = file:read("*all")
        file:close()
        return content and string.find(content, "RM520N") ~= nil
    end
    return false
end

-- 如果未连接模块，则只显示错误信息
if not is_module_connected() then
    m = Map("modem-AK68", translate("AK68已断开或未接入！请接入后重试。"))
    return m
end

m = Map("modem-AK68", translate("AK68移动网络设置"))
section = m:section(TypedSection, "ndis", translate("AK68模组设置-移远RM520N"))
section.anonymous = true
section.addremove = false
section:tab("general", translate("常规设置"))
section:tab("advanced", translate("高级设置"))

enable = section:taboption("general", Flag, "enable", translate("启用模块"))
enable.rmempty  = false

simsel= section:taboption("general", ListValue, "simsel", translate("SIM卡选择"))
simsel:value("0", translate("外置SIM卡"))
simsel:value("1", translate("内置SIM1"))
--simsel:value("2", translate("内置SIM2"))
simsel.rmempty = true

pincode = section:taboption("general", Value, "pincode", translate("PIN密码"))
pincode.default=""
------
apnconfig = section:taboption("general", Value, "apnconfig", translate("APN接入点"))
apnconfig.rmempty = true

sim_card_stat = section:taboption("general", DummyValue, "sim_card_stat", translate("SIM卡状态"))
sim_card_stat.value = luci.sys.exec("cat /tmp/simcardstat-AK68")

current_mod = section:taboption("general", Value, "current_mod", translate("外接模组"))
current_mod.rmempty = true
current_mod.default = ""

function current_mod.cfgvalue(self, section)
    if nixio.fs.access("/tmp/modconf-AK68.conf") then
        return luci.sys.exec("cat /tmp/modconf-AK68.conf")
    else
        return "未知模块或未接入AK68模式"
    end
end

-- POE设置
local poe_status = section:taboption("general", Button, "poe_control", translate("正在加载..."))
function refreshPoeStatus(section)
    local value = luci.sys.exec("cat /sys/class/gpio/cpe-pwr/value 2>/dev/null")
    value = value:match("%d") or "0"

    if value == "0" then
        poe_status.title = translate("POE供电状态")
        poe_status.inputtitle = translate("POE正在供电(点击关闭POE供电)")
    else
        poe_status.title = translate("POE供电状态")
        poe_status.inputtitle = translate("POE未供电(点击打开POE供电)")
    end
end

function poe_status.render(self, section)
    refreshPoeStatus(section)
    Button.render(self, section)
end

function poe_status.write(self, section)
    local value = luci.sys.exec("cat /sys/class/gpio/cpe-pwr/value 2>/dev/null")
    value = value:match("%d") or "0"

    if value == "1" then
        os.execute("echo 0 > /sys/class/gpio/cpe-pwr/value")
    else
        os.execute("echo 1 > /sys/class/gpio/cpe-pwr/value")
    end
    refreshPoeStatus(section)
end
------------
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
            length = length + 1  -- ASCII characters count as 1
        else
            length = length + 2  -- Non-ASCII characters (e.g., Chinese) count as 2
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
        luci.sys.exec('atsd_tools_cli -i cpe -c "AT+CMGF=0"')
        local command3 = 'lua /usr/bin/pdul-AK68.lua "" "' .. number .. '" "' .. message .. '"'
        local pdu1 = luci.sys.exec(command3)
        local pdu2 = string.gsub(pdu1, "%s+", "")
        local vs ='V100'
        --local vs = luci.sys.exec("atsd_tools_cli -i cpe -c \"ATI\" | sed -n '3p' | cut -d ':' -f2 | tr -d ' ' | sed 's/\\r$//'")
        if vs and vs:find("V100") then
            local pdu = string.gsub(pdu2, "\\r", ">")
            local command = 'atsd_tools_cli -i cpe -c "' .. 'AT+CMGS=' .. pdu .. '"'
            local result = luci.sys.exec(command)
            --luci.http.write(command)
            if result and result:find("OK") then
                luci.http.write("<script>alert('短信发送成功！如未发出请检查短信功能开关是否关闭或其他原因');</script>")
            else
                luci.http.write("<script>alert('短信发送失败！请检查短信功能开关是否关闭或其他原因。');</script>")
            end
        else
            local command = 'atsd_tools_cli -i cpe -c "' .. 'AT+CMGS=' .. pdu2 .. '"'
            local result = luci.sys.exec(command)
            luci.http.write(command)
            if result and result:find("OK") then
                luci.http.write("<script>alert('短信发送成功！如未发出请检查短信功能开关是否关闭或其他原因');</script>")
            else
                luci.http.write("<script>alert('短信发送失败！请检查短信功能开关是否关闭或其他原因。');</script>")
            end
        end

    end
    return nil
end
----------------------------------
section:tab("led", translate("LED灯光控制"),translate("说明：永久关闭灯光后,重启依然关闭,自定义LED行为不受本程序影响。"))
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
------------------------------
----------------------
section:tab("ipv6", translate("IPV6操作"), translate("温馨提示：请选择你需要的IPV6执行结果后点击按钮完成设置。"))
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
        command = '/usr/bin/full_ipv6-AK68.run'
        message = {
            done = translate("已完成打开IPV6动作，为了确保IPV6地址下发，建议使你的设备重新连接CPE以便于获取IPV6地址！"),
            fail = translate("执行失败：")
        }
    elseif mode == "half" then
        command = '/usr/bin/half_ipv6-AK68.run'
        message = {
            done = translate("已完成仅限CPE开IPV6，此模式你的设备不接入IPV6，但CPE本身可以通过IPV6通信！"),
            fail = translate("执行失败：")
        }
    elseif mode == "off" then
        command = '/usr/bin/turn_off_ipv6-AK68.run'
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

------------
smode = section:taboption("advanced", ListValue, "smode", translate("网络制式"))
smode.default = "0"
smode:value("0", translate("自动"))
smode:value("1", translate("4G网络"))
smode:value("2", translate("5G网络"))

nrmode = section:taboption("advanced", ListValue, "nrmode", translate("5G模式"))
nrmode:value("0", translate("SA/NSA双模"))
nrmode:value("1", translate("SA模式"))
nrmode:value("2", translate("NSA模式"))
nrmode:depends("smode","2")
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
bandlist_sa = section:taboption("advanced", ListValue, "bandlist_sa", translate("5G-SA频段"))
bandlist_sa.default = "0"
bandlist_sa:value("0", translate("自动"))
bandlist_sa:value("1", translate("BAND 1"))
bandlist_sa:value("3", translate("BAND 3"))
bandlist_sa:value("8", translate("BAND 8"))
bandlist_sa:value("28", translate("BAND 28"))
bandlist_sa:value("41", translate("BAND 41"))
bandlist_sa:value("78", translate("BAND 78"))
bandlist_sa:value("79", translate("BAND 79"))
bandlist_sa:depends("nrmode","1")
bandlist_nsa = section:taboption("advanced", ListValue, "bandlist_nsa", translate("5G-NSA频段"))
bandlist_nsa.default = "0"
bandlist_nsa:value("0", translate("自动"))
bandlist_nsa:value("41", translate("BAND 41"))
bandlist_nsa:value("78", translate("BAND 78"))
bandlist_nsa:depends("nrmode","2")
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
earfcn:depends("bandlist_sa","8")
earfcn:depends("bandlist_sa","28")
earfcn:depends("bandlist_sa","41")
earfcn:depends("bandlist_sa","78")
earfcn:depends("bandlist_sa","79")
earfcn:depends("bandlist_nsa","41")
earfcn:depends("bandlist_nsa","78")
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
cellid:depends("bandlist_sa","8")
cellid:depends("bandlist_sa","28")
cellid:depends("bandlist_sa","41")
cellid:depends("bandlist_sa","78")
cellid:depends("bandlist_sa","79")
cellid:depends("bandlist_nsa","41")
cellid:depends("bandlist_nsa","78")
cellid:depends("bandlist_sa","1")
cellid:depends("bandlist_nsa","41")
cellid.rmempty = true
freqlock = section:taboption("advanced", Flag, "freqlock", translate("频点/小区锁定开关"),"设置后需要勾选开关才可以锁小区基站")
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

dataroaming = section:taboption("advanced", Flag, "datarroaming", translate("国际漫游"),"适用于行动网路漫游的数据体验，可能会产生高昂的费用。")
dataroaming.rmempty = true
enable_imei = section:taboption("advanced", Flag, "enable_imei", translate("修改IMEI"))
enable_imei.default = false
enable_imei:depends("simsel", "0")

modify_imei = section:taboption("advanced", Value, "modify_imei", translate("IMEI"))
modify_imei.default = luci.sys.exec("atsd_tools_cli -i cpe -c 'AT+CGSN'| grep -oE '[0-9]+'")
modify_imei:depends("enable_imei", "1")
modify_imei.validate = function(self, value)
    if not value:match("^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$") then
        return nil, translate("IMEI必须是15位数字")
    end
    return value
end

function m.on_after_commit(self)
    local success, message = require("myos_nbcpe_modem").apply_ui()
    if not success then self.message = translate(message or "Official modem apply failed") end
end
return m,m2
