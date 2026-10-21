(() => {
'use strict';
const amount=v=>Number.isFinite(v)?Number(v.toFixed(1))+' MiB':'—';
function mount(root, url) {
 const $=s=>root.querySelector(s), set=(id,value)=>{const el=$('#'+id);if(el)el.textContent=value??'—';};
 const deviceModel=document.body.dataset.deviceModel||'NRadio C2000';
 const ultra=/ultra/i.test(deviceModel);
 set('device-model',deviceModel);
 // Keep the device artwork preference local to this browser.
 const art=$('.device-art');
 if(art) {
  const original=new URL(art.getAttribute('src'),document.baseURI).href;
  const skins=[
   {id:'max',file:null,name:'经典银白',wish:'愿每一次连接，都有好消息。',symbol:'✦'},
   {id:'788',file:'c2000-788.png',name:'马年限定',wish:'马到成功，好运满格。',symbol:'✦'},
   {id:'mid-autumn',file:'c2000max-mid-autumn.png',name:'中秋限定',wish:'月圆人团圆，远方也在身边。',symbol:'☾'},
   {id:'national-day',file:'c2000max-national-day.png',name:'国庆限定',wish:'山河锦绣，沿途皆是好风景。',symbol:'✦'}
  ];
  if(ultra) skins.forEach(s=>{s.file='c2000ultra.svg';});
  const key='c2000max-device-art',decorKey='c2000max-decorations';
  let selected='max',decorations=true,clearWish;
  try {
   const saved=localStorage.getItem(key);if(skins.some(s=>s.id===saved))selected=saved;
   decorations=localStorage.getItem(decorKey)!=='off';
  } catch {}
  selected=window.C2000Theme?.getSkin()||selected;
  const scene=art.closest('.device-scene');
  const sparkles=document.createElement('span');sparkles.className='device-sparkles';sparkles.setAttribute('aria-hidden','true');
  for(let n=0;n<5;n++){const star=document.createElement('i');star.textContent='✦';star.style.setProperty('--n',n);sparkles.append(star);}
  scene.append(sparkles);
  function celebrate(){
   if(!decorations)return;
   clearTimeout(clearWish);const skin=skins.find(s=>s.id===selected);
   scene.classList.remove('celebrating');void scene.offsetWidth;scene.classList.add('celebrating');
   clearWish=setTimeout(()=>{scene.classList.remove('celebrating');},2400);
  }
  function showArt() {
   const skin=skins.find(s=>s.id===selected);
   art.src=skin.file?new URL(skin.file,original).href:original;
   art.alt=deviceModel+' · '+skin.name;scene.dataset.skin=selected;
   art.setAttribute('aria-label',art.alt+'；双击切换外观，键盘按 Enter 或空格切换');
  }
  function switchArt() {
   selected=skins[(skins.findIndex(s=>s.id===selected)+1)%skins.length].id;showArt();celebrate();
   if(window.C2000Theme)window.C2000Theme.setSkin(selected);
   else try { localStorage.setItem(key,selected); } catch {}
  }
  art.addEventListener('click',celebrate);
  art.setAttribute('role','button');art.tabIndex=0;art.draggable=false;
  art.title='双击切换设备外观';
  // Touch browsers dispatch dblclick too: suppress it after handling a double tap.
  let lastTap=0,lastTouchSwitch=0,start=null;
  art.addEventListener('pointerdown',e=>{if(e.pointerType==='touch')start={x:e.clientX,y:e.clientY};});
  art.addEventListener('pointerup',e=>{
   if(e.pointerType!=='touch'||!start)return;
   const moved=Math.hypot(e.clientX-start.x,e.clientY-start.y)>12;start=null;
   if(moved){lastTap=0;return;}
   const now=Date.now();
   if(lastTap&&now-lastTap<350){switchArt();lastTouchSwitch=now;lastTap=0;}
   else lastTap=now;
  });
  art.addEventListener('pointercancel',()=>{start=null;lastTap=0;});
  art.addEventListener('dblclick',e=>{e.preventDefault();if(Date.now()-lastTouchSwitch>500)switchArt();});
  art.addEventListener('keydown',e=>{if(!e.repeat&&(e.key==='Enter'||e.key===' ')){e.preventDefault();switchArt();}});
  art.addEventListener('error',()=>{if(art.src!==original){selected='max';showArt();}});
  const syncArt=()=>{
   if(!root.isConnected){document.removeEventListener('c2000-skin-change',syncArt);return;}
   selected=window.C2000Theme.getSkin();showArt();
  };
  document.addEventListener('c2000-skin-change',syncArt);
  showArt();
 }
 const progressStarted=new WeakSet();
 function fillProgress(el,percent) {
  if(percent>0&&!progressStarted.has(el)&&el.isConnected){
   el.style.width='0%';
   // Commit the empty state once so even an immediate RPC reply animates.
   void el.offsetWidth;
   progressStarted.add(el);
  }
  el.style.width=Math.max(0,Math.min(100,percent))+'%';
 }
 const paths={wifi:['admin','network','wireless'],signal:['admin','modem','qmodem'],devices:['admin','network','dhcp']};
 root.querySelectorAll('[data-panel]').forEach(button=>{
  const link=document.createElement('a'); link.className=button.className;
  link.dataset.panel=button.dataset.panel;
  link.href=button.dataset.panel==='signal'?'/5700/':url(...paths[button.dataset.panel]); link.replaceChildren(...button.childNodes); button.replaceWith(link);
 });
 const baseSlides=[['更广阔的连接','更自由的生活','Connect a Wider World'],['每一刻，畅快相连','让距离，不再遥远','Stay Close. Wherever You Are.'],['让美好，始终在线','让生活，多点可能','A Simpler, Connected Life.']];
 const festivalSlides={
  'mid-autumn':[['月满今宵','千里共团圆','A Moonlit Connection'],['桂香入怀','好消息抵达','Good Things Find Their Way'],['天涯有此刻','相聚不设限','Together, Near and Far']],
  'national-day':[['山河锦绣','一路皆风景','Celebrate Every Journey'],['假日慢一点','快乐近一点','A Little More Joy'],['把远方连起','与美好同行','Connected to Possibility']],
  '788':[['骏马迎春','好运满格','A Year of Good Fortune'],['一路生花','万事顺意','Make Way for Good Things'],['奔向新程','美好相连','A Bright New Journey']]
 };
 function showSlide(n){
  const slides=festivalSlides[window.C2000Theme?.getSkin()]||baseSlides,s=slides[n];
  $('#story-title').replaceChildren(document.createTextNode(s[0]),document.createElement('br'),document.createTextNode(s[1]));
  set('story-subtitle',s[2]);root.querySelectorAll('[data-slide]').forEach(b=>{const i=Number(b.dataset.slide);b.classList.toggle('selected',i===n);b.setAttribute('aria-pressed',String(i===n));b.setAttribute('aria-label','第 '+(i+1)+' 页：'+slides[i][0]);});
 }
 root.querySelectorAll('[data-slide]').forEach(button=>button.addEventListener('click',()=>showSlide(Number(button.dataset.slide))));
 const onSkin=()=>{if(!root.isConnected){document.removeEventListener('c2000-skin-change',onSkin);return;}showSlide(0);};
 document.addEventListener('c2000-skin-change',onSkin);showSlide(0);
 let slide=0;
 const slideTimer=window.setInterval(()=>{
  if(!root.isConnected){window.clearInterval(slideTimer);return;}
  if(document.hidden||root.querySelector('.story-card:hover,.story-card:focus-within'))return;
  const selected=root.querySelector('[data-slide].selected');
  slide=(Number(selected?.dataset.slide??slide)+1)%baseSlides.length;
  root.querySelector('[data-slide="'+slide+'"]').click();
 },3000);
 function renderResources(data) {
  const {memory:m,flash,swap}=data;
  const valid=Number.isFinite(m.total)&&m.total>0&&Number.isFinite(m.active)&&Number.isFinite(m.cache);
  const used=valid?m.active+m.cache:null,free=valid?m.total-used:null;
  const available=Number.isFinite(m.available)?m.available:null;
  const committed=available!==null?m.total-available:null;
  set('resource-capacity',amount(m.total)+' RAM / '+amount(flash.total)+' 闪存');
  set('ram-percent',committed!==null?Math.round(committed/m.total*100)+'%':'—');
  set('ram-total-detail',amount(committed)+' / '+amount(m.total));set('ram-used',amount(committed));set('ram-free',amount(available));
  for(const [key,value] of [['active',m.active],['cache',m.cache],['free',free]]) {
   set('ram-'+key+'-label',valid?amount(value):'—');
   fillProgress($('#ram-'+key+'-bar'),valid?value/m.total*100:0);
  }
  $('#ram-bar').setAttribute('aria-label',valid?'RAM：使用 '+amount(used)+'，空闲 '+amount(free):'RAM 数据未获取');
  [['flash',flash],['swap',swap]].forEach(([key,s])=>{
   const valid=Number.isFinite(s.total)&&s.total>0&&Number.isFinite(s.used);
   const off=key==='swap'&&s.total===0,pct=valid?Math.round(s.used/s.total*100):0,bar=$('#'+key+'-bar');
   set(key+'-label',off?'未启用':valid?amount(s.used)+' / '+amount(s.total)+' ('+pct+'%)':'未获取');
   fillProgress($('#'+key+'-fill'),valid?s.used/s.total*100:0);
   if(valid)bar.setAttribute('aria-valuenow',pct);else bar.removeAttribute('aria-valuenow');
   bar.setAttribute('aria-valuetext',off?'未启用':valid?pct+'%':'未获取');
  });
 }
 function update(data={}) {
  const D=window.C2000Data,m=D.modem(data.modem,data.sim,data.modemBase),h=D.hardware(data.sensors??{},data.config),n=D.network(data.network,data.sensors?.wireless??data.wireless,data.clients);
  if(data.identity){
   set('device-model',data.identity.model||deviceModel);
   const route=data.identity.modem_route;
   root.querySelectorAll('a[data-panel="signal"]').forEach(link=>{
    link.href=Array.isArray(route)?url(...route):data.identity.modem_url||url('admin','status','overview');
   });
  }
  renderResources(D.resources(data.system));
   const sessionCount=D.number(data.sessions?.count);
   set('session-count',Number.isSafeInteger(sessionCount)&&sessionCount>=0?sessionCount.toLocaleString('zh-CN'):'—');
  set('uptime-value',D.uptime(data.system?.uptime));
  set('connection-heading',n.connected===null?'正在获取状态':n.connected?'连接正常':'上行未连接');
  set('connection-description',n.connected===null?'暂未取得设备状态':n.connected?'上行接口已连接':'请检查上行接口与拨号状态');
  set('wifi-name',n.ssid??'未获取无线信息');
  set('client-count',n.clients===null?'查看设备状态':n.clients+' 台');
  set('network-generation',m.nr?(m.advanced?'5G-A':'5G'):m.mode&&/LTE/.test(m.mode)?'4G':'—');
  set('network-mode',m.mode?.replace('NR5G-SA Mode','5G SA · 独立组网').replace('EN-DC Mode','5G NSA · 双连接')??'未获取网络模式');
  const logo=$('.operator-logo'); const logoFile={mobile:'china-mobile.png',unicom:'china-unicom.png',telecom:'china-telecom.png'}[m.operatorId];
  logo.dataset.operator=m.operatorId||'';logo.hidden=!logoFile; if(logoFile){logo.src=(window.L?.env?.media||'/luci-static/c2000max-ui')+'/assets/'+logoFile;logo.alt=m.operator;}
  set('operator-name',m.operator??'未获取运营商');$('#operator-name').hidden=!!logoFile;logo.onerror=()=>{logo.hidden=true;$('#operator-name').hidden=false;};
  set('nr-rsrp',m.rsrp!==null?String(m.rsrp).replace('-','−'):'—');set('nr-sinr',m.sinr??m.rsrq);set('signal-secondary-label',m.sinr===null&&m.rsrq!==null?'RSRQ':'SINR');
  set('nr-band',m.band);set('nr-ca',m.carriers);$('#nr-ca').setAttribute('aria-label',m.carriers===null?'载波数量未获取':m.carriers+' 个载波');
  if(m.carriers!==null){const unit=document.createElement('small');unit.textContent=m.carriers>1?'CA':'载波';$('#nr-ca').append(unit);}
  $('#nr-ca').title=(m.nr?'NR':'LTE')+' 已上报载波数量；1 表示仅上报主载波';
  const quality=m.rsrp===null?'未获取':m.rsrp>=-80?'优':m.rsrp>=-90?'良好':m.rsrp>=-100?'一般':'较弱';
  set('signal-quality',quality);$('.signal-health').dataset.quality=m.rsrp===null?'unknown':m.rsrp>=-90?'good':m.rsrp>=-100?'fair':'weak';
  $('#signal-quality').title='按 RSRP 估算的参考等级，不代表实际速率';
  set('modem-model',m.model??'未获取');set('modem-firmware',m.firmware??'未获取');
  set('modem-operator',m.operator??'未获取运营商');set('modem-sim-name',m.simName??'读取中');
  set('modem-temperature',m.temperature===null?'—':m.temperature+' °C');set('modem-sim',m.sim??'未获取');
  set('modem-status',m.status==='registered'?'已驻网':m.status==='unregistered'?'未驻网':'未获取');
  $('#modem-status').className='ui-badge '+(m.status==='registered'?'success':'neutral');
  $('.modem-note').hidden=true;$('.modem-note').textContent=m.stale?'信号缓存已过期，请在 QModem 中刷新。':m.cached?'QModem 缓存 · '+(m.section||'模组')+(m.age!==null?' · '+Math.round(m.age)+' 秒前':''):'暂无模组缓存，可打开 QModem 获取。';
  for(const key of ['cpu','wifi'])set(key+'-temperature',h.temperatures[key]===null?'—':Number(h.temperatures[key].toFixed(1)));
  const states={normal:['温度正常','success','均低于已配置的温控阈值'],warning:['温度偏高','warning','已达到配置的提醒温度'],critical:['温度过高','danger','已达到配置的告警温度'],missing:[h.allMissing?'未获取':'读数不全','neutral','传感器未完整上报'],unconfigured:['阈值无效','neutral','请检查提醒阈值是否低于告警阈值']};
  const status=states[h.status];set('thermal-status',status[0]);$('#thermal-status').className='ui-badge '+status[1];$('#thermal-status').title='界面提醒阈值（非芯片保护阈值）：'+Object.entries(h.limits).map(([key,v])=>(key==='cpu'?'CPU':'Wi-Fi')+' '+v.warning+' / '+v.critical+' °C').join('；');set('thermal-note',status[2]);$('#thermal-note').hidden=true;
 }
 update();
 return {update};
}
window.C2000Dashboard=Object.freeze({mount});
})();
