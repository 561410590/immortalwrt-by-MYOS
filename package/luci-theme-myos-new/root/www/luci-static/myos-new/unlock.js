(() => {
 'use strict';
 let dialog,input,status,submit,controller,refreshTimer,returnFocus;
 let busy=false,buffer='',lastKey=0,taps=0,lastTap=0;
 const make=(tag,className,text)=>{const el=document.createElement(tag);if(className)el.className=className;if(text)el.textContent=text;return el;};
 function message(text,kind){status.textContent=text;status.dataset.kind=kind||'';}
 function working(value){busy=value;submit.disabled=value;input.readOnly=value;submit.textContent=value?'正在验证…':'验证并显示';submit.setAttribute('aria-busy',String(value));}
 function create(){
  dialog=make('dialog','m-unlock');dialog.setAttribute('aria-labelledby','m-unlock-title');dialog.setAttribute('aria-describedby','m-unlock-description');
  const close=make('button','m-unlock-close','×');close.type='button';close.setAttribute('aria-label','关闭插件认证');close.addEventListener('click',()=>dialog.close());
  const logo=make('img','m-unlock-logo');logo.src=L.media('brand.png');logo.alt='MYOS';
  const brand=make('p','m-unlock-brand','MYOS-NEW');
  const title=make('h2','','代理插件');title.id='m-unlock-title';
  const description=make('p','m-unlock-description','输入插件口令，验证后显示已安装的代理插件菜单。');description.id='m-unlock-description';
  const form=make('form'),label=make('label','m-unlock-label','插件口令');label.htmlFor='m-unlock-code';
  input=make('input');input.id='m-unlock-code';input.type='password';input.autocomplete='off';input.placeholder='请输入口令';input.spellcheck=false;
  status=make('p','m-unlock-status');status.setAttribute('role','status');status.setAttribute('aria-live','polite');
  submit=make('button','m-unlock-submit','验证并显示');submit.type='submit';
  form.append(label,input,status,submit);form.addEventListener('submit',send);
  dialog.append(close,logo,brand,title,description,form);document.body.append(dialog);
  dialog.addEventListener('close',()=>{controller?.abort();clearTimeout(refreshTimer);input.value='';working(false);returnFocus?.focus({preventScroll:true});});
 }
 function show(){
  if(!dialog)create();if(dialog.open)return;
  returnFocus=document.activeElement;message('');input.value='';working(false);dialog.showModal();input.focus();
 }
 async function send(event){
  event.preventDefault();if(busy)return;
  if(!input.value){message('请输入插件口令。','error');input.focus();return;}
  working(true);message('正在验证口令…');
  const pending=new AbortController();controller=pending;
  const timeout=setTimeout(()=>pending.abort(),20000);
  try{
   const response=await fetch(L.url('admin','myos','unlock'),{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({token:L.env.token,set:input.value}),signal:pending.signal});
   if(!response.ok)throw new Error(response.status===404?'当前固件未提供插件认证接口。':'认证请求失败（'+response.status+'），请重新登录后重试。');
   const data=await response.json();if(!dialog.open)return;
   if(data.resdd==='yes'){
    input.value='';message('验证成功，正在刷新菜单。','success');submit.textContent='正在刷新…';
    const ui=await L.require('ui');ui.menu.flushCache();
    try{sessionStorage.removeItem('luci-session-store');}catch(_){}
    refreshTimer=setTimeout(()=>location.reload(),600);return;
   }
   const errors={no:'口令不正确，请重新输入。',off:'当前认证接口已关闭。',nonet:'无法连接认证服务，请稍后重试。',busy:'请求频繁，请稍后重试。'};
   throw new Error(errors[data.resdd]||'认证接口返回异常，请稍后重试。');
  }catch(error){
   if(dialog.open&&controller===pending){message(error.name==='AbortError'?'请求超时，请稍后重试。':error.message,'error');working(false);input.focus();input.select();}
  }finally{clearTimeout(timeout);if(controller===pending)controller=null;}
 }
 document.addEventListener('keydown',event=>{
  if(event.ctrlKey||event.metaKey||event.altKey||event.isComposing||event.target.closest?.('input,textarea,select,[contenteditable="true"]')||dialog?.open)return;
  if(event.key.length!==1)return;
  const now=Date.now();if(now-lastKey>3000)buffer='';lastKey=now;buffer=(buffer+event.key.toLowerCase()).slice(-8);
  if(buffer==='myosmyos'){event.preventDefault();buffer='';show();}
 });
 document.querySelector('.m-unlock-trigger')?.addEventListener('click',()=>{
  const now=Date.now();taps=now-lastTap>1600?1:taps+1;lastTap=now;
  if(taps===8){taps=0;show();}
 });
})();
