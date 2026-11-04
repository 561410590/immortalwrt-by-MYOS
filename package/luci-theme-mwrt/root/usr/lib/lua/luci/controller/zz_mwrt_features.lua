module("luci.controller.zz_mwrt_features", package.seeall)
function index()
    local board = (require("nixio.fs").readfile("/tmp/sysinfo/model") or ""):lower()
    if not board:find("c5800", 1, true) then return end
    for _, name in ipairs({"dividing", "traffic", "logs", "guest"}) do
        entry({"admin","modem","factory_" .. name}, call("action_factory_" .. name)).leaf = true
    end
    entry({"admin","modem","unlock_state"}, call("action_unlock_state")).leaf = true
    entry({"admin","modem","topo_data"}, call("action_topo_data")).leaf = true
    entry({"admin","network","factory_guest"}, cbi("mwrt"), _("访客 Wi-Fi"), 65)
    entry({"admin","modem","traffic_dividing"}, cbi("mwrt"), _("流量分流"), 101)
    entry({"admin","system","factory_logs"}, cbi("mwrt"), _("日志管理"), 88)
    entry({"admin","modem","wifi_get"}, call("action_wifi_get")).leaf = true
    entry({"admin","modem","wifi_set"}, call("action_wifi_set")).leaf = true
    entry({"admin","modem","simslots"}, call("action_simslots")).leaf = true
    entry({"admin","modem","simset"}, call("action_simset")).leaf = true
    entry({"admin","modem","wlbridge_scan"}, call("action_wlbridge_scan")).leaf = true
    entry({"admin","modem","wlbridge_status"}, call("action_wlbridge_status")).leaf = true
    entry({"admin","modem","wlbridge_join"}, call("action_wlbridge_join")).leaf = true
    entry({"admin","modem","wlbridge_leave"}, call("action_wlbridge_leave")).leaf = true
end

-- C5800 factory 2.3.3: dividing / cpedata / logread / guest.
-- Reuse MYOS services; no factory automatic SIM/quota controls are imported.
local function factory_reply(value)
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control", "no-store")
    luci.http.write_json(value)
end

local function factory_input()
    if not require("luci.dispatcher").test_post_security() then return end
    local value = require("luci.jsonc").parse(luci.http.formvalue("data") or "")
    if type(value) ~= "table" then factory_reply({ok=false,error="参数格式错误"}); return end
    return value
end

function action_factory_traffic()
    local sys, json, quote = require "luci.sys", require "luci.jsonc", require("luci.util").shellquote
    local cmd = "curl -sS --max-time 15 "
    if luci.http.getenv("REQUEST_METHOD") == "POST" then
        local value = factory_input(); if not value then return end
        cmd = cmd .. "-H 'Content-Type: application/json' --data-binary " .. quote(json.stringify(value)) .. " "
    end
    local response = json.parse(sys.exec(cmd .. "http://127.0.0.1:5700/nradio/traffic"))
    factory_reply(response or {ok=false,error="流量统计服务没有响应"})
end

function action_factory_dividing()
    local uci, fs = require("uci").cursor(), require "nixio.fs"
    local function section(package, kind, name, values)
        uci:set(package,name,kind)
        for key,value in pairs(values) do
            if type(value)=="table" and #value==0 then uci:delete(package,name,key)
            else uci:set(package,name,key,value) end
        end
    end
    local function current()
        local value = {ok=true,clients={},main={},poe={},wan={},hosts={}}
        uci:foreach("dividing", "client", function(s)
            value.clients[#value.clients+1] = {mac=s[".name"]:gsub("_",":"),uplink=s.dividing_default}
        end)
        for _, p in ipairs({"main","poe","wan"}) do
            local addresses=uci:get("dividing",p,"address") or {}
            value[p]=type(addresses)=="table" and addresses or {addresses}
        end
        for line in (fs.readfile("/tmp/dhcp.leases") or ""):gmatch("[^\n]+") do
            local mac,ip,name=line:match("^%d+%s+(%S+)%s+(%S+)%s+(%S+)")
            if mac then value.hosts[#value.hosts+1]={mac=mac,ip=ip,name=name} end
        end
        return value
    end
    if luci.http.getenv("REQUEST_METHOD") ~= "POST" then factory_reply(current()); return end
    local v = factory_input(); if not v then return end
    local clients, addresses, domains = {}, {main={},poe={},wan={}}, {main={},poe={},wan={}}
    local dt = require "luci.cbi.datatypes"
    for _, c in ipairs(v.clients or {}) do
        local mac=tostring(c.mac or ""):lower()
        if not dt.macaddr(mac) or (c.uplink~="main" and c.uplink~="poe" and c.uplink~="wan") then
            factory_reply({ok=false,error="请选择终端 MAC 和出口"}); return
        end
        clients[#clients+1]={mac=mac,uplink=c.uplink}
    end
    for _, p in ipairs({"main","poe","wan"}) do
        for _, a in ipairs(v[p] or {}) do
            a=tostring(a):lower():gsub("^%s+",""):gsub("%s+$","")
            if a~="" then
                if dt.ip4addr(a) then addresses[p][#addresses[p]+1]=a
                elseif dt.hostname(a) and a:find("%.") and not a:find("[^%w%.%-]") then
                    domains[p][#domains[p]+1]=a
                else factory_reply({ok=false,error="IP / 域名格式错误："..a}); return end
            end
        end
    end
    if not fs.access("/etc/config/dividing") then fs.writefile("/etc/config/dividing", "") end
    local old={}
    uci:foreach("dividing",nil,function(s) if s[".type"]=="client" or s[".name"]=="main" or s[".name"]=="poe" or s[".name"]=="wan" then old[#old+1]=s[".name"] end end)
    for _,s in ipairs(old) do uci:delete("dividing",s) end
    old={}
    uci:foreach("mwan3",nil,function(s) if s[".name"]:match("^nr_") then old[#old+1]=s[".name"] end end)
    for _,s in ipairs(old) do uci:delete("mwan3",s) end
    local rules={}
    local function rule(name, values)
        values.family,values.proto,values.src_iface="ipv4","all","lan"
        section("mwan3","rule",name,values); rules[#rules+1]=name
    end
    for i,c in ipairs(clients) do
        section("dividing","client",c.mac:gsub(":","_"),{dividing_default=c.uplink})
        rule("nr_mac"..i,{src_mac=c.mac,use_policy="nr_"..c.uplink,class="client"})
    end
    local dns={}
    for _,p in ipairs({"main","poe","wan"}) do
        section("mwan3","member","nr_m_"..p,{interface=p=="main" and "cpe_4" or p=="poe" and "cpe1" or "wan",metric="1",weight="1"})
        section("mwan3","policy","nr_"..p,{use_member={"nr_m_"..p},last_resort="default"})
        local all={}
        for i,a in ipairs(addresses[p]) do
            all[#all+1]=a; rule("nr_"..p..i,{dest_ip=a,use_policy="nr_"..p,class="protocol"})
        end
        for _,a in ipairs(domains[p]) do all[#all+1]=a end
        section("dividing","protocol",p,{address=all})
        if #domains[p]>0 then
            local set="nr_dom_"..p
            rule(set,{ipset=set,use_policy="nr_"..p,class="protocol"})
            dns[#dns+1]="ipset=/"..table.concat(domains[p],"/").."/"..set
            require("luci.sys").call("ipset -exist create "..set.." hash:ip family inet")
        end
    end
    -- The selected factory dividing class is applied by the existing mwan3 library.
    for i=#rules,1,-1 do uci:reorder("mwan3",rules[i],0) end
    uci:commit("dividing"); uci:commit("mwan3")
    local previous=fs.readfile("/etc/dnsmasq.conf") or ""
    local conf=previous
    conf=conf:gsub("\n?# NRADIO DIVIDING BEGIN\n.-# NRADIO DIVIDING END\n?", "")
    if #dns>0 then conf=conf.."\n# NRADIO DIVIDING BEGIN\n"..table.concat(dns,"\n").."\n# NRADIO DIVIDING END\n" end
    local written,rc=true,require("luci.sys").call("/usr/sbin/c5800-route-mode apply")
    if conf ~= previous then
        written=fs.writefile("/etc/dnsmasq.conf",conf)
        for _,p in ipairs({"main","poe","wan"}) do
            if #domains[p]>0 and require("luci.sys").call("ipset flush nr_dom_"..p) ~= 0 then rc=1 end
        end
        -- SIGHUP does not reread ipset directives in this included config file.
        if require("luci.sys").call("/etc/init.d/dnsmasq restart") ~= 0 then rc=1 end
    end
    local ok=written~=nil and written~=false and rc==0
    factory_reply({ok=ok,error=not ok and "配置已保存，服务应用失败，请查看系统日志" or nil})
end

function action_factory_logs()
    local fs, sys, uci = require "nixio.fs", require "luci.sys", require("luci.model.uci").cursor()
    local section
    uci:foreach("system","system",function(s) section=section or s[".name"] end)
    if luci.http.getenv("REQUEST_METHOD")=="POST" then
        local v=factory_input(); if not v then return end
        if v.enabled then uci:set("system",section,"log_file","/mnt/mmcblk0p10/log/messages")
        else uci:delete("system",section,"log_file") end
        uci:commit("system")
        local rc=sys.call("/etc/init.d/log reload")
        factory_reply({ok=rc==0,error=rc~=0 and "日志服务应用失败" or nil}); return
    end
    if luci.http.formvalue("download")=="1" then
        local dir="/tmp/nradio-log-"..require("nixio").getpid()
        fs.mkdir(dir,"700")
        fs.writefile(dir.."/system.log",sys.exec("logread"))
        fs.writefile(dir.."/kernel.log",sys.exec("dmesg"))
        local paths={"-C "..dir.." system.log kernel.log"}
        for _,p in ipairs({"/var/log","/mnt/mmcblk0p10/log"}) do
            if fs.stat(p,"type")=="dir" then paths[#paths+1]="-C "..p.." ." end
        end
        luci.http.header("Content-Disposition",'attachment; filename="C5800-688-logs-'..os.date("%Y%m%d-%H%M%S")..'.tar.gz"')
        luci.http.prepare_content("application/gzip")
        local pipe=io.popen("tar -czf - "..table.concat(paths," ").." 2>/dev/null","r")
        if pipe then
            while true do local chunk=pipe:read(8192); if not chunk then break end; luci.http.write(chunk) end
            pipe:close()
        end
        fs.unlink(dir.."/system.log");fs.unlink(dir.."/kernel.log");fs.rmdir(dir)
        return
    end
    local path=uci:get("system",section,"log_file")
    factory_reply({ok=true,enabled=path~=nil and path~="",path=path or ""})
end

function action_factory_guest()
    local uci=require("luci.model.uci").cursor()
    local first=uci:get_all("wireless","nr_guest_2g") or uci:get_all("wireless","nr_guest_5g") or {}
    if luci.http.getenv("REQUEST_METHOD")~="POST" then
        local bands={}
        for _,b in ipairs({"2g","5g"}) do
            if uci:get("wireless","nr_guest_"..b,"disabled")=="0" then bands[#bands+1]=b end
        end
        local mac=(require("nixio.fs").readfile("/sys/class/net/br-lan/address") or ""):gsub("[^%x]","")
        local applying=false
        local bus=require("ubus").connect()
        if bus then
            local wireless=bus:call("network.wireless","status",{}) or {}
            for _,radio in pairs(wireless) do
                if radio.pending then applying=true end
            end
            bus:close()
        end
        factory_reply({ok=true,enabled=#bands>0,ssid=first.ssid or ("@Guest-"..mac:sub(-4)),encryption=first.encryption or "none",
            isolation=first.isolate~="0",bands=#bands>0 and bands or {"2g","5g"},has_key=first.key~=nil,applying=applying})
        return
    end
    local v=factory_input(); if not v then return end
    local ssid,key=tostring(v.ssid or ""),nil
    key=tostring(v.key or "")
    if #ssid<1 or #ssid>32 or ssid:find("[%c]") or (v.encryption~="none" and v.encryption~="psk-mixed") then
        factory_reply({ok=false,error="SSID 或加密方式不合法"}); return
    end
    if key=="" then key=first.key or "" end
    if v.encryption~="none" and (#key<8 or #key>63) then factory_reply({ok=false,error="密码需要 8–63 个字符"}); return end
    local bands={};for _,b in ipairs(v.bands or {}) do if b=="2g" or b=="5g" then bands[b]=true end end
    if v.enabled and not next(bands) then factory_reply({ok=false,error="请选择应用频段"}); return end
    local isolated=v.isolation~=false
    uci:section("network","device","nr_guest_dev",{name="br-guest",type="bridge",bridge_empty="1"})
    uci:section("network","interface","nr_guest",{device="br-guest",proto="static",ipaddr="192.168.67.1",netmask="255.255.255.0"})
    uci:section("dhcp","dhcp","nr_guest",{interface="nr_guest",start="100",limit="150",leasetime="12h"})
    uci:section("firewall","zone","nr_guest",{name="nr_guest",network={"nr_guest"},input=isolated and "REJECT" or "ACCEPT",output="ACCEPT",forward="REJECT"})
    uci:section("firewall","forwarding","nr_guest_wan",{src="nr_guest",dest="wan"})
    uci:section("firewall","rule","nr_guest_dhcp",{name="Guest DHCP",src="nr_guest",proto="udp",dest_port="67",family="ipv4",target="ACCEPT"})
    uci:section("firewall","rule","nr_guest_dns",{name="Guest DNS",src="nr_guest",proto={"tcp","udp"},dest_port="53",target="ACCEPT"})
    uci:delete("firewall","nr_guest_lan")
    if not isolated then uci:section("firewall","forwarding","nr_guest_lan",{src="nr_guest",dest="lan"}) end
    uci:foreach("wireless","wifi-device",function(s)
        local b=s.band
        if b=="2g" or b=="5g" then
            local name="nr_guest_"..b
            uci:section("wireless","wifi-iface",name,{device=s[".name"],network="nr_guest",mode="ap",ssid=ssid,
                encryption=v.encryption,isolate=isolated and "1" or "0",disabled=v.enabled and bands[b] and "0" or "1"})
            if v.encryption~="none" then uci:set("wireless",name,"key",key) else uci:delete("wireless",name,"key") end
        end
    end)
    for _,p in ipairs({"network","dhcp","firewall","wireless"}) do uci:commit(p) end
    local rc=require("luci.sys").call("ubus call network reload && /etc/init.d/dnsmasq restart && /etc/init.d/firewall reload")
    factory_reply({ok=rc==0,error=rc~=0 and "配置已保存，访客网络应用失败，请查看系统日志" or nil})
end

local function wifi_find(band)
	local uci = require "luci.model.uci".cursor()
	local dev
	uci:foreach("wireless", "wifi-device", function(s)
		if not dev and s.band == band then dev = s[".name"] end
	end)
	if not dev then return nil, nil, "找不到 " .. band .. " 频段的无线设备" end

	local names = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.device == dev and (s.mode or "ap") == "ap" then
			names[#names + 1] = s[".name"]
		end
	end)
	if #names == 0 then return nil, nil, "该频段没有可配置的无线接口" end
	table.sort(names)
	return dev, names[1], nil
end

-- 读当前无线配置。GET,只读。
-- **不回传 key。** 现有无线密码不出现在页面上;前端那个密码框留空即表示不修改。
function action_wifi_get()
	local uci = require "luci.model.uci".cursor()
	local band = luci.http.formvalue("band")

	luci.http.prepare_content("application/json")
	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" })
		return
	end

	local dev, iface, err = wifi_find(band)
	if not iface then
		luci.http.write_json({ ok = false, error = err })
		return
	end

	luci.http.write_json({
		ok = true,
		band = band,
		device = dev,
		ssid = uci:get("wireless", iface, "ssid") or "",
		encryption = (uci:get("wireless", iface, "encryption") or "none"):gsub("%+.*$", ""),
		-- 设备整体关闭 或 这个接口单独关闭,都算关闭
		disabled = (uci:get("wireless", dev, "disabled") == "1"
			or uci:get("wireless", iface, "disabled") == "1")
	})
end

-- 写无线配置。**必须 POST 且带 CSRF token** —— test_post_security() 两件都查,
-- 不通过它自己回 405/403 并且返回 false。
-- 改 SSID/密码是能把用户锁在网络外面的操作,不能让别的站点拿着会话 cookie
-- 用一个 <img src> 式的 GET 就触发。
function action_wifi_set()
	-- dispatcher 此刻必然已加载(它正在调度这个请求), require 走的是缓存,
	-- 但显式写出来才不依赖 package.seeall 碰巧能看到那个全局。
	-- test_post_security 内部用的 http/context 是它自己的闭包 upvalue,
	-- 跨模块调用照样拿得到。
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end

	local uci = require "luci.model.uci".cursor()
	local band = luci.http.formvalue("band")
	local ssid = luci.http.formvalue("ssid") or ""
	local enc = luci.http.formvalue("encryption") or ""
	local key = luci.http.formvalue("key") or ""

	luci.http.prepare_content("application/json")

	-- 前端也校验了一遍,但那只是为了少跑一个来回;真正的闸门在这里。
	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" })
		return
	end
	-- SSID 按**字节**算长度:这是 802.11 的硬限制(SSID 字段 32 字节),
	-- 不是字符数 —— 中文一个字 3 字节,11 个汉字就到顶了。
	if ssid == "" or #ssid > 32 then
		luci.http.write_json({ ok = false, error = "网络名称需要 1 - 32 个字节(中文每字约 3 字节)" })
		return
	end
	-- 控制字符会把 uci 配置写坏,也不可能是用户真想要的名字
	if ssid:find("[%c]") then
		luci.http.write_json({ ok = false, error = "网络名称不能包含控制字符" })
		return
	end
	-- 白名单,不是黑名单:加密方式会被原样写进 uci 并交给 hostapd,
	-- 只放行确实支持的那几种写法。
	local ENC_OK = {
		["none"] = true, ["psk"] = true, ["psk2"] = true,
		["psk-mixed"] = true, ["sae"] = true, ["sae-mixed"] = true
	}
	if not ENC_OK[enc] then
		luci.http.write_json({ ok = false, error = "不支持的加密方式" })
		return
	end
	if enc ~= "none" and key ~= "" and (#key < 8 or #key > 63) then
		luci.http.write_json({ ok = false, error = "无线密码需要 8 - 63 个字符" })
		return
	end

	local dev, iface, err = wifi_find(band)
	if not iface then
		luci.http.write_json({ ok = false, error = err })
		return
	end

	-- 改成加密但没给密码,而原来也没有密码 —— 这会落到一个"加密却无密码"的
	-- 配置上,hostapd 起不来,无线直接消失。挡在写之前。
	local oldkey = uci:get("wireless", iface, "key") or ""
	if enc ~= "none" and key == "" and oldkey == "" then
		luci.http.write_json({ ok = false, error = "该频段当前没有密码, 改为加密时必须设置一个" })
		return
	end

	uci:set("wireless", iface, "ssid", ssid)
	local oldenc = uci:get("wireless", iface, "encryption") or "none"
	uci:set("wireless", iface, "encryption", oldenc:gsub("%+.*$", "") == enc and oldenc or enc)
	if enc == "none" then
		-- 开放网络留着一个 key 没有意义,而且下次切回加密时会悄悄复用旧密码
		uci:delete("wireless", iface, "key")
	elseif key ~= "" then
		uci:set("wireless", iface, "key", key)
	end
	-- key == "" 且 enc ~= "none": 保留原密码不动,这就是前端"留空表示不修改"

	uci:commit("wireless")

	-- **异步重启无线。** 同步跑 `wifi reload` 的话:用户大概率正通过这个频段的
	-- WiFi 访问本页面,无线一断 HTTP 响应就发不出去,浏览器只会看到请求失败,
	-- 分不清是没保存成功还是只是断线。延后一秒再 reload,先把这个 JSON 回出去。
	os.execute("(sleep 1; /sbin/wifi reload) >/dev/null 2>&1 &")

	luci.http.write_json({ ok = true, band = band, device = dev, ssid = ssid })
end

function action_simslots()
    local uci = require("luci.model.uci").cursor()
    local sel = uci:get_all("cpesel", "sim") or {}
    local slots, physical, embedded = {}, 0, 0
    for kind in (tostring(sel.stype or "") .. ","):gmatch("([^,]+),") do
        if kind ~= "4" then physical = physical + 1 else embedded = embedded + 1 end
        slots[#slots + 1] = {v = tostring(#slots + 1), label = kind == "4" and ("内置卡" .. embedded) or ("SIM" .. physical)}
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json({supported = #slots > 0, current = tostring(sel.cur or "1"), slots = slots, title = "C5800 主设备 · " .. #slots .. " 个卡槽"})
end

function action_simset()
    if not require("luci.dispatcher").test_post_security() then return end
    local slot = tonumber(luci.http.formvalue("slot"))
    local ok = require("myos_nradio_data").apply_native_cpesel(slot, 1)
    luci.http.prepare_content("application/json")
    luci.http.write_json({ok = ok == true, native = true, current = tostring(slot or ""), error = not ok and "卡槽设置失败，请读取实际状态" or nil})
end

local WLB_NET = "mwrt_bridge"
local WLB_SID = "mwrt_wlbridge"
-- 无线网桥事件文件: 写 "<epoch>|up|<ssid>" / "<epoch>|down|<ssid>", 供右上角
-- 通知中心(rpcd luci.mwrt-notice)当"一次性事件"读出来显示一小会儿。两边路径一致。
local WLB_EVENT = "/tmp/mwrt-wlbridge.event"

-- 写一条桥接事件。kind = "up" / "down"。ssid 里的竖线/换行剔掉(分隔符安全),
-- 用 nixio.fs.writefile 直接落地(不 fork、不经 shell, 免注入)。
local function wlb_write_event(kind, ssid)
	local fs = require "nixio.fs"
	local s = tostring(ssid or ""):gsub("[|\r\n]", " ")
	local ts = os.time()
	fs.writefile(WLB_EVENT, string.format("%d|%s|%s\n", ts, kind, s))
end

-- 按 band('2g'/'5g')拿 { device=wifi-device名, phy=ra0/rai0, netdev=apcli0/apclii0 }。
-- netdev 由 phy 推: ra0→apcli0, rai0→apclii0(prefix + idx0, 和 l1util apcli_ifname 一致)。
local function wlb_radio(band)
	local uci = require "luci.model.uci".cursor()
	local dev, phy
	uci:foreach("wireless", "wifi-device", function(s)
		if not dev and s.band == band then dev = s[".name"]; phy = s.phy end
	end)
	if not dev then return nil end
	-- ra0 → apcli0 ; rai0 → apclii0(把 phy 里 ra 后面那段插 "cli")
	local netdev
	if phy == "ra0" then netdev = "apcli0"
	elseif phy == "rai0" then netdev = "apclii0"
	else
		-- 兜底: raX → apcliX、raiX → apcliiX
		local base, idx = (phy or ""):match("^(ra[i]?)(%d+)$")
		if base then netdev = base .. "cli" .. idx else netdev = "apcli0" end
	end
	return { device = dev, phy = phy, netdev = netdev }
end

-- 把 iwinfo scan 的 encryption 对象映射成 uci encryption 值(与 mtwifi enc2dat / wireless.js 同口径)。
local function wlb_enc(e)
	if type(e) ~= "table" or not e.enabled then return "none" end
	local auth = e.authentication or {}
	local has = function(t, v) for _, x in ipairs(t) do if x == v then return true end end return false end
	if has(auth, "sae") then
		-- 对端是 WPA3(或 WPA2/WPA3 过渡), 都用 sae(mtwifi sae 能连过渡 AP)
		return "sae"
	end
	if has(auth, "psk") then
		local wpa = e.wpa or {}
		local w1, w2 = has(wpa, 1), has(wpa, 2)
		if w1 and w2 then return "psk-mixed" end
		if w2 then return "psk2" end
		if w1 then return "psk" end
		return "psk2"
	end
	-- 只有 WEP 或未知加密: 不支持桥接(返回空让前端标"不支持")
	if e.wep then return "wep" end
	return "unknown"
end

-- 字符串是否是「干净可显示」的 UTF-8 SSID(过滤隐藏/乱码 AP)。
-- 严格校验 UTF-8: ASCII 可见字符直接过; 多字节必须是合法 UTF-8 续字节序列。
-- 非法字节(单独的 0x80-0xBF、0xFE/0xFF、DEL 0x7F、控制符)→ 判脏。
local function wlb_utf8_ok(s)
	if not s or s == "" then return false end
	if s:find("\239\191\189") then return false end  -- U+FFFD 替换符
	local i, n = 1, #s
	while i <= n do
		local c = s:byte(i)
		if c < 0x20 or c == 0x7F then
			return false  -- 控制字符 / DEL
		elseif c < 0x80 then
			i = i + 1     -- ASCII 可见
		elseif c >= 0xC2 and c <= 0xDF then
			-- 2 字节: 后面 1 个 0x80-0xBF
			local c2 = s:byte(i + 1)
			if not c2 or c2 < 0x80 or c2 > 0xBF then return false end
			i = i + 2
		elseif c >= 0xE0 and c <= 0xEF then
			-- 3 字节(含中文): 后面 2 个 0x80-0xBF
			local c2, c3 = s:byte(i + 1), s:byte(i + 2)
			if not c2 or c2 < 0x80 or c2 > 0xBF or not c3 or c3 < 0x80 or c3 > 0xBF then return false end
			i = i + 3
		elseif c >= 0xF0 and c <= 0xF4 then
			-- 4 字节: 后面 3 个 0x80-0xBF
			local c2, c3, c4 = s:byte(i + 1), s:byte(i + 2), s:byte(i + 3)
			if not c2 or c2 < 0x80 or c2 > 0xBF or not c3 or c3 < 0x80 or c3 > 0xBF or not c4 or c4 < 0x80 or c4 > 0xBF then return false end
			i = i + 4
		else
			return false  -- 0x80-0xC1 单独出现、0xF5-0xFF 非法
		end
	end
	return true
end

function action_wlbridge_scan()
	local band = luci.http.formvalue("band")
	luci.http.prepare_content("application/json")
	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" })
		return
	end
	local r = wlb_radio(band)
	if not r then
		luci.http.write_json({ ok = false, error = "找不到 " .. band .. " 频段的无线设备" })
		return
	end

	-- ubus call iwinfo scan {"device":"ra0"} —— 返回结构化结果, 不用解析 iwpriv。
	local jsonc = require "luci.jsonc"
	local out = luci.sys.exec("ubus call iwinfo scan '{\"device\":\"" .. r.phy .. "\"}' 2>/dev/null") or ""
	local parsed = jsonc.parse(out)
	local results = (type(parsed) == "table" and parsed.results) or {}

	local list = {}
	for _, b in ipairs(results) do
		local enc = wlb_enc(b.encryption)
		local ssid = b.ssid
		local ok = wlb_utf8_ok(ssid)
		-- 保留能连的(psk/psk2/psk-mixed/sae/none); wep/unknown 标出来但置灰不可点
		list[#list + 1] = {
			ssid = ok and ssid or "",
			hidden = (not ok),
			bssid = b.bssid,
			signal = b.signal,
			quality = b.quality,
			quality_max = b.quality_max,
			channel = b.channel,
			enc = enc,
			joinable = ok and (enc ~= "wep" and enc ~= "unknown"),
		}
	end
	-- 按信号质量降序
	table.sort(list, function(a, bb) return (a.quality or 0) > (bb.quality or 0) end)
	luci.http.write_json({ ok = true, band = band, netdev = r.netdev, list = list })
end

-- 读当前桥接状态: 有没有 sta iface、apcli 关联到哪、拿没拿到 IP、是否已设为主路由。
local function wlb_read_status()
	local uci = require "luci.model.uci".cursor()
	local fs = require "nixio.fs"
	local st = { bridged = false, band = "", ssid = "", bssid = "", ip = "",
		signal = nil, associated = false, primary_on = false, netdev = "" }

	-- 找我们建的 sta iface
	local sid
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then sid = s[".name"]; return false end
	end)
	if not sid then return st end
	local dev = uci:get("wireless", sid, "device")
	st.ssid = uci:get("wireless", sid, "ssid") or ""
	-- device → band → netdev
	local band = uci:get("wireless", dev or "", "band") or ""
	st.band = band
	local r = band ~= "" and wlb_radio(band) or nil
	local netdev = r and r.netdev or "apcli0"
	st.netdev = netdev
	st.bridged = true

	-- 关联状态: iwconfig 的 Access Point(Not-Associated 为未连)
	local iw = luci.sys.exec("iwconfig " .. netdev .. " 2>/dev/null") or ""
	local ap = iw:match("Access Point:%s*([%x:]+)")
	st.associated = (ap ~= nil and ap:upper() ~= "NOT-ASSOCIATED" and ap ~= "00:00:00:00:00:00")
	if st.associated then st.bssid = ap:upper() end
	local essid = iw:match('ESSID:"([^"]*)"')
	if essid and essid ~= "" then st.ssid = essid end

	-- 拿到 IP 没(apcli netdev 上的 ipv4)
	local ipout = luci.sys.exec("ip -4 addr show dev " .. netdev .. " 2>/dev/null") or ""
	st.ip = ipout:match("inet%s+([%d%.]+)") or ""

	-- 是否已设为主路由(Mwrt-Assurance)
	local pif = uci:get("Mwrt-Assurance", "wan_failover", "primary_if") or ""
	local en = uci:get("Mwrt-Assurance", "wan_failover", "enabled") or "0"
	st.primary_on = uci:get("network", WLB_NET, "metric") == "0"
	return st
end

function action_wlbridge_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json(wlb_read_status())
end

function action_wlbridge_join()
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end
	luci.http.prepare_content("application/json")

	local band = luci.http.formvalue("band")
	local ssid = luci.http.formvalue("ssid") or ""
	local bssid = luci.http.formvalue("bssid") or ""
	local key = luci.http.formvalue("key") or ""
	local enc = luci.http.formvalue("enc") or ""
	local as_primary = (luci.http.formvalue("primary") == "1")

	if band ~= "2g" and band ~= "5g" then
		luci.http.write_json({ ok = false, error = "频段参数不合法" }); return
	end
	if ssid == "" or #ssid > 32 then
		luci.http.write_json({ ok = false, error = "上游网络名称不合法" }); return
	end
	local ENC_OK = { ["none"]=true, ["psk"]=true, ["psk2"]=true, ["psk-mixed"]=true, ["sae"]=true }
	if not ENC_OK[enc] then
		luci.http.write_json({ ok = false, error = "该网络的加密方式暂不支持桥接" }); return
	end
	if enc ~= "none" and (#key < 8 or #key > 63) then
		luci.http.write_json({ ok = false, error = "上游 WiFi 密码需要 8 - 63 个字符" }); return
	end

	local r = wlb_radio(band)
	if not r then
		luci.http.write_json({ ok = false, error = "找不到 " .. band .. " 频段的无线设备" }); return
	end

	local uci = require "luci.model.uci".cursor()

	-- 幂等: 先删掉所有已存在的 sta iface(本功能固定单桥接), 避免叠加 / 换频段残留。
	local to_del = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then to_del[#to_del + 1] = s[".name"] end
	end)
	for _, n in ipairs(to_del) do uci:delete("wireless", n) end

	-- 建 STA 客户端 iface
	uci:section("wireless", "wifi-iface", WLB_SID, {
		device = r.device,
		mode = "sta",
		network = WLB_NET,
		ssid = ssid,
		encryption = enc,
	})
	if enc ~= "none" then uci:set("wireless", WLB_SID, "key", key) end
	if bssid ~= "" and bssid:match("^[%x:]+$") then uci:set("wireless", WLB_SID, "bssid", bssid) end
	uci:commit("wireless")

	-- network.wwan = dhcp 接口(apcli netdev 由 netifd 自动挂进来)
	uci:delete("network", WLB_NET)
	uci:section("network", "interface", WLB_NET, { proto = "dhcp", metric = as_primary and "0" or "90" })
	uci:commit("network")

	-- 把 wwan 加进 wan 防火墙区域(只增不删其它成员)
	uci:foreach("firewall", "zone", function(s)
		if s.name == "wan" then
			local nets = s.network or {}
			if type(nets) == "string" then nets = { nets } end
			local seen = false
			for _, n in ipairs(nets) do if n == WLB_NET then seen = true end end
			if not seen then nets[#nets + 1] = WLB_NET; uci:set("firewall", s[".name"], "network", nets) end
			return false
		end
	end)
	uci:commit("firewall")

	-- 设为主路由 + 开网关存活(可选, 默认开)。
	-- ⚠ 只改 primary_if, **不动 secondary_if** —— secondary 是原来的故障兜底口
	-- (通常是蜂窝 eth2)。早期版本把旧 primary 塞进 secondary, 结果把蜂窝兜底口
	-- 覆盖成了宽带口 eth1, 断开后"路由切换状态"里蜂窝一直显示离线(它在探 eth1)。
	-- 现在:桥接前的 primary_if / secondary_if / enabled 全存到 sta 节点, 断开时三者
	-- 原样还原, 谁都不越权覆盖。

	-- 异步应用: wifi reload 建 apcli + 关联, ifup wwan 起 DHCP。放后台, 不阻塞本响应
	-- (用户可能正用本机 WiFi 连着这页, 同步跑会把响应闷死)。
	os.execute("( wifi reload; sleep 2; /sbin/ifup " .. WLB_NET .. "; /etc/init.d/firewall reload ) >/dev/null 2>&1 &")

	-- 发一条"已接入"事件给右上角通知中心
	wlb_write_event("up", ssid)

	luci.http.write_json({ ok = true, netdev = r.netdev, band = band })
end

function action_wlbridge_leave()
	local dsp = require "luci.dispatcher"
	if not dsp.test_post_security() then return end
	luci.http.prepare_content("application/json")

	local uci = require "luci.model.uci".cursor()

	-- 记下当前桥接网卡, 好把网关存活主口还原
	local st = wlb_read_status()

	-- 删 sta iface 前, 先读出桥接时存下的"桥接前网关存活状态", 用于精确还原
	local old_en, old_pif, old_sif = nil, nil, nil
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then
			old_en = s.wlb_old_en; old_pif = s.wlb_old_pif; old_sif = s.wlb_old_sif
		end
	end)

	-- 删 sta iface
	local dels = {}
	uci:foreach("wireless", "wifi-iface", function(s)
		if s.mode == "sta" then dels[#dels + 1] = s[".name"] end
	end)
	for _, n in ipairs(dels) do uci:delete("wireless", n) end
	uci:commit("wireless")

	-- 从 wan 区域移除 wwan + 删 network.wwan
	uci:foreach("firewall", "zone", function(s)
		if s.name == "wan" then
			local nets = s.network or {}
			if type(nets) == "string" then nets = { nets } end
			local keep = {}
			for _, n in ipairs(nets) do if n ~= WLB_NET then keep[#keep + 1] = n end end
			uci:set("firewall", s[".name"], "network", keep)
			return false
		end
	end)
	uci:commit("firewall")
	uci:delete("network", WLB_NET)
	uci:commit("network")

	-- 网关存活还原: 只有当前主口确实是桥接网卡时才动它(别人手动改过就不越权)。
	-- 把桥接前存下的 primary_if / secondary_if / enabled 三者原样还原 —— 尤其
	-- secondary_if 必须还原, 否则蜂窝兜底口会一直停在错的网卡上, 状态页显示离线。

	os.execute("( /sbin/ifdown " .. WLB_NET .. "; wifi reload; /etc/init.d/firewall reload ) >/dev/null 2>&1 &")

	-- 发一条"已断开"事件给右上角通知中心(用拆桥前记下的 ssid)
	wlb_write_event("down", st.ssid)

	luci.http.write_json({ ok = true })
end

-- MWRT 1154 topology: read existing native caches and kernel tables only.
function action_unlock_state()
    local fs = require "nixio.fs"
    luci.http.prepare_content("application/json")
    luci.http.write_json({ok=true, unlocked=(fs.readfile("/etc/config/Outofnetwork") or ""):match("^ok") ~= nil})
end

function action_topo_data()
    local fs, json = require "nixio.fs", require "luci.jsonc"
    local uci, sys = require("luci.model.uci").cursor(), require "luci.sys"
    local bus = require("ubus").connect()
    local function rd(p)
        local value = (fs.readfile(p) or ""):gsub("%s+$", "")
        return value
    end
    local function call(o,m,a)
        if not bus then return {} end
        local ok,v = pcall(bus.call,bus,o,m,a or {})
        return ok and type(v)=="table" and v or {}
    end
    local function cached(p) return json.parse(rd(p)) or {} end
    local function iphex(h)
        local n=tonumber(h,16) or 0
        return string.format("%d.%d.%d.%d",n%256,math.floor(n/256)%256,math.floor(n/65536)%256,math.floor(n/16777216)%256)
    end
    local dev,gw,metric
    for line in rd("/proc/net/route"):gmatch("[^\n]+") do
        local d,dst,g,fl,m=line:match("^(%S+)%s+(%x+)%s+(%x+)%s+(%x+)%s+%d+%s+%d+%s+(%d+)")
        if dst=="00000000" and (tonumber(fl,16) or 0)%2==1 and (not metric or tonumber(m)<metric) then
            dev,gw,metric=d,iphex(g),tonumber(m)
        end
    end
    local ifaces=call("network.interface","dump").interface or {}
    local function iface(device)
        local best={}
        for _,s in ipairs(ifaces) do
            if (s.l3_device or s.device)==device and s.up then
                if #(s["ipv4-address"] or {})>0 then return s end
                best=s
            end
        end
        return best
    end
    local lan=iface("br-lan")
    local la=(lan["ipv4-address"] or {})[1] or {}
    local rv={ok=true,ts=os.time(),model="c5800-688",lan={ip=la.address,mask=la.mask},
        ports={},clients={},cells={},counts={wired=0,wifi=0,total=0},usb={}}
    local e=iface(dev)
    local ea=(e["ipv4-address"] or {})[1] or {}
    local kind=not dev and "none" or (dev=="eth1" or dev=="port5") and "cell" or dev:match("^apcli") and "wifi" or "wired"
    rv.egress={kind=kind,dev=dev,gw=gw,metric=metric,ip=ea.address,proto=e.proto,
        rx_bytes=dev and tonumber(rd("/sys/class/net/"..dev.."/statistics/rx_bytes")),
        tx_bytes=dev and tonumber(rd("/sys/class/net/"..dev.."/statistics/tx_bytes"))}
    local names={}
    for line in rd("/tmp/dhcp.leases"):gmatch("[^\n]+") do
        local mac,ip,name=line:match("^%d+%s+(%S+)%s+(%S+)%s+(%S+)")
        if mac then names[mac:lower()]={ip=ip,name=name~="*" and name or nil} end
    end
    local arp={}
    for line in rd("/proc/net/arp"):gmatch("[^\n]+") do
        local ip,flags,mac,net=line:match("^(%S+)%s+%S+%s+(%S+)%s+(%S+)%s+%S+%s+(%S+)")
        if net=="br-lan" and flags=="0x2" then arp[mac:lower()]=ip end
    end
    local clients,portnums={},{}
    local function host(mac)
        mac=mac:lower()
        if not clients[mac] then
            local n=names[mac] or {}
            clients[mac]={mac=mac,ip=arp[mac] or n.ip,name=n.name,type="wired"}
        end
        return clients[mac]
    end
    for name in fs.dir("/sys/class/net/br-lan/brif") or function() end do
        local pn=tonumber(rd("/sys/class/net/"..name.."/brport/port_no"))
        if pn then portnums[pn]=name end
        if name:match("^ra") or name:match("^wlan") then
            local info=call("iwinfo","info",{device=name})
            local band=(tonumber(info.frequency) or 0)<3000 and "2.4G" or "5G"
            local suffix=band=="2.4G" and "24" or "5"
            rv.lan["ssid"..suffix]=info.ssid
            rv.lan["ch"..suffix]=info.channel
            for _,s in ipairs(call("iwinfo","assoclist",{device=name}).results or {}) do
                if s.mac then
                    local c=host(s.mac)
                    c.type,c.port,c.band,c.rssi="wifi",name,band,s.signal
                    c.online_s=s.connected_time
                    c.tx_rate=s.tx and s.tx.rate and s.tx.rate/1000
                    c.rx_rate=s.rx and s.rx.rate and s.rx.rate/1000
                end
            end
        end
    end
    for line in sys.exec("brctl showmacs br-lan 2>/dev/null"):gmatch("[^\n]+") do
        local p,mac,age=line:match("^%s*(%d+)%s+(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)%s+no%s+([%d%.]+)")
        local port=p and portnums[tonumber(p)]
        if port and port:match("^lan") and tonumber(age)<300 then host(mac).port=port end
    end
    for _,c in pairs(clients) do
        rv.clients[#rv.clients+1]=c
        rv.counts[c.type]=rv.counts[c.type]+1
    end
    table.sort(rv.clients,function(a,b) return a.mac<b.mac end)
    rv.counts.total=#rv.clients
    for _,p in ipairs({"lan1","lan2","lan3","port5"}) do
        local base="/sys/class/net/"..p
        if fs.access(base) then
            local up=rd(base.."/carrier")=="1"
            local speed=up and tonumber(rd(base.."/speed")) or nil
            rv.ports[#rv.ports+1]={name=p,role=p=="port5" and "副5G" or "LAN",up=up,speed=speed and speed>0 and speed or nil}
        end
    end
    local operators={ ["46001"]="中国联通",["46006"]="中国联通",["46009"]="中国联通",["46015"]="中国广电",
        ["46000"]="中国移动",["46002"]="中国移动",["46007"]="中国移动",["46008"]="中国移动",
        ["46003"]="中国电信",["46005"]="中国电信",["46011"]="中国电信" }
    local cache=cached("/tmp/infocd/cache/cpestatus")
    for _,name in ipairs({"cpe","cpe1"}) do
        local s,uptime={},nil
        for _,item in ipairs(cache.result or {}) do
            if item.status and item.status.name==name then s=item.status;uptime=item.uptime end
        end
        local netdev=uci:get("network",name,"device") or (name=="cpe" and "eth1" or "port5")
        local ni=iface(netdev)
        local addr=(ni["ipv4-address"] or {})[1] or {}
        local base="/sys/class/net/"..netdev
        local band=s.band
        if band and band~="" and band:match("^%d+$") then band=(tostring(s.mode):match("NR") and "n" or "B")..band end
        local c={id=name,label=name=="cpe" and "内置5G" or "副5G",dev=netdev,active=dev==netdev,
            operator=operators[s.isp] or s.operator or s.isp,plmn=s.isp,rat=s.mode,band=band,arfcn=tonumber(s.earfcn),
            pci=tonumber(s.pci),cell_id=s.cell,rsrp=tonumber(s.rsrp),rsrq=tonumber(s.rsrq),sinr=tonumber(s.sinr),
            module={model=s.model,fw=s.revision,link="以太网 · "..netdev},
            sim={slot=s.simno,state=s.CPIN=="0" and "ready" or nil},
            dial={state=ni.up and addr.address and "connected" or "disconnected",ip=addr.address,
                dns=table.concat(ni["dns-server"] or {}," · "),apn=s.apn,uptime_s=ni.uptime or uptime,
                mtu=tonumber(rd(base.."/mtu")),rx_bytes=tonumber(rd(base.."/statistics/rx_bytes")),tx_bytes=tonumber(rd(base.."/statistics/tx_bytes"))}}
        rv.cells[#rv.cells+1]=c
        if c.active then rv.cell=c end
    end
    if kind=="wifi" then
        local i=call("iwinfo","info",{device=dev})
        rv.uplink_wifi={state=e.up and "up" or "down",ssid=i.ssid,bssid=i.bssid,channel=i.channel,rssi=i.signal,iface=dev,
            band=(tonumber(i.frequency) or 0)<3000 and "2.4G" or "5G"}
    end
    if bus then bus:close() end
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control","no-store")
    luci.http.write_json(rv)
end
