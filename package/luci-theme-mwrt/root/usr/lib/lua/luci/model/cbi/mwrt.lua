local page = require("luci.http").getenv("PATH_INFO") or ""
local mode = page:find("traffic_dividing",1,true) and "traffic" or page:find("factory_logs",1,true) and "logs" or "guest"
local title = mode=="traffic" and "流量分流" or mode=="logs" and "日志管理" or "访客 Wi-Fi"
local m = Map("luci", translate(title))
local section = m:section(NamedSection, "main", "core", translate(title))
section.addremove = false

-- Fields from C5800 factory 2.3.3 dividing/cpedata/syslog/guest pages.
local factory_panels = {
    {"dividing", "分流设置", [==[
<p>线路备份、终端 / IP / 域名分流与本地叠加统一在“模组设定 → PoE 副 5G → 外网设置”中配置。</p>
<a class="cbi-button" href="/cgi-bin/luci/admin/modem/modem?tab=extension">前往 PoE 副 5G 设置</a>
]==]},
    {"traffic", "SIM 流量统计", [==[
<p>按实际使用的卡槽累计上行和下行流量。仅统计，不自动切卡或超额断网；以运营商账单为准。</p>
<label>卡槽 <select id="nr-slot"></select></label>
<button type="button" class="cbi-button" id="nr-refresh-traffic">刷新</button>
<p id="nr-usage"></p>
<label>套餐类型 <select id="nr-kind"><option value="monthly">月套餐</option><option value="total">总量套餐</option></select></label><br>
<label>套餐流量（GB） <input id="nr-limit" type="number" min="0" step="any"></label><br>
<label>每月计费起始日（1–28） <input id="nr-day" type="number" min="1" max="28"></label><br>
<label>总量套餐到期时间 <input id="nr-end" type="datetime-local"></label><br>
<p>月流量在计费日起重新累计；总量套餐到期后清零一次。统计从本功能启用时开始。</p>
<button type="button" class="cbi-button cbi-button-apply" id="nr-save-traffic">保存套餐</button>
<button type="button" class="cbi-button" id="nr-reset-monthly">月流量清零</button>
<button type="button" class="cbi-button" id="nr-reset-total">总流量清零</button>
<span id="nr-traffic-result" role="status"></span>
]==]},
    {"logs", "日志管理", [==[
<label><input type="checkbox" id="nr-log-persist"> 本地存储日志</label>
<p id="nr-log-path"></p>
<button type="button" class="cbi-button cbi-button-apply" id="nr-save-logs">保存日志设置</button>
<a class="cbi-button" id="nr-download-logs">下载日志包</a>
<span id="nr-logs-result" role="status"></span>
]==]},
    {"guest", "访客 Wi-Fi", [==[
<label><input id="nr-guest-enable" type="checkbox"> 启用访客 Wi-Fi</label><br>
<label>SSID <input id="nr-guest-ssid" type="text" maxlength="32"></label><br>
<label>加密方式 <select id="nr-guest-encryption"><option value="none">开放</option><option value="psk-mixed">WPA2-PSK</option></select></label><br>
<label>密码 <input id="nr-guest-key" type="password" autocomplete="new-password" placeholder="留空保留已有密码"></label><br>
<label><input id="nr-guest-isolation" type="checkbox"> 隔离访客与内网</label><br>
<span>应用频段 </span><label><input id="nr-guest-2g" type="checkbox">2.4G</label>
<label><input id="nr-guest-5g" type="checkbox">5G</label>
<p>访客网络地址为 192.168.67.1/24。应用时无线会短暂重新加载。</p>
<button type="button" class="cbi-button cbi-button-apply" id="nr-save-guest">保存并应用访客 Wi-Fi</button>
<span id="nr-guest-result" role="status"></span>
<script>
window.addEventListener('load',function(){
  var el=function(id){return document.getElementById('nr-'+id)}, rows={};
  function status(name,text){el(name+'-result').textContent=text||''}
  function request(name,value){
    var opts={credentials:'same-origin',cache:'no-store'};
    if(value!==undefined){opts.method='POST';opts.headers={'Content-Type':'application/x-www-form-urlencoded'};
      opts.body='token='+encodeURIComponent((L.env&&L.env.token)||'')+'&data='+encodeURIComponent(JSON.stringify(value))}
    return fetch(L.url('admin/modem/factory_'+name),opts).then(function(r){return r.text().then(function(t){
      var data;try{data=JSON.parse(t)}catch(e){throw new Error('接口返回异常（HTTP '+r.status+'）')}
      if(!r.ok||!data.ok)throw new Error(data.error||'操作失败');return data;
    })});
  }
  function bind(id,name,fn){el(id).onclick=function(){var b=this;b.disabled=true;status(name,'处理中…');
    Promise.resolve().then(fn).then(function(){status(name,'已应用')}).catch(function(e){status(name,e.message)}).then(function(){b.disabled=false})}}
  function clientRow(c){
    var row=document.createElement('div'),mac=document.createElement('input'),select=document.createElement('select'),remove=document.createElement('button');
    mac.placeholder='终端 MAC';mac.value=c.mac||'';mac.setAttribute('list','nr-hosts');mac.className='nr-client-mac';
    [['main','内置 5G'],['poe','副 5G']].forEach(function(v){var o=document.createElement('option');o.value=v[0];o.textContent=v[1];select.appendChild(o)});
    select.value=c.uplink||'main';select.className='nr-client-uplink';remove.type='button';remove.className='cbi-button';remove.textContent='移除';remove.onclick=function(){row.remove()};
    row.appendChild(mac);row.appendChild(select);row.appendChild(remove);el('clients').appendChild(row);
  }
  if(el('clients')){
  el('add-client').onclick=function(){clientRow({})};
  request('dividing').then(function(d){
    Array.from(d.clients||[]).forEach(clientRow);el('main').value=Array.from(d.main||[]).join('\n');el('poe').value=Array.from(d.poe||[]).join('\n');
    Array.from(d.hosts||[]).forEach(function(h){var o=document.createElement('option');o.value=h.mac;o.label=(h.name||'')+' '+h.ip;el('hosts').appendChild(o)});
  }).catch(function(e){status('dividing',e.message)});
  bind('save-dividing','dividing',function(){
    var clients=Array.from(el('clients').children).map(function(r){return {mac:r.querySelector('.nr-client-mac').value.trim(),uplink:r.querySelector('.nr-client-uplink').value}});
    return request('dividing',{clients:clients,main:el('main').value.split(/\n/),poe:el('poe').value.split(/\n/)});
  });
  }
  function bytes(n){return (Number(n||0)/1000000000).toFixed(4)+' GB'}
  function showTraffic(){
    var r=rows[el('slot').value];if(!r)return;
    el('usage').textContent=(r.active?'使用中':'未使用')+' · 本月 '+bytes(r.monthly)+' · 总量 '+bytes(r.total)+' · 计费周期 '+r.period;
    el('kind').value=r.kind;el('limit').value=r.limit_gb;el('day').value=r.day;el('end').value=r.end||'';
  }
  function loadTraffic(){return request('traffic').then(function(d){
    rows=d.data.rows||{};var selected=el('slot').value;el('slot').textContent='';
    Object.keys(rows).sort(function(a,b){return rows[a].channel.localeCompare(rows[b].channel)||rows[a].slot-rows[b].slot}).forEach(function(k){var o=document.createElement('option');o.value=k;o.textContent=rows[k].name;el('slot').appendChild(o)});
    if(rows[selected])el('slot').value=selected;showTraffic();if(d.error)throw new Error(d.error);
  })}
  if(el('slot')){
  el('slot').onchange=showTraffic;
  loadTraffic().catch(function(e){status('traffic',e.message)});
  bind('refresh-traffic','traffic',loadTraffic);
  bind('save-traffic','traffic',function(){return request('traffic',{action:'save',key:el('slot').value,kind:el('kind').value,day:Number(el('day').value),limit_gb:Number(el('limit').value),end:el('end').value}).then(loadTraffic)});
  ['monthly','total'].forEach(function(kind){bind('reset-'+kind,'traffic',function(){return request('traffic',{action:'reset',key:el('slot').value,reset:kind}).then(loadTraffic)})});
  }
  function loadLogs(){return request('logs').then(function(d){el('log-persist').checked=d.enabled;el('log-path').textContent=d.enabled?'存储位置：'+d.path:'启用后在数据分区保留系统日志。'})}
  if(el('log-persist')){
  loadLogs().catch(function(e){status('logs',e.message)});
  el('download-logs').href=L.url('admin/modem/factory_logs')+'?download=1';
  bind('save-logs','logs',function(){return request('logs',{enabled:el('log-persist').checked}).then(loadLogs)});
  }
  function loadGuest(){return request('guest').then(function(d){
    el('guest-enable').checked=d.enabled;el('guest-ssid').value=d.ssid;el('guest-encryption').value=d.encryption;
    el('guest-isolation').checked=d.isolation;['2g','5g'].forEach(function(b){el('guest-'+b).checked=(d.bands||[]).indexOf(b)>=0});el('guest-key').value='';
    return d;
  })}
  function guestApplied(){return loadGuest().then(function(d){
    if(d.applying){status('guest','无线正在应用，等待原有网络恢复…');return new Promise(function(resolve){setTimeout(resolve,2000)}).then(guestApplied)}
  })}
  if(el('guest-enable')){
  loadGuest().catch(function(e){status('guest',e.message)});
  bind('save-guest','guest',function(){return request('guest',{enabled:el('guest-enable').checked,ssid:el('guest-ssid').value,encryption:el('guest-encryption').value,key:el('guest-key').value,isolation:el('guest-isolation').checked,bands:['2g','5g'].filter(function(b){return el('guest-'+b).checked})}).then(guestApplied)});
  }
});
</script>
]==]}
}
local script=factory_panels[4][3]:match("(<script>.-</script>)") or ""
factory_panels[4][3]=factory_panels[4][3]:gsub("<script>.-</script>","")
for _, panel in ipairs(factory_panels) do
    if (mode=="traffic" and (panel[1]=="dividing" or panel[1]=="traffic")) or mode==panel[1] then
    section:tab(panel[1], translate(panel[2]))
    local option=section:taboption(panel[1], DummyValue, "_factory_"..panel[1], "")
    option.rawhtml=true
    local html=panel[3] .. ((panel[1]=="traffic" or mode~="traffic") and script or "")
    option.cfgvalue=function() return html end
    end
end

return m
