-- MYOS-NEW 1.2.1. Passive readers: no shell, AT command, daemon or flash write.
local M={}
local fs,json=require "nixio.fs",require "luci.jsonc"
local directory="/tmp/myos-new-v121"
function M.read(path,limit)
 local n=fs.stat(path,"size")
 if n and n>(limit or 1048576) then return "" end
 return fs.readfile(path) or ""
end
function M.json(path)
 if not path then return {} end
 local ok,v=pcall(json.parse,M.read(path))
 return ok and type(v)=="table" and v or {}
end
function M.path(name)
 if not name:match("^[%w_-]+$") then return nil end
 local st=fs.lstat(directory)
 if not st then fs.mkdir(directory,"rwx------");st=fs.lstat(directory) end
 if not st or st.type~="dir" or st.uid~=0 then return nil end
 fs.chmod(directory,"rwx------")
 return directory.."/"..name..".json"
end
function M.cached(name)
 local p=M.path(name)
 if not p then return {} end
 local st=fs.lstat(p)
 if st and (st.type~="reg" or st.uid~=0) then return {} end
 return M.json(p)
end
function M.save(name,value)
 local path=M.path(name);if not path then return false end
 local nixio=require "nixio"
 local tmp=path.."."..tostring(nixio.getpid())..".new"
 local ok,body=pcall(json.stringify,value);if not ok then return false end
 local file=nixio.open(tmp,"w","rw-------");if not file then return false end
 local written=file:writeall(body);file:close()
 if not written then fs.remove(tmp);return false end
 fs.chmod(tmp,"rw-------")
 if not fs.rename(tmp,path) then fs.remove(tmp);return false end
 return true
end
function M.clean(v)
 return (tostring(v or ""):gsub("<[^>]*>"," "):gsub("&nbsp;"," "):gsub("%z",""):gsub("^%s+",""):gsub("%s+$",""))
end
function M.number(v)
 local n=tonumber(M.clean(v):match("%-?%d+%.?%d*"))
 if n and n==n and n~=math.huge and n~=-math.huge then return n end
end
function M.rate(v,unit)
 if v==nil then return nil end
 local raw=M.clean(v):lower():gsub(",","")
 local n=tonumber(raw:match("^%s*(%d+%.?%d*)"))
 if not n or n==math.huge then return nil end
 local suffix=raw:match("^%s*%d+%.?%d*%s*(.-)%s*$")
 if suffix=="" then suffix=(unit or ""):lower() end
 suffix=suffix:gsub("%s","")
 local factors={mbps=1,["mbit/s"]=1,["mbits/s"]=1,gbps=1000,["gbit/s"]=1000,kbps=.001,["kbit/s"]=.001,["kbits/s"]=.001,bps=.000001,["bit/s"]=.000001}
 return factors[suffix] and n*factors[suffix] or nil
end
function M.qci(v)
 local s=M.clean(v)
 if s:upper():find("CQI",1,true) then return nil end
 local n=tonumber(s:match("^%d+$") or s:upper():match("QCI[^%d]*(%d+)"))
 if n and n>=1 and n<=255 and n%1==0 then return n end
end
function M.qos(fields,source)
 local f={};for k,v in pairs(fields or {}) do f[tostring(k):lower()]=v end
 -- MT5700 infocd keeps raw DSAMBR values in kbps (AT manual 16.17).
 -- QModem and AK68 may already include units; explicit suffixes always win.
 local dl=f.nr5g_ambr_dl or f["ambr dl"] or f.ambr_dl
 local ul=f.nr5g_ambr_ul or f["ambr ul"] or f.ambr_ul
 if source=="AK68 line cache" then dl=f.downlink;ul=f.uplink end
 local five=tonumber(f["5qi"] or f.fiveqi)
 if five and (five<1 or five>255 or five%1~=0) then five=nil end
 local mt5700=source=="infocd cache" and tostring(f.model or ""):upper():find("MT5700",1,true)
 local unit=f.ambr_unit or (mt5700 and "kbps" or "Mbps")
 -- This specific producer's CQI key comes from CGEQOSRDP/C5GQOSRDP,
 -- not radio channel quality. Never alias CQI for other modem adapters.
 local qci=M.qci(f.qci)
 if not qci and mt5700 then qci=M.qci(f.cqi) end
 return {down_mbps=M.rate(dl,unit),up_mbps=M.rate(ul,unit),qci=qci,fiveqi=five,source=source}
end
function M.nbcpe()
 -- Direct reader of the supplied myos-nbcpe-status 30-line schema. Its Python
 -- wrapper would fork additional processes; no need to run it on each sample.
 local raw=M.read("/tmp/cpe_cell-AK68.file",65536):gsub("\r","")
 local lines={};for line in (raw.."\n"):gmatch("([^\n]*)\n") do lines[#lines+1]=M.clean(line) end
 if #lines<30 or (lines[1] or ""):find("未检测到信息",1,true) then return nil end
 local keys={"manufacturer","name","revision","temperature","cached_at","sim_label","ISP","IMEI","IMSI","ICCID","phone","network_mode","signal_percent","RSSI","RSRQ","RSRP","SINR","plmn","LAC","CID","Band","ARFCN","PCI","APN","downlink","uplink","QCI","carrier_aggregation","ca2","ca3"}
 local f={};for i,k in ipairs(keys) do f[k]=lines[i] end
 local m={model=f.name,revision=f.revision,isp=f.plmn:gsub("[%s/]",""),operator=f.ISP,mode=f.network_mode,band=f.Band,rsrp=M.number(f.RSRP),rsrq=M.number(f.RSRQ),sinr=M.number(f.SINR),temp=M.number(f.temperature),pci=f.PCI,cell=f.CID,earfcn=f.ARFCN,
 simno=({["外置SIM卡"]="1",["内置SIM1"]="2"})[f.sim_label],sim_label=f.sim_label,iccid=f.ICCID,simstatus=f.ICCID~="" and "READY" or nil,apn=f.APN,dev="port5",qos=M.qos(f,"AK68 line cache")}
 local stamp=fs.stat("/tmp/cpe_cell-AK68.file","mtime");m.cache_age=stamp and math.max(0,os.time()-stamp)
 return m
end
function M.fast(meta)
 local now=os.time();local uptime=tonumber(M.read("/proc/uptime"):match("^[%d.]+"))
 local result={lite=true,ts=now,sample=uptime,system={uptime=uptime,localtime=now},ports={},uplinks={},temperatures={}}
 result.boot=M.read("/proc/sys/kernel/random/boot_id"):gsub("%s+$","")
 local cpu={};for n in (M.read("/proc/stat"):match("^cpu%s+([^\n]+)") or ""):gmatch("%d+") do cpu[#cpu+1]=tonumber(n) end
 local total=0;for i=1,math.min(#cpu,8) do total=total+cpu[i] end
 if #cpu>=4 then result.cpu={total=total,idle=cpu[4]+(cpu[5] or 0)} end
 local mem={};local names={MemTotal="total",MemAvailable="available",MemFree="free",Buffers="buffered",Cached="cached"}
 for k,v in M.read("/proc/meminfo"):gmatch("([%w_]+):%s+(%d+)") do if names[k] then mem[names[k]]=tonumber(v)*1024 end end
 result.system.memory=mem;result.system.load={}
 for n in M.read("/proc/loadavg"):gmatch("[%d.]+") do result.system.load[#result.system.load+1]=tonumber(n)*65536;if #result.system.load==3 then break end end
 result.connections=tonumber(M.read("/proc/sys/net/netfilter/nf_conntrack_count"));result.connection_limit=tonumber(M.read("/proc/sys/net/netfilter/nf_conntrack_max"))
 local t=tonumber(M.read("/sys/class/thermal/thermal_zone0/temp"));if t then result.temperatures={{name="CPU",value=t/1000}} end
 local stats={}
 for name,rest in M.read("/proc/net/dev"):gmatch("([%w_.%-]+):%s*([^\n]+)") do
  local values={};for v in rest:gmatch("%d+") do values[#values+1]=tonumber(v) end
  stats[name]={rx=values[1],tx=values[9]}
 end
 local function dev(n)
  if not tostring(n):match("^[%w_.:%-]+$") then return nil end
  local base="/sys/class/net/"..n
  local v=stats[n] or {rx=tonumber(M.read(base.."/statistics/rx_bytes")),tx=tonumber(M.read(base.."/statistics/tx_bytes"))}
  local c=tonumber(M.read(base.."/carrier"));local speed=tonumber(M.read(base.."/speed"))
  local d={name=n,rx=v.rx,tx=v.tx}
  if c==0 or c==1 then d.carrier=c==1 end
  if speed and speed>0 then d.speed=tostring(speed)..(M.read(base.."/duplex"):match("half") and "H" or "F") end
  return d
 end
 for _,k in ipairs({"ports","uplinks"}) do for _,p in ipairs(meta and meta[k] or {}) do local v=dev(p.name);if v then result[k][#result[k]+1]=v end end end
 result.needs_full=not meta or not meta.profile or meta.boot~=result.boot or nil
 return result
end
return M
