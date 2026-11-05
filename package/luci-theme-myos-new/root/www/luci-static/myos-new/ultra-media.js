/* Ultra uses the bundled original video only on the login page. */
(() => {
 'use strict';
 const body=document.body;
 if(!body.classList.contains('m-auth'))return;
 const model=((body.dataset.deviceModel||'')+' '+(body.dataset.deviceBoard||'')).toLowerCase().replace(/[^a-z0-9]/g,'');
 if(!model.includes('c2000ultra'))return;
 const host=document.createElement('div');host.className='m-video-backdrop';host.setAttribute('aria-hidden','true');
 const video=document.createElement('video');
 video.muted=true;video.defaultMuted=true;video.autoplay=true;video.loop=true;video.playsInline=true;
 video.preload='auto';video.tabIndex=-1;
 video.setAttribute('muted','');video.setAttribute('autoplay','');video.setAttribute('loop','');video.setAttribute('playsinline','');
 const script=document.currentScript;
 video.src=new URL('media/c2000-ultra-promo.mp4',script.src).href;
 host.append(video);body.prepend(host);body.classList.add('m-ultra-background');
 const reduced=matchMedia('(prefers-reduced-motion:reduce)');
 function sync(){
  const paused=document.hidden||body.classList.contains('m-custom-background')||reduced.matches;
  host.hidden=body.classList.contains('m-custom-background');
  if(paused){video.pause();return;}
  const play=video.play();if(play?.catch)play.catch(()=>{});
 }
 video.addEventListener('loadeddata',sync);
 video.addEventListener('error',()=>{host.remove();body.classList.remove('m-ultra-background');});
 document.addEventListener('visibilitychange',sync);
 document.addEventListener('pointerdown',sync,{once:true,passive:true});
 reduced.addEventListener('change',sync);
 window.addEventListener('myos:background',sync);
 window.addEventListener('pagehide',()=>video.pause());
 window.addEventListener('pageshow',sync);
 sync();
})();
