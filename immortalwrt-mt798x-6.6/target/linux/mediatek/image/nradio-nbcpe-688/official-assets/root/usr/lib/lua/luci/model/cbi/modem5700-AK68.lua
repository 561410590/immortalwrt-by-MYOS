local m, section, m2, s2
-- 检查配置文件内容并返回模块类型
local function get_module_type()
    local fs = require "nixio.fs"
    local uci = require "luci.model.uci".cursor()
    local configured_model = uci:get("network", "cpe", "odu_model") or ""
    if uci:get("network", "globals", "nbcpe") == "1"
       and uci:get("network", "cpe", "mode") == "odu"
       and configured_model:match("^MT5700") then
        return "MT5700"
    end
    local identity = require("luci.jsonc").parse(fs.readfile("/tmp/infocd/cache/cpe_dev") or "")
    local parameter = type(identity) == "table" and identity.parameter
    if uci:get("network", "cpe", "mode") == "odu" and type(parameter) == "table"
       and parameter.driver == "odu" and parameter.alias == "mt5700" then
        return "MT5700"
    end
    local file = io.open("/tmp/modconf-AK68.conf", "r")
    local module_type = nil
    if file then
        local content = file:read("*all")
        file:close()
        if content and string.find(content, "MT5700") then
            module_type = "MT5700"
        end
    end
    return module_type
end

function update_rf_mode()
    local uci = require "luci.model.uci".cursor()
    local RF_Mode = uci:get("modem-AK68", "@ndis[0]", "smode") or "0"
    local RF_Mode_file = "/tmp/RF_Mode-AK68"
    local fs = require "nixio.fs"
    local last_RF_Mode = fs.readfile(RF_Mode_file) or "-1"
    if RF_Mode ~= last_RF_Mode then
        local sys = require "luci.sys"
        local command
        if RF_Mode == "0" then
            command = 'atsd_tools_cli -i cpe -c \'AT^SYSCFGEX="080302",2000000680380,1,2,1E200000095,,\''
            sys.exec(command .. " >> /tmp/moduleInit-AK68")
            fs.writefile("/tmp/moduleInit-AK68", "RF_Mode: " .. RF_Mode .. " 自动网络\n")
        elseif RF_Mode == "1" then
            command = 'atsd_tools_cli -i cpe -c \'AT^SYSCFGEX="03",2000000680380,1,2,1E200000095,,\''
            sys.exec(command .. " >> /tmp/moduleInit-AK68")
            fs.writefile("/tmp/moduleInit-AK68", "RF_Mode: " .. RF_Mode .. " 4G网络\n")
        elseif RF_Mode == "2" then
            command = 'atsd_tools_cli -i cpe -c \'AT^SYSCFGEX="08",2000000680380,1,2,1E200000095,,\''
            sys.exec(command .. " >> /tmp/moduleInit-AK68")
            sys.exec('atsd_tools_cli -i cpe -c \'AT^C5GOPTION=1,1,1\'')
            fs.writefile("/tmp/moduleInit-AK68", "RF_Mode: " .. RF_Mode .. " 5G网络\n")
        end
        fs.writefile(RF_Mode_file, RF_Mode)
    else
        fs.writefile("/tmp/moduleInit-AK68", "RF_Mode未变动, 不执行操作\n")
    end
end


-- 根据模块类型设置标题
local module_type = get_module_type()
if not module_type then
    m = Map("modem-AK68", translate("AK68已断开或未接入！请接入后重试。"))
    return m
end

m = Map("modem-AK68", translate("内置蜂窝设置"))
local official_modem = require("myos_nbcpe_modem")
local official_ui_values = official_modem.ui_values()
local intro = m:section(SimpleSection)
intro.template = "zmode-AK68/cellular-settings"
section = m:section(TypedSection, "ndis", translate("移动网络"))
section.anonymous = true
section.addremove = false

section:tab("general", translate("切卡与拨号"))
section:tab("advanced", translate("锁频与选网"))
section:tab("apn", translate("APN"))
section:tab("sim", translate("SIM卡"))
section:tab("smstool", translate("短信"))



enable = section:taboption("general", Flag, "enable", translate("启用蜂窝连接"))
enable.rmempty  = false
enable.description = translate("保存并应用后，设备按当前 SIM 配置建立或关闭蜂窝连接。")

simsel= section:taboption("general", ListValue, "simsel", translate("当前SIM卡"))
simsel:value("0", translate("外置SIM卡"))
simsel:value("1", translate("内置SIM1"))
--simsel:value("2", translate("内置SIM2"))
simsel.rmempty = true
simsel.description = translate("仅在此处手动切卡；断网、弱信号与重启均保持所选卡槽。网络制式、APN与锁频参数应用到所选 SIM。")

------------
pincode = section:taboption("sim", Value, "pincode", translate("SIM PIN"))
pincode.default=""
pincode.password = true
pincode.description = translate("仅在 SIM 要求 PIN 时填写。")
------
apnconfig = section:taboption("apn", Value, "apnconfig", translate("接入点 APN"))
apnconfig.rmempty = true
apnconfig.description = translate("按运营商要求填写；留空使用自动 APN。")


-------------------------------------------------
smode = section:taboption("advanced", ListValue, "smode", translate("网络制式"))
smode.default = "0"
smode:value("0", translate("自动"))
smode:value("1", translate("4G网络"))
smode:value("2", translate("5G网络"))

mobility = section:taboption("advanced", ListValue, "mobility", translate("锁频策略"))
mobility.default = "2"
mobility:value("1", translate("智能"))
mobility:value("0", translate("锁定"))
mobility:value("2", translate("无偏好"))
mobility.rmempty = false
mobility.description = translate("无偏好（默认）：未指定频段或小区时解除锁频，由模组自行选网。智能：应用设备频段列表，允许小区重选与切换。锁定：严格锁定明确指定的频段或小区，禁止重选与切换；频段选择自动且未指定小区时，由模组自行选网。三种策略均可设置频段和小区。")

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


earfcn = section:taboption("advanced", Value, "earfcn", translate("频点 ARFCN"))
earfcn.description = translate("4G 填写 EARFCN，5G 填写 NR-ARFCN。可在蜂窝状态或邻区信息中查看。")
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

cellid = section:taboption("advanced", Value, "cellid", translate("物理小区 PCI"))
cellid.description = translate("填写物理小区标识 PCI，与小区 Cell ID 不同。")
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
luci.sys.exec("/usr/bin/LQXH-AK68.sh > /tmp/LQXH-AK68.file || echo '-' > /tmp/LQXH-AK68.file")--如果没有值要搞个-进去存着，不然这二逼LUA会报错
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
    local raw_value = luci.sys.exec("sed -n '" .. line_number .. "p' /tmp/LQXH-AK68.file")
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
freqlock = section:taboption("advanced", Flag, "freqlock", translate("启用频点 / 小区锁定"),translate("填写频段、频点与 PCI 后开启；关闭后恢复所选网络制式的自动选网。"))
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

dataroaming = section:taboption("advanced", Flag, "datarroaming", translate("国际漫游"),"适用于行动网路漫游的数据体验，可能会产生高昂的费用。")
dataroaming.rmempty = true

smode2 = section:taboption("advanced", ListValue, "smode2", translate("网络制式"))
smode2.default = "0"
smode2:value("0", translate("自动"))
smode2:value("1", translate("4G网络"))
smode2:value("2", translate("5G网络"))
smode2:depends("switchNetwork","1")
-----------------------------------------------------
--sim_card_stat = section:taboption("general", DummyValue, "sim_card_stat", translate("SIM卡状态"))
--sim_card_stat.value = luci.sys.exec("cat /tmp/simcardstat-AK68")
sim_card_stat = section:taboption("sim", DummyValue, "sim_card_stat", translate("SIM卡状态"))
-- 执行命令并获取输出
local sim_status_output = luci.sys.exec("atsd_tools_cli -i cpe -c 'AT^SIMSQ?' | awk '/^\\^SIMSQ:/ {split($0, a, \",\"); print a[2]}'")
sim_status_output = sim_status_output:match("%S+")
-- 根据输出解析 SIM 卡状态
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
    sim_status_description = "未获取到值,请刷新。"
    sim_status_output = "未获取到值,请刷新。"
else

    sim_status_description = "状态码:"..sim_status_description.."  请参考AT手册"
end
-- 将描述设置为选项的值
sim_card_stat.value = sim_status_description

current_mod = section:taboption("sim", DummyValue, "current_mod", translate("当前模组"))
current_mod.rmempty = true
current_mod.default = ""

function current_mod.cfgvalue(self, section)
    if module_type then
        return module_type
    end
    if nixio.fs.access("/tmp/modconf-AK68.conf") then
        return luci.sys.exec("cat /tmp/modconf-AK68.conf")
    else
        return "未知模块或未接入AK68模式"
    end
end
-- POE设置
local poe_status = section:taboption("advanced", Button, "poe_control", translate("正在加载..."),"POE开关仅适用于WT9111主板以上")
function refreshPoeStatus(section)
    local value = luci.sys.exec("cat /sys/class/gpio/cpe-pwr/value 2>/dev/null")
    value = value:match("%d") or "0"

    if value == "1" then
        poe_status.title = translate("POE供电")
        poe_status.inputtitle = translate("POE正在供电(点击关闭POE供电)")
    else
        poe_status.title = translate("POE供电")
        poe_status.inputtitle = translate("POE未供电(点击打开POE供电)")
    end
end
-----------------------------
enable_imei = section:taboption("advanced", Flag, "enable_imei", translate("ModifyI"))
enable_imei.default = false
enable_imei.rmempty = true
enable_imei:depends("simsel", "0")

modify_imei = section:taboption("advanced", Value, "modify_imei", translate("ModifyI"),translate("Warning! Warning! Warning! This is an internal engineering testing program, which is limited to testing whether the module is properly mounted. It is strictly prohibited to use it for other purposes, and the user shall bear all consequences arising from using it for other purposes on their own!"))
modify_imei.default = luci.sys.exec("atsd_tools_cli -i cpe -c AT+CGSN | sed -n '2p'")
modify_imei:depends("enable_imei", "1")
modify_imei.validate = function(self, value)
   if not value:match("^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$") then
       return nil, translate("IMEI必须是15位数字")
  end
  return value
end
-----------------------------------------------
smsen = section:taboption("smstool", Flag, "smsen", translate("启用短信"),translate("收发短信前开启并保存。部分 SIM 的短信功能可能影响 5G 驻网，可根据运营商要求调整。"))
--smsen.rmempty = true
smsen.default = 0
--------------------------------------------------------------
-- The SMS tab includes its enable switch and send controls.
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
    local number = phone_number:formvalue(section) or ""
    local message = message_content:formvalue(section) or ""
    local success = require("myos_nbcpe_modem").send(number, message)
    luci.http.write(success and "<script>alert('短信发送成功！');</script>" or
        "<script>alert('短信发送失败，请检查号码及短信开关。');</script>")
end
----------------------------------
section:tab("led", translate("灯光与供电"),translate("永久关闭灯光后，重启仍保持关闭。"))
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

---------------------------------------
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
----------------------
section:tab("ipv6", translate("IPv6"), translate("选择 IPv6 模式后，点击执行按钮应用。"))
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
-----------------------------------
--section:tab("ATA", translate("Internal testing"))
section:tab("ATA", translate("工程测试"),translate("测试前需启用短信并保存配置。"))
local number5 = section:taboption("ATA", Value, "number1", translate("NUM"))
local take_btn = section:taboption("ATA", Button, "take", translate("Apply"))
local takeof_btn = section:taboption("ATA", Button, "takeof", translate("OFF"))

function take_btn.write(self, section)
    local number6 = number5:formvalue(section)
    if number6 == ""  then
        luci.http.write("<script>alert('输入框不能为空！');</script>")
    else
        local command = 'atsd_tools_cli -i cpe -c "' .. 'ATD' .. number6 .. ';"'
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
    local result = luci.sys.exec('atsd_tools_cli -i cpe -c "ATH"')
    if result and result:find("OK") then
        luci.http.write("<script>alert('complete！');</script>")
    else
        luci.http.write("<script>alert('EER！');</script>")
    end
    return nil
end
------------
function m.on_after_commit(self)
    local success, message = official_modem.apply_ui(official_ui_values)
    if not success then self.message = translate(message or "Official modem apply failed") end
end
-- Display the active official settings, including changes made by the APP.
for _, option in pairs({ enable, simsel, smode, nrmode, apnconfig, pincode,
        bandlist_lte, bandlist_sa, bandlist_nsa, freqlock, earfcn, cellid,
        dataroaming, smsen, mobility }) do
    if option then
        local name = option.option
        if official_ui_values[name] ~= nil then
            option.cfgvalue = function() return official_ui_values[name] end
            option.forcewrite = true
        end
    end
end
return m,m2
