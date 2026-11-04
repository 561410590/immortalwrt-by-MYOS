(function () {
 'use strict';
 var root = document.getElementById('nb-cellular');
 if (!root) return;
 var button = root.querySelector('[data-refresh]');
 var note = root.querySelector('[data-cache-note]');
 var previous = null;
 function text(value, fallback) {
  return value === null || value === undefined || value === '' ? (fallback || '—') : String(value);
 }
 function set(key, value) {
  root.querySelectorAll('[data-field="' + key + '"]').forEach(function (el) { el.textContent = text(value); });
 }
 function number(value, low, high) {
  if (value === null || value === undefined || value === '') return null;
  var n = Number(value);
  return Number.isFinite(n) && n >= low && n <= high ? n : null;
 }
 function check(key, state, label, detail) {
  root.querySelector('[data-check="' + key + '"]').dataset.state = state === true ? 'good' : state === false ? 'bad' : 'unknown';
  set(key, label); set(key + '-detail', detail);
 }
 function render(data) {
  previous = data;
  var cache = data.cache || {}, summary = data.summary || {}, cell = data.serving_cell || {};
  var registration = data.registration || {}, dial = data.dial || {}, network = data.network || {};
  var entries = (data.sources || []).flatMap(function (s) { return s.entries || []; });
  function entry(key) {
   var found = entries.find(function (e) { return String(e.key).toLowerCase() === key.toLowerCase(); });
   return found ? found.value : null;
  }
  var stale = cache.stale !== false, age = number(cache.age, 0, 8640000);
  note.dataset.stale = String(stale);
  note.textContent = cache.state === 'no_sim' ? '未检测到 SIM 卡，请检查当前卡槽。' :
   cache.state === 'missing' || cache.state === 'partial' ? '正在等待模组状态，请稍后刷新。' :
   stale ? '信号数据已过期' + (age === null ? '，等待模组更新。' : ' · ' + Math.round(age) + ' 秒前更新') :
   '状态自动更新 · 信号 ' + (age === null ? '刚刚更新' : Math.round(age) + ' 秒前更新');
  var mode = cell.mode || registration.mode || summary.network_mode || entry('network_mode');
  set('operator', summary.operator || entry('ISP') || '运营商未获取');
  set('mode', mode); set('mode-detail', mode);
  var registered = registration.registered;
  check('registration', registered,
   registered === true ? '已驻网' : registered === false ? '未驻网' : '状态未获取',
   registration.label || (registration.status === null || registration.status === undefined ? '等待注册状态' : '注册状态码 ' + registration.status));
  check('dial', dial.connected,
   dial.connected === true ? '已建立数据会话' : dial.connected === false ? '未拨号' : '状态未获取',
   [dial.cid === null || dial.cid === undefined ? '' : 'CID ' + dial.cid, dial.pdp_type].filter(Boolean).join(' · ') || '等待模组数据会话');
  var addresses = [network.ipv4].concat(network.ipv6 || []).filter(Boolean);
  check('address', addresses.length ? true : network.up === false ? false : null,
   addresses.length ? '已获取地址' : '暂无地址', network.ipv4 || (network.ipv6 || [])[0] || '蜂窝数据接口');
  var routeDevice = network.default_route_device || '';
  var route = network.default_route === true && (!routeDevice || routeDevice === network.device) ? true :
   network.default_route === false ? false : null;
  check('route', route, route === true ? '蜂窝出口已就绪' : route === false ? '蜂窝出口未就绪' : '出口未确认',
   routeDevice || network.interface || '等待默认路由');
  set('model', summary.model || entry('name'));
  set('firmware', summary.revision || entry('revision'));
  set('sim', data.sim_label || entry('sim_label'));
  var temperature = number(entry('temperature'), -40, 150);
  set('temperature', temperature === null ? '温度未获取' : temperature + ' °C');
  set('apn', dial.apn); set('pdp', dial.pdp_type);
  var nr = /NR|5G|EN-DC/i.test(mode || '');
  root.querySelector('[data-arfcn-label]').textContent = nr ? 'NR-ARFCN' : 'EARFCN';
  set('band', cell.band || summary.band || entry('Band'));
  set('plmn', [cell.mcc || entry('MCC'), cell.mnc || entry('MNC')].filter(function (v) { return v !== null && v !== undefined && v !== ''; }).join(' / '));
  set('pci', cell.pci); set('cellid', cell.cellid); set('tac', cell.tac); set('arfcn', cell.arfcn);
  var values = {rsrp:number(cell.rsrp, -156, -31), rsrq:number(cell.rsrq, -43, 20), sinr:number(cell.sinr, -30, 50)};
  Object.keys(values).forEach(function (key) {
   set(key, values[key]);
   var range = key === 'rsrp' ? [-140, -70] : key === 'sinr' ? [-10, 30] : [-20, -3];
   root.querySelector('[data-meter="' + key + '"]').style.width = values[key] === null ? '0%' : Math.max(0, Math.min(100, (values[key] - range[0]) / (range[1] - range[0]) * 100)) + '%';
  });
  set('signal-quality', stale ? '缓存已过期' : values.rsrp === null ? '未获取' : values.rsrp >= -80 ? '优' : values.rsrp >= -90 ? '良好' : values.rsrp >= -100 ? '一般' : '较弱');
  set('interface', network.interface); set('device', network.device);
  set('ipv4', network.ipv4); set('ipv6', (network.ipv6 || []).join(' / '));
  var iface = (data.interfaces || []).find(function (i) { return i.interface === network.interface; }) || {};
  var defaultRoute = (iface.route || []).find(function (r) { return (r.target === '0.0.0.0' || r.target === '::') && Number(r.mask) === 0; });
  set('gateway', network.gateway || (defaultRoute || {}).nexthop); set('dns', (network.dns || iface['dns-server'] || []).join(' / '));
  var carriers = (data.carriers || []).filter(Boolean), container = root.querySelector('[data-carriers]');
  container.replaceChildren();
  var list = document.createElement('div'); list.className = 'nb-carrier-list';
  if (!carriers.length && (cell.band || summary.band || entry('Band'))) carriers = [entry('Band_raw') || entry('Band') || cell.band || summary.band];
  carriers.forEach(function (carrier, index) {
   var card = document.createElement('div'); card.className = 'nb-carrier';
   var label = document.createElement('small'); label.textContent = index === 0 ? '主载波' : '辅载波 ' + index;
   var value = document.createElement('span'); value.textContent = String(carrier);
   card.append(label, value); list.append(card);
  });
  if (carriers.length) { container.append(list); set('carrier-count', carriers.length + ' 个已上报载波'); }
  else { var empty = document.createElement('p'); empty.className = 'nb-footnote'; empty.textContent = '模组尚未上报载波信息。'; container.append(empty); set('carrier-count', '未获取'); }
 }
 function receive(xhr, data) {
  button.disabled = false;
  if (!xhr || xhr.status !== 200 || !data || !Array.isArray(data.sources)) {
   note.dataset.stale = 'true'; note.textContent = '状态更新失败，请稍后刷新。';
   if (previous) ['registration', 'dial', 'address', 'route'].forEach(function (key) { check(key, null, '状态更新失败', '请稍后刷新'); });
   return;
  }
  render(data);
 }
 button.addEventListener('click', function () { button.disabled = true; XHR.get(root.dataset.statusUrl, null, receive); });
 XHR.poll(10, root.dataset.statusUrl, null, receive);
})();
