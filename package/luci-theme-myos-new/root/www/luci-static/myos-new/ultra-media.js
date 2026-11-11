/* Use the bundled video on the device and page it belongs to. */
(() => {
 'use strict';
 const body=document.body;
 const model=((body.dataset.deviceModel||'')+' '+(body.dataset.deviceBoard||'')).toLowerCase().replace(/[^a-z0-9]/g,'');
 const login=body.classList.contains('m-auth');
 const board=(body.dataset.deviceBoard||'').toLowerCase();
 const ultra=model.includes('c2000ultra')||(board==='hcmt7987-snsd'&&!model.includes('c2000max'));
 let file;
 if(login){
  if(model.includes('c8688')||board==='hcmt7981-688-emmc')file='c8-688-login.mp4';
  else if(model.includes('c5800')||board==='hcmt7981-emmc')file='c5800-688-login.mp4';
  else if(ultra)file='c2000-ultra-promo.mp4';
 }else if(ultra&&document.querySelector('.m-workspace')){
  file='c2000-ultra-promo.mp4';
 }
 if(!file)return;
 const backgroundClass=login?'m-login-video':'m-ultra-background';
 const host=document.createElement('div');host.className='m-video-backdrop';host.setAttribute('aria-hidden','true');
 const video=document.createElement('video');
 video.muted=true;video.defaultMuted=true;video.autoplay=true;video.loop=true;video.playsInline=true;
 video.preload='auto';video.tabIndex=-1;
 video.setAttribute('muted','');video.setAttribute('autoplay','');video.setAttribute('loop','');video.setAttribute('playsinline','');
 const script=document.currentScript;
 const source=new URL('media/'+file,script.src);
 if(login)source.search=new URL(script.src).search;
 host.append(video);body.prepend(host);body.classList.add(backgroundClass);
 const reduced=matchMedia('(prefers-reduced-motion:reduce)');
 function sync(){
  const paused=document.hidden||body.classList.contains('m-custom-background')||reduced.matches;
  host.hidden=body.classList.contains('m-custom-background')||(login&&reduced.matches);
  if(paused){video.pause();return;}
  if(!video.hasAttribute('src'))video.src=source.href;
  const play=video.play();if(play?.catch)play.catch(()=>{});
 }
 video.addEventListener('loadeddata',sync);
 if(login)video.addEventListener('playing',()=>host.classList.add('m-video-ready'),{once:true});
 video.addEventListener('error',()=>{host.remove();body.classList.remove(backgroundClass);});
 document.addEventListener('visibilitychange',sync);
 document.addEventListener('pointerdown',sync,{once:true,passive:true});
 reduced.addEventListener('change',sync);
 window.addEventListener('myos:background',sync);
 window.addEventListener('pagehide',()=>video.pause());
 window.addEventListener('pageshow',sync);
 sync();
})();
