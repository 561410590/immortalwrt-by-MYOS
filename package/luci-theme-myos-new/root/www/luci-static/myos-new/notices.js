/* Read-only MYOS notifications. No service, interface or modem control. */
(() => {
 'use strict';
 if(window.__myosNotices)return;
 window.__myosNotices=true;
 const key='myos-new-notices-r4',ttl=15000,reduce=matchMedia('(prefers-reduced-motion:reduce)');
 const box=document.createElement('section');
 box.id='m-notices';box.setAttribute('aria-label','系统通知');document.body.append(box);
 const cards=new Map(),rank={critical:0,warn:1,info:2};
 let state={},busy=false,timer,expiryTimer,homePaused=false,mediaOpen=false;
 try{state=JSON.parse(sessionStorage.getItem(key)||'{}')||{};}catch(_){}
 let events=Array.isArray(state.events)?state.events:[],dismissed=Array.isArray(state.dismissed)?state.dismissed:[];
 function save(){
  state.events=events;state.dismissed=dismissed.slice(-60);
  try{sessionStorage.setItem(key,JSON.stringify(state));}catch(_){}
 }
 function el(tag,cls,text){const n=document.createElement(tag);n.className=cls;if(text!=null)n.textContent=text;return n;}
 function motion(node,out){
  if(reduce.matches||!node.animate){if(out)node.remove();return;}
  const frames=[{opacity:0,transform:'translateX(18px) scale(.97)'},{opacity:1,transform:'translateX(0) scale(1)'}];
  const anim=node.animate(out?frames.slice().reverse():frames,{duration:out?120:180,easing:'cubic-bezier(.2,.8,.2,1)',fill:'both'});
  anim.onfinish=()=>{if(out)node.remove();else anim.cancel();};
 }
 const paths={
  eth:'M5 3h14v12h-4v5H9v-5H5zM9 6v4m3-4v4m3-4v4',
  usb:'M12 21V3m-3 3 3-3 3 3M12 15l-5-4V8m5 5 5-4V6',
  info:'M6 9a6 6 0 0 1 12 0c0 6 3 8 3 8H3s3-2 3-8M10 21h4',
  warn:'M12 3 2 21h20zM12 9v5m0 3v1'
 };
 function icon(kind){
  const svg=document.createElementNS('http://www.w3.org/2000/svg','svg'),path=document.createElementNS(svg.namespaceURI,'path');
  svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('aria-hidden','true');
  path.setAttribute('d',(paths[kind.replace('-off','')]||paths.info)+(kind.endsWith('-off')?'M3 3l18 18':''));
  svg.append(path);return svg;
 }
 function remove(id){
  const card=cards.get(id);if(!card)return;cards.delete(id);motion(card,true);
 }
 function expireLinks(){
  clearTimeout(expiryTimer);
  const now=Date.now();
  const remaining=events.filter(event=>{
   if(event.expires>now)return true;
   remove(event.id);
   return false;
  });
  if(remaining.length!==events.length){events=remaining;save();}
  if(events.length)expiryTimer=setTimeout(expireLinks,Math.min(...events.map(event=>event.expires))-now);
 }
 function render(list){
  const wanted=list.filter(n=>n&&n.id&&!(n.closable&&dismissed.includes(n.id)))
   .sort((a,b)=>(rank[a.level]??2)-(rank[b.level]??2));
  const ids=new Set(wanted.map(n=>n.id));
  cards.forEach((_,id)=>{if(!ids.has(id))remove(id);});
  wanted.forEach(n=>{
   let card=cards.get(n.id);
   if(!card){
    card=el('article','m-notice');card.dataset.level=n.level||'info';card.dataset.kind=n.icon||'info';
    card.setAttribute('role',n.level==='critical'?'alert':'status');
    const symbol=el('span','m-notice-icon');symbol.append(icon(n.icon||(n.level==='info'?'info':'warn')));
    const body=el('div','m-notice-body');body.append(el('strong','m-notice-title'),el('p','m-notice-text'));
    if(n.link==='ota'){const a=el('a','','查看更新');a.href=L.url('admin/system/myos_update');body.append(a);}
    if(n.link==='usb-network'){const a=el('a','','配置 USB 网络');a.href=L.url('admin','network','myos-usb-network');body.append(a);}
    card.append(symbol,body);
    if(n.closable){
     const close=el('button','m-notice-close','×');close.type='button';close.setAttribute('aria-label','关闭通知');
     close.onclick=()=>{dismissed.push(n.id);save();remove(n.id);};card.append(close);
    }else{
     const progress=el('progress','m-notice-progress');progress.max=100;progress.setAttribute('aria-label','处理进度');card.append(progress);
    }
    cards.set(n.id,card);box.append(card);motion(card,false);
   }
   const title=n.title||({critical:'重要提示',warn:'状态提醒',info:'系统通知'}[n.level]||'系统通知');
   const titleNode=card.querySelector('.m-notice-title'),text=card.querySelector('.m-notice-text');
   if(titleNode.textContent!==title)titleNode.textContent=title;
   if(text.textContent!==(n.text||''))text.textContent=n.text||'';
   const progress=card.querySelector('progress');
   if(progress){if(Number.isFinite(n.progress))progress.value=Math.max(0,Math.min(100,n.progress));else progress.removeAttribute('value');}
  });
 }
 function links(data,now){
  if(data.error||!Array.isArray(data.ports))return;
  const fresh=state.boot===data.boot&&now-state.time>=0&&now-state.time<60000;
  if(state.boot!==data.boot)events=[];
  const previous=state.ports||{},next={};
  data.ports.forEach(p=>{
   next[p.name]=p;const old=previous[p.name];
   if(!fresh||!old||![0,1].includes(p.carrier)||![0,1].includes(old.carrier))return;
   const connected=p.carrier===1;
   const rises=Number.isFinite(p.count)&&Number.isFinite(old.count)?p.count-old.count:0;
   const reconnected=connected&&old.carrier===1&&rises>0;
   if(p.carrier===old.carrier&&!reconnected)return;
   const speed=connected&&p.speed>0?'，协商速率 '+(p.speed>=1000?p.speed/1000+' Gbps':p.speed+' Mbps'):'';
   const message=reconnected?'两次检查间链路重新建立 '+rises+' 次':connected?'检测到网口已连接':'检测到网口已断开';
   events=events.filter(n=>n.port!==p.name);
   events.push({id:'link-'+p.name+'-'+now,port:p.name,expires:now+ttl,closable:true,level:'info',
    icon:connected?'eth':'eth-off',title:(p.label||p.name)+(reconnected?' 重新连接':connected?' 已连接':' 已断开'),
    text:message+speed+'。检测时间 '+new Date(now).toLocaleTimeString('zh-CN',{hour12:false})});
  });
  state.ports=next;state.time=now;state.boot=data.boot;
 }
 function usbDevices(data,now){
  if(!data.usb_devices||typeof data.usb_devices!=='object')return;
  const previous=state.usbDevices;
  const devices=Array.isArray(data.usb_devices)?data.usb_devices:[];
  const next=Object.fromEntries(devices.map(device=>[device.name,device]));
  if(previous&&state.usbBoot===data.boot){
   const kinds={ethernet:'USB 网卡',rndis:'安卓 USB 共享',iphone:'iPhone USB 共享'};
   for(const name of new Set([...Object.keys(previous),...Object.keys(next)])){
    if(Boolean(previous[name])===Boolean(next[name]))continue;
    const device=next[name]||previous[name],connected=Boolean(next[name]);
    events=events.filter(event=>event.usb!==name);
    events.push({id:'usb-'+name+'-'+now,usb:name,expires:now+ttl,closable:true,level:'info',
     icon:connected?'usb':'usb-off',title:(kinds[device.kind]||'USB 网络设备')+(connected?' 已接入':' 已拔出'),
     text:connected?name+'，可手动选择 LAN 或 WAN。':name,link:connected?'usb-network':undefined});
   }
  }
  state.usbDevices=next;state.usbBoot=data.boot;
 }
 async function tick(){
  if(busy||document.hidden||homePaused||mediaOpen||window.__MYOSDashboardBusy)return;busy=true;
  try{
   const response=await fetch(L.url('admin/myos/notices'),{credentials:'same-origin',cache:'no-store',signal:AbortSignal.timeout(8000)});
   if(!response.ok)throw new Error('HTTP '+response.status);
   const data=await response.json(),now=Date.now();links(data,now);usbDevices(data,now);
   events=events.filter(n=>n.expires>now);save();render([...(Array.isArray(data.notices)?data.notices:[]),...events]);
   expireLinks();
  }catch(_){/* A failed read is not a physical disconnect. Keep existing notices. */}
  finally{busy=false;}
 }
 // Preserve LuCI's own notification content, buttons and dismissal handlers.
 const main=document.querySelector('#maincontent');
 const relocate=node=>{
  if(node.nodeType!==1||!node.matches('.alert-message.fade-in'))return;
  node.classList.add('m-native-notice');box.append(node);
  node.addEventListener('click',()=>{if(node.classList.contains('fade-out'))setTimeout(()=>node.remove(),220);});
 };
 if(main){main.querySelectorAll(':scope > .alert-message.fade-in').forEach(relocate);
  new MutationObserver(records=>records.forEach(r=>r.addedNodes.forEach(relocate))).observe(main,{childList:true});}
 window.addEventListener('myos:sampling-state',e=>{homePaused=!!e.detail?.paused;});
 window.addEventListener('myos:media-state',e=>{mediaOpen=!!e.detail?.active;});
 function run(){clearInterval(timer);expireLinks();tick();timer=setInterval(tick,30000);}
 document.addEventListener('visibilitychange',()=>{if(!document.hidden){expireLinks();tick();}});
 window.addEventListener('pagehide',()=>{clearInterval(timer);clearTimeout(expiryTimer);});
 window.addEventListener('pageshow',e=>{if(e.persisted)run();});
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',run);else run();
})();
