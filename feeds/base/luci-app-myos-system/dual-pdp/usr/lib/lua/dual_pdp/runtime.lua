local fs=require('nixio.fs')
local json=require('luci.jsonc')
local policy=require('dual_pdp.policy')
local M={}
local RUN='/var/run/dual-pdp-manager'
local TABLE=policy.TABLE
local cached_catalog,cached_selector,last_schema,last_up,last_learned,last_learned_time
local function decode(path)return json.parse(fs.readfile(path) or '')end
local function atomic(path,text)
 if fs.readfile(path)==text then return false end
 local tmp=path..'.new';assert(fs.writefile(tmp,text),'无法写入运行配置');fs.chmod(tmp,'600');assert(fs.rename(tmp,path));return true
end
local function save(path,value)return atomic(path,json.stringify(value))end
local function profile()
 local id=(fs.readfile('/etc/c2000max-traffic/features/active') or ''):match('^(%x+)')
 return id and #id==64 and id:lower() or nil
end
function M.summary()
 return decode(RUN..'/catalog-summary.json') or {categories={},apps_count=0,profile=''}
end
function M.catalog(run)
 local id=profile()
 if cached_catalog and cached_catalog.profile==id then return cached_catalog end
 assert(id,'特征库尚未就绪')
 -- The full shell/JSHN export grows very expensive with thousands of apps.
 -- Reuse its validated semantic TSV, and parse it once with Lua/json-c.
 local raw,ok=run({'/usr/sbin/c2000max-traffic','catalog-info',id})
 local data=ok and json.parse(raw)
 assert(data and data.success and type(data.categories)=='table' and data.profile==id,'特征库目录尚未就绪')
 local path='/tmp/c2000max-traffic/catalogs/'..id..'.semantic-v2.tsv'
 local stat=fs.stat(path);assert(stat and stat.size<=2097152,'应用目录缺失或超过支持范围')
 local valid={};for _,category in ipairs(data.categories)do valid[tonumber(category.id)]=true end
 local apps={}
 for line in (fs.readfile(path) or ''):gmatch('[^\n]+')do
  local app,name,category=line:match('^(%d+)\t([^\t]+)\t(%d+)\t')
  app=tonumber(app);category=tonumber(category)
  assert(app and app>=1001 and app<32768 and app%1000>=1 and app%1000<=512 and valid[category] and #name<=255 and not name:find('[%z\1-\31]'),'应用目录包含无效条目')
  apps[#apps+1]={id=app,name=name,category_id=category}
  assert(#apps<=16384,'应用目录超过支持范围')
 end
 assert(#apps==tonumber(data.total_apps),'应用目录不完整')
 data.apps=apps
 assert(#data.categories<=128,'特征库目录超过支持范围')
 cached_catalog=data
 save(RUN..'/catalog-summary.json',{profile=id,version=data.version,format=data.format,categories=data.categories,apps_count=#data.apps})
 return data
end
function M.rule_catalog(run)
 local data=M.catalog(run);local apps={}
 for _,a in ipairs(data.apps)do apps[#apps+1]={id=a.id,name=a.name,category_id=a.category_id} end
 return {success=true,profile=data.profile,categories=data.categories,apps=apps}
end
local function dns_dirs()
 local out,seen={},{}
 for name in fs.dir('/var/etc') or function()end do
  if name:match('^dnsmasq%.conf%.') then
   for line in (fs.readfile('/var/etc/'..name) or ''):gmatch('[^\n]+')do
    local dir=line:match('^conf%-dir=([/%w_.%-]+)')
    if dir and (dir:match('^/tmp/') or dir:match('^/var/')) and not dir:find('/%.%./') and not seen[dir] then out[#out+1]=dir;seen[dir]=true end
   end
  end
 end
 return out
end
local function dns_validate(text,run)
 if not text:find('%S') then return end
 local path=RUN..'/dns-check-'..require('nixio').getpid()..'.conf'
 atomic(path,text)
 local output,ok=run({'/usr/libexec/inst-dual-pdp/dnsmasq','--test','--conf-file='..path})
 fs.unlink(path)
 assert(ok,'域名表达式无效或 DNS 组件需要更新：'..output:sub(1,120))
end
local function dns_sync(text,run,reset_cache)
 local changed=false
 for _,dir in ipairs(dns_dirs())do
  local path=dir..'/dual-pdp-smart.conf'
  if text=='' then if fs.stat(path)then fs.unlink(path);changed=true end
  else fs.mkdirr(dir);if atomic(path,text)then changed=true end end
 end
 -- SIGHUP does not re-read nftset directives; restart only when this file changes.
 if changed or reset_cache then local _,ok=run({'/etc/init.d/dnsmasq','restart'});assert(ok,'DNS 规则重载失败') end
end
local function nft(text,run,check)
 local path=RUN..'/policy.nft';atomic(path,text)
 local result,ok
 if check then result,ok=run({'nft','-c','-f',path});assert(ok,'分流规则校验失败：'..result:sub(1,200)) end
 result,ok=run({'nft','-f',path});assert(ok,'分流规则加载失败：'..result:sub(1,200))
end
-- dnsmasq writes address sets when it receives an upstream answer. Keep
-- unchanged sets across policy reloads so cached client answers still match.
local function dns_membership(config)
 local rows={}
 local function add(name,line)
  rows[name]=rows[name] or {}
  rows[name][#rows[name]+1]=line
 end
 for line in config:gmatch('[^\n]+') do
  local id=line:match('^pdu%-nftset=(%d+),')
  if id then
   add('dns_r'..id,line)
   if line:match('^pdu%-nftset=%d+,46,') then add('dns6_r'..id,line) end
  else
   for name in line:gmatch('#inet#'..TABLE..'#([%w_]+)') do add(name,line) end
  end
 end
 for name,lines in pairs(rows) do table.sort(lines);rows[name]=table.concat(lines,'\n') end
 return rows
end

local function replace_table(text,dns,existing,exists)
 if not exists then return text,dns~='' end
 local prefix='inet '..TABLE..' '
 local reset='delete table inet '..TABLE..'\n'..text
 if dns=='' then return reset,false end
 local old=dns_membership(fs.readfile(RUN..'/dns-membership.conf') or '')
 local wanted=dns_membership(dns)
 local keep,chains,sets={},{},{}
 for line in existing:gmatch('[^\n]+') do
  local chain=line:match('^%s*chain ([%w_]+) {')
  local set=line:match('^%s*set ([%w_]+) {')
  if chain then chains[#chains+1]=chain end
  if set then
   if wanted[set] and old[set]==wanted[set] then keep[set]=true else sets[#sets+1]=set end
  end
 end
 if not next(keep) then return reset,true end
 local commands={}
 for _,name in ipairs(chains) do commands[#commands+1]='flush chain '..prefix..name end
 for _,name in ipairs(chains) do commands[#commands+1]='delete chain '..prefix..name end
 for _,name in ipairs(sets) do commands[#commands+1]='delete set '..prefix..name end
 for line in text:gmatch('[^\n]+') do
  if not keep[line:match('^set ([%w_]+) {') or ''] then commands[#commands+1]=line end
 end
 local missing=false
 for name in pairs(wanted) do if not keep[name] then missing=true;break end end
 return table.concat(commands,'\n')..'\n',missing
end
local function route_rules(lan,run,enable)
 local output,ok=run({'ip','-4','rule','show'});assert(ok,'无法读取策略路由')
 for pdu=1,2 do
  local pref=tostring(1079+pdu);local mark=pdu==1 and '0x10000000/0x30000000' or '0x20000000/0x30000000';local lookup=tostring(200+pdu)
  local found=false
  for line in output:gmatch('[^\n]+')do if line:match('^'..pref..':')then
   local owned=line:find(mark,1,true) and line:match('lookup%s+'..lookup..'%s*$')
   if enable then assert(owned,'分流策略优先级 '..pref..' 已被其他规则占用') end
   if owned then
    if enable and line:find('iif '..lan..' ',1,true)then found=true
    else run({'ip','-4','rule','del','priority',pref,'fwmark',mark,'lookup',lookup}) end
   end
  end end
  if enable and not found then local _,added=run({'ip','-4','rule','add','priority',pref,'iif',lan,'fwmark',mark,'lookup',lookup});assert(added,'无法添加分流策略路由') end
 end
 if enable then run({'ip','-4','route','replace','unreachable','default','table','202','metric','32767'}) end
end
-- PDU 2 has a source-specific netifd route. Forwarded LAN packets still
-- carry their original source at route lookup, before NAT in postrouting.
local V6_TABLE='32062'
local function route6(lan,link,run,enable)
 local mark='0x20000000/0x30000000'
 local args={'ip','-6','rule','show','pref','1081'}
 local rules,ok=run(args);assert(ok,'无法读取 IPv6 策略路由')
 local found=false
 for line in rules:gmatch('[^\n]+') do
  local ours=line:find('fwmark '..mark,1,true) and line:match('lookup%s+'..V6_TABLE..'%s*$')
  if ours and line:find('iif '..lan..' ',1,true) then found=true
  elseif enable then error('IPv6 策略优先级 1081 已被占用',0) end
 end
 local routes=run({'ip','-6','route','show','table',V6_TABLE})
 local owned={}
 for line in routes:gmatch('[^\n]+') do
  if line:find('proto 196 ',1,true) then owned[#owned+1]=line
  elseif enable and not line:find('does not exist',1,true) and not line:find('Dump terminated',1,true) then error('IPv6 分流路由表已被占用',0) end
 end
 if not enable then
  if found then run({'ip','-6','rule','del','pref','1081','iif',lan,'fwmark',mark,'table',V6_TABLE}) end
  if #owned>0 then run({'ip','-6','route','flush','table',V6_TABLE,'proto','196'}) end
  return
 end
 local function ensure(args)
  local msg,ok=run(args);assert(ok,'IPv6 分流路由未就绪：'..msg:sub(1,96))
 end
 ensure({'ip','-6','route','replace','unreachable','default','table',V6_TABLE,'proto','196','metric','32767'})
 if link then
  ensure({'ip','-6','route','replace','default','via',link.gateway,'dev',link.device,'onlink','table',V6_TABLE,'proto','196','metric','12'})
 else
  for _,line in ipairs(owned) do if line:match('^default via ') then
   ensure({'ip','-6','route','del','default','table',V6_TABLE,'proto','196','metric','12'})
  end end
 end
 if not found then ensure({'ip','-6','rule','add','pref','1081','iif',lan,'fwmark',mark,'table',V6_TABLE}) end
end
local function domain_index(catalog,run)
 local index=RUN..'/domains-'..catalog.profile..'.tsv'
 if not fs.stat(index)then
  local library='/etc/c2000max-traffic/features/libraries/'..catalog.profile
  local command={'ucode','/usr/libexec/c2000max-feature-domains.uc',library..'/feature.cfg'}
  if fs.stat(library..'/domains.tsv')then command[#command+1]=library..'/domains.tsv' end
  local output,ok=run(command)
  assert(ok,'无法解析特征库域名')
  atomic(index,output)
 end
 return fs.readfile(index) or ''
end
local function category_domains(catalog,settings,run)
 local selected,valid={},{ }
 for _,category in ipairs(catalog.categories)do valid[tonumber(category.id)]=true end
 for _,r in ipairs(settings.categories)do if r.target>0 and valid[r.id]then selected[r.id]=r.target end end
 if not next(selected) or settings.profile~=catalog.profile then return {} end
 local app_category={};for _,app in ipairs(catalog.apps)do app_category[tonumber(app.id)]=tonumber(app.category_id) end
 local result,by_category={},{};local count=0
 for line in domain_index(catalog,run):gmatch('[^\n]+')do
  local app,domain=line:match('^(%d+)%s+(%S+)$');local category=app and app_category[tonumber(app)]
  if category and selected[category]and count<4096 then
   local valid_domain,domains=pcall(policy.domains,{domain})
   if valid_domain and #domains==1 and not domains[1]:find('[^%w%.%-]') then
    local entry=by_category[category]
    if not entry then entry={id=category,target=selected[category],domains={}};by_category[category]=entry;result[#result+1]=entry end
    entry.domains[#entry.domains+1]=domains[1];count=count+1
   end
  end
 end
 table.sort(result,function(a,b)return a.id<b.id end)
 return result
end
local function selectors(settings)
 for _,r in ipairs(settings.rules)do if r.enabled and r.match_type~='network' then return true end end
 return false
end
local function selector_data(catalog,settings,run,mismatch)
 local signature=json.stringify({profile=catalog.profile,rules=settings.rules,mismatch=mismatch})
 if cached_selector and cached_selector.signature==signature then return cached_selector.apps,cached_selector.selected,cached_selector.domains end
 local apps,selected,domains={},{},{}
 for _,a in ipairs(catalog.apps)do apps[#apps+1]={id=tonumber(a.id),category_id=tonumber(a.category_id)} end
 local function result()
  cached_selector={signature=signature,apps=apps,selected=selected,domains=domains}
  return apps,selected,domains
 end
 if mismatch or not selectors(settings) then return result() end
 local by_app={}
 for _,a in ipairs(apps)do
  for _,r in ipairs(settings.rules)do
   if r.enabled and ((r.match_type=='application' and r.match_id==a.id) or (r.match_type=='category' and r.match_id==a.category_id)) then
    if not by_app[a.id]then by_app[a.id]={};selected[#selected+1]=a.id end
    by_app[a.id][#by_app[a.id]+1]=r.id;domains[r.id]=domains[r.id] or {}
   end
  end
 end
 local count=0
 for line in domain_index(catalog,run):gmatch('[^\n]+')do
  local app,domain=line:match('^(%d+)%s+(%S+)$');local rules=app and by_app[tonumber(app)]
  if rules and count<4096 then
   local ok,values=pcall(policy.domains,{domain})
   if ok and #values==1 and not values[1]:find('[^%w%.%-]') then for _,id in ipairs(rules)do
    if count>=4096 then break end
    domains[id][#domains[id]+1]=values[1];count=count+1
   end end
  end
 end
 return result()
end
function M.validate_catalog(settings,run)
 if settings.enabled then
  local expr=require('dual_pdp.expression')
  for _,r in ipairs(settings.rules) do
   if r.enabled and r.expression~='' and expr.uses_site(expr.parse(r.expression)) then
    require('dual_pdp.geosite').ensure();break
   end
  end
 end
 dns_validate(policy.check_network_rules(settings),run)
 if not settings.enabled or (#settings.categories==0 and not selectors(settings)) then return end
 local data=M.catalog(run)
 if #settings.categories>0 or selectors(settings) then
  assert(settings.profile==data.profile,'特征库已变更，请重新选择并保存分类')
  local valid={};for _,c in ipairs(data.categories)do valid[tonumber(c.id)]=true end
  for _,c in ipairs(settings.categories)do assert(valid[c.id],'特征库中没有该分类') end
  local apps={};for _,a in ipairs(data.apps)do apps[tonumber(a.id)]=true end
  for _,r in ipairs(settings.rules)do if r.enabled and r.match_type~='network' then
   assert((r.match_type=='category' and valid[r.match_id]) or (r.match_type=='application' and apps[r.match_id]),'特征库中没有所选分类或应用')
  end end
 end
 return data
end
local guard_path=RUN..'/guard-device'
local function guard_off(run)
 local dev=fs.readfile(guard_path)
 if dev and dev:match('^[%w_.%-]+$') then
  local _,ok=run({'/usr/libexec/dual-pdp-guard','off',dev})
  if not ok then return false end
  fs.unlink(guard_path)
 end
 return true
end
function M.clear(run,lan)
 guard_off(run)
 local _,exists=run({'nft','list','table','inet',TABLE})
 if exists then run({'nft','delete','table','inet',TABLE}) end
 dns_sync('',run);route_rules(lan,run,false);route6(lan,nil,run,false)
 last_schema=nil;last_up=nil;last_learned=nil
end
function M.sync(c,env)
 local settings=policy.from_uci(c)
 local run=env.command
 if not settings.enabled then settings.rules={};settings.categories={} end
 local needs_apps=#settings.categories>0 or selectors(settings)
 local catalog={profile='',categories={},apps={}}
 if needs_apps then
  local ok,data=pcall(M.catalog,run)
  if ok then catalog=data end
 end
 local state={enabled=settings.enabled,profile_mismatch=needs_apps and (catalog.profile=='' or catalog.profile~=settings.profile),secondary_up=env.secondary_up,version=policy.VERSION}
 if not settings.enabled and not settings.priority_enabled and #env.devices==0 then
  if last_schema~=false then
   M.clear(run,env.lan.l3_device or 'br-lan')
   last_schema=false
  end
  save(RUN..'/policy-state.json',state)
  return
 end
 local context={lan_device=env.lan.l3_device or 'br-lan',lan_cidr=env.lan_cidr,devices=env.devices,
  secondary_up=env.secondary_up,secondary6=env.secondary6,lan6_prefixes=env.lan6_prefixes,profile_mismatch=state.profile_mismatch}
 context.category_rules=category_domains(catalog,settings,run)
 local apps,selected,domains=selector_data(catalog,settings,run,state.profile_mismatch)
 context.selector_domains=domains
 local clients={}
 if needs_apps and not state.profile_mismatch then
  for _,d in ipairs(env.clients) do
   if policy.ipv4(d.ip) and #clients<256 then clients[#clients+1]={mac=d.mac,ip=d.ip} end
  end
 end
 local config={enabled=needs_apps and not state.profile_mismatch and fs.stat('/proc/sys/oaf/feature_generation')~=nil,profile=catalog.profile,clients=clients,
  apps=apps,selected_apps=selected,categories=settings.categories,learning=true}
 if save(RUN..'/classifier.json',config) then run({'/etc/init.d/dual-pdp-smart','reload'}) end
 assert(guard_off(run),'无法卸载旧版限速规则')
 local schema=json.stringify({settings,context})
 local existing,exists=run({'nft','-t','list','table','inet',TABLE})
 if schema~=last_schema or not exists or last_up~=(env.secondary6 and env.secondary6.gateway or '') then
  local text,dns=policy.render(settings,context)
  dns_validate(dns,run)
  local batch,reset_dns=replace_table(text,dns,existing,exists)
  route_rules(context.lan_device,run,true)
  route6(context.lan_device,env.secondary6,run,true)
  nft(batch,run,true);dns_sync(dns,run,reset_dns)
  atomic(RUN..'/dns-membership.conf',dns)
  for name in fs.dir(RUN) do
   if name:match('^ipdb%-%d+%-[%w_]+%.nft$') and not text:find('/'..name..'"',1,true) then fs.unlink(RUN..'/'..name) end
  end
  last_schema=schema;last_up=env.secondary6 and env.secondary6.gateway or '';last_learned=nil
 end
 if not config.enabled then save(RUN..'/policy-state.json',state);return end
 local learned=decode(RUN..'/learned.json')
 if learned and learned.profile~=catalog.profile then learned=nil end
 local elements=policy.learned_elements((settings.learning and not state.profile_mismatch) and learned or nil,os.time(),settings,env.devices)
 elements.rules=policy.rule_elements(settings,(settings.learning and not state.profile_mismatch) and learned or nil,os.time())
 local signature=json.stringify(elements)
 if signature~=last_learned or os.time()-(last_learned_time or 0)>=60 then
  local batch={'flush set inet '..TABLE..' learned1','flush set inet '..TABLE..' learned2'}
  for index,key in ipairs({'one','two'})do if #elements[key]>0 then batch[#batch+1]='add element inet '..TABLE..' learned'..index..' { '..table.concat(elements[key],', ')..' }' end end
  for _,r in ipairs(settings.rules)do if elements.rules[r.id] then
   batch[#batch+1]='flush set inet '..TABLE..' app_'..r.id
   local values=elements.rules[r.id];if #values>0 then batch[#batch+1]='add element inet '..TABLE..' app_'..r.id..' { '..table.concat(values,', ')..' }' end
  end end
  nft(table.concat(batch,'\n')..'\n',run,false);last_learned=signature;last_learned_time=os.time()
 end
 state.learned_count=#elements.one+#elements.two;state.dpi_running=fs.stat('/proc/sys/oaf/version')~=nil
 state.category_domain_sets=#context.category_rules
 save(RUN..'/policy-state.json',state)
end
function M.status(c)
 local state=decode(RUN..'/policy-state.json') or {}
 local settings=policy.from_uci(c)
 local job=decode(RUN..'/job.json') or {}
 local pending=job.action=='policy' and (job.state=='pending' or job.state=='running')
 local message=(pending or state.error) and fs.readfile(RUN..'/geosite-status')
 if message then
  state.geosite_message=message:match('[^\r\n]+')
  local partial=fs.stat('/usr/share/dual-pdp/.geosite-download','size')
  if partial and partial>0 then state.geosite_message=state.geosite_message..' · '..math.floor(partial/1024)..' KB' end
 end
 return {settings=settings,catalog=M.summary(),state=state}
end
return M
