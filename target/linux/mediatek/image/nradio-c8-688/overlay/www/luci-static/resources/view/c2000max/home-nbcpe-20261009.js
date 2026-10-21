'use strict';
// Compatibility view name; all cellular reads use the C8 native cache.
'require view';
'require rpc';
'require uci';
'require poll';
'require request';
'require fs';

var systemInfo=rpc.declare({object:'system',method:'info'});
var mountPointsInfo=rpc.declare({object:'luci',method:'getMountPoints'});
var firmwareInfo=function(){return Promise.resolve({available:false});};
var interfaceInfo=rpc.declare({object:'network.interface',method:'dump'});
var wirelessInfo=rpc.declare({object:'network.wireless',method:'status',reject:true});
var clientInfo=rpc.declare({object:'c2000max.ui',method:'clients',reject:true});
var identityInfo=function(){return cached('myos_c8_status','identity');};
var sensorsInfo=function(){return cached('myos_c8_status','sensors');};
var simInfo=function(){return cached('modem','simstat');};
var modemInfo=function(){return cached('modem','cpe_status');};
function cached(controller, action) {
 return request.get(L.url('admin',controller,action),{cache:false,timeout:8000}).then(function(response){
  if(!response.ok)throw new Error('C8 status unavailable');
  return response.json();
 });
}

function sessionInfo() {
 return L.resolveDefault(fs.trimmed('/proc/sys/net/netfilter/nf_conntrack_count'),null)
  .then(function(count){return {count:count};});
}

return view.extend({
 load: function() {
  if (L.env.media === '/luci-static/argon') {
   location.replace(L.url('admin','modeminfo'));
   return new Promise(function() {});
  }
  // Shell and placeholders render immediately; optional plugins cannot block the view.
  return request.get('/luci-static/c2000max-ui/dashboard.html',{cache:true}).then(function(response) {
   if(!response.ok)throw new Error('Dashboard template unavailable');
   return response.text();
  });
 },
 render: function(html) {
  var root=E('div',{'class':'c2000-dashboard-root'});
  var firmwareNotice=E('div',{'class':'alert-message notice','style':'display:none'});
  var template=document.createElement('template');template.innerHTML=html;
  root.appendChild(template.content.cloneNode(true));
  root.prepend(firmwareNotice);
  root.querySelectorAll('img[src^="assets/"]').forEach(function(img){img.src='/luci-static/c2000max-ui/'+img.getAttribute('src');});
  var dashboard=window.C2000Dashboard.mount(root,function(){return L.url.apply(L,arguments);});
  var lastSim=0, lastSensors=0, lastModemBase=0, lastFirmware=0, lastMounts=0;
  var state={}, stopped=false, started=false, busy={};
  function setSection(section) {
   if(section===state.section)return;
   state.section=section;state.modem={};state.modemBase={};state.sim={};
   lastSim=0;lastModemBase=0;
  }
  function selectModem() {
   setSection('cpe');
  }
  function readModemCandidates() {
   return modemInfo('cpe').then(function(data){
    var ages=(data.sources||[]).map(function(s){return Number(s.age)||0;});
    return {section:data.section,entries:(data.sources||[]).flatMap(function(s){return s.entries||[];}),receivedAt:Date.now()-Math.max(0,...ages)*1000};
   });
  }
  // Prevent hung optional RPCs from queuing more calls. Successful groups update independently.
  function call(key,fn) {
   if(busy[key])return;
   busy[key]=true;
   var section=state.section;
   Promise.resolve().then(fn).then(function(value) {
    if(stopped)return;
    if(['modem','modemBase','sim'].indexOf(key)>=0 && section!==state.section)return;
    if(key==='modemBase' && value.section)setSection(value.section);
    state[key]=value;dashboard.update(state);
    if(key==='identity'){selectModem();refresh();}
   }).catch(function() {
    if(stopped)return;
    if(['modem','modemBase','sim'].indexOf(key)>=0 && section!==state.section)return;
    state[key]=key==='wireless'?null:{};dashboard.update(state);
   }).finally(function(){busy[key]=false;});
  }
  function refresh() {
   if(!root.isConnected) { if(started){stopped=true;poll.remove(refresh);}return; }
   started=true;
   if(document.hidden)return;
   if(Date.now()-lastFirmware>60000){
    lastFirmware=Date.now();
    L.resolveDefault(firmwareInfo(),{}).then(function(s){
     firmwareNotice.style.display=s.available?'':'none';
     firmwareNotice.replaceChildren(E('span',{},'发现 MYOS 新版本：'+(s.latest||'')+' '),
      E('a',{'href':L.url('admin','system','myos_update')},'查看并更新'));
    });
   }
   call('system',systemInfo);call('network',interfaceInfo);call('wireless',wirelessInfo);
   if(Date.now()-lastMounts>30000){lastMounts=Date.now();call('mounts',mountPointsInfo);}
   if(Date.now()-lastSensors>20000){lastSensors=Date.now();call('sensors',sensorsInfo);}
   call('clients',clientInfo);
   call('sessions',sessionInfo);
   if(state.section){
    if(Date.now()-lastSim>60000){lastSim=Date.now();call('sim',function(){return simInfo(state.section);});}
    call('modem',function(){return modemInfo(state.section);});
    if(Date.now()-lastModemBase>60000){
     lastModemBase=Date.now();
     // Cache-only adapter: the native infocd service owns modem collection.
     call('modemBase',readModemCandidates);
    }
   }
  }
  // UCI access is read-only. Missing optional packages keep their cards unavailable.
  Promise.all([
   L.resolveDefault(uci.load('c2000max_ui'),null)
  ]).then(function(){
   state.config=uci.get('c2000max_ui','main')||{};
   selectModem();
   refresh();
  }).catch(function(){});
  poll.add(refresh,10);
  call('identity',identityInfo);
  window.setTimeout(refresh,0);
  return root;
 },
 handleSaveApply:null,handleSave:null,handleReset:null
});
