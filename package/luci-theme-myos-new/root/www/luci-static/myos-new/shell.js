(() => {
 'use strict';
 const root=document.documentElement, sidebar=document.querySelector('#m-sidebar'), nav=document.querySelector('#m-menu');
 const button=document.querySelector('#m-menu-toggle'),scrim=document.querySelector('#m-scrim');
 const narrow=matchMedia('(max-width:1024px), (max-width:1200px) and (pointer:coarse)');
 const reduce=matchMedia('(prefers-reduced-motion:reduce)');

 try { const appearance=localStorage.getItem('myos-new-appearance');if(appearance==='light'||appearance==='dark')root.dataset.appearance=appearance; } catch (_) {}
 const appearanceButton=document.querySelector('#m-appearance');
 const syncPluginAppearance=()=>{
  const dark=root.dataset.appearance==='dark';
  root.dataset.darkmode=String(dark);
  document.querySelector('meta[name="theme-color"]')?.setAttribute('content',dark?'#090a0c':'#edf0f4');
  if(appearanceButton){
   const label=dark?'切换至浅色':'切换至深色';
   appearanceButton.title=label;appearanceButton.setAttribute('aria-label',label);
   appearanceButton.innerHTML='<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'+(dark?'<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M4.93 4.93l1.42 1.42m11.3 11.3 1.42 1.42M4.93 19.07l1.42-1.42m11.3-11.3 1.42-1.42"/>':'<path d="M20.9 13.2A9 9 0 0 1 10.8 3.1 9 9 0 1 0 20.9 13.2Z"/>')+'</svg>';
  }
 };
 syncPluginAppearance();
 new MutationObserver(syncPluginAppearance).observe(root,{attributes:true,attributeFilter:['data-appearance']});
 function themeDdnsto(){
  const shadow=document.querySelector('.ddnsto-root')?.shadowRoot;if(!shadow||shadow.querySelector('#myos-ddnsto-theme'))return;
  const css=document.createElement('link');css.id='myos-ddnsto-theme';css.rel='stylesheet';css.href=L.media('ddnsto.css')+'?v=20261105';shadow.append(css);
 }
 themeDdnsto();window.addEventListener('load',themeDdnsto,{once:true});
 appearanceButton.addEventListener('click',()=>{
  const dark=root.dataset.appearance==='dark';
  root.dataset.appearance=dark?'light':'dark';
  try { localStorage.setItem('myos-new-appearance',root.dataset.appearance); } catch (_) {}
 });
 let open=false,drawerMotion=null,scrimMotion=null;
 function drawer(value,instant=false){
  const mobile=narrow.matches;
  const animate=mobile&&!instant&&!reduce.matches;
  const current=animate?getComputedStyle(sidebar).transform:'none';
  const opacity=animate&&!scrim.hidden?getComputedStyle(scrim).opacity:'0';
  drawerMotion?.cancel();
  scrimMotion?.cancel();
  drawerMotion=scrimMotion=null;
  open=value&&mobile;
  button.setAttribute('aria-expanded',String(open));
  sidebar.inert=mobile&&!open;
  scrim.inert=!open;
  scrim.hidden=!open&&!animate;
  scrim.style.opacity=open?'1':'0';
  if(!mobile){sidebar.style.transform='';sidebar.style.visibility='';return;}
  const target=open?'translateX(0)':'translateX(-100%)';
  sidebar.style.transform=target;
  sidebar.style.visibility=open||animate?'visible':'hidden';
  if(animate){
   const timing={duration:open?240:180,easing:'cubic-bezier(.32,.72,0,1)'};
   const animation=sidebar.animate([{transform:current},{transform:target}],timing);
   scrimMotion=scrim.animate([{opacity},{opacity:open?'1':'0'}],timing);
   drawerMotion=animation;
   animation.onfinish=()=>{
    if(drawerMotion!==animation)return;
    drawerMotion=scrimMotion=null;
    sidebar.style.visibility=open?'visible':'hidden';
    scrim.hidden=!open;
   };
  }
  if(open)document.querySelector('.m-home').focus({preventScroll:true});else button.focus({preventScroll:true});
 }
 button.addEventListener('click',e=>drawer(!open,e.detail===0));scrim.addEventListener('click',e=>drawer(false,e.detail===0));
 narrow.addEventListener('change',()=>drawer(false,true));sidebar.inert=narrow.matches;
 reduce.addEventListener('change',()=>{if(drawerMotion)drawer(open,true);});
 document.addEventListener('keydown',e=>{
  if(!open)return;if(e.key==='Escape'){drawer(false,true);return;}
  if(e.key==='Tab'){
   const items=[...sidebar.querySelectorAll('a,input,summary')].filter(n=>n.getClientRects().length);
   const first=items[0],last=items[items.length-1];
   if(e.shiftKey&&document.activeElement===first){e.preventDefault();last.focus();}
   else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first.focus();}
  }
 });
 const suppressed=new Set(['admin/myos','admin/modeminfo','admin/home','admin/status/c5800','admin/status/c2000max','admin/status/c2000ultra','admin/status/nbcpe','admin/status/c8']);
 const drawings={
  home:'M3 10 12 3l9 7M5 9v12h5v-7h4v7h5V9',
  close:'M6 6l12 12M18 6 6 18',
  status:'M3 13a9 9 0 1 1 18 0v5H3zM12 13l4-5M6 13h1m10 0h1M12 5v1',
  system:'M9 3h6l1 3 3 1 2 5-2 5-3 1-1 3H9l-1-3-3-1-2-5 2-5 3-1zM15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0',
  network:'M4 4h6v6H4zM14 14h6v6h-6zM14 4h6v6h-6zM7 10v7h7M10 7h4',
  services:'M3 3h7v7H3zM14 3h7v7h-7zM3 14h7v7H3zM14 14h7v7h-7z',
  modem:'M12 9v12M8 21h8M8 6a6 6 0 0 0 0 9M16 6a6 6 0 0 1 0 9M5 3a10 10 0 0 0 0 15M19 3a10 10 0 0 1 0 15M13 9a1 1 0 1 1-2 0 1 1 0 0 1 2 0',
  wifi:'M3 8a14 14 0 0 1 18 0M6 12a9 9 0 0 1 12 0M9 16a4 4 0 0 1 6 0M12 20h.01',
  shield:'M12 3 3 7v5c0 5 5 8 9 10 4-2 9-5 9-10V7zM8 12l3 3 5-6',
  key:'M10 8a4 4 0 1 1-8 0 4 4 0 0 1 8 0M9 11l10 10 3-3-3-3-2 2-2-2 2-2',
  box:'M3 7 12 2l9 5v11l-9 5-9-5zM3 7l9 5 9-5M12 12v11M7 4l10 6',
  store:'M4 8h16l1 13H3zM8 8V6a4 4 0 0 1 8 0v2',
  logout:'M10 3H4v18h6M10 12h11M17 8l4 4-4 4',
  clock:'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M12 6v6l4 2',
  log:'M5 3h14v18H5zM8 7h8M8 11h8M8 15h5',
  chart:'M3 3v18h18M6 15l4-5 4 3 6-8',
  chip:'M6 6h12v12H6zM9 9h6v6H9zM9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4',
  terminal:'M3 4h18v16H3zM6 8l4 4-4 4M13 16h5',
  message:'M3 4h18v13H9l-6 4zM7 8h10M7 12h7',
  drive:'M5 4h14l3 12v5H2v-5zM2 16h20M6 19h.01M10 19h.01',
  update:'M4 8a9 9 0 1 1-1 8M4 3v5h5M12 7v10M8 11l4-4 4 4',
  bolt:'M13 2 4 14h7l-1 8 10-13h-7z',
  globe:'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M3 12h18M12 3c-5 5-5 13 0 18 5-5 5-13 0-18',
  power:'M12 2v10M6 5a9 9 0 1 0 12 0'
 };
 const iconNames={overview:'status',realtime:'chart',routes:'network',nftables:'shield',firewall:'shield',logs:'log',factory_logs:'log',processes:'chip',
  mwan3:'network',interface:'network',member:'network',policy:'shield',rule:'log',notify:'message',globals:'globe',dhcp:'globe',uhttpd:'globe',
  mwrt_wizard:'bolt',admin:'key',password:'key',dropbear:'terminal',sshkeys:'key',repokeys:'key','package-manager':'box',
  myos_theme:'services',startup:'power',crontab:'clock',mounts:'drive',leds:'bolt',myos_update:'update',myos_system:'drive',flash:'drive',myos_nros:'globe',reboot:'power',
  mt5700:'modem',qmodem:'modem',nets:'modem',cellscan:'modem',smsc:'message',smstrun:'message',at:'terminal',at2:'terminal',traffic_dividing:'network',
  wireless:'wifi',factory_guest:'wifi',diagnostics:'terminal',troubleshooting:'terminal',turboacc:'bolt',qiyougamebooster:'bolt',
  passwall:'shield',passwall2:'shield',shadowsocksr:'shield',adguardhome:'shield',openclash:'shield',upnp:'network',vpn:'shield',nas:'drive',istore:'store'};
 const make=(tag,cls,text)=>{const el=document.createElement(tag);if(cls)el.className=cls;if(text!=null)el.textContent=text;return el;};
 const menuMotion=new Map();
 function animateFold(group,content,summary,event,requested){
  if(event)event.preventDefault();
  const expanded=requested==null?!group.open:requested;
  const changes=[{group,content,summary,expanded}];
  if(expanded)[...group.parentElement.children].forEach(sibling=>{
   if(sibling!==group&&sibling.tagName==='DETAILS'&&sibling.open)
    changes.push({group:sibling,content:sibling.querySelector(':scope > .m-menu-children'),summary:sibling.querySelector('summary'),expanded:false});
  });
  const siblings=[...group.parentElement.children];
  const instant=reduce.matches||event?.detail===0;
  const before=instant?[]:siblings.map(el=>el.getBoundingClientRect().top);
  for(const animation of menuMotion.values())animation.cancel();
  menuMotion.clear();
  changes.forEach(item=>{item.group.open=item.expanded;item.summary.setAttribute('aria-expanded',String(item.expanded));});
  if(instant)return;
  // Layout changes once; the browser composites the remaining movement.
  const after=siblings.map(el=>el.getBoundingClientRect().top);
  const play=(el,frames)=>{
   const animation=el.animate(frames,{duration:180,easing:'cubic-bezier(.23,1,.32,1)'});
   menuMotion.set(el,animation);
   animation.onfinish=()=>{if(menuMotion.get(el)===animation)menuMotion.delete(el);};
  };
  siblings.forEach((el,i)=>{const dy=before[i]-after[i];if(Math.abs(dy)>.5)play(el,[{transform:'translateY('+dy+'px)'},{transform:'translateY(0)'}]);});
  changes.forEach(item=>{if(item.expanded)play(item.content,[{opacity:0,transform:'translateY(-5px)'},{opacity:1,transform:'translateY(0)'}]);});
 }
 function enterView(){
  const panel=document.querySelector('#view')||document.querySelector('#maincontent');
  if(!panel||reduce.matches)return;
  const animate=()=>{
   const heading=panel.querySelector('.m-page-heading,.cbi-map-title,h2');
   if(heading)heading.animate([{opacity:0,transform:'translateY(5px)'},{opacity:1,transform:'translateY(0)'}],{duration:180,easing:'cubic-bezier(.2,.8,.2,1)'});
  };
  if(panel.id==='view'&&panel.querySelector('.spinning')){
   const observer=new MutationObserver(()=>{if(!panel.querySelector(':scope > .spinning')){observer.disconnect();animate();}});
   observer.observe(panel,{childList:true});
  }else animate();
 }
 const icon=(name,fallback)=>{
  const key=String(name).toLowerCase(),kind=iconNames[key]||key,ns='http://www.w3.org/2000/svg';
  const wrap=make('span','m-nav-icon'),svg=document.createElementNS(ns,'svg'),path=document.createElementNS(ns,'path');
  svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('fill','none');svg.setAttribute('stroke','currentColor');
  svg.setAttribute('stroke-width','1.65');svg.setAttribute('stroke-linecap','round');svg.setAttribute('stroke-linejoin','round');
  svg.setAttribute('aria-hidden','true');path.setAttribute('d',drawings[kind]||drawings[fallback]||drawings.services);
  svg.append(path);wrap.append(svg);return wrap;
 };
 document.querySelector('.m-home .m-nav-icon').replaceWith(icon('home'));
 function brandWizard(element){
  const brand=element.querySelector?.('.qsw-brand');if(!brand)return;
  const logo=make('img');logo.src=L.media('brand.png');logo.alt='MYOS';
  brand.replaceChildren(logo,make('span','','MYOS-NEW'));
  const close=element.querySelector('.qsw-close');if(close)close.replaceChildren(icon('close'));
 }
 brandWizard(document.body);
 new MutationObserver(records=>records.forEach(record=>record.addedNodes.forEach(brandWizard))).observe(document.body,{childList:true});
 async function load(){
  try{
   const ui=await L.require('ui');let tree=await ui.menu.load();
   let branch=tree;for(const part of (L.env.dispatchpath||[]).slice(0,3))branch=branch?.children?.[part];
   // Plugin activation can add routes while the browser still holds the old menu.
   if(!branch){
    ui.menu.flushCache();const request=await L.require('request');
    tree=(await request.get(L.url('admin/menu'),{cache:false})).json();
   }
   const children=n=>ui.menu.getChildren(n).filter(c=>!c.hidden&&c.satisfied!==false&&c.title);
   const path=location.pathname.replace(/\/$/,'');
   const home=document.querySelector('.m-home');
   const homePaths=[home.href,L.url(),L.url('admin')].map(url=>new URL(url,location.href).pathname.replace(/\/$/,''));
   home.classList.toggle('active',homePaths.includes(path));
   if(home.classList.contains('active'))home.setAttribute('aria-current','page');
   let title=home.classList.contains('active')?'首页':'设备控制台';
   const render=(node,parts)=>{
    const key=parts.join('/');if(suppressed.has(key))return null;
    const label=_(node.title),kids=children(node).map(c=>render(c,parts.concat(c.name))).filter(Boolean);
    if(kids.length){
     const group=make('details','m-menu-group'),summary=make('summary');
     summary.append(icon(node.name,parts[1]),make('span','',label));
     const list=make('div','m-menu-children');kids.forEach(n=>list.append(n));group.append(summary,list);
     summary.addEventListener('click',e=>animateFold(group,list,summary,e));
     if(list.querySelector('[aria-current]'))group.open=true;group.dataset.label=label;return group;
    }
    if(['firstchild','alias'].includes(node.action?.type)&&children(node).length)return null;
    const a=make('a','m-menu-link');a.append(icon(node.name,parts[1]),make('span','',label));a.href=L.url(...parts);a.dataset.label=label;
    const url=new URL(a.href,location.href).pathname;
    if(path===url||path.startsWith(url+'/')){a.classList.add('active');a.setAttribute('aria-current','page');title=label;}
    return a;
   };
   const fragment=document.createDocumentFragment();
   children(tree.children?.admin||tree).forEach(c=>{const el=render(c,['admin',c.name]);if(el)fragment.append(el);});
   nav.replaceChildren(fragment);document.querySelector('#m-page-title').textContent=title;
   const tabmenu=document.querySelector('#tabmenu'),dispatch=L.env.dispatchpath||[];
   tabmenu.replaceChildren();tabmenu.hidden=true;
   let current=tree,parts=[];
   for(let i=0;i<3&&current;i++){parts.push(dispatch[i]);current=current.children?.[dispatch[i]];}
   const renderTabs=(parent,url,depth)=>{
    const items=ui.menu.getChildren(parent).filter(c=>c.title&&c.satisfied!==false);
    if(!items.length)return;
    const tabs=make('ul','tabs');let active;
    items.forEach(child=>{
     const selected=dispatch[depth]===child.name,a=make('a','',_(child.title));
     a.href=L.url(...url,child.name);
     if(selected){a.setAttribute('aria-current','page');active=child;}
     const li=make('li',selected?'active':'');li.append(a);tabs.append(li);
    });
    tabmenu.append(tabs);tabmenu.hidden=false;
    if(active)renderTabs(active,url.concat(active.name),depth+1);
   };
   if(current&&dispatch.length>=3)renderTabs(current,parts,3);
  }catch(error){nav.replaceChildren(make('p','m-error','导航载入失败，请刷新重试。'));}
 }
 document.querySelector('#m-menu-search').addEventListener('input',e=>{
  const q=e.target.value.trim().toLowerCase();
  nav.querySelectorAll('a').forEach(a=>a.hidden=!a.textContent.toLowerCase().includes(q));
  [...nav.querySelectorAll('details')].reverse().forEach(d=>{resetFold(d);d.hidden=!!q&&![...d.querySelectorAll('a')].some(a=>!a.hidden);if(q&&!d.hidden)d.open=true;});
 });
 nav.addEventListener('click',e=>{if(e.target.closest('a')&&open)drawer(false,e.detail===0);});
 enterView();load();
})();
