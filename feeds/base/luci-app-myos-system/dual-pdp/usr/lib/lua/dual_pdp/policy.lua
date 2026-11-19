local M = {MASK='0x30000000', KEEP='0xcfffffff', TABLE='dual_pdp_smart', VERSION='1.10.1-r2'}
local expression=require('dual_pdp.expression')
local function bad(message) error(message,0) end
local function integer(value,min,max,label)
 local n=tonumber(value)
 if not n or n~=math.floor(n) or n<min or n>max then bad(label..'无效') end
 return n
end
function M.ipv4(value)
 if type(value)~='string' then return nil end
 local a,b,c,d=value:match('^(%d+)%.(%d+)%.(%d+)%.(%d+)$')
 if not a then return nil end
 local n=0
 for _,part in ipairs({a,b,c,d}) do
  if (#part>1 and part:sub(1,1)=='0') or tonumber(part)>255 then return nil end
  n=n*256+tonumber(part)
 end
 return n
end
local function iptext(n)
 return math.floor(n/16777216)..'.'..(math.floor(n/65536)%256)..'.'..(math.floor(n/256)%256)..'.'..(n%256)
end
local function octet_patterns(value)
 if not value:match('^re:%^.+%$$') or #value>160 then bad('IPv4 正则须以 ^ 开头、$ 结尾') end
 local body=value:sub(5,-2):gsub('%[%.%]','/'):gsub('\\%.','/')
 if body:find('[^%d%.%[%]%(%)%-%|%+%?%*%^/]') or value:find('/',1,true) then bad('IPv4 正则只支持逐段匹配') end
 local parts={body:match('^([^/]+)/([^/]+)/([^/]+)/([^/]+)$')}
 if #parts~=4 then bad('IPv4 正则须用 [.] 或 \\. 分隔四段') end
 return parts
end
local range_cache,range_count={},0
local function ip_condition(value,source)
 if value:sub(1,3)~='re:' then return (source and 'ip saddr ' or 'ip daddr ')..value end
 local terms={}
 for i,pattern in ipairs(octet_patterns(value)) do
  local ranges=range_cache[pattern]
  if not ranges then
   -- octet_patterns permits only decimal regex metacharacters, never quotes.
   local f=assert(io.popen("/usr/sbin/dual-pdp-smart --octet-ranges '"..pattern.."'"))
   ranges=f:read('*l');f:close()
   if not ranges or not ranges:match('^[%d,%-]+$') then bad('IPv4 正则无效或无法匹配 0–255') end
   if range_count>=256 then range_cache={};range_count=0 end
   range_cache[pattern]=ranges;range_count=range_count+1
  end
  if ranges~='0-255' then terms[#terms+1]=string.format('@nh,%d,8 { %s }',(source and 96 or 128)+(i-1)*8,ranges) end
 end
 return table.concat(terms,' ')
end
function M.cidr(value)
 if value==nil or value=='' then return '' end
 if type(value)~='string' then bad('IPv4 网段无效') end
 if value:sub(1,3)=='re:' then octet_patterns(value);return value end
 if value:find('*',1,true) then
  local parts={value:match('^([%d*]+)%.([%d*]+)%.([%d*]+)%.([%d*]+)$')}
  if #parts~=4 then bad('IPv4 通配符示例：192.0.2.*') end
  local bits=32
  for i,part in ipairs(parts) do
   if part=='*' then bits=math.min(bits,(i-1)*8);parts[i]='0'
   elseif not M.ipv4('0.0.0.'..part) then bad('IPv4 地址段无效')
   elseif bits<32 then
    local re=value:gsub('%*','[0-9]+'):gsub('%.','[.]')
    return M.cidr('re:^'..re..'$')
   end
  end
  value=table.concat(parts,'.')..'/'..bits
 end
 local ip,bits=value:match('^([^/]+)/(%d+)$')
 if not ip then ip=value;bits=32 end
 local n=M.ipv4(ip);bits=integer(bits,0,32,'IPv4 掩码')
 if not n then bad('IPv4 地址无效') end
 return iptext(math.floor(n/2^(32-bits))*2^(32-bits))..'/'..bits
end
function M.mac(value)
 if value==nil or value=='' then return '' end
 if type(value)~='string' then bad('设备 MAC 无效') end
 value=value:lower()
 if not value:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$') or value=='00:00:00:00:00:00' or tonumber(value:sub(1,2),16)%2~=0 then bad('设备 MAC 无效') end
 return value
end
local function words(value)
 if value==nil or value=='' then return {} end
 if type(value)=='table' then return value end
 if type(value)~='string' or #value>2048 then bad('规则列表过长或无效') end
 local out={};for part in value:gmatch('[^,%s]+') do out[#out+1]=part end;return out
end
-- Network conditions are compiled once on save. Wildcards do not cross labels.
local function domain_expression(value)
 local kind,body=value:match('^(%a+):(.+)$')
 if kind=='re' then
  if #body>160 or body:find('[%s%c{},/#]') or body:match('\\%d') then bad('域名正则最多 160 字符，不支持反向引用、花括号或分隔符') end
  return body
 end
 body=kind=='glob' and body or value
 if kind and kind~='glob' then bad('域名模式前缀无效') end
 if #body>120 or not body:find('.',1,true) or body:find('[^%w%-%.*?]') or body:find('..',1,true) or body:match('^%.') or body:match('%.$') then bad('域名通配符无效') end
 local pattern='^'..body:gsub('%.','\\.'):gsub('%*','[^.]*'):gsub('%?','[^.]')..'$'
 if #pattern>192 then bad('域名模式过长') end
 return pattern
end
local function is_pattern(value)
 return value:match('^re:') or value:match('^glob:') or value:find('[*?]')
end

function M.domains(value)
 local out,seen={},{}
 for _,domain in ipairs(words(value)) do
  if type(domain)~='string' then bad('域名无效') end
  if is_pattern(domain) and not domain:match('^%*%.[%w%.%-]+$') then
   domain_expression(domain)
  else
  domain=domain:lower():gsub('^%*%.',''):gsub('%.$','')
  if #domain>253 or not domain:find('.',1,true) or domain:find('..',1,true) or domain:find('[^a-z0-9.%-]') or M.ipv4(domain) then bad('请填写域名，不包含协议或路径') end
  for label in domain:gmatch('[^.]+') do if #label>63 or not label:match('^[a-z0-9]') or not label:match('[a-z0-9]$') then bad('域名标签无效') end end
  end
  if not seen[domain] then out[#out+1]=domain;seen[domain]=true end
 end
 if #out>8 then bad('每条规则最多 8 个域名') end
 return out
end
local DSCP={EF=46,CS0=0,CS1=8,CS2=16,CS3=24,CS4=32,CS5=40,CS6=48,CS7=56}
for a=1,4 do for b=1,3 do DSCP['AF'..a..b]=8*a+2*b end end
function M.dscp(value)
 local out,seen={},{}
 for _,part in ipairs(words(value)) do
  local n=integer(DSCP[tostring(part):upper()] or part,0,63,'DSCP')
  if not seen[n] then out[#out+1]=n;seen[n]=true end
 end
 if #out>8 then bad('每条规则最多 8 个 DSCP 值') end
 table.sort(out);return out
end
function M.ports(value)
 local ranges={}
 for _,part in ipairs(words(value)) do
  local a,b=tostring(part):match('^(%d+)%-(%d+)$')
  if not a then a=part;b=part end
  a=integer(a,1,65535,'端口');b=integer(b,1,65535,'端口')
  if a>b then bad('端口范围顺序无效') end
  ranges[#ranges+1]={a,b}
 end
 if #ranges>16 then bad('每条规则最多 16 个端口/范围') end
 table.sort(ranges,function(a,b)return a[1]<b[1] end)
 local out={};for _,r in ipairs(ranges)do out[#out+1]=r[1]==r[2] and tostring(r[1]) or r[1]..'-'..r[2] end
 return out
end
local function boolean(v,fallback)
 if v==nil then return fallback end
 if type(v)=='boolean' then return v end
 if v=='1' then return true elseif v=='0' then return false end
 bad('开关值无效')
end
local function device_list(input)
 local macs,seen={},{}
 for _,v in ipairs(words(input))do
  local mac=M.mac(v)
  if mac=='' then bad('设备的 MAC 不能为空') end
  if not seen[mac] then macs[#macs+1]=mac;seen[mac]=true end
 end
 if #macs>32 then bad('每组最多选择 32 台设备') end
 table.sort(macs)
 return macs,seen
end
function M.validate(input)
 if type(input)~='table' or type(input.rules)~='table' or #input.rules>32 then bad('最多配置 32 条规则') end
 local profile=input.profile or ''
 if type(profile)~='string' or (profile~='' and (#profile~=64 or not profile:match('^%x+$'))) then bad('特征库标识无效') end
 local out={enabled=boolean(input.enabled,false),profile=profile:lower(),rules={},categories={},
  routing_mode='rules',learning=true,heuristics=false}
 out.priority_macs=device_list(input.priority_macs)
 -- Existing selections predate the independent switch.
 out.priority_enabled=boolean(input.priority_enabled,#out.priority_macs>0)
 local categories=input.categories or {}
 if type(categories)~='table' or #categories>128 then bad('分类列表无效') end
 local used={}
 for _,category in ipairs(categories)do
  local id=integer(category.id,1,2147483647,'分类标识')
  if used[id] then bad('分类重复') end;used[id]=true
  out.categories[#out.categories+1]={id=id,target=integer(category.target,0,2,'分类出口')}
 end
 if #out.categories>0 and profile=='' then bad('请先选择当前特征库') end
 local priorities={}
 for index,r in ipairs(input.rules)do
  if type(r)~='table' then bad('规则格式无效') end
  local name=r.name or ('规则 '..index)
  if type(name)~='string' or #name>96 or name:find('[%z\1-\31]') then bad('规则名称无效') end
  local priority=integer(r.priority or index*10,1,999,'优先级')
  if priorities[priority] then bad('规则优先级不能重复') end;priorities[priority]=true
  local proto=r.protocol or 'all'
  if proto~='all' and proto~='tcp' and proto~='udp' and proto~='icmp' then bad('协议无效') end
  local ports=M.ports(r.ports)
  if #ports>0 and proto~='tcp' and proto~='udp' then bad('端口条件需要选择 TCP 或 UDP') end
  local fallback=r.fallback or 'primary'
  if fallback~='primary' and fallback~='block' then bad('断路处理方式无效') end
  local match_type=r.match_type or 'network'
  if match_type~='network' and match_type~='category' and match_type~='application' then bad('规则类型无效') end
  local match_id=match_type=='network' and 0 or integer(r.match_id,1,2147483647,'分类或应用')
  if match_type~='network' and profile=='' then bad('请先选择当前特征库') end
  local expr=r.expression or ''
  if expr~='' then
   if match_type~='network' then bad('组合表达式仅用于网络规则') end
   expression.parse(expr)
  end
  out.rules[#out.rules+1]={id='r'..priority,name=name,priority=priority,enabled=boolean(r.enabled,true),target=integer(r.target or 1,1,2,'目标会话'),fallback=fallback,
   match_type=match_type,match_id=match_id,expression=expr,protocol=proto,mac=M.mac(r.mac),source=M.cidr(r.source),destination=M.cidr(r.destination),ports=ports,dscp=M.dscp(r.dscp),domains=M.domains(r.domains)}
 end
 table.sort(out.rules,function(a,b)return a.priority<b.priority end)
 return out
end
function M.from_uci(c)
 local input={enabled=c:get('dual_pdp','main','smart_enabled') or '0',
  profile=c:get('dual_pdp','main','feature_profile') or '',rules={},categories={},
  priority_enabled=c:get('dual_pdp','main','priority_enabled'),
  priority_macs=c:get('dual_pdp','main','priority_macs')}
 c:foreach('dual_pdp','rule',function(r)input.rules[#input.rules+1]=r end)
 c:foreach('dual_pdp','category',function(r)input.categories[#input.categories+1]={id=r.id,target=r.target} end)
 return M.validate(input)
end
-- Only unicast IPv6 literals can become learned nft set elements.
function M.ipv6(value,any_prefix)
 if type(value)~='string' or #value>39 or not value:match('^[%x:]+$') then return false end
 local compress=value:find('::',1,true)
 if value:find(':::',1,true) or (compress and value:find('::',compress+2,true)) then return false end
 if (value:sub(1,1)==':' and value:sub(1,2)~='::') or (value:sub(-1)==':' and value:sub(-2)~='::') then return false end
 local count=0
 for group in value:gmatch('[^:]+') do if #group>4 then return false end;count=count+1 end
 return (compress and count<8 or count==8 and not compress) and (any_prefix==true or value:match('^[23]')~=nil or value:lower():match('^f[cd]')~=nil)
end
local function quoted(s) return '"'..s..'"' end
local function list(values) return '{ '..table.concat(values,', ')..' }' end
local function verdict(target,up,fallback)
 if target==2 and not up and fallback~='block' then target=1 end
 local mark=target==2 and '0x20000000' or '0x10000000'
 return 'counter ct mark set (ct mark & '..M.KEEP..') | '..mark..' meta mark set (meta mark & '..M.KEEP..') | '..mark..' return'
end
local function dual_domain(r)
 return r.enabled and r.match_type=='network' and #r.domains>0 and r.source=='' and r.destination==''
end
local function pattern_directive(r,domain)
 local hex=domain_expression(domain):gsub('.',function(ch)return string.format('%02x',ch:byte())end)
 return 'pdu-nftset='..r.priority..(dual_domain(r) and ',46,' or ',')..hex
end

function M.check_network_rules(settings)
 local lines={}
 for _,r in ipairs(settings.rules) do
  if r.source~='' then ip_condition(r.source,true) end
  if r.destination~='' then ip_condition(r.destination,false) end
  for _,domain in ipairs(r.domains) do
   if is_pattern(domain) then
    lines[#lines+1]=pattern_directive(r,domain)
   end
  end
 end
 return table.concat(lines,'\n')..'\n'
end
local function conditions(r,six)
  local match={}
  if r.mac~='' then match[#match+1]='ether saddr '..r.mac end
  if r.source~='' then match[#match+1]=ip_condition(r.source,true) end
  if r.destination~='' then match[#match+1]=ip_condition(r.destination,false) end
  if r.protocol~='all' then match[#match+1]='meta l4proto '..(six and r.protocol=='icmp' and 'ipv6-icmp' or r.protocol) end
  if #r.ports>0 then match[#match+1]=r.protocol..' dport '..list(r.ports) end
  if #r.dscp>0 then match[#match+1]=(six and 'ip6 dscp ' or 'ip dscp ')..list(r.dscp) end
  if #r.domains>0 then match[#match+1]=(six and 'ip6 daddr @dns6_' or 'ip daddr @dns_')..r.id end
  return table.concat(match,' ')
end

function M.priority_devices(settings)
 return settings.priority_enabled and settings.priority_macs or {}
end

local function device_sets(lines,settings,devices)
 local groups={priority_clients=M.priority_devices(settings),assigned1={},assigned2={}}
 for _,device in ipairs(devices or {}) do
  local group=groups['assigned'..device.pdp]
  if group then group[#group+1]=M.mac(device.mac) end
 end
 for _,name in ipairs({'priority_clients','assigned1','assigned2'}) do
  local values=groups[name]
  lines[#lines+1]='set '..name..' { type ether_addr; '..(#values>0 and 'elements = '..list(values)..'; ' or '')..'}'
 end
end

local function device_rules(lines,up)
 lines[#lines+1]='ether saddr @priority_clients '..verdict(1,true)..' comment "priority"'
 lines[#lines+1]='ether saddr @assigned1 '..verdict(1,true)..' comment "device-1"'
 lines[#lines+1]='ether saddr @assigned2 '..verdict(2,up)..' comment "device-2"'
end

function M.render(settings,context)
 assert(context.lan_device:match('^[%w_.%-]+$'),'Invalid LAN interface')
 local lines={'table inet '..M.TABLE..' {'}
 local default_pdu=#M.priority_devices(settings)>0 and 2 or 1
 local advanced=expression.compile(settings.rules,context)
 local dns=advanced.dns
 for _,line in ipairs(advanced.sets) do lines[#lines+1]=line end
 device_sets(lines,settings,context.devices)
 lines[#lines+1]='set learned1 { type ether_addr . ipv4_addr . inet_proto . inet_service; flags timeout; timeout 10m; size 1024; }'
 lines[#lines+1]='set learned2 { type ether_addr . ipv4_addr . inet_proto . inet_service; flags timeout; timeout 10m; size 1024; }'
 for _,r in ipairs(settings.rules)do
  if r.enabled and #r.domains>0 then
   lines[#lines+1]='set dns_'..r.id..' { type ipv4_addr; flags timeout; timeout 1h; size 4096; }'
   local sets='4#inet#'..M.TABLE..'#dns_'..r.id
   if dual_domain(r) then
    lines[#lines+1]='set dns6_'..r.id..' { type ipv6_addr; flags timeout; timeout 1h; size 4096; }'
    sets=sets..',6#inet#'..M.TABLE..'#dns6_'..r.id
   end
   for _,domain in ipairs(r.domains) do
    if is_pattern(domain) then dns[#dns+1]=pattern_directive(r,domain)
    else dns[#dns+1]='nftset=/'..domain..'/'..sets end
   end
  end
  if r.enabled and r.match_type~='network' then
   lines[#lines+1]='set app_'..r.id..' { type ether_addr . ipv4_addr . inet_proto . inet_service; flags timeout; timeout 10m; size 1024; }'
   lines[#lines+1]='set cls_'..r.id..' { type ipv4_addr; flags timeout; timeout 1h; size 4096; }'
   for _,domain in ipairs((context.selector_domains or {})[r.id] or {})do dns[#dns+1]='nftset=/'..domain..'/4#inet#'..M.TABLE..'#cls_'..r.id end
  end
 end
 for _,r in ipairs(context.category_rules or {})do
  lines[#lines+1]='set cat_'..r.id..' { type ipv4_addr; flags timeout; timeout 1h; size 4096; }'
  for _,domain in ipairs(r.domains)do dns[#dns+1]='nftset=/'..domain..'/4#inet#'..M.TABLE..'#cat_'..r.id end
 end
 lines[#lines+1]='chain choose {'
 device_rules(lines,context.secondary_up)
 for _,r in ipairs(settings.rules)do if r.enabled then
  local common=conditions(r,false)
  if r.match_type=='network' then
   for _,term in ipairs(advanced.paths[r.id] and advanced.paths[r.id][4] or {''}) do
     lines[#lines+1]=common..(term=='' and '' or ' '..term)..' '..verdict(r.target,context.secondary_up,r.fallback)..' comment "'..r.id..'"'
    end
  elseif not context.profile_mismatch then
   lines[#lines+1]=common..' meta l4proto { tcp, udp } ether saddr . ip daddr . meta l4proto . th dport @app_'..r.id..' '..verdict(r.target,context.secondary_up,r.fallback)..' comment "'..r.id..'"'
   lines[#lines+1]=common..' ip daddr @cls_'..r.id..' '..verdict(r.target,context.secondary_up,r.fallback)..' comment "'..r.id..'-dns"'
  end
 end end
 for _,r in ipairs(context.category_rules or {})do lines[#lines+1]='ip daddr @cat_'..r.id..' '..verdict(r.target,context.secondary_up) end
 lines[#lines+1]='meta l4proto { tcp, udp } ether saddr . ip daddr . meta l4proto . th dport @learned1 '..verdict(1,true)
 lines[#lines+1]='meta l4proto { tcp, udp } ether saddr . ip daddr . meta l4proto . th dport @learned2 '..verdict(2,context.secondary_up)
 lines[#lines+1]=(default_pdu==2 and verdict(2,context.secondary_up) or 'return')..' comment "default"\n}'
 lines[#lines+1]='chain prerouting { type filter hook prerouting priority -80; policy accept;'
 lines[#lines+1]='iifname != '..quoted(context.lan_device)..' return'
 lines[#lines+1]='meta nfproto ipv6 jump choose6'
 lines[#lines+1]='meta nfproto != ipv4 return'
 lines[#lines+1]='fib daddr type local return'
 lines[#lines+1]='ip daddr '..M.cidr(context.lan_cidr)..' return'
 lines[#lines+1]='ip daddr { 0.0.0.0/8, 127.0.0.0/8, 169.254.0.0/16, 224.0.0.0/4, 255.255.255.255 } return'
 lines[#lines+1]='ct direction != original return'
 -- Linux 6.12 supports bitwise operations with constants, not register OR.
 for _,mark in ipairs({'0x10000000','0x20000000'})do
  lines[#lines+1]='ct mark & '..M.MASK..' == '..mark..' meta mark set (meta mark & '..M.KEEP..') | '..mark..' return'
 end
 lines[#lines+1]='ct state new jump choose'
 lines[#lines+1]='}'
 do
  lines[#lines+1]='chain choose6 {'
  lines[#lines+1]='ip6 daddr != 2000::/3 return'
  lines[#lines+1]='fib daddr type local return'
  for _,prefix in ipairs(context.lan6_prefixes or {}) do lines[#lines+1]='ip6 daddr '..prefix..' return' end
  lines[#lines+1]='ct direction != original return'
  for _,mark in ipairs({'0x10000000','0x20000000'}) do
   lines[#lines+1]='ct mark & '..M.MASK..' == '..mark..' meta mark set (meta mark & '..M.KEEP..') | '..mark..' return'
  end
  lines[#lines+1]='ct state != new return'
  device_rules(lines,context.secondary6~=nil)
  for _,r in ipairs(settings.rules) do
   if r.enabled and r.match_type=='network' and r.source=='' and r.destination=='' then
    for _,term in ipairs(advanced.paths[r.id] and advanced.paths[r.id][6] or {''}) do
     lines[#lines+1]=conditions(r,true)..(term=='' and '' or ' '..term)..' '..verdict(r.target,context.secondary6~=nil,r.fallback)..' comment "'..r.id..'-v6"'
    end
   end
  end
  lines[#lines+1]=(default_pdu==2 and verdict(2,context.secondary6~=nil) or 'return')..' comment "default"'
  lines[#lines+1]='}'
  if context.secondary6 then
   local link=context.secondary6
   assert(link.device:match('^[%w_.%-]+$') and M.ipv6(link.address),'第二路 IPv6 无效')
   lines[#lines+1]='chain srcnat6 { type nat hook postrouting priority 95; policy accept;'
   lines[#lines+1]='meta nfproto ipv6 iifname '..quoted(context.lan_device)..' oifname '..quoted(link.device)..' ct mark & '..M.MASK..' == 0x20000000 counter snat ip6 to '..link.address..'\n}'
  end
 end
 lines[#lines+1]='}'
 for _,line in ipairs(advanced.includes) do lines[#lines+1]=line end
 return table.concat(lines,'\n')..'\n',expression.dns_config(dns,advanced.has_site)
end
-- Compare absolute kernel deadlines. A decreasing relative TTL alone does
-- not justify rebuilding the sets; renew a live member before it expires.
function M.learning_update(elements,previous,now)
 local deadlines,count={},0
 local changed=type(previous)~='table'
 local function collect(set,values)
  for _,row in ipairs(values)do
   local tuple,seconds=row:match('^(.-) timeout (%d+)s$')
   local key=set..'/'..tuple
   local deadline=now+tonumber(seconds)
   if not deadlines[key] then count=count+1 end
   deadlines[key]=math.max(deadlines[key] or 0,deadline)
  end
 end
 for _,set in ipairs({'one','two','six1','six2'})do collect(set,elements[set])end
 for id,values in pairs(elements.rules)do collect('app_'..id,values)end
 if not changed then
  for key,deadline in pairs(deadlines)do
   local old=previous[key]
   if not old or deadline<old or (deadline>old and (old<=now+5 or deadline-old>=30)) then changed=true;break end
  end
  if not changed then
   for key in pairs(previous)do if not deadlines[key] then changed=true;break end end
  end
 end
 return deadlines,count,changed
end
function M.learned_elements(data,now,settings,devices)
 local out={one={},two={},six1={},six2={}}
 if type(data)~='table' or not tonumber(data.updated) or math.abs(now-data.updated)>30 then return out end
 local categories,automatic={},{}
 if settings then
  for _,r in ipairs(settings.categories)do categories[r.id]=r.target end
  for _,d in ipairs(devices or {})do if d.pdp==3 then automatic[d.mac]=true end end
 end
 for _,e in ipairs(data.endpoints or {})do
  if #out.one+#out.two+#out.six1+#out.six2>=1024 then break end
  local p=tonumber(e.port)
  local valid_mac,m=pcall(M.mac,e.mac)
  local permitted=e.source=='dpi' and settings and categories[tonumber(e.category)]==e.target
  if permitted and valid_mac and m~='' and (e.target==1 or e.target==2) and (M.ipv4(e.client) and M.ipv4(e.remote)) and (e.protocol=='tcp' or e.protocol=='udp') and p and p>=1 and p<=65535 and p==math.floor(p) and tonumber(e.expires) and e.expires>now then
   local key=e.target==1 and 'one' or 'two'
   out[key][#out[key]+1]=m..' . '..e.remote..' . '..e.protocol..' . '..p..' timeout '..math.min(600,math.floor(e.expires-now))..'s'
  end
 end
 return out
end
function M.rule_elements(settings,data,now)
 local out={}
 for _,r in ipairs(settings.rules)do if r.enabled and r.match_type~='network' then out[r.id]={} end end
 if type(data)~='table' or not tonumber(data.updated) or math.abs(now-data.updated)>30 then return out end
 local count=0
 for _,e in ipairs(data.endpoints or {})do
  if count>=1024 then break end
  local p=tonumber(e.port);local valid_mac,m=pcall(M.mac,e.mac)
  if e.source=='dpi' and valid_mac and m~='' and M.ipv4(e.remote) and (e.protocol=='tcp' or e.protocol=='udp') and p and p>=1 and p<=65535 and p==math.floor(p) and tonumber(e.expires) and e.expires>now then
   count=count+1
   for _,r in ipairs(settings.rules)do
    if out[r.id] and ((r.match_type=='application' and tonumber(e.appid)==r.match_id) or (r.match_type=='category' and tonumber(e.category)==r.match_id)) then
     out[r.id][#out[r.id]+1]=m..' . '..e.remote..' . '..e.protocol..' . '..p..' timeout '..math.min(600,math.floor(e.expires-now))..'s'
    end
   end
  end
 end
 return out
end
return M
