(function () {
 'use strict';
 const body=document.body;
 const button=document.querySelector('#m-background');
 const model=(body.dataset.deviceModel||'').toLowerCase().replace(/[^a-z0-9]/g,'');
 const board=(body.dataset.deviceBoard||'').toLowerCase();
 const ultra=model.includes('c2000ultra')||(board==='hcmt7987-snsd'&&!model.includes('c2000max'));
 if(!button&&!(ultra&&body.classList.contains('m-auth')))return;
 const reduced=matchMedia('(prefers-reduced-motion:reduce)');
 const make=(tag,cls,text)=>{const n=document.createElement(tag);if(cls)n.className=cls;if(text!=null)n.textContent=text;return n;};
 let database,current=null,mediaURL=null,backdrop=null,backgroundVideo=null,previewVideo=null;
 const makeVideo=()=>{
  const video=make('video');video.muted=true;video.defaultMuted=true;video.loop=true;video.playsInline=true;video.preload='auto';video.tabIndex=-1;
  video.setAttribute('muted','');video.setAttribute('playsinline','');video.setAttribute('aria-hidden','true');return video;
 };
 const disposeVideo=video=>{if(video){video.pause();video.removeAttribute('src');video.load();video.remove();}};
 const openDB=()=>database||(database=new Promise((resolve,reject)=>{
  const request=indexedDB.open('myos-new-appearance',1);
  request.onupgradeneeded=()=>request.result.createObjectStore('settings');
  request.onsuccess=()=>resolve(request.result);
  request.onerror=()=>reject(request.error);
 }));
 const storage=async(value,write)=>{
  const db=await openDB();
  return new Promise((resolve,reject)=>{
   const tx=db.transaction('settings',write?'readwrite':'readonly'),store=tx.objectStore('settings');
   const request=write?(value?store.put(value,'background'):store.delete('background')):store.get('background');
   tx.oncomplete=()=>resolve(request.result);
   tx.onerror=tx.onabort=()=>reject(tx.error||new Error('无法保存背景'));
  });
 };
 const dialog=make('dialog','m-background-dialog');
 dialog.setAttribute('aria-labelledby','m-background-title');
 const heading=make('div','m-background-heading'),title=make('h2','','自定义背景');
 title.id='m-background-title';
 const close=make('button','m-icon-button','×');close.type='button';close.setAttribute('aria-label','关闭背景设置');
 close.addEventListener('click',()=>dialog.close());heading.append(title,close);
 const note=make('p','m-muted','图片或视频应用于主页和登录页，仅保存在当前浏览器，不上传到路由器。视频静音循环播放；恢复默认后，登录页使用默认视频。');
 const preview=make('div','m-background-preview');
 const filename=make('p','m-background-name','默认渐变');
 const file=make('input');file.type='file';file.accept='image/*,video/*';file.hidden=true;
 const choose=make('button','cbi-button cbi-button-apply','选择图片或视频');choose.type='button';
 const reset=make('button','cbi-button','恢复默认');reset.type='button';
 const actions=make('div','m-background-actions');actions.append(choose,reset);
 const label=make('label','m-background-dim','背景压暗');
 const amount=make('output','','45%'),range=make('input');range.type='range';range.min='0';range.max='85';range.step='1';range.value='45';range.disabled=true;range.setAttribute('aria-label','背景压暗程度');
 label.append(amount,range);
 const status=make('p','m-background-status');status.setAttribute('role','status');
 dialog.append(heading,note,preview,filename,file,actions,label,status);document.body.append(dialog);
 function syncVideo(){
  for(const video of [backgroundVideo,previewVideo]){
   if(!video)continue;
   if(document.hidden||(video===previewVideo?!dialog.open:dialog.open||reduced.matches)){video.pause();continue;}
   const play=video.play();if(play?.catch)play.catch(e=>{
    if(e.name==='NotAllowedError')status.textContent='浏览器尚未允许播放视频，点击页面后会重试。';
   });
  }
 }
 function apply(value){
  const old=mediaURL;disposeVideo(backgroundVideo);disposeVideo(previewVideo);
  if(backdrop)backdrop.remove();backdrop=backgroundVideo=previewVideo=null;
  current=value;mediaURL=value?URL.createObjectURL(value.blob):null;
  const isVideo=!!value&&(value.kind==='video'||value.blob.type.startsWith('video/'));
  document.body.classList.toggle('m-custom-background',!!value);
  document.body.classList.toggle('m-custom-video',isVideo);
  if(value){
   if(isVideo){
    document.body.style.removeProperty('--m-custom-wallpaper');
    backdrop=make('div','m-custom-video-backdrop');backdrop.setAttribute('aria-hidden','true');
    backgroundVideo=makeVideo();previewVideo=makeVideo();
    backgroundVideo.src=previewVideo.src=mediaURL;
    backgroundVideo.addEventListener('loadeddata',syncVideo);previewVideo.addEventListener('loadeddata',syncVideo);
    backgroundVideo.addEventListener('error',()=>{status.textContent='视频无法播放，请选择当前浏览器支持的视频格式。';});
    backdrop.append(backgroundVideo);document.body.prepend(backdrop);preview.append(previewVideo);
   }else document.body.style.setProperty('--m-custom-wallpaper','url('+JSON.stringify(mediaURL)+')');
   range.value=String(value.dim==null?45:value.dim);
  }else{
   document.body.style.removeProperty('--m-custom-wallpaper');range.value='45';
  }
  range.disabled=!value;amount.textContent=range.value+'%';
  document.body.style.setProperty('--m-background-dim',Number(range.value)/100);
  preview.style.backgroundImage=value&&!isVideo?'linear-gradient(rgba(5,10,20,'+Number(range.value)/100+'),rgba(5,10,20,'+Number(range.value)/100+')),url('+JSON.stringify(mediaURL)+')':'';
  preview.classList.toggle('m-background-preview-video',isVideo);
  filename.textContent=value?value.name:'默认渐变';
  if(old)URL.revokeObjectURL(old);
  syncVideo();
  window.dispatchEvent(new Event('myos:background'));
 }
 function busy(value){choose.disabled=reset.disabled=value;range.disabled=value||!current;}
 const ready=storage(null,false).then(saved=>{if(saved&&saved.blob)apply(saved);})
  .catch(()=>{status.textContent='无法读取浏览器背景存储，请检查浏览器设置。';});
 if(button)button.addEventListener('click',async()=>{await ready;dialog.showModal();syncVideo();});
 dialog.addEventListener('close',syncVideo);
 document.addEventListener('visibilitychange',syncVideo);
 reduced.addEventListener('change',syncVideo);
 document.addEventListener('pointerdown',syncVideo,{passive:true});
 window.addEventListener('pagehide',()=>{backgroundVideo?.pause();previewVideo?.pause();});
 window.addEventListener('pageshow',syncVideo);
 choose.addEventListener('click',()=>file.click());
 file.addEventListener('change',async()=>{
  const selected=file.files[0];if(!selected)return;
  const isVideo=selected.type.startsWith('video/')||(!selected.type&&/\.(mp4|m4v|webm|mov|ogv)$/i.test(selected.name));
  busy(true);status.textContent=isVideo?'正在载入视频…':'正在载入图片…';
  const url=URL.createObjectURL(selected),media=isVideo?makeVideo():new Image();
  try{
   if(isVideo)await new Promise((resolve,reject)=>{
    media.onloadeddata=resolve;media.onerror=()=>reject(new Error('视频无法解码'));media.src=url;media.load();
   });
   else{media.src=url;await media.decode();}
   const value={blob:selected,name:selected.name,kind:isVideo?'video':'image',dim:Number(range.value)};
   await storage(value,true);apply(value);status.textContent='背景已保存。';
  }catch(e){status.textContent='背景读取或保存失败，请选择当前浏览器支持的图片或视频，或检查浏览器存储空间。';}
  finally{if(isVideo)disposeVideo(media);URL.revokeObjectURL(url);file.value='';busy(false);}
 });
 range.addEventListener('input',()=>{
  const dim=Number(range.value)/100;amount.textContent=range.value+'%';
  document.body.style.setProperty('--m-background-dim',dim);
  if(mediaURL&&!backgroundVideo)preview.style.backgroundImage='linear-gradient(rgba(5,10,20,'+dim+'),rgba(5,10,20,'+dim+')),url('+JSON.stringify(mediaURL)+')';
 });
 range.addEventListener('change',async()=>{
  if(!current)return;
  const value=Object.assign({},current,{dim:Number(range.value)});
  try{await storage(value,true);current=value;status.textContent='明暗设置已保存。';}
  catch(e){status.textContent='明暗设置暂未保存，请检查浏览器存储空间。';}
 });
 reset.addEventListener('click',async()=>{
  busy(true);
  try{await storage(null,true);apply(null);status.textContent='已恢复默认渐变。';}
  catch(e){status.textContent='恢复未完成，请检查浏览器存储设置。';}
  finally{busy(false);}
 });
})();
