/* Use the bundled video on the device and page it belongs to. */
(() => {
 'use strict';
 const body=document.body;
 if(!body.classList.contains('m-auth')||!document.querySelector('.m-login form'))return;
 const model=(body.dataset.deviceModel||'').toLowerCase().replace(/[^a-z0-9]/g,'').replace(/^nradio/,'');
 const board=(body.dataset.deviceBoard||'').toLowerCase().replace(/[^a-z0-9]/g,'');
 const ultra=model==='c2000ultra'||board==='nradioc2000ultra';
 let file;
 if(ultra)file='c2000-ultra-promo.mp4';
 else if(model==='c5800688'||board==='nradioc5800688')file='c5800-688-login.mp4';
 else if(model==='c8688'||model==='c8new688'||board==='hcmt7981688emmc'||
  board==='nradiowt9104no2manpernew688')file='c8-688-login.mp4';
 if(!file)return;
 const host=document.createElement('div');host.className='m-video-backdrop';host.setAttribute('aria-hidden','true');
 const video=document.createElement('video');
 video.muted=true;video.defaultMuted=true;video.autoplay=true;video.loop=true;video.playsInline=true;
 video.preload='auto';video.tabIndex=-1;
 video.setAttribute('muted','');video.setAttribute('autoplay','');video.setAttribute('loop','');video.setAttribute('playsinline','');
 const script=document.currentScript;
 const source=new URL('media/'+file,script.src);
 source.search=new URL(script.src).search;
 host.append(video);body.prepend(host);body.classList.add('m-login-video');
 const reduced=matchMedia('(prefers-reduced-motion:reduce)');
 function sync(){
  host.hidden=body.classList.contains('m-custom-background')||reduced.matches||
   (ultra&&body.dataset.backgroundReady!=='true');
  if(document.hidden||host.hidden){video.pause();return;}
  if(!video.hasAttribute('src'))video.src=source.href;
  const play=video.play();if(play?.catch)play.catch(()=>{});
 }
 video.addEventListener('loadeddata',sync);
 video.addEventListener('playing',()=>host.classList.add('m-video-ready'),{once:true});
 video.addEventListener('error',()=>{host.remove();body.classList.remove('m-login-video');});
 document.addEventListener('visibilitychange',sync);
 document.addEventListener('pointerdown',sync,{once:true,passive:true});
 reduced.addEventListener('change',sync);
 window.addEventListener('myos:background',sync);
 window.addEventListener('pagehide',()=>video.pause());
 window.addEventListener('pageshow',sync);
 sync();
})();
