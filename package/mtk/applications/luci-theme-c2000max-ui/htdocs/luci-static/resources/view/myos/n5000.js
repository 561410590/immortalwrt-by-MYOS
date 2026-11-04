'use strict';
'require view';
'require rpc';
'require uci';
'require poll';
'require request';
'require fs';

var homeUrl = document.body.dataset.homeUrl;
if (homeUrl && location.pathname.replace(/\/$/, '') !== homeUrl.replace(/\/$/, ''))
 location.replace(homeUrl);

var systemInfo=rpc.declare({object:'system',method:'info'});
var firmwareInfo=rpc.declare({object:'myos.update',method:'status'});
var interfaceInfo=rpc.declare({object:'network.interface',method:'dump'});
var wirelessInfo=rpc.declare({object:'network.wireless',method:'status',reject:true});
var clientInfo=rpc.declare({object:'c2000max.ui',method:'clients',reject:true});
var identityInfo=rpc.declare({object:'c2000max.ui',method:'identity',reject:true});
var sensorsInfo=rpc.declare({object:'c2000max.ui',method:'sensors',reject:true});
var portsInfo=rpc.declare({object:'c2000max.ui',method:'ports',reject:true});

function sessionInfo() {
 return L.resolveDefault(fs.trimmed('/proc/sys/net/netfilter/nf_conntrack_count'),null)
  .then(function(count){return {count:count};});
}

return view.extend({
 load: function() {
  // Shell and placeholders render immediately; optional plugins cannot block the view.
  return request.get('/luci-static/c2000max-ui/dashboard.html?v=3.1.36-202610261000',{cache:true}).then(function(response) {
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
  var lastSensors=0, lastFirmware=0;
  var state={}, stopped=false, started=false, busy={};
  // Prevent hung optional RPCs from queuing more calls. Successful groups update independently.
  function call(key,fn) {
   if(busy[key])return;
   busy[key]=true;
   Promise.resolve().then(fn).then(function(value) {
    if(stopped)return;
    state[key]=value;dashboard.update(state);
   }).catch(function() {
    if(stopped)return;
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
   if(Date.now()-lastSensors>20000){lastSensors=Date.now();call('sensors',sensorsInfo);}
   call('clients',clientInfo);
   call('sessions',sessionInfo);
   call('ports',portsInfo);
  }
  // UCI access is read-only. Missing optional packages keep their cards unavailable.
  L.resolveDefault(uci.load('c2000max_ui'),null).then(function(){
   state.config=uci.get('c2000max_ui','main')||{};refresh();
  });
  poll.add(refresh,10);
  call('identity',identityInfo);
  window.setTimeout(refresh,0);
  return root;
 },
 handleSaveApply:null,handleSave:null,handleReset:null
});
