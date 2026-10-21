local fs   = require "nixio.fs"
local sys  = require "luci.sys"
local http = require "luci.http"

local mwrtfile = "/etc/config/mwrtinfo.file"
local is_pure = not fs.access(mwrtfile)

local action = http.formvalue("swiface")
if action == "pure" then
    local nixio = require "nixio"
    local src = "/usr/share/luci/menu.d/"
    local dst = "/usr/share/luci/"
    local files = {
        "luci-app-upnp.json",
        "luci-app-quickstart.json",
        "luci-app-statistics.json",
        "luci-app-ksmbd.json"
    }

    for _, name in ipairs(files) do
        local f1 = src .. name
        local f2 = dst .. name
        if nixio.fs.access(f1) then
            os.execute("mv " .. f1 .. " " .. f2)
        end
    end
    sys.call("rm -f /etc/config/mwrtinfo.file")
    sys.call([[echo '正在切换系统为纯净版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为纯净版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为纯净版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为纯净版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为纯净版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call("reboot")
    sys.call([[echo '正在切换系统为纯净版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
elseif action == "standard" then
    local nixio = require "nixio"
    local src = "/usr/share/luci/"
    local dst = "/usr/share/luci/menu.d/"
    local files = {
        "luci-app-upnp.json",
        "luci-app-quickstart.json",
        "luci-app-statistics.json",
        "luci-app-ksmbd.json"
    }

    for _, name in ipairs(files) do
        local f1 = src .. name
        local f2 = dst .. name
        if nixio.fs.access(f1) then
            os.execute("mv " .. f1 .. " " .. f2)
        end
    end

    sys.call("touch /etc/config/mwrtinfo.file")
    sys.call([[echo '正在切换系统为常规版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为常规版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为常规版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为常规版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call([[echo '正在切换系统为常规版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
    sys.call("reboot")
    sys.call([[echo '正在切换系统为常规版，重启完成后尽量保持清理浏览器缓存状态，正在重启设备...
70
#ffd500ff' > /tmp/lucimsg.file]])
end

local m = Map("mwrt", translate("Mwrt设定"))
local section = m:section(TypedSection, "ndis", translate("Mwrt固件设置"))
section.anonymous = true
section.addremove = false
section:tab("displayst", translate("屏幕保护/USB网卡开关"))
section:tab("osst", translate("固件切换"),
            translate("切换模式后重启设备即可生效,请尽量清理浏览器缓存以保证显示完整。"))


local html = ""
if is_pure then
    html = [[
        <input type="button" class="cbi-button cbi-button-apply" value="切换为常规系统"
            onclick="if(confirm('确认会重启系统以完成切换，是否切换为常规版并重启？')){document.getElementById('swiface').value='standard';this.form.submit();}">
        <input type="hidden" id="swiface" name="swiface" value="">
    ]]
else
    html = [[
        <input type="button" class="cbi-button cbi-button-apply" value="切换至纯净系统"
            onclick="if(confirm('确认会重启系统以完成切换，是否切换为纯净版并重启？')){document.getElementById('swiface').value='pure';this.form.submit();}">
        <input type="hidden" id="swiface" name="swiface" value="">
    ]]
end

local btn = section:taboption("osst", DummyValue, "_swiface_btn", "")
btn.rawhtml = true
btn.cfgvalue = function(self, section)
    return html
end

local rshowtime = section:taboption("displayst", Flag, "rshowtime", translate("屏幕保护"))
rshowtime.rmempty = false
rshowtime.default = 1

local showtime = section:taboption("displayst", ListValue, "showtime", translate("屏幕时间(秒)"))
showtime:depends("rshowtime", "1")
local times = {5,10,20,30,40,50,60,70,80,90,100,120,300,600}
for _,v in ipairs(times) do showtime:value(tostring(v), tostring(v)) end
showtime.rmempty = true
showtime.default = "10"

USBEN = section:taboption("displayst", Flag, "USBEN", translate("USB网口检测开关"),translate("开启USB开关后,USB接口接入外部网卡的时候会自动建立WAN口数据,如果使用的蜂窝模组是RNDIS可能会被误判。"))
USBEN.rmempty = false
USBEN.default=1

local s = m:section(SimpleSection)
s.title = "USB 设备信息 /lsusb"
s.description = "<div style='background:#1a1a1a;color:#fffb00;padding:12px;border-radius:6px;font-family:monospace;font-size:12px;white-space:pre;'>" ..
    luci.sys.exec("lsusb 2>/dev/null") .. "</div>"

local s = m:section(SimpleSection)
s.title = "Banner信息"
s.description = "<div style='background:#1a1a1a;color:#fffb00;padding:12px;border-radius:6px;font-family:monospace;font-size:12px;white-space:pre;'>" ..
    luci.sys.exec("cat /etc/banner") .. "</div>"

return m
