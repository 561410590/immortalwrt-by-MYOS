'use strict';
'require view';
'require request';
'require poll';

const list = v => Array.isArray(v) ? v : [];
const text = v => v == null || v === '' ? '未获取' : String(v);
const node = (tag, cls, children) => E(tag, cls ? {class:cls} : {}, children);
const badge = (value, tone) => node('span','m-badge '+(tone||''),value);
const link = (label,path) => E('a',{href:L.url.apply(L,path.split('/'))},label);
const card = (title,icon,body,extra) => node('section','m-card',[
 node('div','m-card-heading',[node('h2','',[node('span','m-section-icon',icon),title]),extra||node('span')]),body
]);
const kv = pairs => node('dl','m-kv',pairs.map(p=>node('div','',[node('dt','',p[0]),node('dd','',text(p[1]))])));
const bytes = value => {
 if(value==null||!Number.isFinite(Number(value)))return '—';
 let n=Number(value),u=0;const units=['B','KiB','MiB','GiB','TiB'];
 while(n>=1024&&u<4){n/=1024;u++;}
 return n.toFixed(u?1:0)+' '+units[u];
};
const speed = value => {
 if(value==null)return ['—',''];
 let n=value*8,u=0;const units=['bps','Kbps','Mbps','Gbps'];
 while(n>=1000&&u<3){n/=1000;u++;}
 return [n.toFixed(u>1?2:0),units[u]];
};
const duration = value => {
 if(value==null)return '未获取';
 const n=Math.max(0,Number(value));return (n>=86400?Math.floor(n/86400)+' 天 ':'')+Math.floor(n%86400/3600)+' 小时 '+Math.floor(n%3600/60)+' 分钟';
};
const addresses = items => list(items).map(v=>v.address+(v.mask!=null?'/'+v.mask:'')).join(' · ')||'未分配';
const same = (a,b) => JSON.stringify(a)===JSON.stringify(b);

return view.extend({
 handleSave:null,handleSaveApply:null,handleReset:null,
 fetchData:function(){
  return request.get(L.url('admin','myos','data'),{cache:false,timeout:15000}).then(r=>{
   if(!r.ok)throw new Error('HTTP '+r.status);
   const d=r.json();if(!d.board||!d.system)throw new Error('状态响应无效');return d;
  });
 },
 load:function(){return this.fetchData().catch(e=>({failure:e.message}));},
 refreshData:function(){
  if(document.hidden||!this.root.isConnected)return Promise.resolve();
  if(this.refreshing)return this.refreshing;
  this.refreshing=this.fetchData().then(d=>this.update(d))
   .catch(e=>this.showError(e.message)).finally(()=>{this.refreshing=null;});
  return this.refreshing;
 },
 refreshSimNames:function(d){
  const cards=list(d.modems).map(m=>m.sim_id||'').filter(Boolean).join('|');
  if(!cards||this.simLookup||(cards===this.simCards&&Date.now()<this.simNext))return;
  this.simCards=cards;this.simNext=Date.now()+60000;
  this.simLookup=request.get(L.url('admin','myos','sim_names'),{cache:false,timeout:25000})
   .then(r=>{this.simFailure=!r.ok;})
   .catch(()=>{this.simFailure=true;})
   .finally(()=>{this.simLookup=null;});
 },
 render:function(initial){
  this.previous=null;this.cpuReading=null;this.history=[];this.saved={};this.parts={};
  const slot=name=>this.parts[name]=node('div','m-slot m-slot-'+name);
  this.live=node('span','m-live','正在读取设备状态');
  this.error=node('div','m-read-error');this.error.hidden=true;
  this.root=node('div','m-dashboard',[
   node('div','m-page-heading',[node('h1','','设备总览'),this.live]),
   this.error,
   slot('resources'),
   node('div','m-dashboard-grid',[
    node('div','m-details-column',[slot('modems'),slot('ports'),slot('network'),slot('clients')]),
    node('div','m-display-column',[slot('product'),slot('traffic'),slot('wifi'),slot('management'),slot('version')])
   ]),
  ]);
  this.initTraffic();
  this.initClients();
  this.initManagement();
  if(initial.failure)this.showError(initial.failure);else this.update(initial);
  const refresh=()=>this.refreshData();
  poll.add(refresh,5);
  setTimeout(refresh,1000);
  document.addEventListener('visibilitychange',()=>{
   if(!document.hidden)refresh().then(()=>setTimeout(refresh,1000));
  });
  return this.root;
 },
 showError:function(message){
  this.error.hidden=false;this.error.textContent='状态更新失败：'+message+'。'+(this.previous?'当前保留上次数据。':'请刷新页面重试。');
  this.live.textContent='更新中断';
 },
 replace:function(name,data,render){
  if(same(this.saved[name],data))return;
  const open=new Set([...this.parts[name].querySelectorAll('details[open]')].map(n=>n.dataset.key));
  this.parts[name].replaceChildren(render());
  this.parts[name].querySelectorAll('details').forEach(n=>{if(open.has(n.dataset.key))n.open=true;});
  this.saved[name]=data;
 },
 update:function(d){
  const prev=this.previous,dt=prev?d.sample-prev.sample:0;
  if(prev&&(dt<0||(prev.cpu&&d.cpu&&d.cpu.total<prev.cpu.total)))this.cpuReading=null;
  let cpu=null,down=null,up=null;
  if(dt>0&&dt<=30&&prev.cpu&&d.cpu){
   const total=d.cpu.total-prev.cpu.total,idle=d.cpu.idle-prev.cpu.idle;
   if(total>0&&idle>=0)cpu=Math.max(0,Math.min(100,100*(total-idle)/total));
  }
  if(dt>0&&dt<=30){
   let valid=0,rx=0,tx=0;
   list(d.uplinks).forEach(n=>{
    const old=list(prev.uplinks).find(o=>o.name===n.name);
    if(old&&n.rx!=null&&n.tx!=null&&old.rx!=null&&old.tx!=null&&n.rx>=old.rx&&n.tx>=old.tx){rx+=n.rx-old.rx;tx+=n.tx-old.tx;valid++;}
   });
   if(valid){down=rx/dt;up=tx/dt;}
  }
  if(cpu!=null)this.cpuReading=cpu;
  this.history.push({down,up,time:d.sample});if(this.history.length>30)this.history.shift();
  this.previous=d;this.error.hidden=true;
  this.refreshSimNames(d);
  const errorCount=Object.keys(d.errors||{}).length;
  this.live.replaceChildren(node('span','m-dot'),document.createTextNode((errorCount?'部分数据未获取 · ':'')+new Date(d.ts*1000).toLocaleTimeString('zh-CN',{hour12:false})));
  this.renderModems(d);this.renderPorts(d);this.renderNetwork(d);this.renderWifi(d);this.renderProduct(d);
  this.drawTraffic(d,down,up);this.renderResources(d,cpu);this.renderClients(d);this.renderVersion(d);
 },
 renderModems:function(d){
  const data={modems:d.modems,interfaces:d.interfaces,simFailure:this.simFailure};
  this.parts.modems.hidden=d.profile==='n5000'&&!list(d.modems).length;
  this.replace('modems',data,()=>{
   const cells=list(d.modems);
   return card('蜂窝网络','▥',cells.length?node('div','',cells.map(m=>{
    const nets=list(d.interfaces).filter(n=>n.device===m.device&&n.up);
    const ip=nets.flatMap(n=>list(n.ipv4)),v6=nets.flatMap(n=>list(n.ipv6));
    const rat=text(m.rat),band=m.band?String(m.band).split(/[, +]+/).map(b=>/^\d+$/.test(b)?(/NR|5G/i.test(rat)?'n':'B')+b:b).join(' + '):'—';
    const metric=(label,value,unit)=>node('div','',[node('label','',label),node('strong','',value==null?'—':String(value)),node('small','',unit)]);
    const detail=E('details',{'class':'m-inline-details','data-key':m.id},[
     node('summary','','更多蜂窝详情'),
     kv([['模组版本',m.firmware],['APN',m.apn],['PCI',m.pci],['小区',m.cell],['ARFCN',m.arfcn],
       ['模组温度',m.temperature!=null?m.temperature+' °C':null],['IPv4',addresses(ip)],['IPv6',addresses(v6)]])
    ]);
    return node('div','m-cell',[
     node('div','m-cell-head',[
      node('div','',[node('div','m-cell-title',[text(m.operator),badge(m.rat||'制式未获取','blue')]),node('div','m-cell-subtitle',text(m.label)+' · '+text(m.model))]),
      badge(ip.length||v6.length?'已获取地址':'地址未获取',ip.length||v6.length?'good':'')
     ]),
     node('div','m-signal',[metric('RSRP',m.rsrp,'dBm'),metric('RSRQ',m.rsrq,'dB'),metric('SINR',m.sinr,'dB'),metric('频段',band,'')]),
     kv([...(m.selected_sim?[['已选卡槽','SIM '+m.selected_sim+(m.selected_sim_type?' · '+m.selected_sim_type:'')]]:[]),
       ['当前 SIM',m.sim_label||(m.sim?'SIM '+m.sim:null)],['SIM 名称',m.sim_name||(this.simFailure?'云端名称暂不可用':m.sim_name_status)],['数据接口',m.device],
       ['识卡状态',m.pin==='0'||m.pin==='READY'?'已就绪':m.pin!=null?'状态 '+m.pin:null]]),
     detail,node('div','m-cell-age',m.age!=null?'信号缓存更新于 '+m.age+' 秒前':'信号采样时间未获取')
    ]);
   })):node('p','m-empty','暂无模组缓存，网络接口状态见下方。'),link('管理蜂窝',d.profile==='c2000ultra'?'admin/modem/qmodem/overview':'admin/modem'));
  });
 },
 renderPorts:function(d){
  this.replace('ports',d.ports,()=>card('物理网口','⌘',
   list(d.ports).length?node('div','m-port-grid',list(d.ports).map(p=>node('div','m-port'+(p.carrier?' up':''),[
    node('div','m-port-socket'),node('strong','',p.role||p.name),node('small','',p.name),
    badge(p.carrier?(p.speed?String(p.speed).replace(/F$/,' Mbps').replace(/H$/,' Mbps 半双工'):'链路已连接'):'未连接',p.carrier?'good':'')
   ]))):node('p','m-empty','未获取物理网口信息'),link('网络设置','admin/network/network')));
 },
 renderNetwork:function(d){
  this.replace('network',{interfaces:d.interfaces,ports:d.ports,profile:d.profile},()=>{
   const groups={};
   list(d.interfaces).forEach(n=>{const key=n.device||n.name;if(!groups[key])groups[key]=[];groups[key].push(n);});
   return card('网络接口','↗',node('div','',Object.entries(groups).map(([name,nets])=>{
    const ip=nets.flatMap(n=>list(n.ipv4)),v6=nets.flatMap(n=>list(n.ipv6));
    const routes=nets.flatMap(n=>list(n.routes)).filter(r=>Number(r.mask)===0);
    const dns=[...new Set(nets.flatMap(n=>list(n.dns)))];
    const active=nets.some(n=>n.up),label=nets.map(n=>n.name).join(' / ');
    const port=d.profile==='nbcpe688'&&list(d.ports).find(p=>p.name===name);
    const title=port?port.role:name==='br-lan'&&d.profile==='nbcpe688'?'局域网':name;
    return node('div','m-network-item',[
     node('div','m-network-title',[node('span','',[title,node('small','',' · '+(title!==name?name+' · ':'')+label)]),badge(active?'接口已启动':'接口未启动',active?'good':'')]),
     kv([['IPv4',addresses(ip)],['默认网关',[...new Set(routes.map(r=>r.nexthop).filter(Boolean))].join(' · ')||'无默认路由']]),
     E('details',{'class':'m-inline-details','data-key':name},[
      node('summary','','IPv6 与 DNS'),
      kv([['IPv6',addresses(v6)],['DNS',dns.join(' · ')||'未获取'],
       ['IPv6 前缀',addresses(nets.flatMap(n=>list(n.prefixes)))],['协议',nets.map(n=>n.proto).filter(Boolean).join(' / ')]])
     ])
    ]);
   })),link('接口详情','admin/network/network'));
  });
 },
 renderWifi:function(d){
  this.replace('wifi',d.wireless,()=>card('无线网络','⌁',
   list(d.wireless).length?node('div','',list(d.wireless).map(w=>node('div','m-wifi-row',[
    node('span','m-wifi-icon','⌁'),node('div','',[
     node('strong','',text(w.ssid)),node('small','',[
      w.frequency?(w.frequency<3000?'2.4 GHz':w.frequency<5925?'5 GHz':'6 GHz'):'频段未获取',
      ' · 信道 '+text(w.channel)+' · '+text(w.htmode)
     ])
    ]),badge(w.up?w.clients+' 台关联':'已关闭',w.up?'blue':'')
   ]))):node('p','m-empty','未获取无线接口'),link('设置与二维码','admin/network/wireless')));
 },
 renderProduct:function(d){
  this.replace('product',{board:d.board,product:d.product},()=>{
   const img=d.product?E('img',{class:'m-product-image',src:L.media('products/'+d.product),alt:text(d.board.model)}):node('div','m-generic-device','▤');
   if(d.product)img.addEventListener('error',()=>img.replaceWith(node('div','m-generic-device','▤')),{once:true});
   return node('section','m-card m-product',[
    node('span','m-product-kicker','YOUR DEVICE'),img,node('h2','',text(d.board.model)),
    node('span','m-product-line')
   ]);
  });
 },
 initTraffic:function(){
  this.down=node('strong','m-download','—');this.up=node('strong','m-upload','—');
  this.downUnit=node('small');this.upUnit=node('small');
  this.chart=document.createElementNS('http://www.w3.org/2000/svg','svg');
  this.chart.setAttribute('viewBox','0 0 360 140');this.chart.setAttribute('preserveAspectRatio','none');this.chart.setAttribute('role','img');this.chart.setAttribute('aria-label','各外网出口合计上下行实时流量柱状图');
  this.chartEmpty=node('div','m-chart-empty','等待下一次实际采样');
  this.chartSource=node('small');this.chartScale=node('small');
  this.trafficTotal=node('div','m-cell-age');
  this.connectionValue=node('strong');
  this.connectionLimit=node('small');
  this.accelerationName=node('strong','m-acceleration-name');
  this.ppeRows=node('div','m-ppe-rows');
  this.parts.traffic.replaceChildren(card('实时流量','▥',node('div','',[
   node('div','m-traffic-values',[
    node('div','',[node('label','','↓ 下载'),this.down,document.createTextNode(' '),this.downUnit]),
    node('div','',[node('label','','↑ 上传'),this.up,document.createTextNode(' '),this.upUnit])
   ]),
   node('div','m-chart',[this.chart,this.chartEmpty]),
   node('div','m-chart-note',[this.chartSource,this.chartScale]),this.trafficTotal,
   node('div','m-forwarding',[
    node('div','m-forwarding-summary',[
     node('div','',[node('label','','活动连接数'),this.connectionValue,this.connectionLimit]),
     node('div','',[node('label','','硬件加速'),this.accelerationName])
    ]),this.ppeRows
   ])
  ]),badge('多出口合计','blue')));
  this.parts.traffic.firstElementChild.classList.add('m-traffic');
 },
 drawTraffic:function(d,down,up){
  const rx=speed(down),tx=speed(up);this.down.textContent=rx[0];this.downUnit.textContent=rx[1];this.up.textContent=tx[0];this.upUnit.textContent=tx[1];
  const valid=this.history.filter(p=>p.down!=null),max=Math.max(1,...valid.flatMap(p=>[p.down,p.up]));
  const fragment=document.createDocumentFragment();
  this.history.forEach((p,i)=>{
   if(p.down==null)return;const x=(30-this.history.length+i)*12+1;
   [p.down,p.up].forEach((v,j)=>{
    const rect=document.createElementNS(this.chart.namespaceURI,'rect'),height=Math.max(v>0?1:0,Math.min(138,v/max*130));
    rect.setAttribute('x',String(x+j*5));rect.setAttribute('y',String(140-height));rect.setAttribute('width','4');rect.setAttribute('height',String(height));rect.setAttribute('rx','1.8');rect.setAttribute('fill',j?'#a16ce0':'#007aff');
    const title=document.createElementNS(this.chart.namespaceURI,'title');title.textContent=(j?'上传 ':'下载 ')+speed(v).join(' ');rect.append(title);fragment.append(rect);
   });
  });
  this.chart.replaceChildren(fragment);this.chartEmpty.hidden=valid.length>0;
  this.chartSource.textContent=list(d.uplinks).map(n=>n.name).join(' + ')||'暂无外网出口';
  this.chartScale.textContent=valid.length?'峰值 '+speed(max).join(' '):'按实际采样间隔计算';
  const totals=list(d.uplinks);
  this.trafficTotal.textContent=totals.length?'当前出口累计 ↓ '+bytes(totals.reduce((s,n)=>s+(n.rx||0),0))+'　↑ '+bytes(totals.reduce((s,n)=>s+(n.tx||0),0)):'尚无可统计的外网接口';
  this.connectionValue.textContent=d.connections==null?'—':Number(d.connections).toLocaleString('zh-CN');
  this.connectionLimit.textContent=d.connection_limit?' / '+Number(d.connection_limit).toLocaleString('zh-CN'):'';
  const acceleration=d.acceleration||{};
  this.accelerationName.textContent=acceleration.type||'状态未获取';
  this.ppeRows.replaceChildren(...list(acceleration.ppe).map(p=>{
   const percent=p.total>0&&p.used!=null?100*p.used/p.total:null;
   return node('div','m-ppe-row',[
    node('span','','PPE '+p.id),
    node('div','m-meter',E('span',{style:'width:'+(percent==null?0:Math.min(100,percent))+'%'})),
    node('small','',percent==null?'未获取':p.used+' / '+p.total+' · '+percent.toFixed(2)+'%')
   ]);
  }));
 },
 renderResources:function(d,cpu){
  const m=d.system.memory||{},used=m.total!=null?m.total-(m.available!=null?m.available:(m.free||0)+(m.buffered||0)+(m.cached||0)):null;
  const memory=m.total?100*used/m.total:null,storage=list(d.mounts).find(p=>p.mount==='/overlay')||list(d.mounts).find(p=>p.mount==='/');
  const percent=storage&&storage.size?100*(storage.size-storage.avail)/storage.size:null;
  const item=(label,value,description,percent)=>node('div','m-resource',[
   node('label','',label),node('strong','',value),node('p','',description),
   node('div','m-meter',E('span',{style:'width:'+(percent==null?0:Math.min(100,Math.max(0,percent)))+'%'}))
  ]);
  const cpuTemp=list(d.temperatures).find(t=>t.name==='CPU'),wifi=list(d.temperatures).filter(t=>t.name!=='CPU');
  this.parts.resources.replaceChildren(node('section','m-card m-resource-strip',[
   item('CPU 使用率',this.cpuReading==null?(d.cpu?'采样中':'未获取'):this.cpuReading.toFixed(1)+'%',
    (cpu==null?(this.cpuReading==null?'等待新采样':'上次采样 · 正在更新')+' · ':'')+
    (list(d.system.load).length?'负载 '+d.system.load.map(n=>(n/65536).toFixed(2)).join(' / '):'等待实际采样'),this.cpuReading),
   item('运行内存',memory==null?'—':memory.toFixed(0)+'%',bytes(used)+' / '+bytes(m.total),memory),
   item('设备温度',cpuTemp?cpuTemp.value.toFixed(1)+' °C':'—',wifi.length?'Wi-Fi '+wifi.map(t=>t.value+'°').join(' / '):'Wi-Fi 温度未获取',cpuTemp?cpuTemp.value:null),
   item('系统存储',percent==null?'—':percent.toFixed(1)+'%',storage?bytes(storage.size-storage.avail)+' / '+bytes(storage.size):'未获取',percent)
  ]));
 },
 initClients:function(){
  this.clients=[];
  this.clientSearch=E('input',{type:'search',placeholder:'搜索名称、IP 或 MAC','aria-label':'搜索终端'});
  this.clientFilter=E('select',{'aria-label':'筛选接入方式'},[
   E('option',{value:'all'},'全部终端'),E('option',{value:'wifi'},'无线关联'),E('option',{value:'lan'},'LAN 记录')
  ]);
  this.clientSearch.addEventListener('input',()=>this.drawClients());
  this.clientFilter.addEventListener('change',()=>this.drawClients());
  this.clientRows=node('tbody');
  this.clientCount=badge('正在读取');
  this.clientSummary=node('div','m-cell-age');
  this.clientCopyStatus=E('span',{'class':'m-copy-status','role':'status'});
  this.parts.clients.replaceChildren(card('终端与连接','◈',node('div','',[
   node('div','m-client-tools',[this.clientSearch,this.clientFilter]),
   node('div','m-table-scroll',node('table','m-table',[
    node('thead','',node('tr','',['终端','IP 地址','接入 / 状态','复制'].map(t=>node('th','',t)))),this.clientRows
   ])),this.clientSummary,this.clientCopyStatus,
   node('div','m-cell-age','LAN 邻居记录不等同于终端当前在线。')
  ]),this.clientCount));
  this.drawClients();
 },
 copyClient:async function(value){
  try{
   if(navigator.clipboard&&window.isSecureContext)await navigator.clipboard.writeText(value);
   else{
    const focus=document.activeElement,field=E('textarea',{'class':'m-copy-buffer',readonly:true},value);
    document.body.append(field);field.select();
    let copied;
    try{copied=document.execCommand('copy');}finally{field.remove();if(focus)focus.focus();}
    if(!copied)throw new Error('copy');
   }
   this.clientCopyStatus.textContent='已复制 '+value;
  }catch(e){this.clientCopyStatus.textContent='复制未成功，请手动选择地址。';}
 },
 drawClients:function(){
  const query=this.clientSearch.value.trim().toLowerCase(),filter=this.clientFilter.value;
  const clients=this.clients.filter(c=>{
   const matches=[c.name,c.ip,c.mac].some(v=>String(v||'').toLowerCase().includes(query));
   return matches&&(filter==='all'||(filter==='wifi'&&c.state==='无线已关联')||(filter==='lan'&&c.kind==='LAN'));
  });
  const copy=(label,value)=>E('button',{type:'button','class':'m-copy-button',disabled:!value,
   'aria-label':'复制'+label+(value?' '+value:''),click:()=>this.copyClient(value)},label);
  this.clientRows.replaceChildren(...(clients.length?clients.map(c=>node('tr','',[
   node('td','',[node('strong','',c.name||'未命名终端'),node('div','m-muted',c.mac)]),
   node('td','',text(c.ip)),node('td','',[c.kind,node('div','m-muted',c.state+(c.signal!=null?' · '+c.signal+' dBm':''))]),
   node('td','',node('div','m-copy-actions',[copy('IP',c.ip),copy('MAC',c.mac)]))
  ])):[node('tr','',E('td',{colspan:4},this.clients.length?'没有匹配的终端':'暂无终端记录'))]));
  this.clientSummary.textContent='显示 '+clients.length+' / '+this.clients.length+' 条终端记录';
 },
 renderClients:function(d){
  this.clientCount.textContent=(d.connections!=null?d.connections:'—')+' 条连接';
  if(same(this.clients,list(d.clients)))return;
  this.clients=list(d.clients);this.drawClients();
 },
 initManagement:function(){
  const items=[
   ['接口管理','WAN、LAN 与地址','admin/network/network'],
   ['无线管理','Wi-Fi 与接入设备','admin/network/wireless'],
   ['DHCP / DNS','租约与地址分配','admin/network/dhcp'],
   ['防火墙','转发与访问规则','admin/network/firewall'],
   ['网络诊断','Ping 与路由跟踪','admin/network/diagnostics'],
   ['连接详情','查看实时连接','admin/status/realtime/connections']
  ];
  this.parts.management.hidden=true;
  L.require('ui').then(ui=>ui.menu.load()).then(tree=>{
   const links=items.filter(item=>{
    const entry=item[2].split('/').reduce((branch,key)=>branch?.children?.[key],tree);
    return entry&&entry.satisfied!==false&&!entry.hidden;
   }).map(item=>E('a',{'class':'m-manage-link',href:L.url.apply(L,item[2].split('/'))},[
    node('strong','',item[0]),node('small','',item[1]),node('span','m-manage-arrow','↗')
   ]));
   if(links.length){this.parts.management.replaceChildren(card('网络管理','↗',node('div','m-manage-grid',links)));this.parts.management.hidden=false;}
  }).catch(()=>{this.parts.management.replaceChildren(card('网络管理','↗',node('p','m-empty','管理入口暂不可用，请刷新重试。')));this.parts.management.hidden=false;});
 },
 renderVersion:function(d){
  const data={board:d.board,uptime:d.system.uptime,time:d.system.localtime,mac:d.mac};
  this.replace('version',data,()=>card('设备信息','ⓘ',node('dl','m-version-grid',[
   ['固件版本',d.board.release?.version,true],['系统型号',d.board.model],['运行时间',duration(d.system.uptime)],
   ['内核',d.board.kernel],['架构',d.board.release?.target],['主机名',d.board.hostname],['设备 MAC',d.mac],
   ['系统时间',d.system.localtime?new Date(d.system.localtime*1000).toISOString().slice(0,19).replace('T',' '):null,true],
   ['主题','MYOS-NEW 1.0.4-r2',true]
  ].map(row=>node('div',row[2]?'wide':'',[node('dt','',row[0]),node('dd','',text(row[1]))]))),link('系统设置','admin/system/system')));
 }
});
