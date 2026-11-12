/* Browser-local layout. Dragging moves a preview label until drop.
 * The default layout redistributes cards when column heights change. */
(()=>{
 'use strict';
 const labels={product:'本机设备',resources:'系统资源',traffic:'实时流量',quality:'WAN 链路质量',usage:'流量统计',modems:'蜂窝网络',clients:'终端与连接',network:'网络接口',ports:'端口管理',uplinks:'当前出口',wifi:'无线网络',topology:'逻辑连接概览',health:'状态巡检',management:'网络管理',events:'连接事件',controls:'布局与性能',version:'设备信息'};
 const previousDefaults={overview:['product','resources'],main:['traffic','ports','clients','network','usage'],side:['modems','wifi','topology','health','management'],bottom:['events','controls','version']};
 const overviewDefaults={...previousDefaults,overview:['product','resources','ports','uplinks'],main:['traffic','clients','network','usage']};
 const defaults={...overviewDefaults,side:['quality',...overviewDefaults.side]};
 const phoneDefaults={
  overview:['product','resources'],
  main:['traffic','quality','uplinks','ports','modems','clients','wifi','network','usage','topology','health','management'],
  side:[],bottom:defaults.bottom
 };
 const legacyDefaults={overview:['product','resources'],main:['traffic','ports','clients','network'],side:['modems','wifi','topology','health','management','events','controls'],bottom:['version']};
 const legacyN5000={overview:['product','resources'],main:['traffic','ports','clients','network','events'],side:['modems','wifi','topology','health','management'],bottom:['controls','version']};
 const flexibleCards=new Set(['network','wifi','topology','health','management','usage']);
 const defaultZones=()=>JSON.parse(JSON.stringify(defaults));
 window.MYOSLayout=app=>{
  const root=app.root,parts=app.parts,zones={overview:root.querySelector('.m-overview-row'),main:root.querySelector('.m-main-column'),side:root.querySelector('.m-side-column'),bottom:root.querySelector('.m-layout-bottom')};
  let model=app.previous?.profile||'generic',key='myos-liquid-layout-v4:'+model,state={zones:defaultZones(model),hidden:[]},editing=false,drag=null,frame=0,dialog=null;
  const live=document.createElement('div');live.className='m-layout-live';live.setAttribute('role','status');live.setAttribute('aria-live','polite');root.append(live);
  const announce=s=>{live.textContent=s;};
  const narrow=matchMedia('(max-width:1000px)');
  const phone=matchMedia('(max-width:760px)');
  let balanceFrame=0,observing=false,redistributed=false;
  const resize=typeof ResizeObserver==='function'?new ResizeObserver(scheduleBalance):null;

  function restoreColumns(){
   if(!redistributed||[...flexibleCards].some(n=>parts[n].contains(document.activeElement)))return;
   for(const name of ['main','side']){
    let next=null;
    for(const n of state.zones[name].filter(n=>flexibleCards.has(n)).reverse()){
     const card=parts[n],column=zones[name];
     if(card.parentElement!==column||card.nextElementSibling!==next)column.insertBefore(card,next);
     next=card;
    }
   }
   redistributed=false;
   window.dispatchEvent(new Event('myos:layout'));
  }

  function balanceColumns(){
   balanceFrame=0;
   if(editing||state.manual||!root.isConnected)return;
   if(narrow.matches){restoreColumns();return;}
   const mainHeight=zones.main.getBoundingClientRect().height,sideHeight=zones.side.getBoundingClientRect().height;
   const difference=Math.abs(mainHeight-sideHeight);
   if(difference<56)return;
   const [taller,shorter]=mainHeight>sideHeight?[zones.main,zones.side]:[zones.side,zones.main];
   const gap=parseFloat(getComputedStyle(taller).rowGap)||0;
   let candidate=null,improvement=56;
   for(const part of taller.children){
    if(part.hidden||!flexibleCards.has(part.dataset.card)||part.contains(document.activeElement))continue;
    const height=part.getBoundingClientRect().height;
    if(!height)continue;
    const gain=difference-Math.abs(difference-2*(height+gap));
    if(gain>improvement){improvement=gain;candidate=part;}
   }
   if(!candidate)return;
   shorter.append(candidate);
   redistributed=true;
   window.dispatchEvent(new Event('myos:layout'));
   scheduleBalance();
  }

  function scheduleBalance(){
   if(!balanceFrame&&!editing&&!state.manual&&root.isConnected)balanceFrame=requestAnimationFrame(balanceColumns);
  }
  function save(){try{localStorage.setItem(key,JSON.stringify(state));}catch(_){announce('当前浏览器不能保存，布局仅本次有效。');}}
  function load(){
   state={zones:defaultZones(model),hidden:[]};
   try{const data=JSON.parse(localStorage.getItem(key));if(data?.zones){
    const seen=new Set();Object.keys(zones).forEach(z=>{state.zones[z]=(Array.isArray(data.zones[z])?data.zones[z]:[]).filter(n=>Object.hasOwn(parts,n)&&!seen.has(n)&&seen.add(n));});
    const oldDefault=model==='n5000'?legacyN5000:legacyDefaults;
    const defaultOrder=[oldDefault,legacyDefaults,previousDefaults,overviewDefaults,defaults].some(layout=>
     Object.entries(layout).every(([z,names])=>state.zones[z].join('|')===names.join('|')));
    state.manual=data.manual??(Boolean(data.usagePinned)||!defaultOrder);
    if(!state.manual)
     state.zones=defaultZones();
    else {
     if(!seen.has('quality')){state.zones.side.unshift('quality');seen.add('quality');}
     Object.entries(defaultZones()).forEach(([z,names])=>names.forEach(n=>{if(!seen.has(n))state.zones[z].push(n);}));
    }
    state.hidden=(Array.isArray(data.hidden)?data.hidden:[]).filter(n=>Object.hasOwn(parts,n)&&n!=='controls');
   }}catch(_){}
  }
  const available=n=>!(n==='modems'&&model==='n5000'&&!app.previous?.modems?.length);
  function reconcile(d){
   if(d?.profile&&d.profile!==model){model=d.profile;key='myos-liquid-layout-v4:'+model;load();apply();return;}
   let visibilityChanged=false;
   for(const [n,part] of Object.entries(parts)){
    const hidden=state.hidden.includes(n)||!available(n);
    if(part.hidden!==hidden){part.hidden=hidden;visibilityChanged=true;}
   }
   if(!observing&&root.isConnected){
    observing=true;
    resize?.observe(zones.main);
    resize?.observe(zones.side);
    narrow.addEventListener('change',scheduleBalance);
    phone.addEventListener('change',resizePhone);
    root.addEventListener('focusout',scheduleBalance);
    scheduleBalance();
   }
   if(visibilityChanged||!resize)scheduleBalance();
  }
  function apply(){
   redistributed=false;
   const placement=phone.matches&&!state.manual?phoneDefaults:state.zones;
   root.classList.add('m-home-refined');
   Object.entries(placement).forEach(([z,names])=>names.forEach(n=>zones[z].append(parts[n])));
   reconcile();scheduleBalance();window.dispatchEvent(new Event('myos:layout'));
  }
  function resizePhone(){if(!editing&&!state.manual)apply();}
  function capture(){
   state.manual=true;
   Object.entries(zones).forEach(([z,el])=>{
    state.zones[z]=[...el.children].map(n=>n.dataset.card).filter(Boolean);
   });
   save();
  }
  function moveTo(n,zone,before){const part=parts[n],parent=zones[zone];if(!part||!parent||before===part)return;parent.insertBefore(part,before||null);capture();announce(labels[n]+'已移动');window.dispatchEvent(new Event('myos:layout'));}
  function adjacent(n,delta){const part=parts[n],siblings=[...part.parentNode.children].filter(p=>p.dataset.card&&!p.hidden),i=siblings.indexOf(part);
   if(delta<0&&i>0)part.parentNode.insertBefore(part,siblings[i-1]);
   else if(delta>0&&i<siblings.length-1)part.parentNode.insertBefore(part,siblings[i+1].nextSibling);
   else return;
   capture();announce(labels[n]+'顺序已调整');part.querySelector('.m-grab')?.focus();}
  function cross(n){moveTo(n,parts[n].parentNode===zones.main?'side':'main',null);parts[n].querySelector('.m-grab')?.focus();}
  function begin(e,n){if(!editing||e.button>0||drag)return;drag={n,button:e.currentTarget,id:e.pointerId,x:e.clientX,y:e.clientY,sx:e.clientX,sy:e.clientY,active:false,target:null,before:null};e.currentTarget.setPointerCapture(e.pointerId);e.preventDefault();}
  function move(e){if(!drag||e.pointerId!==drag.id)return;drag.x=e.clientX;drag.y=e.clientY;if(!drag.active&&Math.hypot(drag.x-drag.sx,drag.y-drag.sy)<5)return;
   if(!drag.active){drag.active=true;app.dragging=true;root.classList.add('m-layout-dragging');const ghost=document.createElement('div');ghost.className='m-drag-ghost';ghost.textContent='⠿  '+labels[drag.n];document.body.append(ghost);drag.ghost=ghost;parts[drag.n].classList.add('m-drag-source');}
   if(!frame)frame=requestAnimationFrame(paint);
  }
  function paint(){frame=0;if(!drag?.active)return;drag.ghost.style.transform='translate3d('+(drag.x+14)+'px,'+(drag.y+12)+'px,0)';
   const hit=document.elementFromPoint(drag.x,drag.y),zone=hit?.closest('.m-layout-zone');
   root.querySelectorAll('.m-drop-before,.m-drop-after,.m-drop-zone').forEach(n=>n.classList.remove('m-drop-before','m-drop-after','m-drop-zone'));drag.target=null;drag.before=null;
   if(zone){drag.target=Object.keys(zones).find(k=>zones[k]===zone);const card=hit.closest('.m-slot');
    if(card&&card!==parts[drag.n]&&card.parentNode===zone){const r=card.getBoundingClientRect(),before=drag.y<r.top+r.height/2;drag.before=before?card:card.nextElementSibling;card.classList.add(before?'m-drop-before':'m-drop-after');}
    else if(card!==parts[drag.n])zone.classList.add('m-drop-zone');else drag.target=null;}
   if(drag.y<60)window.scrollBy(0,-20);else if(drag.y>innerHeight-60)window.scrollBy(0,20);
  }
  function end(cancel){if(!drag)return;const d=drag;drag=null;cancelAnimationFrame(frame);frame=0;if(d.button.hasPointerCapture?.(d.id))d.button.releasePointerCapture(d.id);d.ghost?.remove();parts[d.n].classList.remove('m-drag-source');root.classList.remove('m-layout-dragging');root.querySelectorAll('.m-drop-before,.m-drop-after,.m-drop-zone').forEach(n=>n.classList.remove('m-drop-before','m-drop-after','m-drop-zone'));
   if(!cancel&&d.active&&d.target)moveTo(d.n,d.target,d.before);app.dragging=false;if(app.pendingData){const data=app.pendingData;app.pendingData=null;app.update(data);}app.scheduleNext();}
  Object.entries(parts).forEach(([n,part])=>{
   part.dataset.card=n;const handle=document.createElement('div');handle.className='m-layout-handle';const grab=document.createElement('button');grab.type='button';grab.className='m-grab';grab.textContent='⠿  '+labels[n];grab.title='拖动调整；方向键上下排序、左右换栏';grab.setAttribute('aria-label','拖动'+labels[n]+'，也可用方向键调整');
   grab.addEventListener('pointerdown',e=>begin(e,n));grab.addEventListener('pointermove',move);grab.addEventListener('pointerup',()=>end(false));grab.addEventListener('pointercancel',()=>end(true));grab.addEventListener('keydown',e=>{if(e.key==='Escape'){end(true);return;}if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();if(e.key==='ArrowUp'||e.key==='ArrowDown')adjacent(n,e.key==='ArrowUp'?-1:1);else cross(n);}});
   handle.append(grab);[['↑','上移',()=>adjacent(n,-1)],['↓','下移',()=>adjacent(n,1)],['⇄','移到另一栏',()=>cross(n)]].forEach(([text,label,fn])=>{const b=document.createElement('button');b.type='button';b.textContent=text;b.title=label;b.setAttribute('aria-label',labels[n]+label);b.addEventListener('click',fn);handle.append(b);});part.prepend(handle);
  });
  function toggleEdit(){editing=!editing;root.classList.toggle('m-layout-editing',editing);app.editButton.textContent=editing?'完成布局':'编辑布局';app.editButton.setAttribute('aria-pressed',String(editing));if(!editing){end(true);if(state.manual)scheduleBalance();else apply();}announce(editing?'拖动手柄调整卡片；自动保存。':'布局已保存。');window.dispatchEvent(new Event('myos:layout'));}
  function reset(){end(true);state={zones:defaultZones(model),hidden:[]};try{localStorage.removeItem(key);}catch(_){}apply();announce('已恢复默认布局。');}
  function picker(){dialog?.remove();dialog=document.createElement('dialog');dialog.className='m-client-dialog m-layout-dialog';const h=document.createElement('h2');h.textContent='添加 / 隐藏卡片';dialog.append(h);const text=document.createElement('p');text.textContent='仅列出本机适用的卡片。勾选后立即显示，不增加服务或修改网络。';dialog.append(text);
   Object.keys(parts).filter(n=>available(n)&&n!=='controls').forEach(n=>{const label=document.createElement('label');label.className='m-card-choice';const check=document.createElement('input');check.type='checkbox';check.checked=!state.hidden.includes(n);check.addEventListener('change',()=>{state.hidden=state.hidden.filter(v=>v!==n);if(!check.checked)state.hidden.push(n);reconcile();save();});label.append(check,document.createTextNode(labels[n]));dialog.append(label);});
   const done=document.createElement('button');done.type='button';done.className='m-pill-button';done.textContent='完成';const current=dialog;done.addEventListener('click',()=>current.close());current.append(done);current.addEventListener('close',()=>current.remove());document.body.append(current);current.showModal();}
  load();apply();return {toggleEdit,reset,picker,reconcile,moveTo,adjacent,get state(){return state;},destroy(){end(true);dialog?.remove();resize?.disconnect();narrow.removeEventListener('change',scheduleBalance);phone.removeEventListener('change',resizePhone);root.removeEventListener('focusout',scheduleBalance);observing=false;cancelAnimationFrame(balanceFrame);balanceFrame=0;}};
 };
})();
