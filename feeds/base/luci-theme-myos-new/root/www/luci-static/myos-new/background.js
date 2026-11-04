(function () {
 'use strict';
 const button=document.querySelector('#m-background');
 if(!button)return;
 const make=(tag,cls,text)=>{const n=document.createElement(tag);if(cls)n.className=cls;if(text!=null)n.textContent=text;return n;};
 let database,current=null,imageURL=null;
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
 const note=make('p','m-muted','图片仅保存在当前浏览器，不上传到路由器。');
 const preview=make('div','m-background-preview');
 const filename=make('p','m-background-name','默认渐变');
 const file=make('input');file.type='file';file.accept='image/*';file.hidden=true;
 const choose=make('button','cbi-button cbi-button-apply','选择图片');choose.type='button';
 const reset=make('button','cbi-button','恢复默认');reset.type='button';
 const actions=make('div','m-background-actions');actions.append(choose,reset);
 const label=make('label','m-background-dim','背景压暗');
 const amount=make('output','','45%'),range=make('input');range.type='range';range.min='0';range.max='85';range.step='1';range.value='45';range.disabled=true;range.setAttribute('aria-label','背景压暗程度');
 label.append(amount,range);
 const status=make('p','m-background-status');status.setAttribute('role','status');
 dialog.append(heading,note,preview,filename,file,actions,label,status);document.body.append(dialog);
 function apply(value){
  const old=imageURL;current=value;imageURL=value?URL.createObjectURL(value.blob):null;
  document.body.classList.toggle('m-custom-background',!!value);
  if(value){
   document.body.style.setProperty('--m-custom-wallpaper','url('+JSON.stringify(imageURL)+')');
   range.value=String(value.dim==null?45:value.dim);
  }else{
   document.body.style.removeProperty('--m-custom-wallpaper');range.value='45';
  }
  range.disabled=!value;amount.textContent=range.value+'%';
  document.body.style.setProperty('--m-background-dim',Number(range.value)/100);
  preview.style.backgroundImage=value?'linear-gradient(rgba(5,10,20,'+Number(range.value)/100+'),rgba(5,10,20,'+Number(range.value)/100+')),url('+JSON.stringify(imageURL)+')':'';
  filename.textContent=value?value.name:'默认渐变';
  if(old)URL.revokeObjectURL(old);
 }
 function busy(value){choose.disabled=reset.disabled=value;range.disabled=value||!current;}
 const ready=storage(null,false).then(saved=>{if(saved&&saved.blob)apply(saved);})
  .catch(()=>{status.textContent='无法读取浏览器背景存储，请检查浏览器设置。';});
 button.addEventListener('click',async()=>{await ready;dialog.showModal();});
 choose.addEventListener('click',()=>file.click());
 file.addEventListener('change',async()=>{
  const selected=file.files[0];if(!selected)return;
  busy(true);status.textContent='正在载入图片…';
  const url=URL.createObjectURL(selected),image=new Image();
  try{
   image.src=url;await image.decode();
   const value={blob:selected,name:selected.name,dim:Number(range.value)};
   await storage(value,true);apply(value);status.textContent='背景已保存。';
  }catch(e){status.textContent='图片读取或保存失败，请更换图片，或检查浏览器存储空间。';}
  finally{URL.revokeObjectURL(url);file.value='';busy(false);}
 });
 range.addEventListener('input',()=>{
  const dim=Number(range.value)/100;amount.textContent=range.value+'%';
  document.body.style.setProperty('--m-background-dim',dim);
  if(imageURL)preview.style.backgroundImage='linear-gradient(rgba(5,10,20,'+dim+'),rgba(5,10,20,'+dim+')),url('+JSON.stringify(imageURL)+')';
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
