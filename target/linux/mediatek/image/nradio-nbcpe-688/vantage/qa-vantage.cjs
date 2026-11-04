/* Exercise the supplied LuCI dashboard load/model using mocked read-only RPC. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

async function main() {
  const root = process.argv[2] || path.join(__dirname, 'root');
  const resources = path.join(root, 'www/luci-static/resources');
  let mode = 'fresh';
  const calls = [];
  const cellular = {present:true, section:'AK68', module:'MT5700M-CN', manufacturer:'TD Tech Ltd.',
    revision:'V200R001C20B024', connected:true, tech:'NR-5G', rat:'NR-5G', operator:'中国移动',
    mcc:'460', mnc:'00', cell_id:'0000000138DF9003', pci:'125', tac:'64451C', arfcn:'504990',
    band:'n41', scs:null, rsrp:-117, rsrq:-12, sinr:2, sim_ready:true, updated:Math.floor(Date.now()/1000)-5};
  const answers = {
    'system.board': {model:'NBCPE-688',hostname:'MYOS',kernel:'6.6.110',release:{distribution:'MYOS',version:'3.1.34-202610092000'}},
    'system.info': {uptime:500,localtime:1700000000,load:[0,0,0],memory:{total:500000000,free:400000000}},
    'file.read': 'cpu  100 0 50 1000 0 0 0 0\n',
    'network.interface.dump': [{interface:'wwan',up:true,l3_device:'port5','ipv4-address':[{address:'192.0.2.2',mask:24}],route:[{target:'0.0.0.0',mask:0,nexthop:'192.0.2.1'}]}],
    'network.device.status': {port5:{up:true,speed:1000,statistics:{rx_bytes:1000,tx_bytes:500}}},
    'luci.vantage.wireless': {}, 'luci-rpc.getHostHints': {},
    'luci.vantage.sensors': {cpu_temp_mc:45000,cpu_sensor:'/sys/class/thermal/thermal_zone0/temp',wifi_temps:[],modem_temp_mc:22000},
    'uci.get': {main:{'.type':'vantage',auto_refresh:'0',names_vendor:'0'}}, 'session.access':false
  };
  const context = vm.createContext({
    console, Date, Math, JSON, Promise, URL, setTimeout, clearTimeout,
    document:{}, window:{}, localStorage:{getItem:()=>null,setItem:()=>{}},
    fetch:async()=>({ok:true,text:async()=>''}),
    _:s=>s, baseclass:{extend:obj=>obj}, view:{extend:obj=>obj},
    ui:{}, dom:{}, poll:{add:()=>{}},
    L:{resource:s=>'/luci-static/resources/'+s,resolveDefault:(promise,fallback)=>Promise.resolve(promise).catch(()=>fallback)},
    rpc:{declare:spec=>(...args)=>{
      const key=spec.object+'.'+spec.method; calls.push({key,args});
      if(key==='luci.vantage.cellular') return Promise.resolve(mode==='fresh'?cellular:
        {...cellular,connected:null,tech:null,rat:null,operator:null,rsrp:null,rsrq:null,sinr:null,sim_ready:null,updated:null});
      if(key==='umdns.hosts') return Promise.reject(new Error('optional daemon absent'));
      if(!Object.hasOwn(answers,key)) return Promise.reject(new Error('Unhandled RPC '+key));
      return Promise.resolve(answers[key]);
    }}
  });
  vm.runInContext("String.prototype.format=function(){let i=0,a=arguments;return this.replace(/%[sd]/g,()=>String(a[i++]));};",context);
  function load(relative) {
    const source=fs.readFileSync(path.join(resources,relative),'utf8');
    return vm.runInContext('(function(){\n'+source+'\n})()',context,{filename:relative});
  }
  context.fmt=load('vantage/fmt.js');
  context.wifi=load('vantage/wifi.js');
  context.names=load('vantage/names.js');
  context.model=load('vantage/model.js');
  context.insight=load('vantage/insight.js');
  context.series=load('vantage/series.js');
  const overview=load('view/vantage/overview.js');
  const loaded=await overview.load();
  assert.equal(loaded.board.model,'NBCPE-688');
  assert.equal(loaded.raw.cellular.connected,true);
  assert.equal(loaded.raw.cellular.rsrp,-117);
  assert.equal(loaded.raw.cellular.band,'n41');
  assert.equal(loaded.canWrite,false);
  assert.equal(loaded.mdns,null);
  const model=context.model.cellular(loaded.raw.cellular);
  assert.equal(model.connected,true);
  assert.equal(model.rsrp,-117);
  assert.equal(context.model.temps(loaded.raw.sensors).modem.mc,22000);
  mode='stale';
  const stale=await overview.fetch();
  const staleModel=context.model.cellular(stale.cellular);
  assert.equal(staleModel.connected,null);
  assert.equal(staleModel.rsrp,null);
  assert.equal(staleModel.simReady,null);
  assert(!calls.some(call=>/set|apply|dial|refresh-radio|AT\^|AT\+/.test(call.key)));
  assert(calls.some(call=>call.key==='luci.vantage.cellular'));
  console.log('Vantage dashboard load/RPC/model fresh + stale + absent optional mDNS: passed');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
