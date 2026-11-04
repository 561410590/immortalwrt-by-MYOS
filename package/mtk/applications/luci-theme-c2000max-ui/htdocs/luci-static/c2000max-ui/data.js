/* Read-only adapters for the V37.01 LuCI router interfaces.
   All capacities entering the view are MiB; temperatures are Celsius. */
(function(root, factory) {
 const api = factory();
 if (typeof module === 'object' && module.exports) module.exports = api;
 else root.C2000Data = api;
})(typeof globalThis === 'object' ? globalThis : this, function() {
'use strict';
const MiB=1024*1024;
function number(value) {
 if (typeof value !== 'number' && typeof value !== 'string') return null;
 if (typeof value === 'string' && !value.trim()) return null;
 const n=Number(value);
 return Number.isFinite(n) ? n : null;
}
function measured(value) {
 if (typeof value==='number') return number(value);
 const m=String(value ?? '').trim().match(/^([+-]?\d+(?:\.\d+)?)\s*(?:°?C|℃|dBm|dB)?$/i);
 return m ? number(m[1]) : null;
}
function bounded(value,low,high) { const n=measured(value); return n!==null && n>=low && n<=high ? n : null; }
function text(value) { return typeof value==='string' && value.trim() && !/^(unknown|n\/a|none|null|未发现|未知.*|-|error)$/i.test(value.trim()) ? value.trim() : null; }
function resources(info={}) {
 const m=info.memory||{}, s=info.swap||{}, r=info.root||{};
 const total=number(m.total), free=number(m.free), cached=number(m.cached), buffered=number(m.buffered);
 const cache=cached!==null && buffered!==null ? cached+buffered : null;
 const valid=total>0 && free!==null && cache!==null && free>=0 && cache>=0 && free+cache<=total;
 const available=number(m.available);
 const swTotal=number(s.total),swFree=number(s.free);
 const rootTotal=number(r.total),rootUsed=number(r.used);
 return {
  memory:{total:total>0?total/MiB:null, active:valid?(total-free-cache)/MiB:null,cache:valid?cache/MiB:null,available:total>0&&available!==null&&available>=0&&available<=total?available/MiB:null},
  flash:{total:rootTotal>0?rootTotal/1024:null,used:rootUsed!==null&&rootUsed>=0&&rootUsed<=rootTotal?rootUsed/1024:null},
  swap:{total:swTotal!==null&&swTotal>=0?swTotal/MiB:null,used:swTotal!==null&&swFree!==null&&swFree>=0&&swFree<=swTotal?(swTotal-swFree)/MiB:null}
 };
}
function hardware(raw={},config={}) {
 const cpu=raw.cpu_sensor ? bounded(number(raw.cpu_temp)===null?null:Number(raw.cpu_temp)/1000,-40,150) : null;
 const wifiValues=(Array.isArray(raw.wifi_temps)?raw.wifi_temps:[]).map(v=>bounded(number(v.milli_c)===null?null:Number(v.milli_c)/1000,-40,150)).filter(v=>v!==null);
 const temperatures={cpu,wifi:wifiValues.length?Math.max(...wifiValues):null}, limits={};
 ['cpu','wifi'].forEach(key=>{
  // UI advisory levels, not hardware throttling/shutdown limits. UCI may override both.
  const warning=bounded(config[key+'_warning']??80,0,150),critical=bounded(config[key+'_critical']??90,0,150);
  if(warning!==null && critical!==null && warning<critical) limits[key]={warning,critical};
 });
 const statuses=Object.entries(temperatures).map(([key,v])=>v===null?'missing':!limits[key]?'unconfigured':v>=limits[key].critical?'critical':v>=limits[key].warning?'warning':'normal');
 const status=['critical','warning','missing','unconfigured','normal'].find(v=>statuses.includes(v));
 return {temperatures,limits,status,allMissing:statuses.every(v=>v==='missing')};
}
function network(raw={},wireless=null,clients={}) {
 const interfaces=Array.isArray(raw.interface)?raw.interface:null;
 // A default route indicates an uplink, not verified Internet reachability.
 const uplinks=(interfaces||[]).filter(i=>i.interface!=='loopback' && i.up===true && (
  (Array.isArray(i.route)?i.route:[]).some(r=>r.target==='0.0.0.0'||r.target==='::') ||
  /^wan/i.test(i.interface||'')
 ));
 const names=[];
 const validWireless=wireless && typeof wireless==='object' && !Array.isArray(wireless);
 if(validWireless) Object.values(wireless).forEach(radio=>{
  if(!radio || radio.disabled===true || radio.disabled==='1' || radio.up===false) return;
  (radio.interfaces||[]).forEach(iface=>{const ssid=text(iface.config?.ssid),disabled=iface.config?.disabled; if(ssid && disabled!==true && disabled!==1 && disabled!=='1' && !names.includes(ssid)) names.push(ssid);});
 });
 return {address:uplinks[0]?.['ipv4-address']?.[0]?.address||null,protocol:uplinks[0]?.proto||null,connected:interfaces?uplinks.length>0:null,ssid:!validWireless||!Object.keys(wireless).length?null:names.length?names.join(' / '):'无线未启用',clients:clients.available===true?bounded(clients.count,0,100000):null};
}
function uptime(seconds) {
 const n=number(seconds);
 if(n===null||n<0) return '—';
 const d=Math.floor(n/86400),h=Math.floor(n/3600)%24,m=Math.floor(n/60)%60;
 return (d?d+' 天 ':'')+(h?h+' 小时 ': '')+m+' 分钟';
}
return Object.freeze({number,measured,bounded,resources,hardware,network,uptime});
});
