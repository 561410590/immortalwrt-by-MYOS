module("luci.controller.zz_mwrt_max_features", package.seeall)
function index()
 entry({"admin","modem","unlock_state"},call("action_unlock_state")).leaf=true
 entry({"admin","modem","cat_adopt"},call("action_cat_adopt")).leaf=true
 entry({"admin","modem","topo_data"},call("action_topo_data")).leaf=true
end

function action_unlock_state()
 local uci=require("luci.model.uci").cursor()
 luci.http.prepare_content("application/json")
 luci.http.write_json({ok=true,unlocked=uci:get("mwrt","cat","adopted")=="1"})
end

function action_cat_adopt()
 if not require("luci.dispatcher").test_post_security() then return end
 luci.http.prepare_content("application/json")
 if luci.http.formvalue("set")~="88888888" then luci.http.write_json({resdd="no"});return end
 local uci=require("luci.model.uci").cursor()
 uci:set("mwrt","cat","cat");uci:set("mwrt","cat","adopted","1")
 local ok=uci:commit("mwrt")
 luci.http.write_json({resdd=ok and "yes" or "failed"})
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
    local modems,cellDevices={},{}
    uci:foreach("qmodem","modem-device",function(s)
        modems[#modems+1]=s
        if s.network then cellDevices[s.network]=true end
    end)
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
    local rv={ok=true,ts=os.time(),model="MAX",lan={ip=la.address,mask=la.mask},
        ports={},clients={},cells={},counts={wired=0,wifi=0,total=0},usb={}}
    local e=iface(dev)
    local ea=(e["ipv4-address"] or {})[1] or {}
    local kind=not dev and "none" or cellDevices[dev] and "cell" or dev:match("^apcli") and "wifi" or "wired"
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
        if port and (port:match("^eth") or port:match("^lan")) and tonumber(age)<300 then host(mac).port=port end
    end
    for _,c in pairs(clients) do
        rv.clients[#rv.clients+1]=c
        rv.counts[c.type]=rv.counts[c.type]+1
    end
    table.sort(rv.clients,function(a,b) return a.mac<b.mac end)
    rv.counts.total=#rv.clients
    for p in fs.dir("/sys/class/net") or function() end do
        local base="/sys/class/net/"..p
        if fs.access(base.."/phydev") then
            local up=rd(base.."/carrier")=="1"
            local speed=up and tonumber(rd(base.."/speed")) or nil
            rv.ports[#rv.ports+1]={name=p,role=fs.access("/sys/class/net/br-lan/brif/"..p) and "LAN" or "WAN",up=up,speed=speed and speed>0 and speed or nil}
        end
    end
    local operators={ ["46001"]="中国联通",["46006"]="中国联通",["46009"]="中国联通",["46015"]="中国广电",
        ["46000"]="中国移动",["46002"]="中国移动",["46007"]="中国移动",["46008"]="中国移动",
        ["46003"]="中国电信",["46005"]="中国电信",["46011"]="中国电信" }
    for _,m in ipairs(modems) do
        local name=m[".name"]
        local cache=cached("/tmp/run/mwrt/modem_info_"..name..".json")
        local s={}
        for _,item in ipairs(cache.modem_info or {}) do s[item.key]=item.value end
        local netdev=m.network
        local ni=iface(netdev)
        local addr=(ni["ipv4-address"] or {})[1] or {}
        local base="/sys/class/net/"..(netdev or "")
        local plmn=tostring(s.MCC or s.MMC or "")..tostring(s.MNC or "")
        local sim=call("c2000max.ui","sim",{section=name})
        local c={id=name,label="内置5G",dev=netdev,active=dev==netdev,
            operator=operators[plmn] or s.operator,plmn=plmn,rat=s.network_mode,
            arfcn=tonumber(s.ARFCN),pci=tonumber(s["Physical Cell ID"]),cell_id=s["Cell ID"],
            rsrp=tonumber(s.RSRP),rsrq=tonumber(s.RSRQ),sinr=tonumber(s.SINR),
            cache_age_s=cache.time and os.time()-cache.time,
            module={model=s.name or m.name,fw=s.revision,link="USB · "..(netdev or "")},
            sim={slot=(sim.sim_label or ""):match("(%d+)"),state=s["SIM Status"]},
            dial={state=ni.up and addr.address and "connected" or "disconnected",ip=addr.address,
                dns=table.concat(ni["dns-server"] or {}," · "),apn=m.apn,uptime_s=ni.uptime,
                mtu=tonumber(rd(base.."/mtu")),rx_bytes=tonumber(rd(base.."/statistics/rx_bytes")),tx_bytes=tonumber(rd(base.."/statistics/tx_bytes"))}}
        rv.cells[#rv.cells+1]=c
        if c.active then rv.cell=json.parse(json.stringify(c)) end
    end
    if kind=="wifi" then
        local i=call("iwinfo","info",{device=dev})
        rv.uplink_wifi={state=e.up and "up" or "down",ssid=i.ssid,bssid=i.bssid,channel=i.channel,rssi=i.signal,iface=dev,
            band=(tonumber(i.frequency) or 0)<3000 and "2.4G" or "5G"}
    end
    if bus then bus:close() end
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control","no-store")
    local encoded=json.stringify(rv)
    -- jsonc encodes an empty Lua table as an object; these fields are lists.
    for _,key in ipairs({"clients","cells","ports"}) do
        encoded=encoded:gsub('"'..key..'":%s*{}','"'..key..'":[]')
    end
    luci.http.write(encoded)
end
