(function (root, factory) {
 'use strict';
 var api = factory();
 if (typeof module === 'object' && module.exports) module.exports = api;
 if (root.document) api.mount(root.document, root);
})(typeof window === 'undefined' ? globalThis : window, function () {
 'use strict';
 function object(value) { return value && typeof value === 'object' && !Array.isArray(value) ? value : {}; }
 function strings(value) { return Array.isArray(value) ? value.filter(function (v) { return typeof v === 'string' && v !== ''; }) : []; }
 function tri(value) { return value === true ? true : value === false ? false : null; }
 function number(value, low, high) {
  if (value === null || value === undefined || value === '' || typeof value === 'boolean') return null;
  var result = Number(value);
  return Number.isFinite(result) && result >= low && result <= high ? result : null;
 }
 function text(value, fallback) { return value === null || value === undefined || value === '' ? (fallback || '--') : String(value); }
 var cacheLabels = { missing: '缓存尚未生成', read_error: '缓存读取失败', invalid: '缓存解析失败', fresh: '正常', stale: '数据过期' };
 function viewModel(input) {
  var data = object(input), state = data.cache_state;
  if (data.schema !== 1 || !Object.keys(data).length) state = 'invalid';
  var sourceAge = number(data.cache_age, 0, 8640000), age = number(data.age, 0, 8640000);
  var totalAge = sourceAge !== null && age !== null ? sourceAge + age : null;
  if (!['missing', 'read_error', 'invalid'].includes(state)) {
   state = data.stale !== false || totalAge === null || totalAge > 90 ? 'stale' : 'fresh';
  }
  var fresh = state === 'fresh', modem = object(data.modem), sim = object(data.sim), signal = object(data.signal);
  var cell = object(data.cell), network = object(data.network), session = object(data.session), settings = object(data.settings);
  var ipv4 = strings(network.ipv4), ipv6 = strings(network.ipv6);
  var passthrough = object(data.passthrough), ptFresh = fresh && passthrough.stale === false && typeof passthrough.enabled === 'boolean';
  var ptLabel = !ptFresh ? '状态未知' : passthrough.enabled === false ? '关闭（模组路由）' :
   passthrough.ipv4_verified === true ? '已透传' : passthrough.ipv4_verified === false ? '已开启，地址未透传' : '已开启，等待地址核验';
  if (ptFresh && passthrough.pending === true) ptLabel += ' · 新配置待重启生效';
  var interfaces = (Array.isArray(network.interfaces) ? network.interfaces : []).filter(function (item) {
   var iface = object(item), device = iface.l3_device || iface.device || '';
   return ['cpe', 'cpe_4', 'cpe_6'].includes(iface.interface) && typeof device === 'string' &&
    device.split('.', 1)[0] === network.device && typeof iface.up === 'boolean';
  });
  var networkSample = interfaces.length > 0;
  var route = null;
  interfaces.forEach(function (iface) {
   if (iface.up !== true) return;
   (Array.isArray(object(iface).route) ? iface.route : []).forEach(function (item) {
    item = object(item);
    if (!route && (item.target === '0.0.0.0' || item.target === '::') && Number(item.mask) === 0) route = item;
   });
  });
  var registered = fresh ? tri(data.registered) : null;
  var connected = fresh && (networkSample || sim.status === 'NOT_INSERTED') ? tri(data.data_connected) : null;
  if (registered === null && connected === false) connected = null;
  var address = fresh && networkSample && (ipv4.length || ipv6.length) ? true : fresh && networkSample && network.online === false ? false : null;
  var routed = fresh && networkSample ? tri(network.default_route) : null;
  var unknown = state === 'stale' ? '数据过期' : '状态未知';
  function check(value, good, bad, detail) {
   return { state: value, label: value === true ? good : value === false ? bad : unknown, detail: text(detail) };
  }
  var checks = {
   registration: check(registered, '已注册', '未注册', object(data.registration).status),
   dial: check(connected, '已连接', '未连接', session.cid === undefined ? '' : 'CID ' + session.cid),
   address: check(address, '已获取地址', '暂无地址', ipv4[0] || ipv6[0]),
   route: check(routed, '蜂窝出口就绪', '无蜂窝默认路由', (route || {}).nexthop || network.device)
  };
  var mode = text(cell.mode || data.rat, ''), physical = fresh ? ({ '1': 'SIM 1（外置下槽）', '2': 'SIM 2（外置上槽）', '3': '内置联通', '4': '内置移动' }[sim.slot] || '未知') : '未知';
  var policy = { '0': '锁定', '1': '智能', '2': '无偏好' }[settings.policy];
  var sampled = number(data.sampled_at, 1, 8640000000000);
  var date = sampled === null ? null : new Date(sampled * 1000);
  var metrics = { rsrp: number(signal.rsrp, -156, -31), sinr: number(signal.sinr, -30, 50), rsrq: number(signal.rsrq, -43, 20) };
  var fields = {
   operator: text(data.operator, '运营商未知'), mode: mode, model: text(modem.model), revision: text(modem.revision),
   temperature: number(modem.temperature, -40, 150) === null ? '--' : modem.temperature + ' °C',
   sim: physical, 'sim-status': !fresh ? unknown :
    ({ READY: '就绪', NOT_INSERTED: '模组未检测到 SIM', 'SIM PIN': '需要 PIN', 'SIM PUK': '需要 PUK' }[sim.status] || text(sim.status, '未知')),
   apn: text(session.apn), band: text(cell.band), plmn: [cell.mcc, cell.mnc].filter(function (v) { return v !== null && v !== undefined && v !== ''; }).join(' / '),
   pci: text(cell.pci), cellid: text(cell.cell), tac: text(cell.tac), arfcn: text(cell.arfcn),
   device: text(network.device), ipv4: ipv4.join(' / '), ipv6: ipv6.join(' / '),
   'ip-passthrough': ptLabel,
   'ip-passthrough-config': settings.ip_passthrough === true ? '开启' : settings.ip_passthrough === false ? '关闭' : '未知',
   'carrier-ipv4': ptFresh ? text(passthrough.carrier_ipv4) : '--',
   'ip-passthrough-ipv4': !ptFresh || passthrough.ipv4_verified == null ? '等待核验' : passthrough.ipv4_verified ? '已透传' : '未透传',
   gateway: text((route || {}).nexthop), dns: strings(network.dns).join(' / '),
   rsrp: text(metrics.rsrp), sinr: text(metrics.sinr), rsrq: text(metrics.rsrq),
   rssi: number(signal.rssi, -160, 0) === null ? '--' : signal.rssi + ' dBm',
   carriers: strings(cell.carriers).join(' / ') || '未上报',
   source: text(data.source), 'cache-state': cacheLabels[state],
   'sampled-at': date && Number.isFinite(date.getTime()) ? date.toLocaleString() : '--',
   'cache-age': totalAge === null ? '--' : Math.round(totalAge) + ' 秒',
   supported: modem.supported === true ? '已识别' : modem.supported === false ? '未识别' : '未知',
   error: text(data.error), hwid: text(modem.hwid), endpoint: text(modem.endpoint), cid: text(session.cid),
   'slot-source': text(sim.slot_source), 'configured-slot': text(sim.configured_slot), policy: text(policy),
   'application-state': !fresh || settings.application_state === 'unknown_physical_slot' ? '保存配置；物理卡槽未知' :
    settings.application_state === 'not_sampled' ? '保存配置；硬件状态未采样' : '未知'
  };
  Object.keys(checks).forEach(function (key) { fields[key] = checks[key].label; fields[key + '-detail'] = checks[key].detail; });
  var note = state === 'missing' ? '采集缓存尚未生成；连接状态未知' : state === 'read_error' ? '采集缓存读取失败；连接状态未知' :
   state === 'invalid' ? '采集缓存解析失败；连接状态未知' : state === 'stale' ? '采集数据过期；连接状态未确认' :
   '缓存已更新' + (totalAge === null ? '' : ' · ' + Math.round(totalAge) + ' 秒前');
  if (fresh && data.error) note += ' · ' + data.error;
  return { state: state, fresh: fresh, note: note, fields: fields, checks: checks, metrics: metrics,
   arfcnLabel: /NR|5G|EN-DC/i.test(mode) ? 'NR-ARFCN' : 'EARFCN', snapshot: data };
 }
 function createPoller(options) {
  var stopped = true, active = null, timer = null, sequence = 0;
  var setTimer = options.setTimer || setTimeout, clearTimer = options.clearTimer || clearTimeout;
  var Controller = options.AbortController || (typeof AbortController === 'function' ? AbortController : null);
  function refresh() {
   if (stopped || active) return Promise.resolve(false);
   if (timer !== null) { clearTimer(timer); timer = null; }
   var job = { id: ++sequence, controller: Controller ? new Controller() : null, timeout: null };
   active = job;
   if (options.onBusy) options.onBusy(true);
   var expiry = new Promise(function (_, reject) {
    job.timeout = setTimer(function () {
     if (job.controller) job.controller.abort();
     var error = new Error('Status request timed out'); error.kind = 'timeout'; reject(error);
    }, options.timeout || 15000);
   });
   return Promise.race([Promise.resolve().then(function () { return options.request(job.controller ? job.controller.signal : undefined); }), expiry])
    .then(function (data) { if (!stopped && active === job) options.onData(data); })
    .catch(function (error) { if (!stopped && active === job) options.onError(error); })
    .finally(function () {
     clearTimer(job.timeout);
     if (active !== job) return;
     active = null;
     if (options.onBusy) options.onBusy(false);
     if (!stopped) timer = setTimer(function () { timer = null; refresh(); }, options.interval || 10000);
    });
  }
  return {
   start: function () { if (!stopped) return; stopped = false; return refresh(); },
   refresh: refresh,
   stop: function () {
    stopped = true;
    if (timer !== null) clearTimer(timer);
    timer = null;
    if (active) { clearTimer(active.timeout); if (active.controller) active.controller.abort(); }
    active = null;
    if (options.onBusy) options.onBusy(false);
   },
   busy: function () { return active !== null; }
  };
 }
 function getRequest(url, environment) {
  return function (signal) {
   return environment.fetch(url, { method: 'GET', credentials: 'same-origin', cache: 'no-store', headers: { Accept: 'application/json' }, signal: signal })
    .then(function (response) {
     if (!response.ok) { var error = new Error('HTTP ' + response.status); error.kind = 'http'; throw error; }
     return response.json().catch(function () { var error = new Error('Invalid status response'); error.kind = 'invalid_response'; throw error; });
    });
  };
 }
 function mount(document, environment) {
  document.querySelectorAll('[data-c8-cellular]').forEach(function (root) {
   var note = root.querySelector('[data-cache-note]'), button = root.querySelector('[data-refresh]');
   function set(key, value) { root.querySelectorAll('[data-field="' + key + '"]').forEach(function (node) { node.textContent = text(value); }); }
   function render(data) {
    var view = viewModel(data);
    note.dataset.state = view.state; note.textContent = view.note;
    root.dataset.uncertain = String(!view.fresh);
    Object.keys(view.fields).forEach(function (key) { set(key, view.fields[key]); });
    Object.keys(view.checks).forEach(function (key) {
     var node = root.querySelector('[data-check="' + key + '"]');
     if (node) node.dataset.state = view.checks[key].state === true ? 'good' : view.checks[key].state === false ? 'bad' : 'unknown';
    });
    Object.keys(view.metrics).forEach(function (key) {
     var node = root.querySelector('[data-meter="' + key + '"]');
     if (node) { node.dataset.unknown = String(!view.fresh || view.metrics[key] === null); node.value = view.metrics[key] === null ? Number(node.min) : view.metrics[key]; }
    });
    var arfcn = root.querySelector('[data-arfcn-label]'), snapshot = root.querySelector('[data-snapshot]');
    if (arfcn) arfcn.textContent = view.arfcnLabel;
    if (snapshot) snapshot.textContent = JSON.stringify(view.snapshot, null, 2);
   }
   function failed(error) {
    render({ schema: 1, cache_state: 'missing', stale: true });
    note.dataset.state = 'error';
    note.textContent = error.kind === 'invalid_response' ? '状态响应解析失败；连接状态未知' :
     error.kind === 'timeout' ? '状态请求超时；连接状态未知' : '状态请求失败；连接状态未知';
    set('cache-state', '请求失败');
   }
   var poller = createPoller({ request: getRequest(root.dataset.statusUrl, environment), onData: render, onError: failed,
    onBusy: function (busy) { button.disabled = busy; root.setAttribute('aria-busy', String(busy)); },
    setTimer: environment.setTimeout.bind(environment), clearTimer: environment.clearTimeout.bind(environment), AbortController: environment.AbortController });
   button.addEventListener('click', function () { poller.refresh(); });
   environment.addEventListener('pagehide', function () { poller.stop(); });
   environment.addEventListener('pageshow', function () { if (!document.hidden) poller.start(); });
   document.addEventListener('visibilitychange', function () { if (document.hidden) poller.stop(); else poller.start(); });
   if (!document.hidden) poller.start();
  });
 }
 return { viewModel: viewModel, createPoller: createPoller, getRequest: getRequest, mount: mount };
});
