-- Compile a bounded boolean expression once, when the routing policy changes.
local M={}
local function check(ok,msg) if not ok then error(msg,0) end return ok end
local function atom(field,op,value)
 local p=require('dual_pdp.policy')
 local neg=op=='ne' or op=='notcontains'
 if neg then op=op=='ne' and 'eq' or 'contains' end
 local a={field=field,op=op,value=value}
 if field=='country' then
  a.value=value:upper();check(op=='eq' and a.value:match('^[A-Z][A-Z]$'),'地区请填写两位代码，如 CN')
 elseif field=='asn' then
  local n=value:upper():gsub('^AS','');check(op=='eq' and n:match('^%d+$') and #n<=10 and tonumber(n)>0 and tonumber(n)<=4294967295,'ASN 无效')
  a.value=string.format('%.0f',tonumber(n))
 elseif field=='site' then
  a.value=value:upper();check((op=='in' or op=='eq') and #value<=64 and a.value:match('^[A-Z0-9][A-Z0-9!%-]*$'),'GeoSite 分组名无效')
 elseif field=='domain' then
  a.value=value:lower();check(#value<=120 and not value:find('..',1,true) and (op=='glob' and not value:find('[^%w%.%-%*%?]') or (op=='eq' or op=='suffix' or op=='contains') and not value:find('[^%w%.%-]')),'域名条件无效')
  check(op=='contains' or (not value:match('^%.') and not value:match('%.$')),'域名不能以点开头或结尾')
 elseif field=='proto' then
  a.value=value:lower();check(op=='eq' and ({tcp=true,udp=true,icmp=true})[a.value],'协议支持 TCP、UDP、ICMP')
 elseif field=='port' then
  check((op=='eq' or op=='in') and value:match('^[%d,-]+$'),'端口格式无效');a.value=table.concat(p.ports(value),', ');check(a.value~='','端口不能为空')
 elseif field=='ip' then
  check(op=='in' or op=='eq','目的 IP 使用 in 或 eq')
  if value:find(':',1,true) then
   local addr,bits=value:match('^([^/]+)/(%d+)$')
   addr=addr or value;check((not bits or tonumber(bits)<=128) and p.ipv6(addr,true),'IPv6 目的地址或网段无效')
   a.family=6
  else check(not value:find('[^%d./]'),'IPv4 目的地址无效');a.value=p.cidr(value);a.family=4 end
 else error('未知字段 '..field,0) end
 return neg and {op='not',left=a} or a
end
function M.parse(s)
 check(type(s)=='string' and #s<=1024,'表达式过长')
 if s=='' then return end
 local tokens={}
 for t in s:gsub('([()])',' %1 '):gmatch('%S+') do tokens[#tokens+1]=t end
 check(#tokens<=96,'表达式最多 96 个词')
 local pos=1
 local function peek() return (tokens[pos] or ''):lower() end
 local function take() local t=tokens[pos];pos=pos+1;return t end
 local parse_or
 local function unary(d)
  check(d<=12,'括号或 NOT 嵌套过深')
  if peek()=='not' then take();return {op='not',left=unary(d+1)} end
  if peek()=='(' then take();local x=parse_or(d+1);check(take()==')','缺少右括号');return x end
  local f,o,v=take(),take(),take();check(f and o and v,'每个条件需要字段、运算符和值');return atom(f:lower(),o:lower(),v)
 end
 local function conjunction(d)
  local x=unary(d);while peek()=='and' do take();x={op='and',left=x,right=unary(d)} end;return x
 end
 parse_or=function(d)
  local x=conjunction(d);while peek()=='or' do take();x={op='or',left=x,right=conjunction(d)} end;return x
 end
 local tree=parse_or(0);check(pos>#tokens,'条件之间请使用 AND 或 OR');return tree
end
function M.uses_site(x)
 return x and (x.field=='site' or M.uses_site(x.left) or M.uses_site(x.right)) or false
end
local function escape(s) return (s:gsub('%.','[.]')) end
local function regex(a)
 local s=escape(a.value)
 if a.op=='contains' then return s end
 if a.op=='suffix' then return '(^|[.])'..s..'$' end
 if a.op=='glob' then s=s:gsub('%*','[^.]*'):gsub('%?','[^.]') end
 return '^'..s..'$'
end
local function load_ranges(kind,value)
 local f=assert(io.popen('/usr/libexec/dual-pdp-ranges '..kind..' '..value))
 local result=f:read('*l') or '';f:close()
 local a,b=result:match('^(%d+) (%d+)$')
 check(a,'地址库没有可用的 '..kind..' '..value..'，请检查代码或编号')
 return tonumber(a)+tonumber(b)
end
local function combine(a,b,is_or)
 local result={}
 if is_or then
  for _,x in ipairs(a) do result[#result+1]=x end
  for _,x in ipairs(b) do result[#result+1]=x end
 else
  check(#a*#b<=32,'表达式展开超过 32 条匹配路径，请拆分规则')
  for _,x in ipairs(a) do for _,y in ipairs(b) do result[#result+1]=x=='' and y or y=='' and x or x..' '..y end end
 end
 check(#result<=32,'表达式展开超过 32 条匹配路径，请拆分规则');return result
end
function M.compile(rules,context)
 local result={sets={},dns={},includes={},paths={},has_site=false}
 local reserved,known,range_keys={}, {}, {}
 local id,intervals,range_count,pattern_count,site_size=999,0,0,0,0
 local regex_ids={}
 for _,r in ipairs(rules) do reserved[r.priority]=true end
 local function register(id,pattern)
  check(#pattern<=192 and #pattern>0,'域名库中的表达式长度超限')
  pattern_count=pattern_count+1;regex_ids[id]=true
  check(pattern_count<=256,'域名正则超过 256 条，请减少分组')
  result.dns[#result.dns+1]='pdu-nftset='..id..',46,'..pattern:gsub('.',function(c)return string.format('%02x',c:byte())end)
 end
 local function dns_set(key,a)
  if known[key] then return known[key] end
  while reserved[id] do id=id-1 end
  local n=id;id=id-1;known[key]=n
  for _,family in ipairs({4,6}) do
   local name=(family==4 and 'dns_r' or 'dns6_r')..n
   result.sets[#result.sets+1]='set '..name..' { type ipv'..family..'_addr; flags timeout; timeout 1h; size '..((a.field=='site' or key=='seen') and 16384 or 4096)..'; }'
  end
  if a.field=='site' then
   result.has_site=true
   local suffix_target='/4#inet#dual_pdp_smart#dns_r'..n..',6#inet#dual_pdp_smart#dns6_r'..n
   local read=context.read_site or require('dual_pdp.geosite').read
   read(a.value,function(kind,s)
    site_size=site_size+1
    check(site_size<=16000,'所选 GeoSite 条目超过 16000 个，请减少分组')
    if kind==2 then result.dns[#result.dns+1]='nftset=/'..s..suffix_target
    elseif kind==3 then register(n,'^'..escape(s)..'$')
    elseif kind==0 then register(n,escape(s))
    else register(n,s) end
   end)
  else register(n,key=='seen' and '.' or regex(a)) end
  return n
 end
 local function predicate(a,family,neg)
  local ip=family==4 and 'ip daddr ' or 'ip6 daddr '
  local op=neg and '!= ' or ''
  if a.field=='country' or a.field=='asn' then
   local name=a.field..'_'..a.value
   if not range_keys[name] then
    range_keys[name]=true;range_count=range_count+1;check(range_count<=16,'最多同时加载 16 个地区或 ASN')
    intervals=intervals+(context.read_ranges or load_ranges)(a.field,a.value)
    check(intervals<=90000,'已选地址段超过 90000 条，请减少地区或 ASN')
    for _,f in ipairs({4,6}) do result.sets[#result.sets+1]='set '..name..'_'..f..' { type ipv'..f..'_addr; flags interval; }' end
    result.includes[#result.includes+1]='include "/var/run/dual-pdp-manager/ipdb-20261101-'..name..'.nft"'
   end
   return {ip..op..'@'..name..'_'..family}
  elseif a.field=='site' or a.field=='domain' then
   local prefix=family==4 and '@dns_r' or '@dns6_r'
   local n=dns_set(a.field..':'..a.op..':'..a.value,a)
   local seen=neg and ip..prefix..dns_set('seen',{field='domain'})..' ' or ''
   return {seen..ip..op..prefix..n}
  elseif a.field=='proto' then
   return {'meta l4proto '..op..(family==6 and a.value=='icmp' and 'ipv6-icmp' or a.value)}
  elseif a.field=='port' then
   local transport='meta l4proto { tcp, udp } th dport '..op..'{ '..a.value..' }'
   return neg and {'meta l4proto != { tcp, udp }',transport} or {transport}
  elseif a.family~=family then return neg and {''} or {}
  else return {ip..op..a.value} end
 end
 local function expand(x,family,neg)
  if x.field then return predicate(x,family,neg) end
  if x.op=='not' then return expand(x.left,family,not neg) end
  return combine(expand(x.left,family,neg),expand(x.right,family,neg),(x.op=='or')~=neg)
 end
 for _,r in ipairs(rules) do if r.enabled and r.expression and r.expression~='' then
  local x=M.parse(r.expression)
  result.paths[r.id]={[4]=expand(x,4,false),[6]=expand(x,6,false)}
 end end
 return result
end

-- dnsmasq chooses the longest suffix. Propagate parent memberships so overlapping
-- GeoSite and user domains both receive the DNS answer.
function M.dns_config(lines,merge)
 local by_domain,rest={},{}
 local ids,pattern_count={},0
 for _,line in ipairs(lines) do
  local n=line:match('^pdu%-nftset=(%d+),')
  if n then ids[n]=true;pattern_count=pattern_count+1 end
  local domain,targets=line:match('^nftset=/([^/]+)/(.+)$')
  if domain and merge then
   local sets=by_domain[domain] or {};by_domain[domain]=sets
   for target in targets:gmatch('[^,]+') do sets[target]=true end
  else rest[#rest+1]=line end
 end
 local count=0;for _ in pairs(ids) do count=count+1 end
 check(count<=32 and pattern_count<=256,'域名规则超过 DNS 组件容量（32 组、256 个表达式）')
 local names={};for k in pairs(by_domain) do names[#names+1]=k end;table.sort(names)
 check(#names<=16000,'所选 GeoSite 域名超过 16000 个')
 for _,k in ipairs(names) do
  local targets={};local d=k
  while d do for t in pairs(by_domain[d] or {}) do targets[t]=true end;d=d:match('^[^.]+%.(.+)$') end
  local row={};for t in pairs(targets) do row[#row+1]=t end;table.sort(row)
  rest[#rest+1]='nftset=/'..k..'/'..table.concat(row,',')
 end
 return table.concat(rest,'\n')..(#rest>0 and '\n' or '')
end
return M
