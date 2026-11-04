(() => {
'use strict';
const amount=v=>Number.isFinite(v)?Number(v.toFixed(1))+' MiB':'—';
function mount(root, url) {
 const $=s=>root.querySelector(s), set=(id,value)=>{const el=$('#'+id);if(el)el.textContent=value??'—';};
 const deviceModel=document.body.dataset.deviceModel||'NRadio N5000';
 set('device-model',deviceModel);
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
 const paths={wifi:['admin','network','wireless'],network:['admin','network','network'],devices:['admin','services','oaf','users','list']};
 root.querySelectorAll('[data-panel]').forEach(button=>{
  const link=document.createElement('a'); link.className=button.className;
  link.dataset.panel=button.dataset.panel;
  link.href=url(...paths[button.dataset.panel]); link.replaceChildren(...button.childNodes); button.replaceWith(link);
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
  const D=window.C2000Data,h=D.hardware(data.sensors??{},data.config),n=D.network(data.network,data.sensors?.wireless??data.wireless,data.clients);
  if(data.identity)set('device-model',data.identity.model||deviceModel);
  renderResources(D.resources(data.system));
   const sessionCount=D.number(data.sessions?.count);
   set('session-count',Number.isSafeInteger(sessionCount)&&sessionCount>=0?sessionCount.toLocaleString('zh-CN'):'—');
  set('uptime-value',D.uptime(data.system?.uptime));
  set('connection-heading',n.connected===null?'正在获取状态':n.connected?'上行已连接':'上行未连接');
  set('connection-description',n.connected===null?'暂未取得设备状态':n.connected?'上行接口已连接':'请检查上行接口与拨号状态');
  set('wifi-name',n.ssid??'未获取无线信息');
  set('client-count',n.clients===null?'查看设备状态':n.clients+' 台');
  set('wan-ip',n.address||'未取得地址');
  set('wan-protocol',n.protocol||'—');
  for(const port of ['wan','lan']) {
   const item=data.ports?.[port];
   set(port+'-link',!item?'读取中':item.carrier?'已连接':'未连接');
   set(port+'-speed',item?.carrier&&item.speed>0?item.speed+' Mbps':'—');
  }
  for(const key of ['cpu','wifi'])set(key+'-temperature',h.temperatures[key]===null?'—':Number(h.temperatures[key].toFixed(1)));
  const states={normal:['温度正常','success','均低于已配置的温控阈值'],warning:['温度偏高','warning','已达到配置的提醒温度'],critical:['温度过高','danger','已达到配置的告警温度'],missing:[h.allMissing?'未获取':'读数不全','neutral','传感器未完整上报'],unconfigured:['阈值无效','neutral','请检查提醒阈值是否低于告警阈值']};
  const status=states[h.status];set('thermal-status',status[0]);$('#thermal-status').className='ui-badge '+status[1];$('#thermal-status').title='界面提醒阈值（非芯片保护阈值）：'+Object.entries(h.limits).map(([key,v])=>(key==='cpu'?'CPU':'Wi-Fi')+' '+v.warning+' / '+v.critical+' °C').join('；');set('thermal-note',status[2]);$('#thermal-note').hidden=true;
 }
 update();
 return {update};
}
window.C2000Dashboard=Object.freeze({mount});
})();
