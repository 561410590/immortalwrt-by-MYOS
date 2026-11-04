'use strict';
// C8 homepage; all cellular reads use the C8 native cache.
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
var modemInfo=function(){return cached('modem','cpe_status');};
function cached(controller, action) {
 // Local remote-control tunnels share a host across ports, hence browser cookies.
 // Use this page's session, as the LuCI RPC requests already do.
 return request.post(L.url('admin',controller,action),'auth='+encodeURIComponent(L.env.sessionid),{
  cache:false,timeout:8000,headers:{'Content-Type':'application/x-www-form-urlencoded'}
 }).then(function(response){
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
  var passthroughValue=E('dd',{},'状态未知');
  var modemDetails=root.querySelector('.modem-card .ui-data-list');
  if(modemDetails)modemDetails.appendChild(E('div',{'class':'ui-data-row'},[E('dt',{},'IP 透传'),passthroughValue]));
  var lastSensors=0, lastFirmware=0, lastMounts=0;
  var state={}, stopped=false, started=false, busy={};
  function updatePassthrough() {
   var modem=state.modem||{}, pt=modem.passthrough||{};
   var fresh=modem.stale===false && pt.stale===false && typeof pt.enabled==='boolean';
   var label=!fresh?'状态未知':pt.enabled===false?'关闭（模组路由）':
    pt.ipv4_verified===true?'已透传':pt.ipv4_verified===false?'已开启，地址未透传':'已开启，等待核验';
   if(fresh && pt.pending===true)label+=' · 待重启';
   passthroughValue.textContent=label;
  }
  // Prevent hung optional RPCs from queuing more calls. Successful groups update independently.
  function call(key,fn) {
   if(busy[key])return;
   busy[key]=true;
   Promise.resolve().then(fn).then(function(value) {
    if(stopped)return;
    if(key==='modem')state.sim={sim_label:value.sim_label};
    if(key==='sensors')lastSensors=Date.now();
    state[key]=value;dashboard.update(state);updatePassthrough();
   }).catch(function() {
    if(stopped)return;
    if(key==='modem')state.sim={};
    state[key]=key==='wireless'?null:{};dashboard.update(state);updatePassthrough();
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
   if(Date.now()-lastSensors>20000)call('sensors',sensorsInfo);
   call('clients',clientInfo);
   call('sessions',sessionInfo);
   // C8 has one fixed modem. Its cache already includes identity, radio and SIM.
   call('modem',modemInfo);
  }
  // UCI access is read-only. Missing optional packages keep their cards unavailable.
  Promise.all([
   L.resolveDefault(uci.load('c2000max_ui'),null)
  ]).then(function(){
   state.config=uci.get('c2000max_ui','main')||{};
   refresh();
  }).catch(function(){});
  poll.add(refresh,10);
  call('identity',identityInfo);
  window.setTimeout(refresh,0);
  return root;
 },
 handleSaveApply:null,handleSave:null,handleReset:null
});
