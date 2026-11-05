'use strict';
'require view';
'require request';


const list = v => Array.isArray(v) ? v : [];
const text = v => v == null || v === '' ? '未获取' : String(v);
const node = (tag, cls, children) => E(tag, cls ? {class:cls} : {}, children);
const badge = (value, tone) => node('span','m-badge '+(tone||''),value);
const link = (label,path) => E('a',{href:L.url.apply(L,path.split('/'))},label);
const glyph = name => {
 const shapes={
  '▥':'M3 19V5m5 14V9m5 10V3m5 16V7M2 21h20',
  '⌘':'M4 4h16v12h-4v4H8v-4H4zM8 7v4m4-4v4m4-4v4',
  '↗':'M4 17 10 11l4 4 7-10M15 5h6v6',
  '⌁':'M3 8a14 14 0 0 1 18 0M6 12a9 9 0 0 1 12 0M9 16a4 4 0 0 1 6 0M12 20h.01',
  '◈':'M4 4h16v12H4zM8 20h8m-4-4v4',
  'ⓘ':'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M12 11v6m0-10v.1',
  'topology':'M9 3h6v5H9zM3 16h6v5H3zm12 0h6v5h-6zM12 8v4M6 16v-4h12v4',
  'health':'M12 3 3 7v5c0 5 5 8 9 10 4-2 9-5 9-10V7zM8 12l3 3 5-6',
  'event':'M12 3a9 9 0 1 1-9 9M3 3v6h6M12 7v5l4 2'
 };
 const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');
 svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('aria-hidden','true');
 const path=document.createElementNS(svg.namespaceURI,'path');path.setAttribute('d',shapes[name]||shapes['▥']);svg.append(path);return svg;
};
const alias = (d,name) => (d.profile==='c8688'||d.profile==='nbcpe688')&&/^port[123]$/.test(name)?'lan'+name.slice(-1):name;
const leaseText = (expires,ts) => expires===0?'无限期':expires==null?'无 DHCP 租约':expires<=ts?'已到期':duration(expires-ts)+'后到期';
const card = (title,icon,body,extra) => node('section','m-card',[
 node('div','m-card-heading',[node('h2','',[node('span','m-section-icon',glyph(icon)),title]),extra||node('span')]),body
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


// Update existing nodes instead of replacing cards under the pointer/focus.
const patchTree=(old,fresh)=>{
 if(old.nodeType!==fresh.nodeType||old.nodeName!==fresh.nodeName){old.replaceWith(fresh);return;}
 if(old.nodeType===3){if(old.nodeValue!==fresh.nodeValue)old.nodeValue=fresh.nodeValue;return;}
 if(old.nodeType!==1)return;
 const keepOpen=old.tagName==='DETAILS',keepLens=old.classList.contains('m-card');
 for(const a of [...old.attributes])if(!fresh.hasAttribute(a.name)&&!(keepOpen&&a.name==='open')&&!(keepLens&&a.name==='style'))old.removeAttribute(a.name);
 for(const a of [...fresh.attributes])if(!(keepOpen&&a.name==='open')&&old.getAttribute(a.name)!==a.value)old.setAttribute(a.name,a.value);
 const a=[...old.childNodes],b=[...fresh.childNodes];
 for(let i=0;i<Math.max(a.length,b.length);i++){if(!a[i])old.append(b[i]);else if(!b[i])a[i].remove();else patchTree(a[i],b[i]);}
};

return view.extend({
 handleSave:null,handleSaveApply:null,handleReset:null,
 fetchData:function(full,allowFallback=true){
  const url=L.url('admin','myos',full===false?'fast':'data');
  return request.get(url,{cache:false,timeout:15000}).then(r=>{
   if(!r.ok)throw new Error('HTTP '+r.status);
   const d=r.json();
   if(d.needs_full&&allowFallback)return this.fetchData(true,false);
   if(!d.system||(!d.lite&&!d.board))throw new Error('状态响应无效');return d;
  });
 },
 load:function(){return this.fetchData(true).catch(e=>({failure:e.message}));},
 intervals:function(){return this.performanceMode==='realtime'?{fast:1000,slow:30000}:this.performanceMode==='eco'?{fast:10000,slow:60000}:this.performanceMode==='responsive'?{fast:3000,slow:30000}:{fast:5000,slow:30000};},
 scheduleNext:function(delay){
  clearTimeout(this.sampleTimer);
  if(document.hidden||this.paused||!this.root?.isConnected||this.disposed)return;
  this.sampleTimer=setTimeout(()=>this.refreshData(),delay==null?this.intervals().fast:delay);
 },
 refreshData:function(force){
  clearTimeout(this.sampleTimer);
  if(document.hidden||!this.root.isConnected||this.paused||this.disposed)return Promise.resolve();
  if(this.refreshing)return this.refreshing;
  if(this.dragging){this.scheduleNext(500);return Promise.resolve();}
  const started=performance.now();
  const full=force===true||!this.previous||Date.now()>=(this.fullNext||0);
  window.__MYOSDashboardBusy=true;
  this.refreshing=this.fetchData(full).then(d=>{
   if(this.disposed||!this.root.isConnected)return;
   if(!d.lite)this.fullNext=Date.now()+this.intervals().slow;
   this.failures=0;if(this.dragging||this.paused||document.hidden)this.pendingData=d;else{this.pendingData=null;this.update(d);}
  }).catch(e=>{this.failures=(this.failures||0)+1;this.showError(e.message);}).finally(()=>{
   this.refreshing=null;window.__MYOSDashboardBusy=false;
   const delay=this.failures?Math.min(60000,10000*this.failures):Math.max(250,this.intervals().fast-(performance.now()-started));
   this.scheduleNext(delay);
  });return this.refreshing;
 },
 mergeSample:function(fast){
  const old=this.previous;if(!old)return fast;
  const d={...old,...fast,lite:false,system:{...old.system,...fast.system}};
  ['ports','uplinks'].forEach(key=>{const values=new Map(list(fast[key]).map(p=>[p.name,p]));d[key]=list(old[key]).map(p=>({...p,...(values.get(p.name)||{})}));});
  return d;
 },
 refreshMaxModem:function(d){
  if(d.profile!=='c2000max'||document.hidden||this.modemRefresh||Date.now()<(this.modemNext||0))return Promise.resolve();
  this.modemNext=Date.now()+30000;
  this.modemRefresh=request.get(L.url('admin','myos','modem_refresh'),{cache:false,timeout:30000})
   .then(r=>{
    if(!r.ok)throw new Error('HTTP '+r.status);
    const data=r.json();
    if(Object.keys(data.errors||{}).length)throw new Error('模组信息暂不可用');
    this.maxSim=data.sim;this.modemFailure=null;
   })
   .catch(e=>{this.modemFailure=e.message;})
   .finally(()=>{this.modemRefresh=null;});
  return this.modemRefresh;
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
  this.previous=null;this.trafficPrevious=null;this.cpuReading=null;this.history=[];this.saved={};this.parts={};this.events=[];this.portRates={};this.paused=false;this.fullNext=Date.now()+30000;
  this.performanceMode='realtime';try{const v=localStorage.getItem('myos-liquid-performance-v3');if(['realtime','eco','balanced','responsive'].includes(v))this.performanceMode=v;}catch(_){}
  const slot=name=>this.parts[name]=node('div','m-slot m-slot-'+name);
  this.error=node('div','m-read-error');this.error.hidden=true;
  this.root=node('div','m-dashboard',[
   node('div','m-page-heading',[
    node('div','m-heading-copy',[node('p','m-eyebrow','MYOS / NETWORK'),node('h1','','网络控制中心')])
   ]),this.error,
   node('div','m-overview-row m-layout-zone',[slot('product'),slot('resources')]),
   node('div','m-dashboard-grid',[
    node('div','m-main-column m-layout-zone',[slot('traffic'),slot('modems'),slot('clients'),slot('network')]),
    node('div','m-side-column m-layout-zone',[slot('ports'),slot('wifi'),slot('topology'),slot('health'),slot('management'),slot('events'),slot('controls')]),
    node('div','m-layout-bottom m-layout-zone',[slot('version')])
   ])
  ]);
  this.initTraffic();
  this.initClients();
  this.initManagement();
  this.initControls();
  if(initial.failure)this.showError(initial.failure);else this.update(initial);
  this.onVisibility=()=>{clearTimeout(this.sampleTimer);if(!document.hidden){this.fullNext=0;this.scheduleNext(250);}};
  document.addEventListener('visibilitychange',this.onVisibility);
  window.addEventListener('pagehide',()=>{this.disposed=true;clearTimeout(this.sampleTimer);this.layout?.destroy();},{once:true});
  window.addEventListener('pageshow',event=>{if(event.persisted){this.disposed=false;this.scheduleNext(250);}});
  setTimeout(()=>{
   if(!this.root.isConnected)return;
   if(window.MYOSLayout)this.layout=window.MYOSLayout(this);
   this.scheduleNext(1000);
   if(typeof IntersectionObserver!=='undefined'){this.chartVisibility=new IntersectionObserver(entries=>{if(entries.some(e=>e.isIntersecting)&&this.previous)this.drawTraffic(this.previous,this.lastRates?.down,this.lastRates?.up);});this.chartVisibility.observe(this.parts.traffic);}
   window.dispatchEvent(new Event('myos:layout'));
  },0);
  return this.root;
 },
 showError:function(message){
  this.error.hidden=false;this.error.textContent='状态更新失败：'+message+'。'+(this.previous?'当前保留上次数据。':'请刷新页面重试。');
 },
 replace:function(name,data,render){
  if(same(this.saved[name],data))return;
  const fresh=render(),old=this.parts[name].querySelector(':scope > .m-card');
  if(old&&fresh.classList.contains('m-card'))patchTree(old,fresh);
  else {this.parts[name].querySelectorAll(':scope > :not(.m-layout-handle)').forEach(n=>n.remove());this.parts[name].append(fresh);}
  this.saved[name]=data;
 },
 update:function(d){
  const light=d.lite===true;if(light)d=this.mergeSample(d);
  const prev=this.previous,dt=prev?d.sample-prev.sample:0;
  if(prev&&(dt<0||(prev.cpu&&d.cpu&&d.cpu.total<prev.cpu.total)))this.cpuReading=null;
  let cpu=null,down=null,up=null;
  if(dt>0&&dt<=90&&prev.cpu&&d.cpu){
   const total=d.cpu.total-prev.cpu.total,idle=d.cpu.idle-prev.cpu.idle;
   if(total>0&&idle>=0)cpu=Math.max(0,Math.min(100,100*(total-idle)/total));
  }
  // Keep the traffic baseline separate from metadata and incomplete responses.
  const current=list(d.uplinks),base=this.trafficPrevious;
  const trafficDt=base?d.sample-base.sample:0;
  const counters=current.length>0&&current.every(n=>Number.isFinite(n.rx)&&Number.isFinite(n.tx));
  const reboot=base&&d.boot&&base.boot&&d.boot!==base.boot;
  if(reboot){this.history=[];this.lastRates=null;}
  if(counters&&base&&!reboot&&trafficDt>0){
   const old=new Map(list(base.uplinks).map(n=>[n.name,n]));
   if(current.length===old.size&&current.every(n=>old.has(n.name)&&n.rx>=old.get(n.name).rx&&n.tx>=old.get(n.name).tx)){
    down=current.reduce((v,n)=>v+n.rx-old.get(n.name).rx,0)/trafficDt;
    up=current.reduce((v,n)=>v+n.tx-old.get(n.name).tx,0)/trafficDt;
   }
  }
  if(counters&&(!base||d.sample>base.sample||reboot))
   this.trafficPrevious={sample:d.sample,boot:d.boot,uplinks:current.map(n=>({name:n.name,rx:n.rx,tx:n.tx}))};
  if(cpu!=null)this.cpuReading=cpu;
  if(down!=null&&up!=null){
   this.history.push({down,up,time:d.sample,interval:trafficDt});
   if(this.history.length>30)this.history.shift();
   this.lastRates={down,up};
  }else if(base&&trafficDt===0&&this.lastRates){
   down=this.lastRates.down;up=this.lastRates.up;
  }
  if(dt!==0)this.portRates={};
  if(dt>0&&dt<=90)list(d.ports).forEach(p=>{
   const old=list(prev.ports).find(q=>q.name===p.name);
   if(old&&p.rx!=null&&p.tx!=null&&old.rx!=null&&old.tx!=null&&p.rx>=old.rx&&p.tx>=old.tx)
    this.portRates[p.name]={rx:(p.rx-old.rx)/dt,tx:(p.tx-old.tx)/dt};
  });
  this.error.hidden=true;this.root.classList.remove('m-sampling-paused');

  if(d.profile==='c2000max'&&this.maxSim){
   const state=this.maxSim,slot=state.actual||null;
   d={...d,modems:list(d.modems).map(m=>m.id!==state.modem?m:{...m,sim:slot,
    sim_label:({'1':'SIM 卡 1','2':'SIM 卡 2','3':'内置卡 1'})[slot]||null,
    pin:state.cpin?state.cpin.toUpperCase():m.pin})};
  }
  this.previous=d;

  this.recordEvents(d,prev,light);this.renderTopology(d);this.renderHealth(d);this.renderEvents(d);this.renderPorts(d);this.renderProduct(d);
  this.drawTraffic(d,down,up);this.renderResources(d,cpu);
  if(!light||this.renderedSim!==this.maxSim){this.renderModems(d);this.renderedSim=this.maxSim;}
  if(!light){this.refreshMaxModem(d);this.renderNetwork(d);this.renderWifi(d);this.renderClients(d);this.renderVersion(d);}
  if(this.layout)this.layout.reconcile(d);
  if(!light)window.dispatchEvent(new Event('myos:layout'));

 },
 renderModems:function(d){
  const data={modems:d.modems,interfaces:d.interfaces,simFailure:this.simFailure,modemFailure:this.modemFailure};
  this.parts.modems.hidden=d.profile==='n5000'&&!list(d.modems).length;
  this.replace('modems',data,()=>{
   const cells=list(d.modems);
   return card('蜂窝网络','▥',cells.length?node('div','',cells.map(m=>{
    const nets=list(d.interfaces).filter(n=>n.device===m.device&&n.up);
    const ip=Array.isArray(m.ipv4)?m.ipv4:nets.flatMap(n=>list(n.ipv4));
    const v6=Array.isArray(m.ipv6)?m.ipv6:nets.flatMap(n=>list(n.ipv6));
    const rat=text(m.rat),band=m.band?String(m.band).split(/[, +]+/).map(b=>/^\d+$/.test(b)?(/NR|5G/i.test(rat)?'n':'B')+b:b).join(' + '):'—';
    const metric=(label,value,unit)=>node('div','',[node('label','',label),node('strong','',value==null?'—':String(value)),node('small','',unit)]);
    const qos=m.qos||{},rate=v=>v==null?'未获取':Number(v).toLocaleString('zh-CN',{maximumFractionDigits:3})+' Mbps';
    const contract=node('div','m-contract',[
     node('div','',[node('label','','签约下行'),node('strong','',rate(qos.down_mbps))]),
     node('div','',[node('label','','签约上行'),node('strong','',rate(qos.up_mbps))]),
     node('div','',[node('label','','QCI'),node('strong','',qos.qci==null?'未获取':String(qos.qci))])
    ]);
    const detail=E('details',{'class':'m-inline-details','data-key':m.id},[
     node('summary','','更多蜂窝详情'),
     kv([['模组版本',m.firmware],['APN',m.apn],['PCI',m.pci],['小区',m.cell],['ARFCN',m.arfcn],
       ['模组温度',m.temperature!=null?m.temperature+' °C':null],['签约信息来源',qos.source],['5QI（独立字段）',qos.fiveqi],['IPv4',addresses(ip)],['IPv6',addresses(v6)]])
    ]);
    return node('div','m-cell',[
     node('div','m-cell-head',[
      node('div','',[node('div','m-cell-title',[text(m.operator),badge(m.rat||'制式未获取','blue')]),node('div','m-cell-subtitle',text(m.label)+' · '+text(m.model))]),
      badge(ip.length||v6.length?'已获取地址':'地址未获取',ip.length||v6.length?'good':'')
     ]),
     node('div','m-signal',[metric('RSRP',m.rsrp,'dBm'),metric('RSRQ',m.rsrq,'dB'),metric('SINR',m.sinr,'dB'),metric('频段',band,'')]),contract,
     node('p','m-contract-note','AMBR 来自模组缓存，不代表实时网速；未上报时显示未获取。'),
     kv([...(m.selected_sim?[['已选卡槽',m.selected_sim_label||('SIM '+m.selected_sim+(m.selected_sim_type?' · '+m.selected_sim_type:''))]]:[]),
       ['当前 SIM',m.sim_label||(m.sim?'SIM '+m.sim:null)],['SIM 名称',m.sim_name||(this.simFailure?'云端名称暂不可用':m.sim_name_status)],['数据接口',m.device],
       ['识卡状态',m.pin==='0'||m.pin==='READY'?'已就绪':m.pin!=null?'状态 '+m.pin:null]]),
     detail,node('div','m-cell-age',(m.age!=null?'本次读取时缓存已存在 '+m.age+' 秒':'信号采样时间未获取')+(this.modemFailure?' · 刷新失败：'+this.modemFailure:''))
    ]);
   })):node('p','m-empty','暂无模组缓存，网络接口状态见下方。'),node('div','m-action-buttons',[
    link('管理蜂窝',d.profile==='nbcpe688'?'admin/modem-AK68/nets':(d.profile==='c2000ultra'||d.profile==='c2000max')?'admin/modem/qmodem/overview':'admin/modem')]));
  });
 },
 renderPorts:function(d){
  this.replace('ports',{ports:d.ports,rates:this.portRates,profile:d.profile},()=>card('端口管理','⌘',
   list(d.ports).length?node('div','m-port-grid',list(d.ports).map(p=>{
    const rate=this.portRates[p.name]||{};
    const label=alias(d,p.name),desc=p.carrier===true?'链路已连接':p.carrier===false?'未连接':'状态未获取';
    const detail=E('details',{class:'m-port-details','data-key':p.name},[
     node('summary','','详情'),kv([['实际接口',p.name],['角色',p.role],['MAC',p.mac],['MTU',p.mtu],['累计接收',bytes(p.rx)],['累计发送',bytes(p.tx)]])
    ]);
    return node('div','m-port'+(p.carrier?' up':''),[
     node('div','m-port-top',[node('div','m-port-socket'),node('span','m-port-led')]),
     node('strong','',label),node('small','',p.role||'物理网口'),
     badge(p.carrier&&p.speed?String(p.speed).replace(/F$/,' Mbps').replace(/H$/,' Mbps · 半双工').replace(/^(\d+)$/,'$1 Mbps'):desc,p.carrier?'good':''),
     node('div','m-port-traffic',[node('span','','RX '+speed(rate.rx).join(' ')),node('span','','TX '+speed(rate.tx).join(' '))]),detail
    ]);
   })):node('p','m-empty','未获取物理网口信息'),link('接口设置','admin/network/network')));
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
    const title=alias(d,name)==='br-lan'?'局域网':alias(d,name);
    return node('div','m-network-item',[
     node('div','m-network-title',[node('span','',[title,node('small','',' · '+label)]),badge(active?'接口已启动':'接口未启动',active?'good':'')]),
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
  this.replace('product',{board:d.board,profile:d.profile,product:d.product,uptime:Math.floor((d.system.uptime||0)/60)},()=>{
   const img=d.product?E('img',{class:'m-product-image',decoding:'async',width:128,height:110,src:L.media('products/'+d.product),alt:text(d.board.model)}):node('div','m-generic-device','▤');
   if(d.product)img.addEventListener('error',()=>img.replaceWith(node('div','m-generic-device','▤')),{once:true});
   return node('section','m-card m-product',[
    img,node('div','m-product-copy',[
     node('span','m-product-kicker',d.profile==='n5000'?'双网口路由器 · USB 3.2':'本地网关'),
     node('h2','',text(d.board.model)),node('small','','运行 '+duration(d.system.uptime)),
     ...(d.profile==='n5000'?[badge('ROUTER','blue')]:[])
    ])
   ]);
  });
 },
 initTraffic:function(){
  this.down=node('strong','m-download','—');this.up=node('strong','m-upload','—');
  this.downUnit=node('small');this.upUnit=node('small');
  this.chart=document.createElementNS('http://www.w3.org/2000/svg','svg');
  this.chart.setAttribute('viewBox','0 0 600 150');this.chart.setAttribute('preserveAspectRatio','none');this.chart.setAttribute('role','img');this.chart.setAttribute('aria-label','外网出口上下行曲线，横轴为有效采样顺序；每点按实际时间间隔计算平均速率');
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
  ]),this.sampleBadge=badge((this.intervals().fast/1000)+' 秒轻采样','blue')));
  this.parts.traffic.firstElementChild.classList.add('m-traffic');
 },
 drawTraffic:function(d,down,up){
  const rx=speed(down),tx=speed(up);this.down.textContent=rx[0];this.downUnit.textContent=rx[1];this.up.textContent=tx[0];this.upUnit.textContent=tx[1];
  const rect=this.parts.traffic.getBoundingClientRect();
  if(!this.parts.traffic.hidden&&rect.bottom>=0&&rect.top<=innerHeight){
   const ns=this.chart.namespaceURI,svg=(tag,attrs)=>{const el=document.createElementNS(ns,tag);Object.entries(attrs).forEach(([k,v])=>el.setAttribute(k,v));return el;};
   if(!this.chartPaths){
    const frag=document.createDocumentFragment();[20,60,100,140].forEach(y=>frag.append(svg('line',{x1:0,y1:y,x2:600,y2:y,class:'m-chart-gridline'})));
    this.chartPaths=['down','up'].map((key,i)=>{
     const fill=svg('path',{class:'m-chart-area '+key}),line=svg('path',{class:'m-chart-line '+key,fill:'none','stroke-width':2.2,'stroke-linecap':'round','stroke-linejoin':'round','vector-effect':'non-scaling-stroke'}),dot=svg('circle',{r:3,class:'m-chart-dot '+key});
     frag.append(fill,line,dot);return {fill,line,dot};
    });this.chart.replaceChildren(frag);
   }
   const valid=this.history.filter(p=>p.down!=null),max=Math.max(1,...valid.flatMap(p=>[p.down,p.up]));
   const span=Math.max(1,valid.length-1);
   ['down','up'].forEach((key,index)=>{
    let segment=[],lineD='',areaD='',last=null;
    const flush=()=>{if(!segment.length)return;const d=segment.map((p,i)=>(i?'L':'M')+p.x.toFixed(1)+','+p.y.toFixed(1)).join(' ');lineD+=d+' ';if(segment.length>1)areaD+=d+' L'+segment[segment.length-1].x.toFixed(1)+',148 L'+segment[0].x.toFixed(1)+',148 Z ';last=segment[segment.length-1];segment=[];};
    valid.forEach((p,i)=>{segment.push({x:4+i/span*592,y:138-Math.min(122,p[key]/max*122)});});flush();
    const els=this.chartPaths[index];els.line.setAttribute('d',lineD);els.fill.setAttribute('d',areaD);els.dot.style.display=last?'':'none';if(last){els.dot.setAttribute('cx',last.x);els.dot.setAttribute('cy',last.y);}
   });
   this.chartEmpty.hidden=valid.length>0;
   this.chartSource.textContent='出口：'+(list(d.uplinks).map(n=>alias(d,n.name)).join(' + ')||'未获取');
   this.chartScale.textContent=valid.length?'最近 '+valid.length+' 次有效采样 · 峰值 '+speed(max).join(' '):'等待实际采样';
   const latest=valid[valid.length-1];
   this.chartScale.title=latest?'本次实际采样间隔 '+latest.interval.toFixed(1)+' 秒；每点为区间平均速率。':'等待实际采样';
  }
  const totals=list(d.uplinks);
  this.trafficTotal.textContent=totals.length?'当前出口累计 ↓ '+bytes(totals.reduce((s,n)=>s+(n.rx||0),0))+'　↑ '+bytes(totals.reduce((s,n)=>s+(n.tx||0),0)):'尚无可统计的外网接口';
  this.connectionValue.textContent=d.connections==null?'—':Number(d.connections).toLocaleString('zh-CN');
  this.connectionLimit.textContent=d.connection_limit?' / '+Number(d.connection_limit).toLocaleString('zh-CN'):'';
  const acceleration=d.acceleration||{};
  this.accelerationName.textContent=acceleration.type||'未获取';
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
  const fresh=node('section','m-card m-resource-strip',[
   item('CPU 使用率',this.cpuReading==null?(d.cpu?'采样中':'未获取'):this.cpuReading.toFixed(1)+'%',
    (cpu==null?(this.cpuReading==null?'等待新采样':'上次采样 · 正在更新')+' · ':'')+
    (list(d.system.load).length?'负载 '+d.system.load.map(n=>(n/65536).toFixed(2)).join(' / '):'等待实际采样'),this.cpuReading),
   item('运行内存',memory==null?'—':memory.toFixed(0)+'%',bytes(used)+' / '+bytes(m.total),memory),
   item('设备温度',cpuTemp?Number(cpuTemp.value).toFixed(1)+' °C':'—',wifi.length?'Wi-Fi '+wifi.map(t=>t.value+'°').join(' / '):'无线温度不自动轮询',cpuTemp?cpuTemp.value:null),
   item('系统存储',percent==null?'—':percent.toFixed(1)+'%',storage?bytes(storage.size-storage.avail)+' / '+bytes(storage.size):'未获取',percent)
  ]);
  const old=this.parts.resources.querySelector('.m-resource-strip');if(old)patchTree(old,fresh);else this.parts.resources.append(fresh);
 },
 initClients:function(){
  this.clients=[];this.clientPage=0;this.clientPageSize=12;
  this.clientSearch=E('input',{type:'search',placeholder:'搜索设备名称、IP、MAC 或 SSID','aria-label':'搜索终端'});
  this.clientFilter=E('select',{'aria-label':'筛选接入方式'},[
   E('option',{value:'all'},'全部终端'),E('option',{value:'wifi'},'无线关联'),E('option',{value:'lan'},'LAN 邻居'),E('option',{value:'dhcp'},'仅租约记录')
  ]);
  this.clientSearch.addEventListener('input',()=>{this.clientPage=0;this.drawClients();});
  this.clientFilter.addEventListener('change',()=>{this.clientPage=0;this.drawClients();});
  this.clientRows=node('tbody');
  this.clientCount=badge('正在读取');
  this.clientSummary=node('div','m-cell-age');
  this.clientPager=node('div','m-client-pager');
  this.clientCopyStatus=E('span',{'class':'m-copy-status','role':'status'});
  this.parts.clients.replaceChildren(card('终端与连接','◈',node('div','',[
   node('div','m-client-tools',[this.clientSearch,this.clientFilter,E('button',{type:'button',class:'m-pill-button',click:()=>this.exportClients()},'导出 CSV')]),
   node('div','m-table-scroll',node('table','m-table',[
    node('thead','',node('tr','',['终端 / MAC','IP 地址','接入与信号','详情'].map(t=>node('th','',t)))),this.clientRows
   ])),node('div','m-client-footer',[this.clientSummary,this.clientPager]),this.clientCopyStatus,
   node('div','m-cell-age','无线关联可确认接入；DHCP / ARP 记录不代表当前在线。点“详情”查看租约与协商速率。')
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
   const matches=[c.name,c.ip,c.mac,c.ssid].some(v=>String(v||'').toLowerCase().includes(query));
   return matches&&(filter==='all'||(filter==='wifi'&&c.state==='无线已关联')||(filter==='lan'&&c.kind==='LAN')||(filter==='dhcp'&&c.kind==='DHCP'));
  });
  const totalPages=Math.max(1,Math.ceil(clients.length/this.clientPageSize));
  this.clientPage=Math.min(this.clientPage,totalPages-1);
  const shown=clients.slice(this.clientPage*this.clientPageSize,(this.clientPage+1)*this.clientPageSize);
  const copy=(label,value)=>E('button',{type:'button','class':'m-copy-button',disabled:!value,
   'aria-label':'复制'+label+(value?' '+value:''),click:()=>this.copyClient(value)},label);
  this.clientRows.replaceChildren(...(clients.length?shown.map(c=>node('tr','',[
   node('td','',[node('strong','',c.name||'未命名终端'),node('div','m-muted',c.mac)]),
   node('td','m-mono',text(c.ip)),node('td','',[node('span','m-client-state'+(c.state==='无线已关联'?' associated':''),c.ssid||c.kind),node('div','m-muted',c.state+(c.signal!=null?' · '+c.signal+' dBm':''))]),
   node('td','',node('div','m-copy-actions',[E('button',{type:'button',class:'m-copy-button',click:()=>this.showClient(c)},'详情'),copy('IP',c.ip)]))
  ])):[node('tr','',E('td',{colspan:4},this.clients.length?'没有匹配的终端':'暂无终端记录'))]));
  this.clientSummary.textContent='筛选 '+clients.length+' / '+this.clients.length+' 条记录 · 第 '+(this.clientPage+1)+' / '+totalPages+' 页';
  this.clientPager.replaceChildren(...(totalPages>1?[
   E('button',{type:'button',class:'m-copy-button',disabled:this.clientPage===0,click:()=>{this.clientPage--;this.drawClients();}},'上一页'),
   E('button',{type:'button',class:'m-copy-button',disabled:this.clientPage>=totalPages-1,click:()=>{this.clientPage++;this.drawClients();}},'下一页')]:[]));
 },
 renderClients:function(d){
  this.clientCount.textContent=list(d.clients).length+' 条记录';
  if(same(this.clients,list(d.clients)))return;
  this.clients=list(d.clients);this.drawClients();
 },
 initControls:function(){
  const select=E('select',{'aria-label':'首页采样模式'},[
   E('option',{value:'realtime'},'实时 · 1 秒 / 30 秒'),E('option',{value:'responsive'},'即时 · 3 秒 / 30 秒'),E('option',{value:'balanced'},'均衡 · 5 秒 / 30 秒'),E('option',{value:'eco'},'省资源 · 10 秒 / 60 秒')
  ]);select.value=this.performanceMode;
  select.addEventListener('change',()=>{this.performanceMode=select.value;try{localStorage.setItem('myos-liquid-performance-v3',select.value);}catch(_){}this.sampleBadge.textContent=(this.intervals().fast/1000)+' 秒轻采样';this.fullNext=0;this.scheduleNext(50);});
  this.parts.controls.append(card('布局与性能','ⓘ',node('div','m-layout-controls',[
   node('p','m-cell-age','拖动手柄调整卡片顺序与所在栏。布局只保存到当前浏览器，不改路由配置。'),
   node('div','m-action-buttons',[
    this.editButton=E('button',{type:'button',class:'m-pill-button',click:()=>this.layout?.toggleEdit()},'拖拽调整'),
    E('button',{type:'button',class:'m-pill-button',click:()=>this.layout?.picker()},'添加 / 隐藏卡片'),
    E('button',{type:'button',class:'m-pill-button',click:()=>this.layout?.reset()},'恢复默认布局')
   ]),node('label','m-performance-label',['采样模式',select]),
   node('p','m-cell-age','前者读取 CPU、内存和流量；后者刷新终端与模组缓存。后台页面停止采样。'),
   E('button',{type:'button',class:'m-copy-button',click:()=>{if(this.previous)this.refreshSimNames(this.previous);}},'按需读取 SIM 云端名称')
  ])));
 },
 initManagement:function(){
  const items=[
   ['接口管理','WAN、LAN 与地址','admin/network/network'],
   ['无线管理','Wi-Fi 与接入设备','admin/network/wireless'],
   ['DHCP / DNS','租约与地址分配','admin/network/dhcp'],
   ['防火墙','转发与访问规则','admin/network/firewall'],
   ['网络诊断','Ping 与路由跟踪','admin/network/diagnostics'],
   ['连接详情','查看实时连接','admin/status/realtime/connections'],
   ['系统日志','查阅故障与事件','admin/status/syslog']
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
 renderTopology:function(d){
  const clients=list(d.clients),wifi=clients.filter(c=>c.state==='无线已关联').length;
  const activePorts=list(d.ports).filter(p=>p.carrier===true).length;
  const up=list(d.uplinks),net=list(d.interfaces).find(n=>n.device==='br-lan');
  this.replace('topology',{up:up.map(n=>n.name),model:d.board.model,ip:net?.ipv4,wifi,activePorts,records:clients.length,radios:d.wireless},()=>{
   const endpoint=(icon,title,description,tone)=>node('div','m-topology-node '+(tone||''),[
    node('div','m-topology-icon',glyph(icon)),node('strong','',title),node('small','',description)
   ]);
   const branches=node('div','m-topology-branches',[
    node('div','m-topology-branch',[glyph('⌘'),node('div','',[node('strong','',activePorts+' 个网口有链路'),node('small','','不等同于有线终端数量')])]),
    node('div','m-topology-branch',[glyph('⌁'),node('div','',[node('strong','',wifi+' 台无线已关联'),node('small','',list(d.wireless).filter(w=>w.up).length+' 个无线接口启用')])])
   ]);
   return card('逻辑连接概览','topology',node('div','',[
    node('div','m-topology-flow',[
     endpoint('↗',up.length?up.length+' 个默认出口':'未获取默认出口',up.map(n=>alias(d,n.name)).join(' + ')||'请检查接口与路由',up.length?'active':'quiet'),
     node('div','m-topology-wire',node('span','','默认路由')),
     endpoint('topology','本地网关',net?addresses(net.ipv4):text(d.board.hostname),'gateway'),
     node('div','m-topology-wire',node('span','','本地连接')),branches
    ]),node('div','m-topology-foot',[node('span','',clients.length+' 条终端记录'),node('small','','逻辑示意，不推断物理拓扑；默认路由不代表互联网已连通。')])
   ]),badge('实时快照','blue'));
  });
 },
 renderHealth:function(d){
  const m=d.system.memory||{},available=m.available!=null?m.available:(m.free||0)+(m.buffered||0)+(m.cached||0);
  const rows=[],errors=Object.keys(d.errors||{}),cpu=list(d.temperatures).find(t=>t.name==='CPU');
  if(!d.cpu||!m.total)rows.push(['部分系统指标未读取','CPU 或内存数据缺失，无法完整判断资源状态。','warn']);
  if(!list(d.uplinks).length)rows.push(['未读取到默认出口','检查 WAN / 蜂窝接口和默认路由。','warn']);
  if(cpu&&cpu.value>=85)rows.push(['设备温度较高',Number(cpu.value).toFixed(1)+' °C · 提醒阈值 85 °C，非厂商限值。','warn']);
  if(m.total&&available/m.total<.1)rows.push(['可用内存低于 10%','请检查进程和当前服务负载。','warn']);
  if(this.cpuReading!=null&&this.cpuReading>=90)rows.push(['CPU 当前占用较高',this.cpuReading.toFixed(1)+'% · 关注持续高负载。','warn']);
  if(d.connections!=null&&d.connection_limit>0&&d.connections/d.connection_limit>.8)rows.push(['连接表使用超过 80%','当前 '+d.connections+' / '+d.connection_limit,'warn']);
  if(errors.length)rows.push(['部分采集源暂不可用',errors.join('、'),'warn']);
  const clear=!rows.length;
  this.replace('health',{rows,ports:list(d.ports).filter(p=>p.carrier).length},()=>card('状态巡检','health',node('div','',[
   node('div','m-health-hero '+(clear?'good':'warn'),[
    node('span','m-health-emblem',glyph('health')),node('div','',[node('strong','',clear?'未发现阈值告警':rows.length+' 项值得关注'),node('small','',clear?'仅针对当前可读取的指标':'查看下方提示定位问题')])
   ]),node('div','m-health-list',clear?[
    node('div','m-check-row',[node('span','m-dot'),node('span','','默认出口与系统资源已读取')]),
    node('div','m-check-row',[node('span','m-dot'),node('span','','链路与客户端状态持续采样')])
   ]:rows.slice(0,4).map(r=>node('div','m-health-warning',[node('strong','',r[0]),node('small','',r[1])]))),
   node('p','m-cell-age','不含主动 Ping、测速或安全漏洞扫描。')
  ]),badge(clear?'已检查':'需留意',clear?'good':'warn')));
 },
 recordEvents:function(d,prev,light){
  if(!prev)return;
  const add=(title,detail,tone)=>this.events.unshift({time:d.ts,title,detail,tone:tone||'info'});
  if(d.sample<prev.sample){add('设备运行计时已重置','可能发生了重启，流量计数重新采样。','warn');this.history=this.history.slice(-1);}
  else{
   list(d.ports).forEach(p=>{
    const old=list(prev.ports).find(q=>q.name===p.name);
    if(old&&typeof p.carrier==='boolean'&&typeof old.carrier==='boolean'&&p.carrier!==old.carrier)
     add(alias(d,p.name)+(p.carrier?' 链路已接通':' 链路已断开'),p.role||'物理网口',p.carrier?'good':'warn');
   });
   const bad=snapshot=>Object.keys(snapshot.errors||{}).some(k=>/iwinfo|wireless/.test(k));
   if(!light&&!bad(d)&&!bad(prev)){
    const before=list(prev.clients).filter(c=>c.state==='无线已关联'),after=list(d.clients).filter(c=>c.state==='无线已关联');
    const oldKeys=new Set(before.map(p=>p.mac)),newKeys=new Set(after.map(p=>p.mac));
    after.filter(c=>!oldKeys.has(c.mac)).forEach(c=>add('无线终端已关联',c.name||c.mac,'good'));
    before.filter(c=>!newKeys.has(c.mac)).forEach(c=>add('无线终端关联已消失',c.name||c.mac));
   }
  }
  this.events=this.events.slice(0,40);
 },
 renderEvents:function(d){
  this.replace('events',this.events.map(e=>({...e})),()=>card('连接事件','event',node('div','',[
   this.events.length?node('ol','m-event-list',this.events.slice(0,6).map(e=>node('li','',[
    node('span','m-event-dot '+e.tone),node('div','',[node('strong','',e.title),node('small','',e.detail)]),
    node('time','',new Date(e.time*1000).toLocaleTimeString('zh-CN',{hour12:false}))
   ]))):node('div','m-event-empty',[glyph('event'),node('strong','','正在观察连接变化'),node('small','','首次读取建立基线，不生成虚构历史。')]),
   node('p','m-cell-age','仅记录本次页面打开期间的变化，最多 40 条；显示最近 6 条。')
  ]),badge(this.events.length+' 条')));
 },
 exportClients:function(){
  const query=this.clientSearch.value.trim().toLowerCase(),filter=this.clientFilter.value;
  const rows=this.clients.filter(c=>[c.name,c.ip,c.mac,c.ssid].some(v=>String(v||'').toLowerCase().includes(query))&&
   (filter==='all'||filter==='wifi'&&c.state==='无线已关联'||filter==='lan'&&c.kind==='LAN'||filter==='dhcp'&&c.kind==='DHCP'));
  const csvCell=value=>{
   let s=String(value==null?'':value);if(/^[\s]*[=+\-@]/.test(s)||/^[\t\r\n]/.test(s))s="'"+s;
   return '"'+s.replace(/"/g,'""')+'"';
  };
  const body=[['名称','IP','MAC','接入','状态','SSID','信号 dBm','租约到期时间'],...rows.map(c=>[
   c.name,c.ip,c.mac,c.kind,c.state,c.ssid,c.signal,c.lease_expires===0?'无限期':c.lease_expires?new Date(c.lease_expires*1000).toISOString():''
  ])].map(r=>r.map(csvCell).join(',')).join('\r\n');
  const blob=new Blob(['\ufeff'+body],{type:'text/csv;charset=utf-8'}),url=URL.createObjectURL(blob),a=E('a',{href:url,download:'MYOS-clients-'+new Date().toISOString().slice(0,10)+'.csv'});
  document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),3000);
  this.clientCopyStatus.textContent='已导出当前筛选结果，共 '+rows.length+' 条记录。';
 },
 showClient:function(c){
  if(this.clientDialog)this.clientDialog.remove();
  const close=E('button',{type:'button',class:'m-icon-button','aria-label':'关闭终端详情'},'×');
  const dialog=E('dialog',{class:'m-client-dialog','aria-labelledby':'m-client-dialog-title'},[
   node('div','m-background-heading',[E('h2',{id:'m-client-dialog-title'},c.name||'终端详情'),close]),
   badge(c.state,c.state==='无线已关联'?'good':''),
   kv([['IPv4 地址',c.ip],['MAC 地址',c.mac],['接入接口',c.device||c.kind],['SSID',c.ssid],
    ['信号强度',c.signal!=null?c.signal+' dBm':null],['租约',leaseText(c.lease_expires,this.previous?.ts||Date.now()/1000)],
    ['接收协商速率',c.rx!=null?(Number(c.rx)/1000).toFixed(1)+' Mbps':null],
    ['发送协商速率',c.tx!=null?(Number(c.tx)/1000).toFixed(1)+' Mbps':null]]),
   node('p','m-cell-age','协商速率不是实际吞吐量；租约和 ARP 记录不证明设备当前在线。'),
   node('div','m-action-buttons',[
    E('button',{type:'button',class:'m-pill-button',disabled:!c.ip,click:()=>{this.copyClient(c.ip);}},'复制 IP'),
    E('button',{type:'button',class:'m-pill-button',disabled:!c.mac,click:()=>{this.copyClient(c.mac);}},'复制 MAC'),
    link('DHCP 设置','admin/network/dhcp')
   ])
  ]);
  close.addEventListener('click',()=>dialog.close());
  dialog.addEventListener('close',()=>{dialog.remove();if(this.clientDialog===dialog)this.clientDialog=null;});
  document.body.append(dialog);this.clientDialog=dialog;dialog.showModal();
 },

 renderVersion:function(d){
  const data={board:d.board,uptime:Math.floor(d.system.uptime/60),time:Math.floor(d.system.localtime/60),mac:d.mac};
  this.replace('version',data,()=>card('设备信息','ⓘ',node('dl','m-version-grid',[
   ['固件版本',d.board.release?.version,true],['系统型号',d.board.model],['运行时间',duration(d.system.uptime)],
   ['内核',d.board.kernel],['架构',d.board.release?.target],['主机名',d.board.hostname],['设备 MAC',d.mac],
   ['系统时间',d.system.localtime?new Date(d.system.localtime*1000).toISOString().slice(0,19).replace('T',' '):null,true],
   ['主题','MYOS-NEW 1.2.2-r2',true]
  ].map(row=>node('div',row[2]?'wide':'',[node('dt','',row[0]),node('dd','',text(row[1]))]))),link('系统设置','admin/system/system')));
 }
});
