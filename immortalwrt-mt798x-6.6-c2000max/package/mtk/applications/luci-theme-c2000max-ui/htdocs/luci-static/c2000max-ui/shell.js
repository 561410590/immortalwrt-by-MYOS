(() => {
'use strict';
const $ = s => document.querySelector(s);
const paths = { home:['admin','status','c2000max'], wifi:['admin','network','wireless'], signal:['admin','modem','qmodem'], devices:['admin','network','dhcp'] };
document.querySelectorAll('.mobile-nav [data-panel]').forEach(button => {
 const path = paths[button.dataset.panel];
 if (!path) return;
 const link = document.createElement('a'); link.className = button.className; link.replaceChildren(...button.childNodes);
 link.href = button.dataset.panel === 'home' ? document.body.dataset.homeUrl : L.url(...path); link.classList.toggle('active',location.pathname === link.pathname);
 if (link.classList.contains('active')) link.setAttribute('aria-current','page');
 button.replaceWith(link);
});
const account = $('#account-button'), menu = $('#account-menu');
function close() { if (menu) menu.hidden = true; account?.setAttribute('aria-expanded','false'); }
account?.addEventListener('click',() => { const opening = menu.hidden; menu.hidden = !opening; account.setAttribute('aria-expanded',String(opening)); });
$('#logout-button')?.addEventListener('click',() => location.assign(L.url('admin','logout')));
document.addEventListener('click',e => { if (!e.target.closest('.account,.account-menu')) close(); });
document.addEventListener('keydown',e => { if (e.key === 'Escape') close(); });
const indicators=document.querySelector('#indicators');
function labelRefresh(){indicators?.querySelectorAll('[data-indicator="poll-status"]').forEach(el=>{el.setAttribute('role','button');el.tabIndex=0;el.setAttribute('aria-label',el.getAttribute('data-style')==='active'?'暂停自动刷新':'恢复自动刷新');el.title=el.getAttribute('aria-label');
 if(!el.querySelector('svg')){
  const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg'),path=document.createElementNS(ns,'use');
  svg.setAttribute('class','icon');svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('aria-hidden','true');
  path.setAttribute('href','#refresh');
  svg.append(path);el.replaceChildren(svg);
 }});}
if(indicators){new MutationObserver(labelRefresh).observe(indicators,{childList:true,subtree:true,attributeFilter:['data-style']});indicators.addEventListener('keydown',e=>{if(e.target.matches('[data-indicator="poll-status"]')&&['Enter',' '].includes(e.key)){e.preventDefault();e.target.click();}});labelRefresh();}
const dropdownSearches=new Map();
function enhanceDropdowns() {
 for(const [widget,cleanup] of dropdownSearches) {
  if(!widget.isConnected||!widget.hasAttribute('open')) { cleanup();dropdownSearches.delete(widget); }
 }
 document.querySelectorAll('.cbi-dropdown:not(.btn)[open]').forEach(widget=>{
  if(dropdownSearches.has(widget))return;
  const list=widget.querySelector('ul.dropdown');if(!list)return;
  const options=[...list.children].filter(el=>el.hasAttribute('data-value')&&!el.querySelector('input'));
  if(options.length<2)return;
  const row=document.createElement('li'),input=document.createElement('input');
  row.className='dropdown-search';row.setAttribute('unselectable','');
  input.type='search';input.placeholder='筛选名称、IP 或 MAC';input.setAttribute('aria-label','筛选选项');
  const hidden=options.map(el=>el.hidden);
  function restore(){options.forEach((el,i)=>{el.hidden=hidden[i];});row.remove();}
  dropdownSearches.set(widget,restore);
  input.addEventListener('input',()=>{const q=input.value.trim().toLowerCase();options.forEach((el,i)=>{el.hidden=hidden[i]||!(el.textContent+' '+el.dataset.value).toLowerCase().includes(q);});});
  ['click','touchstart'].forEach(type=>input.addEventListener(type,e=>e.stopPropagation(),{passive:true}));
  input.addEventListener('keydown',e=>{if(e.key==='Escape'){widget.dispatchEvent(new CustomEvent('cbi-dropdown-close'));}else if(e.key==='ArrowDown'){options.find(el=>!el.hidden)?.focus();}e.stopPropagation();});
  row.append(input);list.prepend(row);list.scrollTop=0;
 });
}
new MutationObserver(enhanceDropdowns).observe(document.body,{subtree:true,childList:true,attributes:true,attributeFilter:['open']});
L.require('ui').then(async ui => {
 const tree = await window.C2000MenuReady || await ui.menu.load();
 const path = L.env.dispatchpath || [], container = $('#tabmenu');
 let node = tree;
 for (let i=0; i<3 && node; i++) node = node.children?.[path[i]];
 function tabs(parent, segments, level) {
  const children = ui.menu.getChildren(parent);
  if (!children.length || !container) return;
  const ul = document.createElement('ul'); ul.className = 'tabs';
  children.forEach(child => {
   const li=document.createElement('li'), a=document.createElement('a');
   const active=child.name===path[level]; li.classList.toggle('active',active);
   a.textContent=_(child.title); a.href=L.url(...segments,child.name);
   if (active) a.setAttribute('aria-current','page');
   li.append(a); ul.append(li);
  });
  container.append(ul); container.hidden=false;
  const active=children.find(child=>child.name===path[level]);
  if(active) tabs(active,[...segments,active.name],level+1);
 }
 if(node) tabs(node,path.slice(0,3),3);
}).catch(() => {});
})();
