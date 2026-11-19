'use strict';
'require view';
'require request';

return view.extend({
 load:function(){
  return request.get(L.url('admin','system','myos_panel_led','status'),{cache:false})
   .then(response=>response.json()).catch(()=>null);
 },
 render:function(state){
  const title=E('h2',{},'面板指示灯');
  if(!state?.supported)return E('div',{},[title,E('p',{},state?'当前设备没有可用的面板灯控。':'暂时无法读取指示灯设置。')]);
  const toggle=E('input',{type:'checkbox',role:'switch'});
  toggle.checked=state.enabled;
  toggle.disabled=!state.writable;
  const status=E('p',{role:'status','class':'cbi-value-description'});
  const description=E('p',{'class':'cbi-section-descr'},'开启时按设备原有状态和定时规则显示，关闭时熄灭面板指示灯。');
  toggle.addEventListener('change',async()=>{
   toggle.disabled=true;
   status.textContent='正在应用…';
   try{
    const response=await request.post(L.url('admin','system','myos_panel_led','save'),
     new URLSearchParams({token:L.env.token,enabled:toggle.checked?'1':'0'}).toString(),
     {headers:{'Content-Type':'application/x-www-form-urlencoded'},timeout:15000});
    const result=response.json();
    if(typeof result.enabled==='boolean')state.enabled=result.enabled;
    if(!response.ok||typeof result.enabled!=='boolean')throw new Error(result.error||'指示灯设置未能应用。');
    status.textContent=state.enabled?'已恢复自动显示。':'已关闭面板指示灯。';
   }catch(error){status.textContent=error.message||'应用失败，请重新读取状态。';}
   finally{toggle.checked=state.enabled;toggle.disabled=false;}
  });
  return E('div',{},[title,E('section',{'class':'cbi-section'},[
   E('label',{'class':'cbi-checkbox'},[toggle,'允许面板指示灯显示']),description,status
  ])]);
 },
 handleSaveApply:null,handleSave:null,handleReset:null
});
