/* Original, dependency-free rounded-lens renderer for MYOS-NEW.
 * SVG displacement samples the live backdrop; content remains sharp.
 * Kyant0/AndroidLiquidGlass is a visual reference, not an imported dependency.
 */
(() => {
 'use strict';
 const root=document.documentElement,ns='http://www.w3.org/2000/svg';
 const reduce=matchMedia('(prefers-reduced-motion:reduce)'),contrast=matchMedia('(prefers-contrast:more)'),opaque=matchMedia('(prefers-reduced-transparency:reduce)');
 const chromium=/Chrome|Chromium|Edg\//.test(navigator.userAgent)&&!/EdgiOS|CriOS/.test(navigator.userAgent);
 const supported=chromium&&typeof CSS!=='undefined'&&CSS.supports('backdrop-filter','url("#myos-liquid-probe") blur(1px)');
 let mode='refraction',scheduled=false,serial=0,timer=0;
 try{const stored=localStorage.getItem('myos-liquid-material');if(['refraction','frosted','solid'].includes(stored))mode=stored;}catch(_){}
 root.dataset.glass=mode;root.dataset.lensEngine=supported?'svg':'blur';
 const svg=document.createElementNS(ns,'svg'),defs=document.createElementNS(ns,'defs');
 svg.setAttribute('aria-hidden','true');svg.setAttribute('width','0');svg.setAttribute('height','0');svg.style.cssText='position:absolute;pointer-events:none;overflow:hidden;';svg.append(defs);document.body.append(svg);
 const filters=new Map(),targets=new Set();
 const make=(name,attrs)=>{const el=document.createElementNS(ns,name);Object.entries(attrs||{}).forEach(([k,v])=>el.setAttribute(k,String(v)));return el;};
 function displacement(w,h,r){
  // Limit bitmap size and cache by geometry: no per-frame pixel work.
  const scale=Math.min(.5,512/Math.max(w,h)),cw=Math.max(4,Math.round(w*scale)),ch=Math.max(4,Math.round(h*scale));
  const canvas=document.createElement('canvas');canvas.width=cw;canvas.height=ch;
  const ctx=canvas.getContext('2d');if(!ctx)throw new Error('Canvas unavailable');
  const image=ctx.createImageData(cw,ch),pixels=image.data,thick=Math.min(24,r*.95);
  for(let y=0;y<ch;y++)for(let x=0;x<cw;x++){
   const px=(x+.5)*w/cw-w/2,py=(y+.5)*h/ch-h/2;
   const qx=Math.abs(px)-(w/2-r),qy=Math.abs(py)-(h/2-r),ax=Math.max(qx,0),ay=Math.max(qy,0),len=Math.hypot(ax,ay);
   const distance=Math.min(Math.max(qx,qy),0)+len-r;
   let dx=0,dy=0;
   if(distance<=0&&distance>-thick){
    const t=-distance/thick,bend=Math.sin(Math.PI*t)*Math.pow(1-t,1.25)*1.6;
    const nx=len?ax/len:(qx>qy?1:0),ny=len?ay/len:(qy>=qx?1:0);
    dx=-Math.sign(px)*nx*bend;dy=-Math.sign(py)*ny*bend;
   }
   const i=(y*cw+x)*4;pixels[i]=Math.round(128+dx*120);pixels[i+1]=Math.round(128+dy*120);pixels[i+2]=128;pixels[i+3]=255;
  }
  ctx.putImageData(image,0,0);return canvas.toDataURL('image/png');
 }
 function lens(w,h,r){
  const key=[w,h,r].join('x');if(filters.has(key))return filters.get(key);
  const id='myos-liquid-'+(++serial),f=make('filter',{id,x:0,y:0,width:w,height:h,filterUnits:'userSpaceOnUse','color-interpolation-filters':'sRGB'});
  // Soften only the background before refraction. Separate red/blue displacements
  // add restrained, real chromatic separation along the lens edge.
  const image=make('feImage',{x:0,y:0,width:w,height:h,preserveAspectRatio:'none',result:'map'});
  image.setAttribute('href',displacement(w,h,r));
  f.append(make('feGaussianBlur',{in:'SourceGraphic',stdDeviation:14,result:'soft'}),image);
  f.append(make('feDisplacementMap',{in:'soft',in2:'map',scale:32,xChannelSelector:'R',yChannelSelector:'G',result:'main'}));
  f.append(make('feDisplacementMap',{in:'soft',in2:'map',scale:35,xChannelSelector:'R',yChannelSelector:'G',result:'redshift'}));
  f.append(make('feColorMatrix',{in:'redshift',type:'matrix',values:'1 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 1 0',result:'red'}));
  f.append(make('feColorMatrix',{in:'main',type:'matrix',values:'0 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 1 0',result:'gb'}));
  f.append(make('feBlend',{in:'red',in2:'gb',mode:'screen',result:'lens'}));
  f.append(make('feColorMatrix',{in:'lens',type:'saturate',values:1.45}));
  defs.append(f);const entry={id,node:f,key};filters.set(key,entry);return entry;
 }
 function scan(){
  scheduled=false;
  const active=new Set(),enabled=supported&&mode==='refraction'&&!contrast.matches&&!opaque.matches;
  const nodes=document.querySelectorAll('.m-card,.m-toolbar,.m-sidebar,.m-login,.cbi-section');
  for(const node of targets){if(!node.isConnected){resize?.unobserve(node);targets.delete(node);}}
  for(const node of nodes){
   if(node.matches('.cbi-section')&&node.parentElement.closest('.cbi-section,.modal'))continue;
   if(!targets.has(node)){targets.add(node);resize?.observe(node);}
   const rect=node.getBoundingClientRect();
   if(!enabled||rect.width<40||rect.height<25||rect.bottom< -100||rect.top>innerHeight+150){node.style.removeProperty('--lg-filter');continue;}
   const w=Math.round(rect.width),h=Math.round(rect.height),r=Math.min(parseFloat(getComputedStyle(node).borderTopLeftRadius)||22,w/2,h/2);
   try{
    const entry=lens(w,h,Math.round(r));active.add(entry.key);
    const value='url("#'+entry.id+'")';if(node.style.getPropertyValue('--lg-filter')!==value)node.style.setProperty('--lg-filter',value);
   }catch(_){node.style.removeProperty('--lg-filter');}
  }
  // Bound cache and detach filters that no visible element references.
  if(filters.size>48)for(const [key,item] of filters){if(!active.has(key)){item.node.remove();filters.delete(key);}if(filters.size<=32)break;}
 }
 function schedule(){if(scheduled||document.hidden)return;scheduled=true;requestAnimationFrame(scan);}
 const resize=typeof ResizeObserver!=='undefined'?new ResizeObserver(schedule):null;
 window.addEventListener('myos:layout',schedule);
 const content=document.getElementById('maincontent');
 if(content)new MutationObserver(records=>{
  if(records.some(record=>[...record.addedNodes].some(node=>node.nodeType===1&&(node.matches('.cbi-section')||node.querySelector('.cbi-section')))))schedule();
 }).observe(content,{childList:true,subtree:true});
 window.addEventListener('resize',schedule,{passive:true});window.addEventListener('scroll',()=>{clearTimeout(timer);timer=setTimeout(schedule,90);},{passive:true});
 document.addEventListener('visibilitychange',()=>{if(!document.hidden)schedule();});
 contrast.addEventListener('change',schedule);opaque.addEventListener('change',schedule);
 const button=document.getElementById('m-glass-mode');
 function label(){if(button){const caption=mode==='solid'?'纯色':mode==='frosted'?'轻量磨砂':supported?'液态折射':'兼容磨砂';button.title='玻璃材质：'+caption+'（点击切换）';button.setAttribute('aria-label',button.title);button.dataset.mode=mode;}}
 if(button)button.addEventListener('click',()=>{
  mode=mode==='refraction'?'frosted':mode==='frosted'?'solid':'refraction';root.dataset.glass=mode;
  try{localStorage.setItem('myos-liquid-material',mode);}catch(_){}label();schedule();
 });
 let pointerFrame=0,pointerTarget=null,position=null;
 document.addEventListener('pointermove',event=>{
  if(reduce.matches||event.pointerType==='touch'||mode==='solid')return;
  pointerTarget=event.target.closest?.('.m-card,.m-toolbar');if(!pointerTarget)return;
  position={x:event.clientX,y:event.clientY};if(pointerFrame)return;
  pointerFrame=requestAnimationFrame(()=>{
   pointerFrame=0;if(!pointerTarget?.isConnected)return;
   const r=pointerTarget.getBoundingClientRect();pointerTarget.style.setProperty('--lg-x',((position.x-r.left)/r.width*100).toFixed(1)+'%');pointerTarget.style.setProperty('--lg-y',((position.y-r.top)/r.height*100).toFixed(1)+'%');
  });
 },{passive:true});
 label();schedule();
})();
