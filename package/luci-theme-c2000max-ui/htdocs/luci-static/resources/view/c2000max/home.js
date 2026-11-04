'use strict';
'require view';
'require rpc';
'require uci';
'require poll';
'require request';
'require fs';

if (location.pathname.replace(/\/$/, '') === L.url('admin','status','c2000max'))
 location.replace(L.url('admin','status','c5800'));

var systemInfo=rpc.declare({object:'system',method:'info'});
var firmwareInfo=function(){return Promise.resolve({available:false});};
var interfaceInfo=rpc.declare({object:'network.interface',method:'dump'});
var wirelessInfo=rpc.declare({object:'network.wireless',method:'status',reject:true});
var clientInfo=rpc.declare({object:'c2000max.ui',method:'clients',reject:true});
var identityInfo=rpc.declare({object:'c2000max.ui',method:'identity',reject:true});
var sensorsInfo=rpc.declare({object:'c2000max.ui',method:'sensors',reject:true});
var simInfo=rpc.declare({object:'c2000max.ui',method:'sim',params:['section'],reject:true});
// Separate from initial rendering and cache reads; QModem owns AT locking.
var modemBaseInfo=rpc.declare({object:'qmodem',method:'base_info',params:['config_section'],reject:true,timeout:20000});
var modemCellInfo=rpc.declare({object:'qmodem',method:'cell_info',params:['config_section'],reject:true,timeout:20000});
var modemInfo=rpc.declare({object:'c2000max.ui',method:'modem',params:['section']});
var modemConfig=rpc.declare({object:'uci',method:'get',params:['config'],expect:{values:{}},reject:true});

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
  var lastSim=0, lastSensors=0, lastModemBase=0, lastSelection=0, lastFirmware=0;
  var state={}, stopped=false, started=false, busy={};
  function setSection(section) {
   if(section===state.section)return;
   state.section=section;state.modem={};state.modemBase={};state.sim={};
   lastSim=0;lastModemBase=0;
  }
  function selectModem(sections) {
   if(state.identity?.native_nros){state.candidates=['cpe','cpe1'];setSection(state.section||'cpe');return;}
   var requested=state.config?.modem_section;
   var active=sections.filter(function(s){return s.state==='enabled' && s.enabled!=='0' && s.at_port;});
   state.candidates=active.map(function(s){return s['.name'];});
   var selected=active.find(function(s){return s['.name']===state.section;}) ||
    active.find(function(s){return s['.name']===requested;}) || active[0];
   setSection(selected?.['.name']);
  }
  function readModemCandidates() {
   if(state.identity?.native_nros) return modemInfo(state.section||'cpe').then(function(data){
    return {section:data.section,entries:(data.sources||[]).flatMap(function(s){return s.entries||[];}),receivedAt:Date.now()};
   });
   var candidates=[state.section].concat(state.candidates||[]).filter(function(s,i,list){return s && list.indexOf(s)===i;});
   var fallback;
   function read(index) {
    if(index>=candidates.length)return Promise.resolve(fallback || {entries:[],receivedAt:Date.now()});
    var section=candidates[index], entries=[];
    return L.resolveDefault(modemBaseInfo(section),null).then(function(base){
     entries=base?.modem_info||[];
     return L.resolveDefault(modemCellInfo(section),null);
    }).then(function(cell){
     entries=entries.concat(cell?.modem_info||[]);
     var result={section:section,entries:entries,receivedAt:Date.now()};
     if(!fallback)fallback=result;
     var signal=entries.some(function(e){
      var value=parseFloat(e.value);
      return /^RSRP$/i.test(e.key||'') && Number.isFinite(value) && value>=-156 && value<=-31;
     });
     return signal ? result : read(index+1);
    });
   }
   return read(0);
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
    if(key==='identity' && value.native_nros){state.candidates=['cpe','cpe1'];setSection('cpe');refresh();}
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
   if(Date.now()-lastSelection>30000){
    lastSelection=Date.now();
    if(!state.identity?.native_nros) call('selection',function(){return modemConfig('qmodem').then(function(values){selectModem(Object.values(values));return state.section;});});
   }
   call('system',systemInfo);call('network',interfaceInfo);call('wireless',wirelessInfo);
   if(Date.now()-lastSensors>20000){lastSensors=Date.now();call('sensors',sensorsInfo);}
   call('clients',clientInfo);
   call('sessions',sessionInfo);
   if(state.section){
    if(Date.now()-lastSim>60000){lastSim=Date.now();call('sim',function(){return simInfo(state.section);});}
    call('modem',function(){return modemInfo(state.section);});
    if(Date.now()-lastModemBase>60000){
     lastModemBase=Date.now();
     // Try each currently enabled modem in sequence; a failed read cannot pin the UI to one port.
     call('modemBase',readModemCandidates);
    }
   }
  }
  // UCI access is read-only. Missing optional packages keep their cards unavailable.
  Promise.all([
   L.resolveDefault(uci.load('qmodem'),null),
   L.resolveDefault(uci.load('c2000max_ui'),null)
  ]).then(function(){
   state.config=uci.get('c2000max_ui','main')||{};
   selectModem(uci.sections('qmodem','modem-device'));
   lastSelection=Date.now();
   refresh();
  }).catch(function(){});
  poll.add(refresh,10);
  call('identity',identityInfo);
  window.setTimeout(refresh,0);
  return root;
 },
 handleSaveApply:null,handleSave:null,handleReset:null
});
