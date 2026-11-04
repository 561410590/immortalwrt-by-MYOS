/* Read-only adapters for the V37.01 LuCI / QModem / c2000max interfaces.
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
function modem(raw={},simState={},liveBase={}) {
 const sources=Array.isArray(raw.sources)?[...raw.sources]:[];
 const liveAge=number(liveBase.receivedAt)===null?null:(Date.now()-liveBase.receivedAt)/1000;
 if(liveAge!==null&&liveAge>=0&&liveAge<=120&&Array.isArray(liveBase.entries))
  sources.unshift({kind:'base',age:liveAge,entries:liveBase.entries});
 const fresh=sources.filter(s=>number(s.age)!==null && s.age>=0 && s.age<=120);
 const all=sources.flatMap(s=>Array.isArray(s.entries)?s.entries:[]);
 const entries=fresh.flatMap(s=>Array.isArray(s.entries)?s.entries:[]);
 const get=(key,list=entries)=>list.find(e=>String(e.key).toLowerCase()===key.toLowerCase() && e.value!=null && text(String(e.value))!==null)?.value;
 const mode=text(get('network_mode'));
 const nrMode=!!mode && /NR|5G|EN-DC/i.test(mode);
 const nsa=!!mode && /NSA|EN-DC/i.test(mode);
 const lteMode=!!mode && /LTE|4G/i.test(mode) && !nrMode;
 const radioEntries=entries.filter(e=>{
  const tag=String(e.extra_info||'');
  if(/CA/i.test(tag))return false;
  return nrMode ? (nsa?/^(NR|5G)/i.test(tag):!/LTE|4G/i.test(tag)) : lteMode&&!/NR|5G/i.test(tag);
 });
 const parseBand=value=>String(value??'').trim().match(nrMode?/^(?:NR\s*)?n?(\d{1,3})$/i:/^(?:LTE\s*(?:BAND)?\s*)?b?(\d{1,3})$/i);
 const band=['Band','LTE_BAND','Freq band indicator'].map(k=>parseBand(get(k,radioEntries))).find(Boolean);
 const ca=entries.filter(e=>/^(Band \(CA\)|Band [1-9]\d*)$/i.test(e.key||'') &&
  (nrMode?!/LTE|4G/i.test(e.extra_info||''):lteMode&&!/NR|5G/i.test(e.extra_info||'')) && parseBand(e.value));
 const nrEntries=radioEntries;
 const carrier=text(simState.carrier)||text(get('ISP'));
 const numericCarrier=carrier && /^\d{5,6}$/.test(carrier);
 const mcc=numericCarrier?carrier.slice(0,3):String(get('MCC',nrEntries)||get('MMC',nrEntries)||get('MCC')||get('MMC')||'');
 const mnc=numericCarrier?carrier.slice(3):String(get('MNC',nrEntries)||get('MNC')||'').padStart(2,'0');
 const mobile=mcc==='460'&&['00','02','04','07','08','13'].includes(mnc);
 const operator=(!numericCarrier && carrier)||(mobile?'中国移动':mcc==='460'&&['01','06','09'].includes(mnc)?'中国联通':mcc==='460'&&['03','05','11'].includes(mnc)?'中国电信':mcc==='460'&&mnc==='15'?'中国广电':null);
 const operatorId=/中国移动|china mobile|CMCC/i.test(operator||'')?'mobile':/中国联通|unicom/i.test(operator||'')?'unicom':/中国电信|telecom/i.test(operator||'')?'telecom':/中国广电|broadnet|CBN/i.test(operator||'')?'broadnet':null;
 const bands=band?[Number(band[1]),...ca.map(e=>Number(parseBand(e.value)[1]))]:[];
 const advanced=nrMode && ((['telecom','unicom'].includes(operatorId)&&bands.filter(b=>b===78).length>=2)||(['mobile','broadnet'].includes(operatorId)&&bands.filter(b=>b===41).length>=2&&bands.includes(79))); 
 const slots={external1:'外置 SIM 1',external2:'外置 SIM 2',internal:'内置 SIM'};
 const ages=fresh.filter(s=>s.kind==='cell').map(s=>s.age);
 return {
  model:text(get('name',all)), firmware:text(get('revision',all)),
  temperature:bounded(get('temperature'),-40,150),
  sim:text(simState.sim_label)?.split(' · ')[0]||slots[simState.current_slot]||null,
  simName:text(simState.sim_name)||text(get('SIM Name'))||(text(get('ICCID'))?'普通卡':/not inserted|absent|未插卡/i.test(String(get('SIM Status')||''))?'未插卡':null), mode, nr:nrMode,
  rsrp:bounded(get('RSRP',radioEntries),-156,-31),sinr:bounded(get('SINR',radioEntries),-30,50),rsrq:bounded(get('RSRQ',radioEntries),-43,20),
  band:band?(nrMode?'n':'B')+band[1]:null,
  carriers:band?1+ca.length:null, operatorId, advanced,
  operator,chinaMobile:mobile||/中国移动|china mobile|CMCC/i.test(operator||''),
  status:mode ? (/No Service|SEARCH|NOCONN|未注册|无服务/i.test(mode)?'unregistered':'registered') : 'unknown',
  cached:sources.length>0,stale:sources.length>0&&!fresh.some(s=>s.kind==='cell'),
  age:ages.length?Math.max(...ages):null,section:text(raw.section)
 };
}
function network(raw={},wireless=null,clients={}) {
 const interfaces=Array.isArray(raw.interface)?raw.interface:null;
 // A default route indicates an uplink, not verified Internet reachability.
 const uplinks=(interfaces||[]).filter(i=>i.interface!=='loopback' && i.up===true && (
  (Array.isArray(i.route)?i.route:[]).some(r=>r.target==='0.0.0.0'||r.target==='::') ||
  /^wan|^wwan|^modem|^cellular/i.test(i.interface||'')
 ));
 const names=[];
 const validWireless=wireless && typeof wireless==='object' && !Array.isArray(wireless);
 if(validWireless) Object.values(wireless).forEach(radio=>{
  if(!radio || radio.disabled===true || radio.disabled==='1' || radio.up===false) return;
  (radio.interfaces||[]).forEach(iface=>{const ssid=text(iface.config?.ssid),disabled=iface.config?.disabled; if(ssid && disabled!==true && disabled!==1 && disabled!=='1' && !names.includes(ssid)) names.push(ssid);});
 });
 return {connected:interfaces?uplinks.length>0:null,ssid:!validWireless||!Object.keys(wireless).length?null:names.length?names.join(' / '):'无线未启用',clients:clients.available===true?bounded(clients.count,0,100000):null};
}
function uptime(seconds) {
 const n=number(seconds);
 if(n===null||n<0) return '—';
 const d=Math.floor(n/86400),h=Math.floor(n/3600)%24,m=Math.floor(n/60)%60;
 return (d?d+' 天 ':'')+(h?h+' 小时 ': '')+m+' 分钟';
}
return Object.freeze({number,measured,bounded,resources,hardware,modem,network,uptime});
});
