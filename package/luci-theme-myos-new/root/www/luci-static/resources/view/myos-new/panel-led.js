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
   E('label',{'class':'cbi-checkbox'},[toggle,'允许面板指示灯显示']),description,status,
   state.schedule?this.renderSchedule(state.schedule,state.writable):null
  ])]);
 },
 renderSchedule:function(schedule,writable){
  const enabled=E('input',{type:'checkbox'});
  const start=E('input',{type:'time',value:schedule.start});
  const finish=E('input',{type:'time',value:schedule.finish});
  enabled.checked=schedule.active;
  const checked=schedule.days.split(/\s+/);
  const days=['周日','周一','周二','周三','周四','周五','周六'].map((day,index)=>{
   const input=E('input',{type:'checkbox',value:String(index)});
   input.checked=checked.includes(String(index));
   return {input,label:E('label',{'class':'cbi-checkbox'},[input,day+' '])};
  });
  const status=E('p',{role:'status'});
  const save=E('button',{type:'submit','class':'cbi-button cbi-button-apply'},'应用定时');
  const fields=E('fieldset',{},[
   E('legend',{},'每周定时熄灯'),E('label',{'class':'cbi-checkbox'},[enabled,'启用定时']),
   E('p',{},[E('label',{},['开始 ',start]),' ',E('label',{},['结束 ',finish])]),
   E('p',{},days.map(day=>day.label)),
   E('p',{'class':'cbi-value-description'},'跨午夜时，选择熄灯开始的星期。'),save,status
  ]);
  fields.disabled=!writable;
  const form=E('form',{},fields);
  form.addEventListener('submit',async event=>{
   event.preventDefault();
   save.disabled=true;
   status.textContent='正在应用…';
   try{
    const params=new URLSearchParams({token:L.env.token,active:enabled.checked?'1':'0',
     start:start.value,finish:finish.value,days:days.filter(day=>day.input.checked).map(day=>day.input.value).join(' ')});
    const response=await request.post(L.url('admin','system','myos_panel_led','schedule'),params.toString(),
     {headers:{'Content-Type':'application/x-www-form-urlencoded'},timeout:15000});
    const result=response.json();
    if(!response.ok||!result.schedule)throw new Error(result.error||'应用失败');
    status.textContent='已应用定时。';
   }catch(error){status.textContent=error.message||'应用失败';}
   finally{save.disabled=false;}
  });
  return form;
 },
 handleSaveApply:null,handleSave:null,handleReset:null
});
