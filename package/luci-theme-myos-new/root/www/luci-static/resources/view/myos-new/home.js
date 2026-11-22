'use strict';
'require view';
'require request';
'require rpc';

const firmwareStatus = rpc.declare({object:'myos.update',method:'status'});
const samplingRPC=Object.fromEntries(['fast','link_quality','signals'].map(method=>[
 method,rpc.declare({object:'myos-sampling',method,reject:true,nobatch:true,
  params:method==='fast'?['traffic_since','quality_since']:[]})
]));
const firmwareModels = {c5800688:'c5800-688',c8688:'c8-688',nbcpe688:'nbcpe-688',c2000max:'max',c2000ultra:'ultra',n5000:'n5000'};

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
const alias = (d,name) => {
 if(d.profile==='nbcpe688'&&/^port[123]$/.test(name))return 'lan'+(4-Number(name.slice(-1)));
 if(d.profile==='nbcpe688'&&name==='port5')return list(d.ports).find(port=>port.name===name)?.label||name;
 if(d.profile==='c8688'&&name==='port5')return 'WAN';
 return d.profile==='c8688'&&/^port[123]$/.test(name)?'lan'+name.slice(-1):name;
};
const leaseText = (expires,ts) => expires===0?'无限期':expires==null?'无 DHCP 租约':expires<=ts?'已到期':duration(expires-ts)+'后到期';
const card = (title,icon,body,extra) => node('section','m-card',[
 node('div','m-card-heading',[node('h2','',[node('span','m-section-icon',glyph(icon)),title]),extra||node('span')]),body
]);
const kv = pairs => node('dl','m-kv',pairs.map(([label,value,cls])=>node('div','',[
 node('dt','',label),node('dd',cls||'',value instanceof Node?value:text(value))
])));
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

const trafficWindow = 60;
const trafficChartTop = 20, trafficChartMiddle = 80, trafficChartBottom = 140;
const trafficAxisLimit = rate => {
 const bits = Math.max(1000, rate * 8);
 const unit = 10 ** Math.floor(Math.log10(bits));
 return unit * [1,2,5,10].find(step => unit * step >= bits) / 8;
};
const trafficTick = rate => {
 const [value,unit] = speed(rate);
 return Number(value)+' '+unit;
};

function trafficScale(samples, expanded){
 const rates=samples.map(point=>Math.max(point.down,point.up)).sort((a,b)=>a-b);
 const peak=rates.at(-1)||0;
 const limit=trafficAxisLimit(peak);
 // Use the 85th percentile so an isolated burst cannot set the detail scale.
 const typical=rates[Math.floor((rates.length-1)*0.85)]||0;
 const detail=trafficAxisLimit(typical);
 return {limit,detail:rates.length>=8&&peak>=detail*(expanded?6:8)?detail:null};
}

const secondsAgo = seconds => '−'+seconds+' 秒';
const maxSampleGap = interval => Math.max(12,3*interval/1000);
const samplingModes={
 realtime:{label:'实时',fast:1000,slow:30000},
 responsive:{label:'即时',fast:3000,slow:30000},
 balanced:{label:'均衡',fast:5000,slow:30000},
 eco:{label:'省资源',fast:10000,slow:60000}
};
const chartHistoryKey='myos-new-chart-history-v1';
const qualityFields = [
 ['latency','延迟','ms'],
 ['jitter','抖动','ms'],
 ['loss','丢包率','%']
];

const qualityX=(ts,start,end)=>end>start?1+598*(ts-start)/(end-start):599;

function qualityChart(fields, label, unit, inspect){
 const svg=(tag,attrs)=>{
  const el=document.createElementNS('http://www.w3.org/2000/svg',tag);
  Object.entries(attrs).forEach(([key,value])=>el.setAttribute(key,value));
  return el;
 };
 const plot=svg('svg',{viewBox:'0 0 600 70',preserveAspectRatio:'none',role:'img','aria-label':label});
 [5,35,65].forEach(y=>plot.append(svg('line',{x1:0,y1:y,x2:600,y2:y,class:'m-chart-gridline'})));
 const series=fields.map(([field])=>{
  const path=svg('path',{class:'m-quality-line m-quality-'+field,fill:'none','vector-effect':'non-scaling-stroke'});
  plot.append(path);
  return [field,path];
 });
 const cursor=inspect?svg('line',{class:'m-quality-cursor',y1:5,y2:65,visibility:'hidden','aria-hidden':'true'}):null;
 if(cursor)plot.append(cursor);
 const limit=node('span'),middle=node('span');
 return {
  plot,
  root:node('div','m-quality-chart',[node('div','m-quality-scale',[limit,middle,node('span','','0')]),plot]),
  draw(samples,start,end,max,selected){
   if(cursor){
    cursor.setAttribute('visibility',selected?'visible':'hidden');
    if(selected){
     const x=qualityX(selected.ts,start,end);
     cursor.setAttribute('x1',x);cursor.setAttribute('x2',x);
    }
   }
   const top=max+unit,mid=String(max/2);
   if(limit.textContent!==top)limit.textContent=top;
   if(middle.textContent!==mid)middle.textContent=mid;
   series.forEach(([field,path])=>{
    let previous=false;
    const commands=[];
    for(const sample of samples){
     if(!Number.isFinite(sample[field])){previous=false;continue;}
     const x=qualityX(sample.ts,start,end);
     const y=65-60*sample[field]/max;
     const point=x.toFixed(1)+','+y.toFixed(1);
     if(previous&&!sample.gap)commands.push('L'+point);
     else commands.push('M'+point+'h0.01');
     previous=true;
    }
    path.setAttribute('d',commands.join(' '));
   });
  }
 };
}

// Update existing nodes instead of replacing cards under the pointer/focus.
const patchTree=(old,fresh)=>{
 if(old.nodeType!==fresh.nodeType||old.nodeName!==fresh.nodeName){old.replaceWith(fresh);return;}
 if(old.nodeType===3){if(old.nodeValue!==fresh.nodeValue)old.nodeValue=fresh.nodeValue;return;}
 if(old.nodeType!==1)return;
 if(old.classList.contains('m-route-lookup')&&fresh.classList.contains('m-route-lookup'))return;
 const keepOpen=old.tagName==='DETAILS',keepLens=old.classList.contains('m-card');
 for(const a of [...old.attributes])if(!fresh.hasAttribute(a.name)&&!(keepOpen&&a.name==='open')&&!(keepLens&&a.name==='style'))old.removeAttribute(a.name);
 for(const a of [...fresh.attributes])if(!(keepOpen&&a.name==='open')&&old.getAttribute(a.name)!==a.value)old.setAttribute(a.name,a.value);
 if(old.classList.contains('m-home-interfaces')&&fresh.classList.contains('m-home-interfaces')){
  const existing=new Map(Array.from(old.children,row=>[row.dataset.key,row]));
  const focused=document.activeElement;
  let next=old.firstElementChild;
  for(const incoming of [...fresh.children]){
   const key=incoming.dataset.key,previous=existing.get(key);
   const row=previous||incoming;
   if(previous){patchTree(previous,incoming);existing.delete(key);}
   if(row!==next)old.insertBefore(row,next);
   next=row.nextElementSibling;
  }
  for(const row of existing.values())row.remove();
  if(old.contains(focused)&&document.activeElement!==focused)focused.focus({preventScroll:true});
  return;
 }
 const a=[...old.childNodes],b=[...fresh.childNodes];
 for(let i=0;i<Math.max(a.length,b.length);i++){if(!a[i])old.append(b[i]);else if(!b[i])a[i].remove();else patchTree(a[i],b[i]);}
};

return view.extend({
 handleSave:null,handleSaveApply:null,handleReset:null,
 sampleRequest:async function(method,endpoint,options){
  this.rpcRetryAt??={};
  if(samplingRPC[method]&&Date.now()>=(this.rpcRetryAt[method]||0)){
   try{
    const args=method==='fast'?[options.query.traffic_since,options.query.quality_since]:[];
    return await samplingRPC[method](...args);
   }
   catch(_){this.rpcRetryAt[method]=Date.now()+30000;}
  }
  const response=await request.get(L.url('admin','myos',endpoint),{cache:false,...options});
  if(!response.ok)throw new Error('HTTP '+response.status);
  return response.json();
 },
 fetchData:function(full,allowFallback=true){
  const endpoint=full===false?'fast':'data';
  const query={
   traffic_since:String(this.trafficHistoryTime||0),
   quality_since:String(this.qualityHistoryTime||0)
  };
  return this.sampleRequest(endpoint,endpoint,{query,timeout:15000}).then(d=>{
   if(d.needs_full&&allowFallback)return this.fetchData(true,false);
   if(!d.system||(!d.lite&&!d.board))throw new Error('状态响应无效');return d;
  });
 },
 load:function(){
  this.initialSample=null;
  return this.fetchData(true).then(async first=>{
   // Take two real counter samples before the browser paints the dashboard.
   await new Promise(resolve=>setTimeout(resolve,250));
   const next=await this.fetchData(false).catch(()=>null);
   if(!next)return first;
   if(next.boot!==first.boot)return next.lite?this.fetchData(true):next;
   this.initialSample=first;
   return next.lite?this.mergeSample(next,first):next;
  }).catch(e=>({failure:e.message}));
 },
 intervals:function(){
  return samplingModes[this.performanceMode];
 },
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
  const pingRevision=this.pingRevision;
  const full=force===true||!this.previous||(!this.resumeFast&&Date.now()>=(this.fullNext||0));
  this.resumeFast=false;
  window.__MYOSDashboardBusy=true;
  this.refreshing=this.fetchData(full).then(d=>{
   if(this.disposed||!this.root.isConnected)return;
   if(!d.lite)this.fullNext=Date.now()+this.intervals().slow;
   this.lastSampleAt=Date.now();
   this.failures=0;if(this.dragging||this.paused||document.hidden)this.pendingData=d;else{this.pendingData=null;this.update(d,pingRevision===this.pingRevision);}
  }).catch(e=>{this.failures=(this.failures||0)+1;this.showError(e.message);}).finally(()=>{
   this.refreshing=null;window.__MYOSDashboardBusy=false;
   const interval=this.intervals().fast;
   const delay=this.failures?Math.min(30000,interval*2**Math.min(this.failures-1,5)):Math.max(250,interval-(performance.now()-started));
   this.scheduleNext(this.resumeFast?0:delay);
  });return this.refreshing;
 },
 mergeSample:function(fast,baseline){
  const old=baseline||this.previous;if(!old)return fast;
  const d={...old,...fast,lite:false,system:{...old.system,...fast.system}};
  ['ports','uplinks'].forEach(key=>{
   const values=new Map(list(fast[key]).map(p=>[p.name,p]));
   d[key]=list(old[key]).map(port=>{
    const sample=values.get(port.name);
    const merged={...port,...sample,rx:sample?.rx,tx:sample?.tx};
    if(key==='ports'&&port.carrier!==merged.carrier){
     merged.client_ipv4=[];
     merged.networks=[];
    }
    return merged;
   });
  });
  return d;
 },
 scheduleSignals:function(delay){
  clearTimeout(this.signalTimer);this.signalTimer=null;
  if(document.hidden||this.paused||this.disposed||!this.root?.isConnected||!['c5800688','c8688','nbcpe688','c2000max','c2000ultra'].includes(this.previous?.profile))return;
  this.signalTimer=setTimeout(()=>this.refreshSignals(),delay==null?this.intervals().fast:delay);
 },
 refreshSignals:function(){
  clearTimeout(this.signalTimer);this.signalTimer=null;
  if(document.hidden||this.paused||this.disposed||!this.root?.isConnected||!['c5800688','c8688','nbcpe688','c2000max','c2000ultra'].includes(this.previous?.profile))return Promise.resolve();
  if(this.signalRequest)return this.signalRequest;
  if(this.dragging){this.scheduleSignals(1000);return Promise.resolve();}
  const started=performance.now();
  this.signalRequest=this.sampleRequest('signals','modem_refresh',{query:{signals:1},timeout:30000})
   .then(data=>{
    if(this.disposed||!this.root.isConnected)return;
    const signals={};
    Object.entries(data.signals||{}).forEach(([id,value])=>{
     const current={...value};
     ['rsrp','rsrq','sinr','operator','rat','band','band_summary','band_detail','carrier_aggregation',
      'sim','sim_label','sim_name','sim_id','sim_name_status','pin','apn','pci','cell','arfcn','temperature',
      'selected_sim','selected_sim_label','selected_sim_type'].forEach(key=>{current[key]=value[key]??null;});
     current.carriers=Array.isArray(value.carriers)?value.carriers:[];
     current.signal_note=value.signal_note||null;
     current.qos=value.qos||{};
     signals[id]=current;
    });
    list(this.previous?.modems).forEach(m=>{if(!signals[m.id])signals[m.id]={rsrp:null,rsrq:null,sinr:null,carriers:[]};});
    this.liveSignals=signals;
    this.refreshSimNames({...this.previous,modems:list(this.previous?.modems).map(m=>({...m,...signals[m.id]}))});
    this.signalErrors=data.errors||{};
   })
   .catch(e=>{
    this.signalErrors={service:e.message};
    this.liveSignals=Object.fromEntries(list(this.previous?.modems).map(m=>[m.id,{rsrp:null,rsrq:null,sinr:null,carriers:[]}]));
   })
   .finally(()=>{
    this.signalRequest=null;
    if(!this.disposed&&!document.hidden&&!this.dragging&&!this.paused&&this.root.isConnected&&this.previous){
     this.renderModems(this.previous);this.layout?.reconcile(this.previous);
    }
    this.scheduleSignals(Math.max(250,this.intervals().fast-(performance.now()-started)));
   });
  return this.signalRequest;
 },
 refreshQModem:function(d){
  if(!['c2000max','c2000ultra'].includes(d.profile)||document.hidden||this.modemRefresh||Date.now()<(this.modemNext||0))return Promise.resolve();
  this.modemNext=Date.now()+30000;
  this.modemRefresh=request.get(L.url('admin','myos','modem_refresh'),{cache:false,timeout:30000})
   .then(r=>{
    if(!r.ok)throw new Error('HTTP '+r.status);
    const data=r.json();
    if(Object.keys(data.errors||{}).length)throw new Error('模组信息暂不可用');
    if(d.profile==='c2000max')this.maxSim=data.sim;
    this.modemFailure=null;
   })
   .catch(e=>{this.modemFailure=e.message;})
   .finally(()=>{this.modemRefresh=null;});
  return this.modemRefresh;
 },
 refreshSimNames:function(d){
  const modems=list(d.modems),cards=modems.map(m=>m.sim_id||'').filter(Boolean).join('|');
  if(!cards||this.simLookup||(cards===this.simCards&&Date.now()<this.simNext))return;
  this.simCards=cards;this.simNext=Date.now()+60000;
  this.simLookup=request.get(L.url('admin','myos','sim_names'),{cache:false,timeout:25000})
   .then(r=>{
    if(!r.ok)throw new Error('HTTP '+r.status);
    const names=r.json(),matched={};
    modems.forEach(m=>{if(m.sim_id&&names[m.id])matched[m.sim_id]=names[m.id];});
    this.simNames={...(this.simNames||{}),...matched};this.simFailure=false;
    if(this.previous)this.renderModems(this.previous);
   })
   .catch(()=>{this.simFailure=true;})
   .finally(()=>{this.simLookup=null;});
 },
 restoreChartHistory:function(data){
  try{
   const saved=JSON.parse(sessionStorage.getItem(chartHistoryKey));
   if(!data.boot||saved?.boot!==data.boot)return;
   this.history=list(saved.traffic).filter(p=>p.time>=data.sample-trafficWindow&&p.time<=data.sample);
   const last=this.history.at(-1);
   const uplinks=list(data.uplinks).map(u=>u.name).sort();
   this.trafficGap=!!last&&(!same(saved.uplinks,uplinks)||data.sample-last.time>maxSampleGap(this.intervals().fast));
   for(const {name} of list(data.uplinks)){
    const samples=list(saved.quality?.[name]).filter(s=>s.ts>=data.ts-trafficWindow&&s.ts<=data.ts+1);
    if(samples.length)this.qualityHistory.set(name,{samples,target:samples.at(-1).target});
   }
  }catch(_){}
 },
 saveChartHistory:function(){
  if(!this.previous?.boot)return;
  try{
   const uplinks=list(this.previous.uplinks).map(u=>u.name).sort();
   const quality=Object.fromEntries(uplinks.map(name=>[name,this.qualityHistory.get(name)?.samples||[]]));
   sessionStorage.setItem(chartHistoryKey,JSON.stringify({boot:this.previous.boot,uplinks,traffic:this.history,quality}));
  }catch(_){}
 },
 render:function(initial){
  this.previous=null;this.trafficPrevious=null;this.trafficGap=false;this.cpuReading=null;this.history=[];this.saved={};this.parts={};this.events=[];this.portRates={};this.paused=false;
  if(!initial.failure&&this.initialSample){
   this.previous=this.initialSample;
   this.trafficPrevious={sample:this.initialSample.sample,boot:this.initialSample.boot,uplinks:list(this.initialSample.uplinks)};
  }
  this.initialSample=null;this.liveSignals={};this.signalErrors={};
  this.performanceMode='realtime';
  try{
   const saved=localStorage.getItem('myos-liquid-performance-v3');
   if(Object.hasOwn(samplingModes,saved))this.performanceMode=saved;
  }catch(_){}
  const {deviceModel,deviceBoard}=document.body.dataset;
  const identity=((deviceModel||'')+(deviceBoard||'')).toLowerCase().replace(/[^a-z0-9]/g,'');
  const c8Home=initial.profile==='c8688'||/(?:c8(?:new)?688|hcmt7981688)/.test(identity);
  this.c5800Home=deviceModel==='NRadio-C5800-688';
  this.compactHome=this.c5800Home||c8Home||initial.profile==='nbcpe688'||/(?:nbcpe|nb68)/.test(identity);
  this.wiredOnline=this.c5800Home||c8Home;
  const status=this.compactHome?this.initHomeStatus():null;
  this.fullNext=Date.now()+this.intervals().slow;
  const slot=name=>this.parts[name]=node('div','m-slot m-slot-'+name);
  this.error=node('div','m-read-error');this.error.hidden=true;
  this.firmwareUpdateEnabled=false;
  this.nextFirmwareStatus=0;
  this.firmwareVersion=node('span');
  this.firmwareNotice=E('a',{
   class:'m-firmware-update',href:L.url('admin','system','myos_update'),hidden:''
  },[this.firmwareVersion,E('span',{'aria-hidden':'true'},'→')]);
  this.root=node('div','m-dashboard m-home-refined'+(this.c5800Home?' m-c5800-home':this.compactHome?' m-home-compact':''),[
   node('div','m-page-heading',[
    node('h1','','网络控制中心'),...(status?[status]:[])
   ]),this.firmwareNotice,this.error,
   node('div','m-overview-row m-layout-zone',['product','resources','ports','uplinks'].map(slot)),
   node('div','m-dashboard-grid',[
    node('div','m-main-column m-layout-zone',[slot('traffic'),slot('clients'),slot('network'),slot('usage')]),
    node('div','m-side-column m-layout-zone',['quality','modems','wifi','topology','health','management'].map(slot)),
    node('div','m-layout-bottom m-layout-zone',['events','controls','version'].map(slot))
   ])
  ]);
  this.initTraffic();
  this.initQuality(initial.profile);
  this.initClients();
  this.initPortDialog();
  if(this.compactHome)this.initUplinkDialog(this.c5800Home||c8Home);
  this.initManagement();
  this.initControls();
  if(initial.failure)this.showError(initial.failure);else{
   this.restoreChartHistory(initial);
   this.update(initial);
  }
  this.onVisibility=()=>{
   clearTimeout(this.sampleTimer);clearTimeout(this.signalTimer);clearTimeout(this.qualityTimer);this.signalTimer=null;
   if(document.hidden){this.samplingLeftAt=Date.now();this.saveChartHistory();}
   else this.resumeSampling();
  };
  document.addEventListener('visibilitychange',this.onVisibility);
  window.addEventListener('focus',()=>{
   if(!document.hidden&&!this.disposed&&Date.now()-(this.lastSampleAt||0)>this.intervals().fast*2)
    this.resumeSampling();
  });
  window.addEventListener('pagehide',()=>{this.samplingLeftAt=Date.now();this.saveChartHistory();this.disposed=true;clearTimeout(this.sampleTimer);clearTimeout(this.signalTimer);clearTimeout(this.qualityTimer);this.signalTimer=null;this.layout?.destroy();});
  window.addEventListener('pageshow',event=>{if(event.persisted){this.disposed=false;this.layout?.reconcile(this.previous);this.resumeSampling();}});
  setTimeout(()=>{
   if(!this.root.isConnected)return;
   if(window.MYOSLayout)this.layout=window.MYOSLayout(this);
   this.scheduleNext();this.scheduleSignals(0);this.scheduleQuality(0);
   if(typeof IntersectionObserver!=='undefined'){this.chartVisibility=new IntersectionObserver(entries=>{this.chartInView=entries.some(e=>e.isIntersecting);if(this.chartInView&&this.previous)this.drawTraffic(this.previous,this.lastRates?.down,this.lastRates?.up);});this.chartVisibility.observe(this.parts.traffic);}
   window.dispatchEvent(new Event('myos:layout'));
  },0);
  return this.root;
 },
 resumeSampling:function(){
  this.pendingData=null;
  this.resumeFast=!!this.previous;this.fullNext=0;
  if(this.samplingLeftAt&&Date.now()-this.samplingLeftAt>maxSampleGap(this.intervals().fast)*1000)
   this.samplingPauseUntil=Date.now()+trafficWindow*1000;
  this.samplingLeftAt=0;
  this.setSamplingState('resuming');
  this.scheduleNext(0);this.scheduleSignals(0);
  if(!this.routerQualityHistory)this.scheduleQuality(0);
 },
 setSamplingState:function(state){
  if(!this.samplingState)return;
  this.samplingState.dataset.state=state;
  this.samplingState.textContent=state==='error'?'等待重试':state==='resuming'?'恢复采样':this.intervals().fast/1000+' 秒刷新';
  const resumed=!this.routerTrafficHistory&&Date.now()<(this.samplingPauseUntil||0);
  this.samplingNote.hidden=!resumed;
  this.samplingNote.textContent=resumed?'已恢复页面采样。':'';
 },
 showError:function(message){
  this.setSamplingState('error');
  this.error.hidden=false;this.error.textContent='状态更新失败：'+message+'。'+(this.previous?'当前保留上次数据。':'请刷新页面重试。');
 },
 refreshFirmwareNotice:function(){
  const model=firmwareModels[this.previous?.profile];
  if(!model||!this.firmwareUpdateEnabled||document.hidden||this.disposed||Date.now()<this.nextFirmwareStatus)return;
  this.nextFirmwareStatus=Date.now()+60000;
  firmwareStatus().then(status=>{
   if(this.disposed||!this.root.isConnected)return;
   const available=status?.available===true&&status.model===model&&
    /^V?\d+\.\d+\.\d+-\d{12}$/.test(status.latest);
   this.firmwareNotice.hidden=!available;
   if(available)this.firmwareVersion.textContent='发现新版本：'+status.latest;
  }).catch(()=>{});
 },
 replace:function(name,data,render){
  if(same(this.saved[name],data))return;
  const fresh=render(),old=this.parts[name].querySelector(':scope > .m-card');
  if(old&&fresh.classList.contains('m-card'))patchTree(old,fresh);
  else {this.parts[name].querySelectorAll(':scope > :not(.m-layout-handle)').forEach(n=>n.remove());this.parts[name].append(fresh);}
  this.saved[name]=data;
 },
 update:function(d,applyQuality=true){
  const trafficHistory=d.traffic_history,qualityHistory=d.quality_history,ping=d.ping;
  const light=d.lite===true;if(light)d=this.mergeSample(d);
  delete d.traffic_history;delete d.quality_history;delete d.ping;
  const prev=this.previous,dt=prev?d.sample-prev.sample:0;
  if(prev&&(dt<0||(prev.cpu&&d.cpu&&d.cpu.total<prev.cpu.total)))this.cpuReading=null;
  let cpu=null,down=null,up=null,links;
  if(dt>0&&dt<=90&&prev.cpu&&d.cpu){
   const total=d.cpu.total-prev.cpu.total,idle=d.cpu.idle-prev.cpu.idle;
   if(total>0&&idle>=0)cpu=Math.max(0,Math.min(100,100*(total-idle)/total));
  }
  // Keep the traffic baseline separate from metadata and incomplete responses.
  const current=list(d.uplinks),base=this.trafficPrevious;
  const trafficDt=base?d.sample-base.sample:0;
  const counters=current.length>0&&current.every(n=>Number.isFinite(n.rx)&&Number.isFinite(n.tx));
  const reboot=prev?.boot&&d.boot&&d.boot!==prev.boot;
  const maxTrafficInterval=maxSampleGap(this.intervals().fast);
  if(reboot){this.history=[];this.lastRates=null;this.trafficGap=false;this.trafficHistoryTime=0;this.qualityHistoryTime=0;}
  if(!trafficHistory&&counters&&base&&!reboot&&trafficDt>0&&trafficDt<=maxTrafficInterval){
   const old=new Map(list(base.uplinks).map(n=>[n.name,n]));
   if(current.length===old.size&&current.every(n=>old.has(n.name)&&n.rx>=old.get(n.name).rx&&n.tx>=old.get(n.name).tx)){
    down=current.reduce((v,n)=>v+n.rx-old.get(n.name).rx,0)/trafficDt;
    up=current.reduce((v,n)=>v+n.tx-old.get(n.name).tx,0)/trafficDt;
    if(this.compactHome)links=Object.fromEntries(current.map(n=>[n.name,{
     down:(n.rx-old.get(n.name).rx)/trafficDt,
     up:(n.tx-old.get(n.name).tx)/trafficDt
    }]));
   }
  }
  if(counters&&(!base||d.sample>base.sample||reboot))
   this.trafficPrevious={sample:d.sample,boot:d.boot,uplinks:current.map(n=>({name:n.name,rx:n.rx,tx:n.tx}))};
  if(cpu!=null)this.cpuReading=cpu;
  if(trafficHistory){
   for(const point of list(trafficHistory.points)){
    this.trafficHistoryTime=point.time;
    if(point.time<=(this.history.at(-1)?.time||0))continue;
    const previous=this.history.at(-1);
    if(previous&&point.time-previous.time>3)point.gap=true;
    this.history.push(point);
   }
  }else if(down!=null&&up!=null){
   const last=this.history.at(-1);
   if(last&&d.sample-last.time>maxTrafficInterval)this.trafficGap=true;
   this.history.push({down,up,time:d.sample,gap:this.trafficGap,links});
   this.trafficGap=false;
   this.lastRates={down,up};
  }else if(base&&trafficDt===0&&this.lastRates){
   down=this.lastRates.down;up=this.lastRates.up;
  }else if(base&&!reboot&&trafficDt>0){
   this.trafficGap=true;this.lastRates=null;
  }
  this.routerTrafficHistory=!!trafficHistory;
  while(this.history[0]?.time<d.sample-trafficWindow)this.history.shift();
  if(trafficHistory){
   const latest=this.history.at(-1);
   this.lastRates=latest&&d.sample-latest.time<=3?latest:null;
   down=this.lastRates?.down;up=this.lastRates?.up;
  }
  if(dt!==0)this.portRates={};
  if(dt>0&&dt<=90)list(d.ports).forEach(p=>{
   const old=list(prev.ports).find(q=>q.name===p.name);
   if(light&&old&&old.carrier!==p.carrier)this.fullNext=0;
    if(!reboot&&this.eventBoot===d.boot&&old&&typeof old.carrier==='boolean'&&typeof p.carrier==='boolean'&&old.carrier!==p.carrier){
     this.events.unshift({time:d.ts,kind:'port',device:p.name,up:p.carrier});
     if(this.events.length>40)this.events.pop();
    }
   if(old&&p.rx!=null&&p.tx!=null&&old.rx!=null&&old.tx!=null&&p.rx>=old.rx&&p.tx>=old.tx)
    this.portRates[p.name]={rx:(p.rx-old.rx)/dt,tx:(p.tx-old.tx)/dt};
  });
  this.previous=d;this.error.hidden=true;this.root.classList.remove('m-sampling-paused');
  this.setSamplingState('live');
  this.refreshFirmwareNotice();
  this.simNamesButton.hidden=d.profile==='n5000';

  if(d.profile==='c2000max'&&this.maxSim){
   list(d.modems).forEach(m=>{
    if(m.id!==this.maxSim.modem)return;
    m.sim=this.maxSim.actual||null;
    m.sim_label=({'1':'SIM 卡 1','2':'SIM 卡 2','3':'内置卡 1'})[m.sim]||null;
    if(this.maxSim.cpin)m.pin=this.maxSim.cpin.toUpperCase();
   });
  }

  if(!light)this.recordWirelessEvents(d);
  this.renderTopology(d);this.renderHealth(d);this.renderEvents(d);this.renderPorts(d);this.renderProduct(d);this.renderUplinks(d);
  this.drawTraffic(d,down,up);this.renderResources(d,cpu);
  if(!light)this.updateQualityLinks(d);
  if(applyQuality){
   this.setPingSettings(ping);
   this.updateQualityHistory(qualityHistory,reboot,d.ts);
  }
  if(!light){this.renderModems(d);this.renderNetwork(d);this.renderWifi(d);this.renderClients(d);this.renderVersion(d);}
  if(!light)this.refreshQModem(d);
  if(this.layout)this.layout.reconcile(d);
  if(this.compactHome)this.updateHomeStatus(d);
  if(!light)window.dispatchEvent(new Event('myos:layout'));
  if(!light)this.refreshSimNames(d);
  if(!this.signalTimer&&!this.signalRequest)this.scheduleSignals(0);

 },
 renderModems:function(d){
  const modems=list(d.modems).map(m=>{
   const signal=this.liveSignals?.[m.id];
   return signal?{...m,...signal,carriers:signal.carriers??list(m.carriers).map(c=>c.index===0?{...c,...signal}:c)}:m;
  });
  const data={modems,interfaces:d.interfaces,simNames:this.simNames,simFailure:this.simFailure,modemFailure:this.modemFailure,signalErrors:this.signalErrors};
  this.parts.modems.hidden=d.profile==='n5000'&&!list(d.modems).length;
  this.replace('modems',data,()=>{
   const cells=list(modems).slice();
   if(d.profile==='c5800688'){
    const order={cpe:0,cpe1:1};
    cells.sort((a,b)=>(order[a.id]??2)-(order[b.id]??2));
   }
   return card('蜂窝网络','▥',cells.length?node('div','',cells.map(m=>{
    const cloud=this.simNames?.[m.sim_id]||{};
    const nets=list(d.interfaces).filter(n=>n.device===m.device&&n.up);
    const ip=list(m.ipv4).length?m.ipv4:nets.flatMap(n=>list(n.ipv4));
    const v6=list(m.ipv6).length?m.ipv6:nets.flatMap(n=>list(n.ipv6));
    const carriers=d.profile==='c5800688'?list(m.carriers):[],carrierCount=d.profile==='c5800688'?m.carrier_count:0;
    const rat=text(m.rat),band=(d.profile==='c5800688'&&m.band_summary)||(m.band?String(m.band).split(/[, +]+/).sort((a,b)=>Number(b.match(/\d+/)?.[0]||0)-Number(a.match(/\d+/)?.[0]||0)).map(b=>/^\d+$/.test(b)?(/NR|5G/i.test(rat)?'n':'B')+b:b).join(' + '):'—');
    const reported=(value,unit)=>value==null||value===''?'未上报':String(value)+(unit||'');
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
       ...(['nbcpe688','c8688','c5800688'].includes(d.profile)?[['频段与带宽',m.band_detail],['载波聚合',m.carrier_aggregation]]:[]),
       ['模组温度',m.temperature!=null?m.temperature+' °C':null],['签约信息来源',qos.source],['5QI（独立字段）',qos.fiveqi],['IPv4',addresses(ip)],['IPv6',addresses(v6)]]),
     ...carriers.map(c=>node('div','',[
      node('h4','m-cell-subtitle',c.label+' · '+reported(c.band)),
      kv([['PCI',reported(c.pci)],['Cell ID',reported(c.cell)],
       ['下行频点',reported(c.dl_fcn)],['上行频点',reported(c.ul_fcn)],
       ['下行带宽',reported(c.down_mhz,' MHz')],['上行带宽',reported(c.up_mhz,' MHz')],
       ['RSRP',reported(c.rsrp,' dBm')],['RSRQ',reported(c.rsrq,' dB')],['SINR',reported(c.sinr,' dB')]])
     ]))
    ]);
    const signalError=this.signalErrors?.[m.id]||this.signalErrors?.service||this.modemFailure;
    return node('div','m-cell'+(d.profile==='nbcpe688'?' m-cell-nbcpe':''),[
     node('div','m-cell-head',[
      node('div','',[node('div','m-cell-title',[text(m.operator),badge(m.rat||'制式未获取','blue'),...(carrierCount>1?[badge(carrierCount+'CC','blue')]:[])]),node('div','m-cell-subtitle',text(m.label)+' · '+text(m.model))]),
      badge(ip.length||v6.length?'已获取地址':'地址未获取',ip.length||v6.length?'good':'')
     ]),
     node('div','m-signal',[metric('RSRP',m.rsrp,'dBm'),metric('RSRQ',m.rsrq,'dB'),metric('SINR',m.sinr,'dB'),metric('频段',band,'')]),contract,
     kv([...(m.selected_sim?[['已选卡槽',m.selected_sim_label||('SIM '+m.selected_sim+(m.selected_sim_type?' · '+m.selected_sim_type:''))]]:[]),
       ['当前 SIM',m.sim_label||(m.sim?'SIM '+m.sim:null)],['SIM 名称',cloud.name||m.sim_name||m.selected_sim_type||cloud.error||(this.simFailure?'云端名称暂不可用':m.sim_name_status)],['数据接口',m.device],
       ['识卡状态',m.pin==='0'||m.pin==='READY'?'已就绪':m.pin!=null?'状态 '+m.pin:null]]),
     detail,...(signalError?[node('div','m-cell-age','信号读取失败：'+signalError)]:m.signal_note?[node('div','m-cell-age',m.signal_note)]:[])
    ]);
   })):node('p','m-empty','暂无模组缓存，网络接口状态见下方。'),node('div','m-action-buttons',[
    link('管理蜂窝',d.profile==='nbcpe688'?'admin/modem-AK68/nets':(d.profile==='c2000ultra'||d.profile==='c2000max')?'admin/modem/qmodem/overview':'admin/modem')]));
  });
 },
 initPortDialog:function(){
  this.portDialogTitle=E('h2',{id:'m-port-dialog-title'},'端口详情');
  this.portDialogBody=node('div','m-port-dialog-body');
  this.portProbeResult=E('p',{role:'status'});
  this.portProbeButton=E('button',{type:'button',class:'cbi-button',click:()=>this.probePort()},'检测对端');
  this.portProbeControls=E('div',{hidden:true},[this.portProbeButton,this.portProbeResult]);
  const close=E('button',{type:'button',class:'m-icon-button','aria-label':'关闭端口详情',autofocus:''},'×');
  this.portDialog=E('dialog',{class:'m-client-dialog m-port-dialog','aria-labelledby':'m-port-dialog-title'},[
   node('div','m-background-heading',[this.portDialogTitle,close]),this.portDialogBody,this.portProbeControls
  ]);
  close.addEventListener('click',()=>this.portDialog.close());
  this.portDialog.addEventListener('close',()=>{
   const trigger=[...this.parts.ports.querySelectorAll('button[data-port]')].find(button=>button.dataset.port===this.portDialogName);
   this.portDialogName=null;
   trigger?.focus({preventScroll:true});
  });
  this.parts.ports.addEventListener('click',event=>{
   const trigger=event.target.closest('button[data-port]');
   if(!trigger)return;
   this.portDialogName=trigger.dataset.port;
   this.portProbeResult.textContent='';
   this.updatePortDialog(this.previous,this.portSummaries);
   this.portDialog.showModal();
  });
  this.root.append(this.portDialog);
 },
 probePort:async function(){
  const port=this.portDialogName;
  this.portProbeButton.disabled=true;
  this.portProbeResult.textContent='正在检测…';
  try{
   const response=await request.post(L.url('admin','myos','port_probe'),
    new URLSearchParams({token:L.env.token,port}).toString(),
    {headers:{'Content-Type':'application/x-www-form-urlencoded'},timeout:6000});
   const result=response.json();
   if(!response.ok||result.state==='error')throw new Error(result.error||'对端检测失败');
   if(this.portDialogName!==port)return;
   const replies=[];
   if(result.dhcp){
    const offer=result.dhcp;
    replies.push('DHCP 应答：'+text(offer.gw||offer.srv));
    if(offer.ip)replies.push('分配地址：'+offer.ip+(offer.mask?'，掩码 '+offer.mask:''));
    if(offer.overlap)replies.push('对端网段与本机 LAN '+offer.overlap+' 重叠');
    else if(offer.overlap===false)replies.push('对端网段与本机 LAN 无重叠');
   }
   if(result.pppoe)replies.push('PPPoE 应答：'+text(result.pppoe.ac||result.pppoe.mac));
   this.portProbeResult.textContent=replies.join(' · ')||
    (result.state==='nolink'?'网口未连接':'未收到 DHCP 或 PPPoE 应答');
  }catch(error){
   if(this.portDialogName===port)this.portProbeResult.textContent=error.message||'对端检测失败';
  }finally{this.portProbeButton.disabled=false;}
 },
 updatePortDialog:function(d,summaries){
  const port=list(d?.ports).find(p=>p.name===this.portDialogName);
  const summary=summaries?.find(p=>p.name===this.portDialogName);
  if(!port||!summary){
   if(this.portDialog.open)this.portDialog.close();
   return;
  }
  const title=alias(d,port.name)+' 端口详情';
  this.portProbeControls.hidden=!d.port_probe;
  if(this.portDialogTitle.textContent!==title)this.portDialogTitle.textContent=title;
  const rate=this.portRates[port.name]||{};
  const lan=port.role==='LAN'&&['c5800688','c8688','nbcpe688'].includes(d.profile);
  const down=lan?rate.tx:rate.rx,up=lan?rate.rx:rate.tx;
  patchTree(this.portDialogBody,node('div','m-port-dialog-body',[
   node('div','m-port-traffic',[node('span','m-download','↓ 下载 '+speed(down).join(' ')),node('span','m-upload','↑ 上传 '+speed(up).join(' '))]),
   kv([['实际接口',port.name],['角色',port.role],['MAC',port.mac],['MTU',port.mtu],['IP 地址',summary.ips.join(' · ')],
    ['累计接收',bytes(port.rx)],['累计发送',bytes(port.tx)]])
  ]));
 },
 renderPorts:function(d){
  const ports=list(d.ports);
  const status=ports.map(p=>{
   let ips=[];
   if(p.carrier===true){
    if(p.role==='LAN')ips=list(p.client_ipv4);
    else{
     const networks=list(p.networks).filter(n=>n.up===true);
     ips=networks.flatMap(n=>list(n.ipv4).map(ip=>ip.address));
     if(!ips.length)ips=networks.flatMap(n=>list(n.ipv6).map(ip=>ip.address));
    }
   }
   return {name:p.name,role:p.role,carrier:p.carrier,speed:p.speed,ips:[...new Set(ips.filter(Boolean))]};
  });
  this.portSummaries=status;
  this.replace('ports',{ports:status,profile:d.profile},()=>card('端口管理','⌘',
   status.length?node('div','m-port-grid',status.map(p=>{
    const label=alias(d,p.name),desc=p.carrier===true?'链路已连接':p.carrier===false?'未连接':'状态未获取';
    const ip=p.ips.length?p.ips[0]+(p.ips.length>1?' +'+(p.ips.length-1):''):'';
    const cls='m-port'+(p.carrier?' up':'');
    const title=node(this.compactHome?'span':'div','m-port-title',[E('span',{class:'m-port-led','aria-hidden':'true'}),node('strong','',label)]);
    const readings=[
     node('small','',p.role||'物理网口'),
     badge(p.carrier&&p.speed?String(p.speed).replace(/F$/,' Mbps').replace(/H$/,' Mbps · 半双工').replace(/^(\d+)$/,'$1 Mbps'):desc,p.carrier?'good':''),
     E(this.compactHome?'span':'div',{class:'m-port-ip',title:p.ips.join(' · ')},ip)
    ];
    if(this.compactHome)return E('button',{
     type:'button',class:cls+' m-port-action','data-port':p.name,'aria-haspopup':'dialog'
    },[title,...readings]);
    return node('div',cls,[
     title,E('button',{type:'button',class:'m-port-button','data-port':p.name,'aria-label':label+' 详情','aria-haspopup':'dialog'},'详情'),
     ...readings
    ]);
   })):node('p','m-empty','未获取物理网口信息'),link('接口设置','admin/network/network')));
  if(this.portDialog.open)this.updatePortDialog(d,status);
 },
 renderUplinks:function(d){
  const c5800=d.profile==='c5800688';
  const c8=d.profile==='c8688';
  const nbcpe=d.profile==='nbcpe688';
  const lineNames={cpe:'内置 5G',cpe1:'PoE 副 5G',wan:'有线 WAN',wan6:'有线 WAN'};
  const rows=list(d.uplinks).map(u=>{
   const nets=list(d.interfaces).filter(n=>n.device===u.name&&n.up===true&&list(n.routes).some(r=>Number(r.mask)===0));
   const addresses=[...new Set(nets.flatMap(n=>list(n.ipv4).map(ip=>ip.address).concat(list(n.ipv6).map(ip=>ip.address))).filter(Boolean))];
   const gateways=[...new Set(nets.flatMap(n=>list(n.routes).filter(r=>Number(r.mask)===0).map(r=>r.nexthop)).filter(g=>g&&g!=='0.0.0.0'&&g!=='::'))];
   const labels=c5800||c8?[...new Set(nets.map(n=>lineNames[n.name.replace(/_[46]$/,'')]).filter(Boolean))]:[];
   const physical=c8?{eth1:'内置 5G',port5:'有线 WAN'}[u.name]:nbcpe?u.label:null;
   return {device:u.name,name:labels.length===1?labels[0]:physical||alias(d,u.name),networks:nets.map(n=>n.name).join(' / '),
    ip:addresses[0],gateway:gateways[0],carrier:u.carrier,...(this.uplinkDialog?{addresses,gateways}:{}),
    mainRoutes:u.main_routes};
  });
  this.uplinkLabels=new Map(rows.map(row=>[row.device,row.name]));
  const routeLookup=c5800||c8;
  this.replace('uplinks',{rows,routeLookup,c5800},()=>{
   if(this.uplinkDialog)return this.renderCompactUplinks(rows);
   const body=rows.length?node('div','m-uplink-list',rows.map(r=>
    node('div','m-uplink-row',[
     node('div','m-uplink-heading',[
      node('div','m-uplink-id',[node('strong','',r.name),
       ...(r.networks&&r.networks!==r.name?[node('small','',r.networks)]:[])]),
      badge(r.carrier===true?'链路接通':r.carrier===false?'链路断开':'状态未知',r.carrier===true?'good':'')
     ]),
     kv([['地址',r.ip||'—'],['网关',r.gateway||'—']])
    ]))):node('p','m-empty','未读取到默认路由');
   return card('当前出口','↗',routeLookup?node('div','',[body,this.routeLookupForm()]):body);
  });
 },
 renderCompactUplinks:function(rows){
  const mainMetrics=routes=>[...list(routes)].sort((a,b)=>Number(!!a.source)-Number(!!b.source)).map(route=>
   (route.metric??'—')+(route.source?'（源前缀 '+route.source+'）':'')
  ).join(' · ')||'—';
  const summary=rows.length?node('div','m-uplink-list',rows.map(r=>
   node('div','m-uplink-compact',[
    node('strong','',r.name),
    E('span',{class:'m-uplink-address',title:r.ip||''},r.ip||'—'),
    badge(r.carrier===true?'链路接通':r.carrier===false?'链路断开':'状态未知',r.carrier===true?'good':'')
   ])
  )):node('p','m-empty','未读取到默认路由');
  patchTree(this.uplinkDetailRows,node('div','m-uplink-detail-rows',rows.map(r=>node('div','',[
    node('strong','',r.name),
    kv([['实际接口',r.device],['网络',r.networks||'—'],
     ['地址',r.addresses.join(' · ')||'—'],['网关',r.gateways.join(' · ')||'—'],
     ['IPv4 主表优先级',mainMetrics(r.mainRoutes?.ipv4)],
     ['IPv6 主表优先级',mainMetrics(r.mainRoutes?.ipv6)]])
   ]))));
  const trigger=E('button',{
   type:'button',class:'m-copy-button','data-uplink-details':'','aria-haspopup':'dialog',
   click:()=>this.uplinkDialog.showModal()
  },this.uplinkHasRouteLookup?'详情与选路':'线路详情');
  return card('当前出口','↗',summary,trigger);
 },
 initUplinkDialog:function(hasRouteLookup){
  this.uplinkHasRouteLookup=hasRouteLookup;
  const title=E('h2',{id:'m-uplink-dialog-title'},hasRouteLookup?'线路详情与选路':'线路详情');
  const close=E('button',{type:'button',class:'m-icon-button','aria-label':'关闭线路详情',autofocus:''},'×');
  this.uplinkDetailRows=node('div','m-uplink-detail-rows');
  this.uplinkDialog=E('dialog',{class:'m-client-dialog m-uplink-dialog','aria-labelledby':'m-uplink-dialog-title'},[
   node('div','m-background-heading',[title,close]),this.uplinkDetailRows,
   ...(hasRouteLookup?[this.routeLookupForm()]:[])
  ]);
  close.addEventListener('click',()=>this.uplinkDialog.close());
  this.uplinkDialog.addEventListener('close',()=>{
   this.parts.uplinks.querySelector('button[data-uplink-details]')?.focus({preventScroll:true});
  });
  this.root.append(this.uplinkDialog);
 },
 routeLookupForm:function(){
  const target=E('input',{type:'text',class:'cbi-input-text',required:'',maxlength:45,
   'aria-label':'目标 IP 地址',placeholder:'IPv4 或 IPv6 地址',spellcheck:'false'});
  const result=E('p',{role:'status',class:'m-muted'});
  const submit=E('button',{type:'submit',class:'cbi-button'},'查询本机选路');
  const form=E('form',{class:'m-route-lookup'},[target,' ',submit,result]);
  form.addEventListener('submit',async event=>{
   event.preventDefault();
   submit.disabled=true;
   result.textContent='正在查询…';
   try{
    const response=await request.post(L.url('admin','myos','route_lookup'),
     new URLSearchParams({token:L.env.token,target:target.value}).toString(),
     {headers:{'Content-Type':'application/x-www-form-urlencoded'},timeout:6000});
    const data=response.json();
    result.textContent=data.error||[
     data.target+' 经 '+data.path+'（'+data.device+'）',
     data.gateway?'网关 '+data.gateway:null,data.source?'源地址 '+data.source:null
    ].filter(Boolean).join(' · ');
   }catch(_){result.textContent='查询失败，请重试';}
   finally{submit.disabled=false;}
  });
  return form;
 },
 renderNetwork:function(d){
  this.replace('network',{interfaces:d.interfaces,ports:d.ports,profile:d.profile,labels:Array.from(this.uplinkLabels)},()=>{
   const groups=new Map();
   for(const net of list(d.interfaces)){
    const key=net.device||net.name;
    if(!groups.has(key))groups.set(key,[]);
    groups.get(key).push(net);
   }
   const rows=Array.from(groups,([name,nets])=>{
    const ip=nets.flatMap(n=>list(n.ipv4)),v6=nets.flatMap(n=>list(n.ipv6));
    const routes=nets.flatMap(n=>list(n.routes)).filter(r=>Number(r.mask)===0);
    const dns=[...new Set(nets.flatMap(n=>list(n.dns)))];
    const active=nets.some(n=>n.up);
    const cellular=nets.find(n=>n.proto==='wwan');
    let role=this.uplinkLabels.get(name);
    if(d.profile==='c5800688'&&cellular){
     if(cellular.name==='cpe')role='内置 5G';
     if(cellular.name==='cpe1')role='PoE 副 5G';
    }
    if(!role&&d.profile==='c8688')role={eth1:'内置 5G',port5:'有线 WAN'}[name];
    if(!role&&d.profile==='c5800688'&&nets.some(n=>/^wan(?:6|_[46])?$/.test(n.name)))role='有线 WAN';
    const label=name==='br-lan'||nets.some(n=>n.name==='lan')?'局域网':role||alias(d,name);
    const primary=ip[0]||v6[0];
    const gateways=[...new Set(routes.map(r=>r.nexthop).filter(g=>g&&g!=='0.0.0.0'&&g!=='::'))];
    return E('details',{class:'m-home-interface','data-key':name},[
     node('summary','',[
      node('strong','',label),
      node('span','m-interface-primary',primary?addresses([primary]):'未分配地址'),
      badge(active?'已启动':'未启动',active?'good':'')
     ]),
     kv([['设备',name],['逻辑接口',node('span','m-interface-links',nets.map(n=>n.name==='loopback'?n.name:
      E('a',{href:L.url('admin','network','network')+'?iface='+encodeURIComponent(n.name),
       'aria-label':'查看 '+n.name+' 接口详情'},n.name)))],['IPv4',addresses(ip)],['IPv6',addresses(v6)],
      ['默认网关',gateways.join(' · ')||(routes.length?'直连':'无默认路由')],
      ['DNS',dns.join(' · ')||(active?'未配置':'未分配')],['IPv6 前缀',addresses(nets.flatMap(n=>list(n.prefixes)))],
      ['协议',[...new Set(nets.map(n=>n.proto).filter(Boolean))].join(' / ')]])
    ]);
   });
   return card('网络接口','↗',rows.length?node('div','m-home-interfaces',rows):node('p','m-empty','暂无接口记录'),link('接口详情','admin/network/network'));
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
    ]),w.up&&w.name?E('button',{type:'button',class:'m-pill-button','data-wifi-clients':w.name,
    'aria-label':'查看 '+text(w.ssid)+' · '+text(w.name)+' 的关联终端'},w.clients+' 台关联'):badge('已关闭')
   ]))):node('p','m-empty','未获取无线接口'),link('设置与二维码','admin/network/wireless')));
 },
 initHomeStatus:function(){
  this.homeStatus={};
  const wiredFilter=this.wiredOnline?'wired-online':'lan';
  const metrics=[
   ['ports','网口已连接','ports'],
   ['wired',this.wiredOnline?'有线在线':'LAN 邻居','clients',wiredFilter],
   ['wifi','无线关联','clients','wifi']
  ];
  return E('div',{class:'m-home-status',role:'group','aria-label':'接口和终端状态'},metrics.map(([key,label,section,filter])=>{
   const reading=node('strong','','—');
   const button=E('button',{type:'button',disabled:''},[node('span','',label),reading]);
   button.addEventListener('click',()=>this.openHomeStatus(section,filter));
   this.homeStatus[key]={button,reading,section};
   return button;
  }));
 },
 updateHomeStatus:function(d){
  const ports=list(d.ports),clients=list(d.clients);
  const values={
   ports:ports.length&&ports.every(p=>typeof p.carrier==='boolean')?ports.filter(p=>p.carrier).length+'/'+ports.length:'—',
   wired:Array.isArray(d.clients)&&d.client_sources?.arp!==false?clients.filter(c=>this.wiredOnline?c.state==='有线在线':c.kind==='LAN').length:'—',
   wifi:Array.isArray(d.clients)&&!d.errors?.['network.wireless']?clients.filter(c=>c.state==='无线已关联').length:'—'
  };
  for(const [key,{button,reading,section}] of Object.entries(this.homeStatus)){
   const value=String(values[key]);
   if(reading.textContent!==value)reading.textContent=value;
   button.disabled=this.parts[section].hidden||value==='—';
  }
 },
 openHomeStatus:function(section,filter){
  const part=this.parts[section];
  if(part.hidden)return;
  if(filter){
   this.clientSearch.value='';
   this.clientFilter.value=filter;
   this.clientPage=0;
   this.drawClients();
   this.clientFilter.focus({preventScroll:true});
  }else{
   const heading=part.querySelector('.m-card-heading h2');
   if(heading){
    heading.tabIndex=-1;
    heading.focus({preventScroll:true});
   }
  }
  part.scrollIntoView({block:'start'});
 },
 renderProduct:function(d){
  const fallback=d.product&&L.media('products/'+d.product);
  const src=fallback&&d.product_upstream?L.url('admin','myos','product_image'):fallback;
  this.replace('product',{board:d.board,profile:d.profile,src,uptime:Math.floor((d.system.uptime||0)/60)},()=>{
   const img=src?E('img',{class:'m-product-image',decoding:'async',width:128,height:110,src,alt:text(d.board.model)}):node('div','m-generic-device','▤');
   if(src)img.addEventListener('error',()=>{
    if(img.getAttribute('src')!==fallback)img.src=fallback;
    else img.replaceWith(node('div','m-generic-device','▤'));
   });
   return node('section','m-card m-product',[
    img,node('div','m-product-copy',[
     node('span','m-product-kicker',d.profile==='n5000'?'双网口路由器 · USB 3.2':'本地网关'),
     node('h2','',text(d.board.model)),node('small','','运行 '+duration(d.system.uptime)),
     ...(d.profile==='n5000'?[badge('ROUTER','blue')]:[])
    ])
   ]);
  });
 },
 initQuality:function(profile){
  this.qualityHistory=new Map();
  this.qualityLinks=[];
  this.pingRevision=0;
  this.qualitySelect=E('select',{'class':'m-quality-select','aria-label':'选择 WAN 出口',disabled:''});
  this.qualitySelect.addEventListener('change',()=>{
   if(this.compactHome){
    this.qualitySelectedTime=undefined;
    this.showTrafficOutlet(this.qualitySelect.value);
   }
   this.drawQuality();
  });
  this.qualityStatus=node('p','m-quality-status');
  this.qualityState=E('span',{'class':'m-quality-state','role':'status',hidden:''});
  const settings=this.initPingSettings();
  const {deviceModel,deviceBoard}=document.body.dataset;
  const identity=((deviceModel||'')+(deviceBoard||'')).toLowerCase().replace(/[^a-z0-9]/g,'');
  const compact=['c8688','nbcpe688'].includes(profile)||/(?:c8(?:new)?688|hcmt7981688|nbcpe|nb68)/.test(identity);
  const inspect=deviceModel==='NRadio-C5800-688'||compact;
  this.qualityLatency=qualityChart(qualityFields.slice(0,2),'Ping 延迟与抖动，单位毫秒',' ms',inspect);
  this.qualityLoss=qualityChart(qualityFields.slice(2),'Ping 丢包率，单位百分比','%',inspect);
  this.qualityLoss.root.classList.add('m-quality-chart-loss');
  if(inspect)this.initQualityInspection(compact);
  this.qualityAxis=node('div','m-quality-time',[node('span','',secondsAgo(trafficWindow)),node('span','',secondsAgo(trafficWindow/2)),node('span','','最新采样')]);
  const legend=(field,label,unit)=>{
   const value=node('strong','','—');
   this.qualityValues[field]=value;
   return E('span',{'class':'m-quality-legend',title:field==='latency'?(inspect?'当前显示的 Ping 往返时间':'最新 Ping 往返时间'):'最近 3 次探测'},[
    node('span','m-quality-label',[E('i',{'class':'m-quality-swatch m-quality-'+field,'aria-hidden':'true'}),label]),
    node('span','m-quality-reading',[value,node('small','',unit)])
   ]);
  };
  this.qualityValues={};
  this.parts.quality.append(card('WAN 链路质量','↗',node('div','m-quality-body',[
   node('div','m-quality-legends',qualityFields.map(args=>legend(...args))),
   ...(inspect?[this.qualityReading]:[]),
   this.qualityLatency.root,
   this.qualityLoss.root,this.qualityAxis,node('div','m-quality-footer',[
    node('div','m-quality-info',inspect?[this.qualityStatus]:[this.qualityStatus,this.qualityState]),settings])
  ]),inspect?node('div','m-quality-controls',[
   this.qualitySelect,this.qualitySwitch,...(this.qualityOutlet?[this.qualityOutlet]:[])
  ]):this.qualitySelect));
  this.drawQuality();
 },
 initQualityInspection:function(compact){
  this.qualityOutlet=compact?E('span',{class:'m-quality-outlet',hidden:''}):null;
  this.qualitySwitch=E('div',{class:'m-quality-switch',role:'group','aria-label':'选择 WAN 出口',hidden:''});
  this.qualityLive=E('button',{class:'m-copy-button',type:'button',hidden:''},'回到实时');
  this.qualityReading=node('div','m-quality-context',[
   this.qualityState,this.qualityLive
  ]);
  const select=sample=>{
   this.qualitySelectedTime=sample?.ts;
   this.drawQuality();
  };
  this.qualityLive.addEventListener('click',()=>{this.qualityLatency.plot.focus();select();});
  for(const {plot} of [this.qualityLatency,this.qualityLoss]){
   plot.classList.add('m-quality-inspect');
   plot.setAttribute('tabindex','0');
   plot.setAttribute('aria-label',plot.getAttribute('aria-label')+'。点按图表或用左右方向键查看采样。');
   plot.addEventListener('click',event=>{
    if(!this.qualitySamples.length)return;
    const bounds=plot.getBoundingClientRect();
    const fraction=Math.min(1,Math.max(0,((event.clientX-bounds.left)/bounds.width*600-1)/598));
    const time=this.qualityEnd-trafficWindow+fraction*trafficWindow;
    const closest=this.qualitySamples.reduce((a,b)=>Math.abs(b.ts-time)<Math.abs(a.ts-time)?b:a);
    select(closest);
   });
   plot.addEventListener('keydown',event=>{
    if(event.key==='Escape'&&this.qualitySelectedTime!=null){event.preventDefault();select();return;}
    if(!this.qualitySamples.length||!['ArrowLeft','ArrowRight'].includes(event.key))return;
    event.preventDefault();
    const index=this.qualitySamples.findIndex(sample=>sample.ts===this.qualitySelectedTime);
    const newest=this.qualitySamples.length-1;
    const next=index<0?newest:Math.min(newest,Math.max(0,index+(event.key==='ArrowLeft'?-1:1)));
    select(this.qualitySamples[next]);
   });
  }
 },
 initPingSettings:function(){
  const trigger=E('button',{type:'button',class:'m-copy-button'},'设置 Ping');
  const title=E('h2',{id:'m-ping-title'},'Ping 探测目标');
  const close=E('button',{type:'button',class:'m-icon-button','aria-label':'关闭 Ping 设置'},'×');
  const help=E('p',{id:'m-ping-help',class:'m-muted'},'支持域名、IPv4 或 IPv6。留空保存后使用 www.baidu.com，所有 WAN 出口共用此目标。');
  const status=E('p',{role:'status',class:'m-muted',hidden:''});
  const input=E('input',{id:'m-ping-target',type:'text',placeholder:'www.baidu.com',maxlength:'254',autocapitalize:'none',
   autocorrect:'off',autocomplete:'off',spellcheck:'false','aria-describedby':'m-ping-help'});
  const save=E('button',{type:'submit',class:'m-pill-button'},'保存');
  const form=E('form',{class:'m-ping-form'},[
   E('label',{for:'m-ping-target'},'地址'),input,help,status,node('div','m-action-buttons',save)
  ]);
  const dialog=E('dialog',{class:'m-client-dialog','aria-labelledby':'m-ping-title'},[
   node('div','m-background-heading',[title,close]),form
  ]);
  trigger.addEventListener('click',()=>{
   input.value=this.pingSettings?.custom||'';
   status.hidden=true;
   dialog.showModal();
   input.focus();
  });
  close.addEventListener('click',()=>dialog.close());
  dialog.addEventListener('close',()=>trigger.focus({preventScroll:true}));
  form.addEventListener('submit',async event=>{
   event.preventDefault();
   if(save.disabled)return;
   save.disabled=input.disabled=true;
   status.hidden=false;
   status.textContent='正在保存…';
   try{
    const response=await request.post(L.url('admin','myos','ping_target'),
     new URLSearchParams({token:L.env.token,target:input.value}).toString(),
     {headers:{'Content-Type':'application/x-www-form-urlencoded'},timeout:10000});
    const result=response.json();
    if(!response.ok||!result?.target)throw new Error(result?.error||'保存 Ping 目标失败');
    this.pingRevision++;
    this.setPingSettings(result);
    dialog.close();
    this.scheduleNext(0);
    this.scheduleQuality(0);
   }catch(error){
    status.textContent=error.message||'保存 Ping 目标失败';
   }finally{
    save.disabled=input.disabled=false;
   }
  });
  this.root.append(dialog);
  return trigger;
 },
 setPingSettings:function(settings){
  if(!settings?.target)return;
  const previous=this.pingSettings?.target;
  this.pingSettings=settings;
  if(settings.target===previous)return;
  if(previous){
   this.qualityHistory.clear();
   this.qualityHistoryTime=0;
  }
  this.drawQuality();
 },
 updateQualityLinks:function(d){
  const links=list(d.uplinks).map(u=>({name:u.name,label:this.uplinkLabels?.get(u.name)||u.label||alias(d,u.name)}));
  if(same(links,this.qualityLinks))return;
  const selected=this.qualitySelect.value;
  this.qualityLinks=links;
  this.qualitySelect.replaceChildren(...links.map(u=>E('option',{value:u.name},u.label===u.name?u.name:u.label+' · '+u.name)));
  if(links.some(u=>u.name===selected))this.qualitySelect.value=selected;
  this.qualitySelect.disabled=!links.length;
  if(this.trafficOutlet&&((this.qualityOutlet&&links.length<2)||!links.some(u=>u.name===this.trafficOutlet)))this.showTrafficOutlet('');
  if(this.qualitySwitch){
   const useButtons=links.length>1;
   const single=this.qualityOutlet&&links.length===1;
   this.qualitySelect.hidden=useButtons||!!single;
   this.qualitySwitch.hidden=!useButtons;
   if(this.qualityOutlet){
    this.qualityOutlet.hidden=!single;
    this.qualityOutlet.textContent=single?links[0].label:'';
   }
   if(this.qualitySelect.value!==selected)this.qualitySelectedTime=undefined;
   this.qualitySwitch.replaceChildren(...(useButtons?links.map(u=>{
    const button=E('button',{type:'button',title:u.name,value:u.name},u.label);
    button.addEventListener('click',()=>{
     if(this.qualitySelect.value!==u.name){
      this.qualitySelect.value=u.name;
      this.qualitySelectedTime=undefined;
      this.drawQuality();
     }
     if(this.compactHome)this.showTrafficOutlet(u.name);
    });
    return button;
   }):[]));
  }
  this.drawQuality();
  this.scheduleQuality(0);
 },
 updateQualityHistory:function(history,reboot,now){
  if(!history){
   if(this.routerQualityHistory){
    this.routerQualityHistory=false;
    this.scheduleQuality(0);
   }
   this.drawQuality();
   return;
  }
  clearTimeout(this.qualityTimer);
  const resumed=!this.routerQualityHistory;
  if(reboot)this.qualityHistory.clear();
  this.routerQualityHistory=true;
  for(const point of list(history.points)){
   for(const [name,sample] of Object.entries(point.samples||{})){
    if(resumed&&sample.ts<=this.qualityHistory.get(name)?.samples.at(-1)?.ts)continue;
    this.addQualitySample(name,sample,5);
   }
   this.qualityHistoryTime=point.time;
  }
  for(const entry of this.qualityHistory.values()){
   while(entry.samples[0]?.ts<now-trafficWindow)entry.samples.shift();
  }
  this.drawQuality();
 },
 scheduleQuality:function(delay=this.intervals().fast){
  clearTimeout(this.qualityTimer);
  if(this.routerQualityHistory||document.hidden||this.disposed||!this.root?.isConnected||!this.qualityLinks.length)return;
  this.qualityTimer=setTimeout(()=>this.refreshQuality(),delay);
 },
 refreshQuality:async function(){
  if(this.routerQualityHistory||document.hidden||this.disposed||!this.root?.isConnected)return;
  this.scheduleQuality();
  if(this.paused||this.parts.quality.hidden||this.qualityPending)return;
  const links=this.qualityLinks;
  const revision=this.pingRevision;
  this.qualityPending=true;
  try{
   const {samples,ping}=await this.sampleRequest('link_quality','link_quality',{query:{batch:1},timeout:3500});
   if(!this.routerQualityHistory&&revision===this.pingRevision){
    this.setPingSettings(ping);
    for(const {name} of links)this.addQualitySample(name,samples?.[name]);
   }
  }catch(_){
   if(!this.routerQualityHistory&&revision===this.pingRevision)for(const {name} of links)this.addQualitySample(name);
  }finally{
   this.qualityPending=false;
   if(!document.hidden&&!this.disposed&&this.root.isConnected)this.drawQuality();
  }
 },
 addQualitySample:function(name,sample,maxGap=maxSampleGap(this.intervals().fast)){
  let entry=this.qualityHistory.get(name);
  if(!entry){entry={samples:[]};this.qualityHistory.set(name,entry);}
  entry.requestFailed=!Number.isFinite(sample?.ts);
  if(entry.requestFailed)return;
  if(sample.target&&entry.target&&sample.target!==entry.target)entry.samples=[];
  if(sample.target)entry.target=sample.target;
  if(sample.ts<entry.samples.at(-1)?.ts)entry.samples=[];
  if(sample.ts===entry.samples.at(-1)?.ts)return;
  const previous=entry.samples.at(-1);
  sample.gap=!previous||sample.ts-previous.ts>maxGap;
  if(sample.sent>0){
   const recent=[...entry.samples.slice(-2),sample].filter(s=>s.sent>0&&s.target===sample.target&&sample.ts-s.ts<=maxGap);
   const sent=recent.reduce((count,s)=>count+s.sent,0),received=recent.reduce((count,s)=>count+s.received,0);
   sample.loss=100*(sent-received)/sent;
   const rtts=recent.map(s=>s.latency).filter(Number.isFinite);
   if(Number.isFinite(sample.latency)&&rtts.length>1)
    sample.jitter=rtts.slice(1).reduce((sum,rtt,i)=>sum+Math.abs(rtt-rtts[i]),0)/(rtts.length-1);
  }
  entry.samples.push(sample);
  while(entry.samples[0]?.ts<sample.ts-trafficWindow)entry.samples.shift();
 },
 drawQuality:function(){
  const entry=this.qualityHistory.get(this.qualitySelect.value);
  const last=entry?.samples.at(-1);
  const end=Math.max(this.previous?.ts||0,last?.ts||0),start=end-trafficWindow;
  const samples=(entry?.samples||[]).filter(sample=>sample.ts>=start);
  const stale=last&&end-last.ts>(this.routerQualityHistory?5:maxSampleGap(this.intervals().fast));
  const selected=this.qualitySwitch&&samples.find(sample=>sample.ts===this.qualitySelectedTime);
  const sample=selected||last;
  if(this.qualitySwitch){
   this.qualitySamples=samples;
   this.qualityEnd=end;
   if(!selected)this.qualitySelectedTime=undefined;
   for(const button of this.qualitySwitch.children){
    const pressed=String(button.value===this.qualitySelect.value);
    if(button.getAttribute('aria-pressed')!==pressed)button.setAttribute('aria-pressed',pressed);
   }
   this.qualityLive.hidden=!selected;
  }
  const peak=samples.reduce((max,sample)=>Math.max(max,sample.latency||0,sample.jitter||0),0);
  const step=10**Math.floor(Math.log10(Math.max(peak,10)));
  const limit=step*[1,2,5,10].find(n=>step*n>=peak);
  this.qualityLatency.draw(samples,start,end,limit,selected);
  this.qualityLoss.draw(samples,start,end,100,selected);
  this.qualityAxis.hidden=!last;
  qualityFields.forEach(([field])=>{
   const value=!selected&&(stale||entry?.requestFailed)?null:sample?.[field];
   const reading=Number.isFinite(value)?value.toFixed(field==='loss'&&value%1===0?0:1):'—';
   if(this.qualityValues[field].textContent!==reading)this.qualityValues[field].textContent=reading;
  });
  const target=this.pingSettings?.target||last?.target;
  const timedOut=sample?.sent>0&&sample.received===0;
  const state=!this.qualityLinks.length?'暂无 WAN 出口':!selected&&entry?.requestFailed?'未能读取最新采样':
   !selected&&stale?'等待新采样':!sample?'等待首次采样':sample.error==='no_route'?'该出口暂无探测路由':
   sample.error?'本次 Ping 无有效结果':timedOut?'本次 Ping 未收到回复':'';
  const context=selected?'所选采样 '+new Date(selected.ts*1000).toLocaleTimeString('zh-CN',{hour12:false}):'';
  const status=[context,target?'Ping '+target:'','丢包率按最近有效探测计算（最多 3 次）'].filter(Boolean).join(' · ');
  if(this.qualityStatus.textContent!==status)this.qualityStatus.textContent=status;
  if(this.qualityState.textContent!==state)this.qualityState.textContent=state;
  this.qualityState.hidden=!state;
  this.qualityState.classList.toggle('is-timeout',state==='本次 Ping 未收到回复');
 },
 initTraffic:function(){
  this.samplingState=E('span',{'class':'m-sampling-state','role':'status'});
  this.samplingNote=E('p',{'class':'m-sampling-note',hidden:''});
  this.setSamplingState('live');
  let controls=this.samplingState;
  if(this.compactHome){
   this.trafficTitle=E('span',{tabindex:'-1'},'实时流量');
   this.trafficAll=E('button',{type:'button',class:'m-copy-button',hidden:'',click:()=>{
    this.showTrafficOutlet('');
    this.trafficTitle.focus({preventScroll:true});
   }},'全部出口');
   controls=node('div','m-traffic-controls',[this.samplingState,this.trafficAll]);
  }
  this.down=node('strong','m-download','—');this.up=node('strong','m-upload','—');
  this.downUnit=node('small');this.upUnit=node('small');
  this.chart=document.createElementNS('http://www.w3.org/2000/svg','svg');
  this.chart.setAttribute('viewBox','0 0 600 150');this.chart.setAttribute('preserveAspectRatio','none');this.chart.setAttribute('role','img');this.chart.setAttribute('aria-label','外网出口上下行曲线，横轴为实际采样时间');
  this.chartEmpty=node('div','m-chart-empty','等待下一次实际采样');
  this.initTrafficInspector();
  this.chartScaleLabel=E('span',{class:'m-chart-scale-label',hidden:'',
   title:'纵轴分段显示，放大低流量区间并保留尖峰'},'分段刻度');
  this.chartLimit=node('span');
  this.chartMid=node('span');
  this.chartScale=node('div','m-chart-scale',[this.chartLimit,this.chartMid,node('span','','0')]);
  this.chartScale.hidden=true;
  this.chartTimeStart=node('span');
  this.chartTimeMiddle=node('span');
  this.chartTime=node('div','m-chart-time',[this.chartTimeStart,this.chartTimeMiddle,node('span','','最新采样')]);
  this.chartTime.hidden=true;
  this.trafficUsed=node('strong','','—');
  this.parts.usage.append(card('流量统计','▥',node('div','m-usage-stat',[
   node('span','m-muted','已使用流量'),this.trafficUsed
  ])));
  this.connectionValue=node('strong');
  this.connectionLimit=node('small');
  this.accelerationName=node('strong','m-acceleration-name');
  this.ppeRows=node('div','m-ppe-rows');
  const reading=(label,direction,value,unit)=>node('div','',[
   node('span','m-traffic-direction '+direction,label),
   node('span','m-traffic-reading',[value,unit])
  ]);
  const forwarding=this.compactHome?E('details',{class:'m-dashboard-details m-forwarding-detail'},[
   node('summary','',[
    node('span','m-forwarding-count',[node('span','','活动连接'),this.connectionValue]),this.accelerationName
   ]),
   node('div','m-forwarding-detail-body',[
    node('div','m-forwarding-capacity',[node('span','','连接上限'),this.connectionLimit]),this.ppeRows
   ])
  ]):node('div','m-forwarding',[
   node('div','m-forwarding-summary',[
    node('div','',[node('label','','活动连接数'),this.connectionValue,this.connectionLimit]),
    node('div','',[node('label','','硬件加速'),this.accelerationName])
   ]),this.ppeRows
  ]);
  this.parts.traffic.replaceChildren(card(this.trafficTitle||'实时流量','▥',node('div','',[
   node('div','m-traffic-values',[
    reading('下载','m-download',this.down,this.downUnit),
    reading('上传','m-upload',this.up,this.upUnit)
   ]),
   node('div','m-chart-frame',[
    this.chartScale,this.trafficPlot,this.chartScaleLabel,this.chartTime
   ]),this.samplingNote,forwarding
  ]),controls));
  this.parts.traffic.firstElementChild.classList.add('m-traffic');
 },
 initTrafficInspector:function(){
  const clock=node('strong'),download=node('span','m-download'),upload=node('span','m-upload');
  const tip=E('div',{id:'m-traffic-tip',class:'m-traffic-tip',role:'tooltip',hidden:''},[clock,download,upload]);
  const plot=this.trafficPlot=E('div',{class:'m-chart m-traffic-inspect',tabindex:'0',role:'group',
   'aria-label':'实时流量，左右方向键查看采样，Esc 退出'},[this.chart,this.chartEmpty,tip]);
  const timeFormat=new Intl.DateTimeFormat('zh-CN',{hour:'2-digit',minute:'2-digit',second:'2-digit',hourCycle:'h23'});
  let pointer=null,selected=null;
  this.updateTrafficTip=()=>{
   const data=this.trafficPlotData;
   let point;
   if(data&&pointer!=null&&data.points.length){
    const t=data.start+pointer*trafficWindow;
    const right=data.points.findIndex(p=>p.time>=t);
    const after=data.points[right],before=right<0?data.points.at(-1):data.points[right-1];
    point=before&&(!after||t-before.time<after.time-t)?before:after;
    // Only snap to the edge of an empty interval when the pointer is on its sample.
    if(point&&(!before||!after||after.gap)&&Math.abs(point.time-t)>0.55)point=null;
   }else if(data&&selected!=null)point=data.points.find(p=>p.time===selected);
   if(!point){
    selected=null;
    if(!tip.hidden){
     tip.hidden=true;plot.removeAttribute('aria-describedby');
     this.trafficCursor.setAttribute('visibility','hidden');
    }
    return;
   }
   selected=point.time;
   const fraction=(point.time-data.start)/trafficWindow,x=(4+592*fraction).toFixed(1);
   this.trafficCursor.setAttribute('x1',x);this.trafficCursor.setAttribute('x2',x);
   this.trafficCursor.setAttribute('visibility','visible');
   tip.classList.toggle('at-right',fraction<0.5);
   clock.textContent=Number.isFinite(data.wallOffset)?timeFormat.format((data.wallOffset+point.time)*1000):
    secondsAgo(Math.round(data.start+trafficWindow-point.time));
   download.textContent='下载 '+speed(point.down).join(' ');
   upload.textContent='上传 '+speed(point.up).join(' ');
   if(tip.hidden){tip.hidden=false;plot.setAttribute('aria-describedby',tip.id);}
  };
  const inspect=event=>{
   const box=this.chart.getBoundingClientRect();
   if(!box.width)return;
   pointer=Math.max(0,Math.min(1,((event.clientX-box.left)*600/box.width-4)/592));
   this.updateTrafficTip();
  };
  this.clearTrafficTip=()=>{pointer=null;selected=null;this.updateTrafficTip();};
  plot.addEventListener('pointermove',event=>{if(event.pointerType!=='touch')inspect(event);});
  plot.addEventListener('pointerleave',event=>{if(event.pointerType!=='touch'&&pointer!=null)this.clearTrafficTip();});
  plot.addEventListener('click',event=>{if(event.detail){plot.focus({preventScroll:true});inspect(event);}});
  plot.addEventListener('blur',this.clearTrafficTip);
  plot.addEventListener('keydown',event=>{
   if(event.key==='Escape'){this.clearTrafficTip();return;}
   const points=this.trafficPlotData?.points;
   if(!points?.length||!['ArrowLeft','ArrowRight'].includes(event.key))return;
   event.preventDefault();
   const index=points.findIndex(p=>p.time===selected);
   const next=index<0?points.length-1:Math.max(0,Math.min(points.length-1,index+(event.key==='ArrowLeft'?-1:1)));
   pointer=null;selected=points[next].time;this.updateTrafficTip();
  });
 },
 showTrafficOutlet:function(name){
  if(this.trafficOutlet===name)return;
  this.clearTrafficTip();
  this.trafficOutlet=name;
  this.trafficAll.hidden=!name;
  this.chartScaleExpanded=false;
  if(this.previous)this.drawTraffic(this.previous,this.lastRates?.down,this.lastRates?.up);
 },
 drawTraffic:function(d,down,up){
  const outlet=this.trafficOutlet;
  const label=outlet&&(this.qualityLinks.find(u=>u.name===outlet)?.label||outlet);
  let valid=this.history;
  if(outlet){
   valid=[];
   let gap=false;
   for(const point of this.history){
    const rates=point.links?.[outlet];
    if(rates){
     valid.push({time:point.time,down:rates.down,up:rates.up,gap:point.gap||gap});
     gap=false;
    }else gap=true;
   }
   const latest=valid.at(-1);
   const recent=latest&&d.sample-latest.time<=(this.routerTrafficHistory?3:maxSampleGap(this.intervals().fast));
   down=recent?latest.down:null;
   up=recent?latest.up:null;
  }
  if(this.trafficTitle){
   const title=label?'实时流量 · '+label:'实时流量';
   if(this.trafficTitle.textContent!==title)this.trafficTitle.textContent=title;
  }
  const rx=speed(down),tx=speed(up);this.down.textContent=rx[0];this.downUnit.textContent=rx[1];this.up.textContent=tx[0];this.upUnit.textContent=tx[1];
  if(!this.parts.traffic.hidden&&this.chartInView!==false){
   const ns=this.chart.namespaceURI,svg=(tag,attrs)=>{const el=document.createElementNS(ns,tag);Object.entries(attrs).forEach(([k,v])=>el.setAttribute(k,v));return el;};
   if(!this.chartPaths){
    const frag=document.createDocumentFragment();
    [trafficChartTop,trafficChartMiddle,trafficChartBottom].forEach(y=>{
     const gridline=svg('line',{x1:0,y1:y,x2:600,y2:y,class:'m-chart-gridline'});
     if(y===trafficChartMiddle)this.chartMiddleLine=gridline;
     frag.append(gridline);
    });
    this.chartPaths=['down','up'].map(key=>{
     const fill=svg('path',{class:'m-chart-area '+key}),line=svg('path',{class:'m-chart-line '+key,fill:'none','stroke-width':2.2,'stroke-linecap':'round','stroke-linejoin':'round','vector-effect':'non-scaling-stroke'}),dot=svg('circle',{r:3,class:'m-chart-dot '+key});
     frag.append(fill,line,dot);return {fill,line,dot};
    });
    this.trafficCursor=svg('line',{y1:trafficChartTop,y2:trafficChartBottom,class:'m-quality-cursor',visibility:'hidden'});
    frag.append(this.trafficCursor);this.chart.replaceChildren(frag);
   }
   const {limit,detail}=trafficScale(valid,this.chartScaleExpanded);
   this.chartScaleExpanded=detail!==null;
   const topTick=trafficTick(limit),midTick=trafficTick(detail??limit/2);
   if(this.chartLimit.textContent!==topTick)this.chartLimit.textContent=topTick;
   if(this.chartMid.textContent!==midTick)this.chartMid.textContent=midTick;
   this.chartScaleLabel.hidden=!this.chartScaleExpanded;
   this.chartMiddleLine.classList.toggle('m-chart-gridline-detail',this.chartScaleExpanded);
   const description=(label||'外网出口')+'上下行曲线，横轴为实际采样时间'+(detail?'，纵轴分段放大低流量区间':'');
   if(this.chart.getAttribute('aria-label')!==description)this.chart.setAttribute('aria-label',description);
   this.chartScale.hidden=!valid.length;
   const bottom=trafficChartBottom,middle=trafficChartMiddle,top=trafficChartTop;
   const yForRate=rate=>{
    if(!detail)return bottom-(bottom-top)*rate/limit;
    if(rate<=detail)return bottom-(bottom-middle)*rate/detail;
    return middle-(middle-top)*(rate-detail)/(limit-detail);
   };
   const end=Math.max(d.sample||0,valid.at(-1)?.time||0),start=end-trafficWindow;
   this.trafficPlotData={points:valid,start,wallOffset:d.ts-d.sample};
   this.updateTrafficTip();
   this.chartTimeStart.textContent=secondsAgo(trafficWindow);
   this.chartTimeMiddle.textContent=secondsAgo(trafficWindow/2);
   this.chartTime.hidden=!valid.length;
   ['down','up'].forEach((key,index)=>{
    let segment=[],lineD='',areaD='',last=null;
    const flush=()=>{
     if(!segment.length)return;
     const d=segment.map((p,i)=>(i?'L':'M')+p.x.toFixed(1)+','+p.y.toFixed(1)).join(' ');
     lineD+=d+(segment.length===1?'h0.01 ':' ');
     last=segment[segment.length-1];
     if(segment.length>1)areaD+=d+` L${last.x.toFixed(1)},${bottom} L${segment[0].x.toFixed(1)},${bottom} Z `;
     segment=[];
    };
    valid.forEach(p=>{
     if(p.gap)flush();
     const progress=(p.time-start)/trafficWindow;
     const x=4+progress*592,rate=p[key],previous=segment.at(-1);
     if(detail&&previous&&(previous.rate-detail)*(rate-detail)<0){
      const fraction=(detail-previous.rate)/(rate-previous.rate);
      segment.push({x:previous.x+fraction*(x-previous.x),y:middle});
     }
     segment.push({x,y:yForRate(rate),time:p.time,rate});
    });flush();
    const els=this.chartPaths[index];els.line.setAttribute('d',lineD);els.fill.setAttribute('d',areaD);
    const recent=last&&end-last.time<=(this.routerTrafficHistory?3:0);
    els.dot.style.display=recent?'':'none';
    if(last){els.dot.setAttribute('cx',last.x);els.dot.setAttribute('cy',last.y);}
   });
   this.chartEmpty.hidden=valid.length>0;
  }
  const uplinks=list(d.uplinks);
  const used=uplinks.length&&uplinks.every(n=>Number.isFinite(n.rx)&&Number.isFinite(n.tx))
   ? (uplinks.reduce((sum,n)=>sum+n.rx+n.tx,0)/1024**3).toFixed(2)+' G'
   : '—';
  if(this.trafficUsed.textContent!==used)this.trafficUsed.textContent=used;
  this.connectionValue.textContent=d.connections==null?'—':Number(d.connections).toLocaleString('zh-CN');
  this.connectionLimit.textContent=d.connection_limit?
   (this.compactHome?'':' / ')+Number(d.connection_limit).toLocaleString('zh-CN'):(this.compactHome?'—':'');
  const acceleration=d.acceleration||{};
  this.accelerationName.textContent=acceleration.type||'未获取';
  if(!same(this.saved.ppe,acceleration.ppe)){
   const rows=list(acceleration.ppe).map(p=>{
   const percent=p.total>0&&p.used!=null?100*p.used/p.total:null;
   return node('div','m-ppe-row',[
    node('span','','PPE '+p.id),
    node('div','m-meter',E('span',{style:'transform:scaleX('+(percent==null?0:Math.min(1,percent/100))+')'})),
    node('small','',percent==null?'未获取':p.used+' / '+p.total+' · '+percent.toFixed(2)+'%')
   ]);
   });
   patchTree(this.ppeRows,node('div','m-ppe-rows',rows));
   this.saved.ppe=acceleration.ppe;
  }
 },
 renderResources:function(d,cpu){
  const m=d.system.memory||{},used=m.total!=null?m.total-(m.available!=null?m.available:(m.free||0)+(m.buffered||0)+(m.cached||0)):null;
  const memory=m.total?100*used/m.total:null,storage=list(d.mounts).find(p=>p.mount==='/overlay')||list(d.mounts).find(p=>p.mount==='/');
  const percent=storage&&storage.size?100*(storage.size-storage.avail)/storage.size:null;
  const item=(label,value,description,percent)=>node('div','m-resource',[
   node('label','',label),node('strong','',value),node('p','',description),
   node('div','m-meter',E('span',{style:'transform:scaleX('+(percent==null?0:Math.min(1,Math.max(0,percent/100)))+')'}))
  ]);
  const cpuTemp=list(d.temperatures).find(t=>t.name==='CPU'),wifi=list(d.temperatures).filter(t=>t.name!=='CPU');
  const fresh=node('section','m-card m-resource-strip',[
   item('CPU 使用率',this.cpuReading==null?(d.cpu?'采样中':'未获取'):this.cpuReading.toFixed(1)+'%',
    (cpu==null?(this.cpuReading==null?'等待新采样':'上次采样 · 正在更新')+' · ':'')+
    (list(d.system.load).length?'负载 '+d.system.load.map(n=>(n/65536).toFixed(2)).join(' / '):'等待实际采样'),this.cpuReading),
   item('运行内存',memory==null?'—':memory.toFixed(0)+'%',bytes(used)+' / '+bytes(m.total),memory),
   item('设备温度',cpuTemp?Number(cpuTemp.value).toFixed(1)+' °C':'—',wifi.length?'Wi-Fi '+wifi.map(t=>t.value+'°').join(' / '):'',cpuTemp?cpuTemp.value:null),
   item('系统存储',percent==null?'—':percent.toFixed(1)+'%',storage?bytes(storage.size-storage.avail)+' / '+bytes(storage.size):'未获取',percent)
  ]);
  const old=this.parts.resources.querySelector('.m-resource-strip');if(old)patchTree(old,fresh);else this.parts.resources.append(fresh);
 },
 initClients:function(){
  this.clients=[];this.clientPage=0;this.clientPageSize=12;
  this.clientSearch=E('input',{type:'search',placeholder:'搜索设备名称、IP、MAC 或 SSID','aria-label':'搜索终端'});
  this.clientFilter=E('select',{'aria-label':'筛选接入方式'},[
   E('option',{value:'all'},'全部终端'),E('option',{value:'wifi'},'无线关联'),
   ...(this.wiredOnline?[E('option',{value:'wired-online'},'有线在线')]:[]),
   E('option',{value:'lan'},'LAN 邻居'),E('option',{value:'dhcp'},'仅租约记录')
  ]);
  this.clientSearch.addEventListener('input',()=>{this.clientPage=0;this.drawClients();});
  this.clientFilter.addEventListener('change',()=>{this.clientPage=0;this.drawClients();});
  this.parts.wifi.addEventListener('click',event=>{
   const button=event.target.closest('button[data-wifi-clients]');
   if(!button)return;
   if(this.parts.clients.hidden){
    location.href=L.url('admin','network','wireless')+'#wifi_assoclist_table';
    return;
   }
   this.clientSearch.value='';
   this.clientFilter.value='wifi:'+button.dataset.wifiClients;
   this.clientPage=0;
   this.drawClients();
   this.clientFilter.focus({preventScroll:true});
   this.parts.clients.scrollIntoView({block:'start'});
  });
  this.clientRows=E('tbody',{role:'rowgroup'});
  this.clientCount=badge('正在读取');
  this.clientSummary=node('div','m-cell-age');
  this.clientReset=E('button',{type:'button',class:'m-copy-button',hidden:true,click:()=>{
   this.clientSearch.value='';
   this.clientFilter.value='all';
   this.clientPage=0;
   this.drawClients();
   this.clientSearch.focus({preventScroll:true});
  }},'清除筛选');
  this.clientPager=node('div','m-client-pager');
  this.clientButtons=[this.clientPageButton(-1),this.clientPageButton(1)];
  this.clientPager.append(...this.clientButtons);
  this.clientCopyStatus=E('span',{'class':'m-copy-status','role':'status'});
  this.parts.clients.replaceChildren(card('终端与连接','◈',node('div','',[
   node('div','m-client-tools',[this.clientSearch,this.clientFilter,E('button',{type:'button',class:'m-pill-button',click:()=>this.exportClients()},'导出 CSV')]),
   node('div','m-table-scroll',E('table',{class:'m-table',role:'table'},[
    E('thead',{role:'rowgroup'},E('tr',{role:'row'},['终端 / MAC','IP 地址','接入与信号','详情'].map(t=>E('th',{role:'columnheader',scope:'col'},t)))),this.clientRows
   ])),node('div','m-client-footer',[node('div','m-client-results',[this.clientSummary,this.clientReset]),this.clientPager]),this.clientCopyStatus,
   node('div','m-cell-age','无线关联可确认接入；DHCP / ARP 记录不代表当前在线。点“详情”查看租约与协商速率。')
  ]),this.clientCount));
  this.drawClients();
 },
 copyClient:async function(value,status=this.clientCopyStatus){
  try{
   if(navigator.clipboard&&window.isSecureContext)await navigator.clipboard.writeText(value);
   else{
    const focus=document.activeElement,field=E('textarea',{'class':'m-copy-buffer',readonly:true},value);
    const host=status.closest('dialog')||document.body;
    host.append(field);field.select();
    let copied;
    try{copied=document.execCommand('copy');}finally{field.remove();if(focus)focus.focus();}
    if(!copied)throw new Error('copy');
   }
   status.textContent='已复制 '+value;
  }catch(e){status.textContent='复制未成功，请手动选择地址。';}
 },
 clientPageButton:function(step){
  return E('button',{type:'button',class:'m-copy-button',click:()=>{this.clientPage+=step;this.drawClients();}},step<0?'上一页':'下一页');
 },
 clientRow:function(client,row){
  if(!row){
   const details=E('button',{type:'button',class:'m-copy-button',click:()=>this.showClient(row.client,details)},'详情');
   const copy=E('button',{type:'button',class:'m-copy-button',click:()=>this.copyClient(row.client.ip)},'IP');
   row=E('tr',{class:'m-client-row',role:'row'},[
    E('td',{class:'m-client-id',role:'cell'},[node('strong'),node('div','m-muted m-client-mac')]),
    E('td',{class:'m-mono m-client-ip',role:'cell'}),
    E('td',{class:'m-client-link',role:'cell'},[node('span','m-client-state'),node('div','m-muted')]),
    E('td',{class:'m-client-actions',role:'cell'},node('div','m-copy-actions',[details,copy]))
   ]);
  }else if(same(row.client,client))return row;
  row.client=client;
  const [id,ip,connection,actions]=row.cells;
  const state=connection.firstElementChild,copy=actions.firstElementChild.lastElementChild;
  id.firstElementChild.textContent=client.name||'未命名终端';
  id.lastElementChild.textContent=client.mac||'';
  ip.textContent=text(client.ip);
  state.textContent=client.ssid||client.kind||'';
  state.classList.toggle('associated',client.state==='无线已关联');
  connection.lastElementChild.textContent=client.state+(client.signal!=null?' · '+client.signal+' dBm':'')+
   (client.nat_forwarded?' · 经本机 NAT':'');
  copy.disabled=!client.ip;
  copy.setAttribute('aria-label','复制IP'+(client.ip?' '+client.ip:''));
  return row;
 },
 filteredClients:function(){
  const terms=this.clientSearch.value.trim().toLowerCase().split(/\s+/).filter(Boolean);
  const filter=this.clientFilter.value;
  return this.clients.filter(c=>{
   if(terms.length){
    const mac=String(c.mac||'').toLowerCase().replace(/-/g,':');
    const fields=[c.name,c.ip,c.ssid,c.device,c.state,mac,mac.replace(/:/g,'-'),mac.replace(/:/g,'')];
    const values=fields.map(value=>String(value||'').toLowerCase());
    if(!terms.every(term=>values.some(value=>value.includes(term))))return false;
   }
   if(filter.startsWith('wifi:'))return c.state==='无线已关联'&&c.device===filter.slice(5);
   if(filter==='wired-online')return c.state==='有线在线';
   return filter==='all'||(filter==='wifi'&&c.state==='无线已关联')||(filter==='lan'&&c.kind==='LAN')||(filter==='dhcp'&&c.kind==='DHCP');
  });
 },
 drawClients:function(){
  const clients=this.filteredClients();
  const totalPages=Math.max(1,Math.ceil(clients.length/this.clientPageSize));
  this.clientPage=Math.min(this.clientPage,totalPages-1);
  const shown=clients.slice(this.clientPage*this.clientPageSize,(this.clientPage+1)*this.clientPageSize);
  const focus=document.activeElement;
  const rowFocused=this.clientRows.contains(focus);
  const pagerFocused=this.clientPager.contains(focus);
  const previousRows=new Map();
  for(const row of this.clientRows.rows){
   if(row.client?.mac)previousRows.set(row.client.mac,row);
  }
  const rows=shown.length?shown.map(client=>{
   const row=previousRows.get(client.mac);
   previousRows.delete(client.mac);
   return this.clientRow(client,row);
  }):[E('tr',{role:'row'},E('td',{role:'cell',colspan:4},this.clients.length?'没有匹配的终端':'暂无终端记录'))];
  rows.forEach((row,index)=>{
   const current=this.clientRows.rows[index];
   if(current!==row)this.clientRows.insertBefore(row,current||null);
  });
  while(this.clientRows.rows.length>rows.length)this.clientRows.lastElementChild.remove();
  const filtered=!!this.clientSearch.value.trim()||this.clientFilter.value!=='all';
  this.clientReset.hidden=!filtered;
  const count=filtered?'匹配 '+clients.length+' / '+this.clients.length+' 条':'共 '+clients.length+' 条';
  this.clientSummary.textContent=totalPages>1?count+' · 第 '+(this.clientPage+1)+' / '+totalPages+' 页':count;
  const [prev,next]=this.clientButtons;
  prev.disabled=this.clientPage===0;
  next.disabled=this.clientPage>=totalPages-1;
  this.clientPager.hidden=totalPages===1;
  if(rowFocused&&(focus.disabled||document.activeElement!==focus)){
   (this.clientRows.contains(focus)&&!focus.disabled?focus:this.clientSearch).focus({preventScroll:true});
  }
  if(pagerFocused&&(focus.disabled||this.clientPager.hidden)){
   (this.clientPager.hidden?this.clientSearch:focus===prev?next:prev).focus({preventScroll:true});
  }
 },
 renderClients:function(d){
  const radios=list(d.wireless).filter(w=>w.up&&w.name);
  const options=radios.map(w=>({value:'wifi:'+w.name,label:text(w.ssid)+' · '+
   (w.frequency?(w.frequency<3000?'2.4 GHz':w.frequency<5925?'5 GHz':'6 GHz'):w.name)}));
  const optionsChanged=!same(this.clientRadios,options);
  if(optionsChanged){
   const selected=this.clientFilter.value;
   this.clientFilter.querySelectorAll('option[data-radio]').forEach(option=>option.remove());
   this.clientFilter.append(...options.map(w=>E('option',{value:w.value,'data-radio':''},w.label)));
   this.clientFilter.value=[...this.clientFilter.options].some(o=>o.value===selected)?selected:'all';
   this.clientRadios=options;
  }
  this.clientCount.textContent=list(d.clients).length+' 条记录';
  if(!optionsChanged&&same(this.clients,list(d.clients)))return;
  this.clients=list(d.clients);this.drawClients();
 },
 initControls:function(){
  const select=E('select',{'aria-label':'首页采样模式'},
   Object.entries(samplingModes).map(([value,mode])=>E('option',{value},mode.label+' · '+mode.fast/1000+' 秒 / '+mode.slow/1000+' 秒')));
  select.value=this.performanceMode;
  select.addEventListener('change',()=>{this.performanceMode=select.value;this.setSamplingState('live');try{localStorage.setItem('myos-liquid-performance-v3',select.value);}catch(_){}this.fullNext=0;this.scheduleNext(50);this.scheduleSignals(0);this.scheduleQuality(0);});
  const themeUpdateStatus=E('p',{class:'m-muted',role:'status',hidden:''});
  const themeProgress=E('progress',{'aria-label':'主题下载进度',hidden:''});
  const refreshTheme=E('button',{type:'button',class:'m-copy-button',hidden:'',click:()=>location.reload()},'刷新页面');
  const readResult=response=>{
   if(!response.ok)throw new Error();
   const result=response.json();
   if(result.error)throw new Error(result.error);
   return result;
  };
  const installTheme=E('button',{type:'button',class:'m-copy-button',hidden:'',click:async ()=>{
   checkTheme.disabled=installTheme.disabled=true;
   refreshTheme.hidden=true;
   themeUpdateStatus.textContent='正在下载并安装主题…';
   try{
    const started=readResult(await request.post(L.url('admin','myos','theme_install'),
     new URLSearchParams({token:L.env.token}).toString(),
     {headers:{'Content-Type':'application/x-www-form-urlencoded'},timeout:18000}));
    const size=Number(started.size);
    const hasSize=Number.isFinite(size)&&size>0;
    if(hasSize){
     themeProgress.max=size;
     themeProgress.value=0;
     themeProgress.hidden=false;
    }
    while(this.root.isConnected&&!this.disposed){
     const statusUrl=L.url('admin','myos','theme_install_status')+'?job='+encodeURIComponent(started.job);
     const result=readResult(await request.get(statusUrl,{cache:false,timeout:15000}));
     if(!result.done){
      if(hasSize)themeProgress.value=result.installing?size:Math.max(0,Number(result.downloaded)||0);
      themeUpdateStatus.textContent=result.installing?'正在安装主题…':
       '正在下载主题 '+bytes(result.downloaded)+(hasSize?' / '+bytes(size):'');
      await new Promise(resolve=>setTimeout(resolve,2000));
      continue;
     }
     if(result.version!==started.latest)throw new Error('安装结束后，请重新检查主题版本');
     installTheme.hidden=true;
     themeUpdateStatus.textContent='主题 '+result.version+' 安装成功';
     refreshTheme.hidden=false;
     break;
    }
   }catch(error){
    themeUpdateStatus.textContent=error.message||'无法取得安装结果，请重新检查主题版本';
   }finally{
    themeProgress.hidden=true;
    checkTheme.disabled=installTheme.disabled=false;
   }
  }},'下载并安装主题');
  const checkTheme=E('button',{type:'button',class:'m-copy-button',click:async ()=>{
   checkTheme.disabled=true;
   installTheme.hidden=true;
   refreshTheme.hidden=true;
   themeUpdateStatus.hidden=false;
   themeUpdateStatus.textContent='正在检查主题更新…';
   try{
    const result=readResult(await request.get(L.url('admin','myos','theme_update'),{cache:false,timeout:15000}));
    themeUpdateStatus.textContent=result.available
     ? '发现主题新版本：'+result.latest+'（当前 '+result.current+'）'
     : '当前主题无需更新（'+result.current+'）';
    installTheme.hidden=!result.available;
   }catch(error){
    themeUpdateStatus.textContent=error.message||'主题更新检查失败';
   }finally{
    checkTheme.disabled=false;
   }
  }},'检查主题 IPK 更新');
  this.parts.controls.append(card('布局与性能','ⓘ',node('div','m-layout-controls',[
   node('p','m-cell-age','拖动手柄调整卡片顺序与所在栏。布局只保存到当前浏览器，不改路由配置。'),
   node('div','m-action-buttons',[
    this.editButton=E('button',{type:'button',class:'m-pill-button',click:()=>this.layout?.toggleEdit()},'拖拽调整'),
    E('button',{type:'button',class:'m-pill-button',click:()=>this.layout?.picker()},'添加 / 隐藏卡片'),
    E('button',{type:'button',class:'m-pill-button',click:()=>this.layout?.reset()},'恢复默认布局')
   ]),node('label','m-performance-label',['采样模式',select]),
   node('p','m-cell-age','前者采集流量、系统资源、Ping 和蜂窝信号，后者刷新终端与网络状态。后台页面停止采样。'),
   this.simNamesButton=E('button',{type:'button',class:'m-copy-button',hidden:true,click:()=>{if(this.previous)this.refreshSimNames(this.previous);}},'按需读取 SIM 云端名称'),
   node('div','m-theme-update',[
    node('div','m-action-buttons',[checkTheme,installTheme,refreshTheme]),
    themeUpdateStatus,themeProgress
   ])
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
   ['系统日志','查阅故障与事件','admin/status/logs/syslog'],
   ['软件包','安装与管理软件','admin/system/package-manager'],
   ['终端','打开 Web 终端','admin/system/ttyd/ttyd']
  ];
  this.parts.management.hidden=true;
  (window.MYOSMenu || L.require('ui').then(ui=>ui.menu.load(true))).then(tree=>{
   const updateEntry=tree?.children?.admin?.children?.system?.children?.myos_update;
   this.firmwareUpdateEnabled=!!updateEntry&&updateEntry.satisfied!==false&&!updateEntry.hidden;
   this.refreshFirmwareNotice();
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
   ]:rows.map(r=>node('div','m-health-warning',[node('strong','',r[0]),node('small','',r[1])]))),
   node('p','m-cell-age','Ping 延迟、抖动和丢包见 WAN 链路质量。')
  ]),badge(clear?'已检查':'需留意',clear?'good':'warn')));
 },
 recordWirelessEvents:function(d){
  if(this.eventBoot!==d.boot){this.eventBoot=d.boot;this.wirelessEventBaseline=null;this.events=[];}
  if(Object.keys(d.errors||{}).some(key=>/iwinfo\.assoclist|network\.wireless/.test(key))){
   this.wirelessEventBaseline=null;
   return;
  }
  const stations=new Map(list(d.clients).filter(c=>c.state==='无线已关联'&&c.device&&c.mac)
   .map(c=>[c.device+'/'+c.mac,c]));
  const previous=this.wirelessEventBaseline;
  if(previous){
   for(const [from,to,up] of [[stations,previous,true],[previous,stations,false]]){
    for(const [key,c] of from){
     if(!to.has(key))this.events.unshift({time:d.meta_ts||d.ts,kind:'wifi',device:c.device,mac:c.mac,up});
    }
   }
   this.events=this.events.slice(0,40);
  }
  this.wirelessEventBaseline=stations;
 },
 renderEvents:function(d){
  const log=d.connection_log;
  const history=list(log?.events);
  const events=[...history,...this.events.filter(e=>!history.some(h=>
   h.kind===e.kind&&h.device===e.device&&h.mac===e.mac&&h.up===e.up&&Math.abs(h.time-e.time)<=65))]
   .sort((a,b)=>b.time-a.time).slice(0,40);
  this.replace('events',{events,available:log?.available},()=>{
   const eventList=items=>node('ol','m-event-list',items.map(e=>{
    const station=list(d.clients).find(c=>c.mac===e.mac);
    const radio=list(d.wireless).find(w=>w.name===e.device);
    const title=e.kind==='wifi'?(e.up?'无线终端已关联':'无线终端已断开'):
     e.kind==='dhcp'?'终端取得 DHCP 租约':alias(d,e.device)+(e.up?' 链路已接通':' 链路已断开');
    const detail=e.mac?[station?.name||e.mac,radio?.ssid||e.device].filter(Boolean).join(' · '):e.device;
    return node('li','',[node('span','m-event-dot '+(e.up?'good':'warn')),
     node('time','',new Date(e.time*1000).toLocaleTimeString('zh-CN',{hour12:false})),
     node('div','',[node('strong','',title),node('small','',detail)])]);
   }));
   return card('连接事件','event',node('div','',[
    events.length?eventList(events.slice(0,6)):node('p','m-muted m-event-quiet',log?.available?'暂无连接变化':'系统日志暂不可用'),
    events.length>6?E('details',{class:'m-inline-details m-earlier-events'},[
     node('summary','','更早的 '+(events.length-6)+' 条事件'),eventList(events.slice(6))
    ]):null,
    events.length&&!log?.available?node('p','m-cell-age','系统日志暂不可用，显示本次采集的连接变化。'):null
   ].filter(Boolean)),link('系统日志','admin/status/logs/syslog'));
  });
 },
 exportClients:function(){
  const rows=this.filteredClients();
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
 showClient:function(c,trigger){
  if(this.clientDialog)this.clientDialog.remove();
  const close=E('button',{type:'button',class:'m-icon-button','aria-label':'关闭终端详情'},'×');
  const copyStatus=E('p',{class:'m-copy-status',role:'status'});
  const dialog=E('dialog',{class:'m-client-dialog','aria-labelledby':'m-client-dialog-title'},[
   node('div','m-background-heading',[E('h2',{id:'m-client-dialog-title'},c.name||'终端详情'),close]),
   badge(c.state,c.state==='无线已关联'?'good':''),
   kv([['IPv4 地址',c.ip],['MAC 地址',c.mac],['接入接口',c.device||c.kind],['SSID',c.ssid],
    ['信号强度',c.signal!=null?c.signal+' dBm':null],['租约',leaseText(c.lease_expires,this.previous?.ts||Date.now()/1000)],
    ['接收协商速率',c.rx!=null?(Number(c.rx)/1000).toFixed(1)+' Mbps':null],
    ['发送协商速率',c.tx!=null?(Number(c.tx)/1000).toFixed(1)+' Mbps':null]]),
   node('p','m-cell-age','协商速率不是实际吞吐量；租约和 ARP 记录不证明设备当前在线。'),
   node('div','m-action-buttons',[
    E('button',{type:'button',class:'m-pill-button',disabled:!c.ip,click:()=>this.copyClient(c.ip,copyStatus)},'复制 IP'),
    E('button',{type:'button',class:'m-pill-button',disabled:!c.mac,click:()=>this.copyClient(c.mac,copyStatus)},'复制 MAC'),
    c.mac && c.lease_expires != null
     ? E('a',{href:L.url('admin','status','overview')+'?lease='+encodeURIComponent(c.mac)+'#status_leases'},'查看此设备租约')
     : link('DHCP 设置','admin/network/dhcp')
   ]),copyStatus
  ]);
  close.addEventListener('click',()=>dialog.close());
  dialog.addEventListener('close',()=>{
   dialog.remove();
   if(this.clientDialog!==dialog)return;
   this.clientDialog=null;
   const row=c.mac&&[...this.clientRows.rows].find(entry=>entry.client?.mac===c.mac);
   const target=trigger?.isConnected?trigger:row?.querySelector('.m-client-actions button')||this.clientSearch;
   target.focus({preventScroll:true});
  });
  document.body.append(dialog);this.clientDialog=dialog;dialog.showModal();
 },

 renderVersion:function(d){
  const data={board:d.board,uptime:Math.floor(d.system.uptime/60),time:Math.floor(d.system.localtime/60),mac:d.mac,theme:d.theme_version};
  this.replace('version',data,()=>card('设备信息','ⓘ',node('dl','m-version-grid',[
   ['固件版本',d.board.release?.version,true],['系统型号',d.board.model],['运行时间',duration(d.system.uptime)],
   ['内核',d.board.kernel],['架构',d.board.release?.target],['主机名',d.board.hostname],['设备 MAC',d.mac],
   ['系统时间',d.system.localtime?new Date(d.system.localtime*1000).toISOString().slice(0,19).replace('T',' '):null,true],
   ['主题',d.theme_version?'MYOS-NEW '+d.theme_version:'MYOS-NEW',true]
  ].map(row=>node('div',row[2]?'wide':'',[node('dt','',row[0]),node('dd','',text(row[1]))]))),link('系统设置','admin/system/system')));
 }
});
