'use strict';
'require view';

return view.extend({
 handleSaveApply:null,
 handleSave:null,
 handleReset:null,

 async render() {
  const base=L.url('admin','system','myos_peripherals');
  const get=async name=>(await fetch(base+'/'+name,{cache:'no-store'})).json();
  const command=async (action,value='')=>(await fetch(base+'/command',{
   method:'POST',body:new URLSearchParams({token:L.env.token,action,value})
  })).json();
  const message=E('p',{role:'status'});
  const image=E('img',{alt:'USB 摄像头预览',hidden:true,style:'max-width:100%'});
  const cameraText=E('p');
  const audioText=E('p');
  const tracks=E('select',{'aria-label':'曲目'});
  const volume=E('input',{type:'range',min:0,max:100,value:50,'aria-label':'播放音量'});
  const playback=E('meter',{min:0,max:100,value:0,'aria-label':'播放电平'});
  const microphone=E('meter',{min:0,max:100,value:0,'aria-label':'麦克风电平'});
  const controls=E('div',{hidden:true});
  const modem=E('p');
  let preview=false, audioReady=false;
  const button=(label,action)=>E('button',{class:'cbi-button',type:'button',click:async event=>{
   const control=event.currentTarget;
   control.disabled=true;
   try { await action(); }
   catch(error) { message.textContent=error.message; }
   finally { control.disabled=false; }
  }},label);
  const run=async (name,value)=>{
   const result=await command(name,value);
   if(!result.ok)throw new Error(result.err||({nocam:'未检测到 USB 摄像头',nobin:'缺少 FFmpeg',busy:'摄像头正忙'}[result.error])||'操作失败');
   message.textContent='';
   return result;
  };
  const start=button('开始预览',async()=>{await run('camera_start');preview=true;image.hidden=false;});
  const stop=button('停止预览',async()=>{preview=false;image.hidden=true;image.removeAttribute('src');await run('camera_stop');});
  controls.append(
   E('p',{},[E('label',{},['播放音量 ',volume]),button('设置音量',()=>run('volume',volume.value))]),
   E('p',{},[tracks,button('播放',()=>run('play',tracks.value)),button('暂停',()=>run('player','pause')),
    button('继续',()=>run('player','resume')),button('停止',()=>run('player','stop'))]),
   E('p',{},['播放电平 ',playback,'　麦克风电平 ',microphone]),
   E('p',{},[button('开启麦克风电平',()=>run('mic','1')),button('关闭麦克风电平',()=>run('mic','0'))])
  );
  const refresh=async()=>{
   const data=await get('status');
   const cameras=Array.isArray(data.camera?.cams)?data.camera.cams:[];
   cameraText.textContent=cameras.length?cameras.map(cam=>cam.name||cam.dev).join(' · '):'未检测到 USB 摄像头';
   if(!data.ffmpeg)cameraText.textContent+=' · 未安装 FFmpeg';
   const cards=Array.isArray(data.audio?.cards)?data.audio.cards:[];
   audioReady=cards.length>0;
   audioText.textContent=audioReady?cards.map(card=>card.name).join(' · '):'未检测到 USB 声卡';
   controls.hidden=!audioReady;
   const songs=Array.isArray(data.audio?.tracks)?data.audio.tracks:[];
   tracks.replaceChildren(...songs.map(song=>E('option',{value:song.id},song.title)));
   modem.replaceChildren(data.qmodem?E('a',{href:L.url('admin','modem','qmodem','overview')},'QModem · 5G、短信'):document.createTextNode('当前未安装 QModem'));
  };
  const section=(title,children)=>E('section',{class:'cbi-section'},[E('h3',{},title),...children]);
  const root=E('div',{},[
   E('h2',{},'USB 外接设备'),button('刷新设备',refresh),message,
   section('摄像头',[cameraText,start,stop,image]),
   section('声卡',[audioText,controls]),section('外接 5G',[modem])
  ]);
  await refresh();
  const timer=setInterval(async()=>{
   if(!root.isConnected||document.hidden)return;
   if(preview && (image.complete||!image.hasAttribute('src')))image.src=base+'/frame?t='+Date.now();
   if(audioReady){
    try{
     const levels=await get('levels');
     const last=values=>Array.isArray(values)&&values.length?Number(values[values.length-1])||0:0;
     playback.value=last(levels.out);
     microphone.value=last(levels.mic);
    }catch(_){}
   }
  },700);
  window.addEventListener('pagehide',()=>{clearInterval(timer);},{once:true});
  return root;
 }
});
