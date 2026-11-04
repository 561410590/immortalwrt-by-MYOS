(() => {
'use strict';
const input=document.querySelector('#luci_password'),button=document.querySelector('#password-toggle');
button?.addEventListener('click',()=>{
 const visible=input.type==='password';input.type=visible?'text':'password';
 button.setAttribute('aria-pressed',String(visible));button.setAttribute('aria-label',visible?'隐藏密码':'显示密码');
});
const image=document.querySelector('.login-wallpaper');
const credit=document.querySelector('.wallpaper-credit a');
const fallbackDate=image?.dataset.date,fallbackCredit=credit?.textContent,fallbackTitle=credit?.title;
let pending=false;
async function refreshWallpaper() {
 if(!image||pending||document.hidden)return;
 pending=true;
 try {
  const response=await fetch('/luci-static/c2000max-ui/wallpaper/daily.json',{cache:'no-store',signal:AbortSignal.timeout(8000)});
  if(!response.ok)return;
  const info=await response.json();
  if(!/^\d{8}$/.test(info.date)||info.date===image.dataset.date)return;
  const probe=new Image(),src='/luci-static/c2000max-ui/wallpaper/daily.jpg?v='+encodeURIComponent(info.date);
  const loaded=await new Promise(resolve=>{
   const timer=setTimeout(()=>{probe.src='';resolve(false);},8000);
   probe.onload=()=>{clearTimeout(timer);resolve(true);};
   probe.onerror=()=>{clearTimeout(timer);resolve(false);};
   probe.src=src;
  });
  if(!loaded)return;
  image.src=src;image.dataset.date=info.date;
  if(credit){credit.textContent='Bing 每日壁纸 · '+(info.copyright||info.date);credit.title=info.copyright||'';}
 } catch(e) { /* Keep the bundled or previously loaded image on failure. */ }
 finally {pending=false;}
}
image?.addEventListener('error',()=>{
 if(image.getAttribute('src')!==image.dataset.fallback){
  image.src=image.dataset.fallback;image.dataset.date=fallbackDate||'';
  if(credit){credit.textContent=fallbackCredit;credit.title=fallbackTitle;}
 }
});
refreshWallpaper();
setInterval(refreshWallpaper,60000);
window.addEventListener('online',refreshWallpaper);
document.addEventListener('visibilitychange',refreshWallpaper);
// Credentials remain a native LuCI POST. They are never persisted or logged here.
})();
