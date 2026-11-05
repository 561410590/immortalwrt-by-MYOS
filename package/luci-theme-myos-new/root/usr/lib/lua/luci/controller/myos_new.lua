module("luci.controller.myos_new", package.seeall)

function index()
    local active = require("luci.model.uci").cursor():get("luci", "main", "mediaurlbase") == "/luci-static/myos-new"
    local page = entry({"admin", "myos"}, view("myos-new/home"), active and _("首页") or nil, 1)
    page.dependent = false
    entry({"admin", "myos", "data"}, call("snapshot")).leaf = true
    entry({"admin", "myos", "fast"}, call("fast_snapshot")).leaf = true
    entry({"admin", "myos", "acceleration"}, call("acceleration")).leaf = true
    entry({"admin", "myos", "sim_names"}, call("sim_names")).leaf = true
    entry({"admin", "myos", "modem_refresh"}, call("modem_refresh")).leaf = true
    entry({"admin", "myos", "notices"}, call("notices")).leaf = true
    entry({"admin", "myos", "unlock"}, post("unlock_plugins")).leaf = true
end

local function device_profile(board)
    local identity = ((board.model or "") .. " " .. (board.board_name or "")):lower():gsub("[^%w]", "")
    if identity:find("n5000") then return "n5000"
    elseif identity:find("ultra") then return "c2000ultra"
    elseif identity:find("c2000max") or identity:find("hcmt7987") then return "c2000max"
    elseif identity:find("nbcpe") or identity:find("nb68") then return "nbcpe688"
    elseif identity:find("c8688") or identity:find("hcmt7981688") then return "c8688"
    elseif identity:find("c5800") then return "c5800688" end
    return "generic"
end

local function physical_ports(profile, devices)
    local known = {c5800688={"lan1","lan2","lan3","port5"},c8688={"port1","port2","port3","port5"},
        nbcpe688={"port3","port2","port1","port5"},c2000max={"eth1"},
        n5000={"eth0","eth1"},c2000ultra={"eth0","eth1"}}
    if known[profile] then return known[profile] end
    local names, fs = {}, require "nixio.fs"
    for name,d in pairs(devices) do
        if d.devtype=="dsa" or (d.devtype=="ethernet" and fs.access("/sys/class/net/"..name.."/phydev")) then
            names[#names+1]=name
        end
    end
    table.sort(names)
    return names
end

-- Presentation aliases only: do not rename netdevs or rewrite UCI network state.
local function port_label(profile, name)
    if (profile == "c8688" or profile == "nbcpe688") and name:match("^port[123]$") then
        return "lan" .. name:sub(-1)
    end
    if profile == "nbcpe688" and name == "port5" then return "2.5G PoE / ODU" end
    return name
end

function notices()
    local fs, bus = require "nixio.fs", require("ubus").connect()
    local result = {notices={}, ports={}, boot=fs.readfile("/proc/sys/kernel/random/boot_id")}
    local function call(object, method)
        if not bus then return nil end
        local ok,data=pcall(bus.call,bus,object,method,{})
        return ok and type(data)=="table" and data or nil
    end
    local perf=require "luci.model.myos_liquid"
    local native=perf.cached("notices")
    if not native.ts or os.time()-native.ts<0 or os.time()-native.ts>=25 then
        native=call("luci.mwrt-notice","list") or {};native.ts=os.time();perf.save("notices",native)
    end
    -- Keep existing USB, bridge and system notices; physical links below use
    -- the same per-model mapping as the dashboard instead of C5800-only names.
    for _,n in ipairs(native and native.notices or {}) do
        if n.icon~="eth" and n.icon~="eth-off" then result.notices[#result.notices+1]=n end
    end
    local meta=perf.cached("portmeta")
    local names,profile={},meta.profile
    if profile and meta.boot==(result.boot or ""):gsub("%s+$","") then
        for _,p in ipairs(meta.ports or {}) do names[#names+1]=p.name end
    else
        local board,devices=call("system","board"),call("network.device","status")
        if board and devices then profile=device_profile(board);names=physical_ports(profile,devices)
        else result.error="网口状态暂不可用" end
    end
    for _,name in ipairs(names) do
        local base="/sys/class/net/"..name
        local carrier=tonumber(fs.readfile(base.."/carrier") or "")
        if carrier==0 or carrier==1 then result.ports[#result.ports+1]={name=name,label=port_label(profile,name),carrier=carrier,
            count=tonumber(fs.readfile(base.."/carrier_up_count") or ""),speed=tonumber(fs.readfile(base.."/speed") or "")} end
    end
    if bus then bus:close() end
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control","no-store")
    luci.http.write_json(result)
end

function unlock_plugins()
    local available, system = pcall(require, "myos_plugins")
    if available and type(system.unlock) == "function" then return system.unlock() end
    local ok, backend = pcall(require, "luci.controller.zz_mwrt_fancontrol")
    if ok and type(backend.action_Outofnetwork) == "function" then
        return backend.action_Outofnetwork()
    end
    luci.http.status(404, "Not Found")
    luci.http.prepare_content("application/json")
    luci.http.write_json({error="当前固件未提供插件认证接口"})
end

local function sim_name_cache(iccid)
    iccid=tostring(iccid or "")
    if #iccid<18 or #iccid>22 or not iccid:match("^%d+$") then return nil end
    return "/tmp/myos-new-sim-name-"..iccid
end

local function nbcpe_cache_record()
    return require("luci.model.myos_liquid").nbcpe()
end

-- QModem versions publish either split caches or one merged cache_info_<id>.
-- Read the cache already produced by QModem; never query the AT port here.
local function qmodem_cache_records()
    local fs,json=require "nixio.fs",require "luci.jsonc"
    local paths,records={},{}
    for file in fs.glob("/tmp/cache_base_info_*") or function() end do
        paths[file:match("cache_base_info_(.+)$")]=file
    end
    for file in fs.glob("/tmp/cache_info_*") or function() end do
        local id=file:match("cache_info_(.+)$")
        if not paths[id] or (fs.stat(file,"mtime") or 0)>=(fs.stat(paths[id],"mtime") or 0) then paths[id]=file end
    end
    for id,file in pairs(paths) do
        local record={id=id,fields={}}
        local function read(path)
            local function collect(t,depth)
                if type(t)~="table" or depth>7 then return end
                if t.key and t.value~=nil and tostring(t.value)~="" then record.fields[tostring(t.key):lower()]=t.value end
                for _,child in pairs(t) do if type(child)=="table" then collect(child,depth+1) end end
            end
            collect(json.parse(fs.readfile(path) or ""),0)
            return fs.stat(path,"mtime")
        end
        if file:find("/tmp/cache_info_",1,true)==1 then
            record.updated=read(file)
            local simfile="/tmp/cache_sim_info_"..id
            if (fs.stat(simfile,"mtime") or 0)>=(record.updated or 0) then read(simfile) end
        else
            for _,kind in ipairs({"base","network","sim","cell"}) do
                local stamp=read("/tmp/cache_"..kind.."_info_"..id)
                if kind=="cell" then record.updated=stamp end
            end
        end
        records[#records+1]=record
    end
    table.sort(records,function(a,b) return a.id<b.id end)
    return records
end

-- MAX's existing QModem reader is request-driven, not a background sampler.
-- Keep its slow, read-only refresh separate from CPU and traffic snapshots.
function modem_refresh()
    local uci,json,sys=require("luci.model.uci").cursor(),require "luci.jsonc",require "luci.sys"
    local bus=require("ubus").connect()
    local result={refreshed={},errors={}}
    if bus then
        local board=bus:call("system","board",{}) or {}
        if device_profile(board)=="c2000max" then
            uci:foreach("qmodem","modem-device",function(s)
                if s.state=="enabled" then
                    local ok,data=pcall(bus.call,bus,"qmodem","info",{config_section=s[".name"]})
                    if ok and type(data)=="table" then
                        result.refreshed[#result.refreshed+1]=s[".name"]
                    else result.errors[s[".name"]]="模组信息暂不可用" end
                    local read,sim=pcall(bus.call,bus,"qmodem","sim_info",{config_section=s[".name"]})
                    if not read or type(sim)~="table" then result.errors[s[".name"]]="SIM 信息暂不可用" end
                end
            end)
            local state=json.parse(sys.exec("/etc/init.d/c2000max-sim-select status_json 2>/dev/null")) or {}
            result.sim={modem=state.modem,actual=state.actual,target=state.target,cpin=state.cpin}
        end
        bus:close()
    end
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control","no-store")
    luci.http.write_json(result)
end

function sim_names()
    local fs,json,sys,util=require "nixio.fs",require "luci.jsonc",require "luci.sys",require "luci.util"
    local source=json.parse(fs.readfile("/tmp/infocd/cache/cpestatus") or "") or {}
    local entries=source.result or {}
    local native=nbcpe_cache_record()
    if native then entries={{status={name="AK68",iccid=native.iccid}}} end
    if #entries==0 then
        for _,record in ipairs(qmodem_cache_records()) do
            entries[#entries+1]={status={name=record.id,iccid=record.fields.iccid}}
        end
    end
    local result,seen={},{}
    for _,entry in ipairs(entries) do
        local s=entry.status or {}
        local path=sim_name_cache(s.iccid)
        if path and not seen[path] then
            seen[path]=true
            local data=json.parse(fs.readfile(path) or "")
            if not data or os.time()-(data.updated or 0)>=(data.name and 300 or 60) then
                data={iccid=tostring(s.iccid),updated=os.time(),error="云端名称暂不可用"}
                local native=fs.access("/usr/sbin/nradio_crypto")
                local available,codec=pcall(require,"myos_nradio_app_crypto")
                local payload
                if native then
                    payload=sys.exec("/usr/sbin/nradio_crypto -O -e -s "..util.shellquote(json.stringify({iccid=tostring(s.iccid)})).." 2>/dev/null")
                elseif available and type(codec.encrypt_card_data)=="function" and type(codec.decrypt_app_data)=="function" then
                    local ok,encrypted=pcall(codec.encrypt_card_data,json.stringify({iccid=tostring(s.iccid)}))
                    if ok and encrypted then payload=json.stringify({data=encrypted}) end
                end
                if payload then
                    if json.parse(payload) then
                        local response=sys.exec("curl -sS --connect-timeout 4 --max-time 8 -H 'Content-Type: application/json' --data-binary "..util.shellquote(payload).." https://wechat.vapyun.com/nradio/app/equipment/queryTypeByIccid 2>/dev/null")
                        if response~="" then
                            local decoded
                            if native then
                                decoded=sys.exec("/usr/sbin/nradio_crypto -O -d -s "..util.shellquote(json.stringify({data=response})).." 2>/dev/null")
                            else
                                local ok,value=pcall(codec.decrypt_app_data,response)
                                if ok then decoded=value end
                            end
                            local envelope=json.parse(decoded or "") or {}
                            local answer=type(envelope.data)=="string" and json.parse(envelope.data) or envelope
                            if answer and answer.success==true and tonumber(answer.code)==200 then
                                local value=type(answer.result)=="string" and answer.result or answer.message
                                local name=type(value)=="string" and value:match("^%s*(.-)%s*$") or ""
                                if name~="" and name~="unknown" then data.name=name;data.error=nil end
                            end
                        end
                    end
                end
                fs.writefile(path,json.stringify(data))
                fs.chmod(path,"600")
            end
            result[s.name or "modem"]={name=data.name,error=data.error}
        end
    end
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control","no-store")
    luci.http.write_json(result)
end

local function output(value)
    luci.http.prepare_content("application/json")
    luci.http.header("Cache-Control","no-store")
    luci.http.write_json(value)
end
local function merge_fast(full,fast)
    full.ts=fast.ts;full.sample=fast.sample;full.cpu=fast.cpu;full.boot=fast.boot
    full.system=full.system or {}
    for k,v in pairs(fast.system or {}) do full.system[k]=v end
    full.connections=fast.connections;full.connection_limit=fast.connection_limit;full.temperatures=fast.temperatures
    for _,key in ipairs({"ports","uplinks"}) do
        local byname={};for _,p in ipairs(fast[key] or {}) do byname[p.name]=p end
        for _,p in ipairs(full[key] or {}) do for k,v in pairs(byname[p.name] or {}) do p[k]=v end end
    end
    return full
end
function fast_snapshot()
    local perf=require "luci.model.myos_liquid"
    output(perf.fast(perf.cached("portmeta")))
end
local function read_acceleration()
    local p=require "luci.model.myos_liquid"
    local old=p.cached("acceleration")
    if old.ts and os.time()-old.ts>=0 and os.time()-old.ts<30 then return old end
    local bus=require("ubus").connect()
    local result={ts=os.time(),ppe={}}
    local function read(method)
        if not bus then return {} end
        local ok,data=pcall(bus.call,bus,"luci.turboacc",method,{})
        return ok and type(data)=="table" and data or {}
    end
    local fp,fc,ppe=read("getFastPathStat"),read("getFullConeStat"),read("getMTKPPEStat")
    result.type=fp.type;result.fullcone=fc.type
    for i=0,(tonumber(ppe.PPE_NUM) or 0)-1 do result.ppe[#result.ppe+1]={id=i,used=tonumber(ppe["BIND_PPE"..i]),total=tonumber(ppe["ALL_PPE"..i])} end
    if bus then bus:close() end
    p.save("acceleration",result);return result
end
function acceleration()
    output(read_acceleration())
end
function snapshot()
    local perf=require "luci.model.myos_liquid"
    local previous=perf.cached("snapshot")
    local fast=perf.fast(previous)
    local explicit=luci.http.formvalue and luci.http.formvalue("fresh")=="1"
    local ttl=explicit and 2 or 25
    if previous.meta_ts and os.time()-previous.meta_ts>=0 and os.time()-previous.meta_ts<ttl and not fast.needs_full then
        return output(merge_fast(previous,fast))
    end
    local fs, json = require "nixio.fs", require "luci.jsonc"
    local uci = require("luci.model.uci").cursor()
    local bus = require("ubus").connect()
    local errors = {}
    local function rd(path)
        local s = perf.read(path)
        return s and s:gsub("%s+$", "") or ""
    end
    local function call(object, method, args)
        if not bus then errors[object] = "无法连接 ubus"; return {} end
        local ok, data = pcall(bus.call, bus, object, method, args or {})
        if not ok or type(data) ~= "table" then errors[object .. "." .. method] = "未获取"; return {} end
        return data
    end
    local function cached(path)
        local data = json.parse(rd(path))
        return type(data) == "table" and data or {}
    end
    local function append(list, value) list[#list + 1] = value end
    local stable=previous.stable_ts and os.time()-previous.stable_ts>=0 and os.time()-previous.stable_ts<300 and not fast.needs_full
    local board = stable and previous.board or call("system", "board")
    local profile = device_profile(board)
    local pictures = {
        n5000="n5000.png", c2000ultra="c2000ultra.png", c2000max="c2000max.png",
        nbcpe688="nbcpe688.png", c8688="c8688.png", c5800688="c5800688.png"
    }
    local info = fast.system
    local interfaces = call("network.interface", "dump").interface or {}
    local devices = call("network.device", "status")
    local result = {board=board, profile=profile, product=pictures[profile], system=info,
        ts=os.time(), sample=tonumber(rd("/proc/uptime"):match("^[%d.]+")), errors=errors,
        interfaces={}, ports={}, uplinks={}, wireless={}, clients={}, modems={}, temperatures={},
        mounts=stable and previous.mounts or (call("luci", "getMountPoints").result or {}),
        stable_ts=stable and previous.stable_ts or os.time(),meta_ts=os.time(),boot=fast.boot,
        hardware=profile=="n5000" and {kind="双网口路由器",usb="USB 3.2"} or nil}
    local cpu = {}
    for n in (rd("/proc/stat"):match("^cpu%s+([^\n]+)") or ""):gmatch("%d+") do append(cpu, tonumber(n)) end
    local total = 0
    for i=1,math.min(#cpu,8) do total=total+cpu[i] end
    if #cpu>=4 then result.cpu={total=total,idle=cpu[4]+(cpu[5] or 0)} end
    result.connections=tonumber(rd("/proc/sys/net/netfilter/nf_conntrack_count"))
    result.connection_limit=tonumber(rd("/proc/sys/net/netfilter/nf_conntrack_max"))
    result.mac=devices["br-lan"] and devices["br-lan"].macaddr
    local bydev, uplinks = {}, {}
    for _, net in ipairs(interfaces) do
        if net.interface ~= "loopback" then
            local item = {name=net.interface, device=net.l3_device or net.device, up=net.up,
                proto=net.proto, uptime=net.uptime, ipv4=net["ipv4-address"] or {},
                ipv6=net["ipv6-address"] or {}, prefixes=net["ipv6-prefix"] or {},
                dns=net["dns-server"] or {}, routes=net.route or {}, pending=net.pending}
            append(result.interfaces,item)
            if item.device then
                bydev[item.device]=bydev[item.device] or {}
                append(bydev[item.device],item)
            end
            for _, route in ipairs(item.routes) do
                if net.up and tonumber(route.mask)==0 and (route.target=="0.0.0.0" or route.target=="::") then
                    local d=item.device
                    if d and d~="br-lan" and d~="lo" and not d:match("^tun") and not d:match("^wg") then
                        uplinks[d]=true
                    end
                end
            end
        end
    end
    local function devinfo(name, role)
        local d=devices[name] or {}
        local base="/sys/class/net/"..name
        local carrier=d.carrier
        if carrier==nil then
            local c=tonumber(rd(base.."/carrier"))
            if c~=nil then carrier=c==1 end
        end
        local link_speed=d.speed
        if not link_speed then
            local n=tonumber(rd(base.."/speed"))
            if n and n>0 then link_speed=tostring(n)..(rd(base.."/duplex")=="half" and "H" or "F") end
        end
        return {name=name,label=port_label(profile,name),role=role,carrier=carrier,up=d.up,speed=link_speed,
            mac=d.macaddr,mtu=d.mtu,networks=bydev[name] or {},
            rx=tonumber(rd(base.."/statistics/rx_bytes")),tx=tonumber(rd(base.."/statistics/tx_bytes"))}
    end
    for name in pairs(uplinks) do append(result.uplinks,devinfo(name)) end
    table.sort(result.uplinks,function(a,b) return a.name<b.name end)
    local portnames = physical_ports(profile,devices)
    local lanmembers={}
    for _,name in ipairs((devices["br-lan"] or {})["bridge-members"] or {}) do lanmembers[name]=true end
    for _,name in ipairs(portnames) do
        if fs.access("/sys/class/net/"..name) then
            local role=lanmembers[name] and "LAN" or "外网"
            if profile=="c2000ultra" and not lanmembers[name] then role="WAN" end
            if name=="port5" and profile=="c5800688" then role=uci:get("network","cpe1","device")==name and "副 5G / PoE" or role
            elseif profile=="nbcpe688" then
                role=name=="port5" and "2.5G PoE / ODU" or role
            elseif name=="port5" and profile=="c8688" then role=lanmembers[name] and "LAN" or "WAN" end
            append(result.ports,devinfo(name,role))
        end
    end
    local names,clients={},{}
    result.client_sources={dhcp=fs.access("/tmp/dhcp.leases") and true or false,
        arp=fs.access("/proc/net/arp") and true or false}
    result.leases={}
    for line in rd("/tmp/dhcp.leases"):gmatch("[^\n]+") do
        local expires,mac,ip,name=line:match("^(%d+)%s+(%S+)%s+(%S+)%s+(%S+)")
        expires=tonumber(expires)
        if mac and expires and (expires==0 or expires>result.ts) then
            local key=mac:lower()
            local lease={name=name~="*" and name or nil,ip=ip,mac=key,expires=expires}
            names[key]=lease
            append(result.leases,lease)
            clients[key]={mac=key,ip=ip,name=lease.name,kind="DHCP",state="租约记录",lease_expires=expires}
        end
    end
    -- Do not treat ARP cache entries or DHCP leases as proof of present reachability.
    local lanbridges={["br-lan"]=true}
    for dev,d in pairs(devices) do
        if d.type=="bridge" or d.devtype=="bridge" then lanbridges[dev]=not uplinks[dev] end
    end
    for line in rd("/proc/net/arp"):gmatch("[^\n]+") do
        local ip,flags,mac,dev=line:match("^(%S+)%s+%S+%s+(%S+)%s+(%S+)%s+%S+%s+(%S+)")
        if lanbridges[dev] and flags=="0x2" and mac~="00:00:00:00:00:00" then
            local key=mac:lower()
            local n=names[key] or {}
            clients[key]={mac=key,ip=ip,name=n.name,kind="LAN",state="邻居记录",device=dev,lease_expires=n.expires}
        end
    end
    local radios=call("network.wireless","status")
    for radio,r in pairs(radios) do
        for _,i in ipairs(r.interfaces or {}) do
            if i.ifname then
                local w=call("iwinfo","info",{device=i.ifname})
                local stations=call("iwinfo","assoclist",{device=i.ifname}).results or {}
                local c=i.config or {}
                append(result.wireless,{radio=radio,name=i.ifname,up=r.up and not c.disabled,
                    ssid=w.ssid or c.ssid,channel=w.channel,frequency=w.frequency,
                    htmode=w.htmode or (r.config or {}).htmode,encryption=w.encryption,
                    clients=#stations,mode=w.mode})
                for _,s in ipairs(stations) do
                    if s.mac then
                        local key=s.mac:lower()
                        local n=names[key] or {}
                        clients[key]={mac=key,name=n.name,ip=n.ip,kind=i.ifname,state="无线已关联",
                            signal=s.signal,rx=s.rx and s.rx.rate,tx=s.tx and s.tx.rate,
                            ssid=w.ssid or c.ssid,device=i.ifname,lease_expires=n.expires}
                    end
                end

            end
        end
    end
    table.sort(result.wireless,function(a,b) return a.name<b.name end)
    for _,c in pairs(clients) do append(result.clients,c) end
    table.sort(result.clients,function(a,b) return a.mac<b.mac end)
    local cpu_temp=tonumber(rd("/sys/class/thermal/thermal_zone0/temp"))
    if cpu_temp then table.insert(result.temperatures,1,{name="CPU",value=cpu_temp/1000}) end
    local operators={["46000"]="中国移动",["46002"]="中国移动",["46007"]="中国移动",["46008"]="中国移动",
        ["46001"]="中国联通",["46006"]="中国联通",["46009"]="中国联通",
        ["46003"]="中国电信",["46005"]="中国电信",["46011"]="中国电信",["46015"]="中国广电"}
    local cachepath="/tmp/infocd/cache/cpestatus"
    local cache=cached(cachepath)
    local stamp=fs.stat(cachepath,"mtime")
    local entries=profile=="n5000" and {} or (cache.result or {})
    local native=profile=="nbcpe688" and nbcpe_cache_record() or nil
    if native then
        entries={{status={name="AK68",model=native.model,revision=native.revision,
            isp=native.isp,isp_company=native.operator,mode=native.mode,band=native.band,
            rsrp=native.rsrp,rsrq=native.rsrq,sinr=native.sinr,pci=native.pci,
            cell=native.cell,earfcn=native.earfcn,model_temp=native.temp,
            simno=native.simno,sim_label=native.sim_label,iccid=native.iccid,
            CPIN=native.simstatus,apn=native.apn,device=native.dev,qos=native.qos}}}
        stamp=native.cache_age and os.time()-native.cache_age or nil
    end
    for _,entry in ipairs(entries) do
        local s=entry.status
        if type(s)=="table" and s.name then
            local sim_name=tostring(s.sim_name or ""):match("^%s*(.-)%s*$")
            if sim_name=="unknown" then sim_name="" end
            local path=sim_name_cache(s.iccid)
            local cloud=path and cached(path) or {}
            if cloud.iccid==tostring(s.iccid) and cloud.name then sim_name=cloud.name end
            append(result.modems,{id=s.name,label=s.name=="cpe1" and "副 5G" or "内置蜂窝",
                qos=s.qos or perf.qos(s,"infocd cache"),model=s.model,firmware=s.revision,operator=operators[s.isp] or s.isp_company or s.isp,
                rat=s.mode,band=s.band,rsrp=tonumber(s.rsrp),rsrq=tonumber(s.rsrq),sinr=tonumber(s.sinr),
                pci=s.pci,cell=s.cell or s.cellid,arfcn=s.earfcn,temperature=tonumber(s.model_temp),
                sim=s.simno,sim_label=s.sim_label,sim_name=sim_name,sim_type=s.simtype,sim_id=path and tostring(s.iccid),
                sim_name_status=cloud.error or (path and "未读取云端名称" or nil),pin=s.CPIN,apn=s.apn,
                device=s.device or uci:get("network",s.name,"device"),age=stamp and math.max(0,os.time()-stamp)})
        end
    end
    -- QModem boards publish caches themselves. Reading the files does not request AT commands.
    if #result.modems==0 and profile~="n5000" then
        for _,record in ipairs(qmodem_cache_records()) do
            local id,f=record.id,record.fields
            local m={id=id,qos=perf.qos(f,"QModem cache"),label=profile=="c2000ultra" and "内置 5G" or "蜂窝模组",
                device=uci:get("qmodem",id,"network"),apn=uci:get("qmodem",id,"apn"),
                age=record.updated and math.max(0,os.time()-record.updated)}
            local fields={name="model",revision="firmware",rsrp="rsrp",rsrq="rsrq",sinr="sinr",
                band="band",temperature="temperature",iccid="sim_id",["sim slot"]="sim",["sim name"]="sim_name",
                ["cell id"]="cell",apn="apn"}
            for source,target in pairs(fields) do if f[source]~=nil then m[target]=f[source] end end
            m.rat=f.network_mode or f.mode
            m.pci=f["physical cell id"] or f.pci
            m.arfcn=f.arfcn or f.earfcn
            m.pin=f["sim status"] or f.cpin
            local plmn=tostring(f.mcc or f.mmc or "")..tostring(f.mnc or "")
            local operator=f.operator or f.isp
            m.operator=operators[plmn] or operators[tostring(operator)] or operator
            for _,key in ipairs({"temperature","rsrp","rsrq","sinr"}) do
                m[key]=tonumber(tostring(m[key] or ""):match("%-?%d+%.?%d*"))
            end
            if profile=="c2000ultra" then
                local state=cached("/var/run/c2000ultra-sim-select.status.json")
                m.selected_sim=uci:get("cpesel","sim","cur")
                if state.modem==id then m.sim=m.sim or state.actual;m.pin=m.pin or state.cpin end
                m.apn=m.apn or uci:get("embed_simcard","general","apn")
                local types={}
                for value in (uci:get("cpesel","sim","stype") or ""):gmatch("[^,]+") do append(types,value) end
                local kind=types[tonumber(m.selected_sim)]
                m.selected_sim_type=kind=="0" and "普通卡" or kind=="4" and "云卡" or nil
            end
            if profile=="c2000max" then
                -- QModem's cached SIM Slot is the modem channel, not MAX's
                -- logical external/internal slot. The refresh returns actual.
                local selection=cached("/var/run/c2000max-sim-select.status.json")
                m.sim=selection.modem==id and selection.actual or nil
                m.sim_label=m.sim and ({["1"]="SIM 卡 1",["2"]="SIM 卡 2",["3"]="内置卡 1"})[tostring(m.sim)] or nil
                m.label="内置 5G"
                m.selected_sim=uci:get("cpesel","sim","cur")
                m.selected_sim_label=({["1"]="SIM 卡 1",["2"]="SIM 卡 2",["3"]="内置卡 1"})[m.selected_sim]
            end
            if m.pin then m.pin=tostring(m.pin):upper() end
            local path=sim_name_cache(m.sim_id)
            local cloud=path and cached(path) or {}
            if cloud.iccid==tostring(m.sim_id) and cloud.name then m.sim_name=cloud.name end
            m.sim_name_status=cloud.error or (path and "未读取云端名称" or nil)
            append(result.modems,m)
        end
    end
    result.acceleration=read_acceleration()
    if bus then bus:close() end
    -- Both sample routes use one counter source and a matching monotonic timestamp.
    result=merge_fast(result,perf.fast(result))
    perf.save("snapshot",result)
    local meta={profile=profile,boot=result.boot,ports={},uplinks={}}
    for _,k in ipairs({"ports","uplinks"}) do for _,p in ipairs(result[k]) do meta[k][#meta[k]+1]={name=p.name,label=p.label} end end
    perf.save("portmeta",meta)
    output(result)
end
