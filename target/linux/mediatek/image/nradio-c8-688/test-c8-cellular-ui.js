'use strict';
const assert = require('node:assert/strict');
const test = require('node:test');
const fs = require('node:fs');
const path = require('node:path');
const ui = require('./native-overlay/www/luci-static/myos/c8-cellular.js');
function fresh(extra = {}) {
 return { schema: 1, cache_state: 'fresh', stale: false, age: 0, cache_age: 1, sampled_at: 1791670000,
  registered: true, data_connected: true, modem: { supported: true, model: 'MT5700M-CN', temperature: 40 },
  sim: { slot: '2', configured_slot: '1', status: 'READY' }, signal: { rsrp: -90, sinr: 12, rsrq: -10 },
  cell: { mode: '5G SA', cell: '123', carriers: [] }, session: { cid: 8, apn: 'cmnet' }, registration: { status: 'register' },
  network: { device: 'eth1', ipv4: ['10.0.0.2'], ipv6: [], online: true, default_route: true, dns: ['1.1.1.1'],
   interfaces: [{ interface: 'cpe_4', l3_device: 'eth1', up: true, route: [{ target: '0.0.0.0', mask: 0, nexthop: '10.0.0.1' }] }] }, settings: { policy: '2', application_state: 'not_sampled' }, ...extra };
}
test('fresh C8 fields and real physical slot map to status', () => {
 const view = ui.viewModel(fresh());
 assert.equal(view.state, 'fresh');
 for (const check of Object.values(view.checks)) assert.equal(check.state, true);
 assert.equal(view.fields.sim, 'SIM 2'); assert.equal(view.fields['configured-slot'], '1');
 assert.equal(view.fields.ipv4, '10.0.0.2'); assert.equal(view.fields.gateway, '10.0.0.1');
 assert.equal(view.fields.cellid, '123'); assert.equal(view.fields.rsrp, '-90');
 assert.equal(view.fields.carriers, '未上报'); assert.equal(view.arfcnLabel, 'NR-ARFCN');
});
test('empty IPv4 arrays do not prove an address exists', () => {
 const view = ui.viewModel(fresh({ network: { ipv4: [], ipv6: [], online: false, default_route: false,
  interfaces: [{ interface: 'cpe', device: 'eth1', up: false }] } }));
 assert.equal(view.checks.address.state, false); assert.equal(view.checks.route.state, false);
});
test('IPv6-only data has an address without fabricating IPv4', () => {
 const view = ui.viewModel(fresh({ network: { ipv4: [], ipv6: ['2001:db8::1'], online: true, default_route: true,
  interfaces: [{ interface: 'cpe_6', l3_device: 'eth1', up: true, route: [{ target: '::', mask: 0, nexthop: 'fe80::1' }] }] } }));
 assert.equal(view.checks.address.state, true); assert.equal(view.fields.ipv4, ''); assert.equal(view.fields.ipv6, '2001:db8::1');
 assert.equal(view.fields.gateway, 'fe80::1'); assert.equal(view.checks.route.state, true);
});
test('missing netifd samples cannot prove that a registered modem is disconnected', () => {
 for (const interfaces of [[], [{ interface: 'wan', device: 'eth1', up: false }],
  [{ interface: 'cpe', device: 'eth0', up: false }], [{ interface: 'cpe', device: 'eth1' }]]) {
  const view = ui.viewModel(fresh({ data_connected: false, network: { ipv4: [], ipv6: [], online: false, default_route: false, interfaces } }));
  assert.equal(view.checks.registration.state, true);
  for (const name of ['dial', 'address', 'route']) assert.equal(view.checks[name].state, null);
 }
});
test('a valid down C8 interface is a confirmed disconnected sample', () => {
 const view = ui.viewModel(fresh({ data_connected: false, network: { ipv4: [], ipv6: [], online: false, default_route: false,
  interfaces: [{ interface: 'cpe', l3_device: 'eth1', up: false }] } }));
 for (const name of ['dial', 'address', 'route']) assert.equal(view.checks[name].state, false);
});
test('default gateway ignores routes retained by a down interface', () => {
 const data = fresh();
 data.network.interfaces.unshift({ interface: 'cpe_6', l3_device: 'eth1', up: false, route: [{ target: '::', mask: 0, nexthop: 'fe80::dead' }] });
 const view = ui.viewModel(data);
 assert.equal(view.fields.gateway, '10.0.0.1'); assert.equal(view.checks.route.detail, '10.0.0.1');
});
test('unknown registration does not become disconnected', () => {
 const view = ui.viewModel(fresh({ registered: null, data_connected: false }));
 assert.equal(view.checks.registration.state, null); assert.equal(view.checks.dial.state, null);
 assert.equal(view.fields.registration, '状态未知');
});
test('confirmed false remains a real failure', () => {
 const view = ui.viewModel(fresh({ registered: false, data_connected: false }));
 assert.equal(view.checks.registration.state, false); assert.equal(view.checks.dial.state, false);
});
test('stale flags and combined source age invalidate every connection indicator', () => {
 for (const extra of [{ stale: true }, { cache_age: 90, age: 1 }, { cache_age: null }]) {
  const view = ui.viewModel(fresh(extra));
  assert.equal(view.state, 'stale'); for (const check of Object.values(view.checks)) assert.equal(check.state, null);
  assert.equal(view.fields.sim, '未知');
  assert.match(view.note, /过期/);
 }
});
test('missing, unreadable and malformed cache show distinct unknown states', () => {
 for (const state of ['missing', 'read_error', 'invalid']) {
  const view = ui.viewModel({ schema: 1, cache_state: state, stale: true });
  assert.equal(view.state, state); for (const check of Object.values(view.checks)) assert.equal(check.state, null);
 }
 assert.match(ui.viewModel({ schema: 1, cache_state: 'missing' }).note, /尚未生成/);
 assert.match(ui.viewModel({ schema: 1, cache_state: 'invalid' }).note, /解析失败/);
 for (const malformed of [null, [], 'text', 5, { schema: 0 }]) assert.equal(ui.viewModel(malformed).state, 'invalid');
});
test('native degraded cache with null source age retains diagnostics as stale', () => {
 const view = ui.viewModel(fresh({ cache_state: 'stale', stale: true, cache_age: null, error: 'Official cache unavailable' }));
 assert.equal(view.state, 'stale'); assert.equal(view.fields.model, 'MT5700M-CN');
 assert.equal(view.fields.error, 'Official cache unavailable'); assert.equal(view.fields.policy, '无偏好');
 assert.equal(view.fields.sim, '未知'); for (const check of Object.values(view.checks)) assert.equal(check.state, null);
});
test('physical slot is not inferred from configured slot and signal sentinels are absent', () => {
 const view = ui.viewModel(fresh({ sim: { slot: '', configured_slot: '2' }, signal: { rsrp: -999, sinr: 255, rsrq: null } }));
 assert.equal(view.fields.sim, '未知'); assert.equal(view.metrics.rsrp, null); assert.equal(view.metrics.sinr, null);
});
test('saved policy remains configuration-only, not hardware applied', () => {
 const view = ui.viewModel(fresh({ settings: { policy: '1', application_state: 'unknown_physical_slot' } }));
 assert.equal(view.fields.policy, '智能'); assert.match(view.fields['application-state'], /保存配置；物理卡槽未知/);
});
test('refresh request is GET only with same-origin credentials', async () => {
 let options;
 const get = ui.getRequest('/admin/modem/status', { fetch: async (url, value) => { options = value; return { ok: true, json: async () => fresh() }; } });
 assert.equal((await get()).schema, 1); assert.equal(options.method, 'GET'); assert.equal(options.credentials, 'same-origin'); assert.equal(options.cache, 'no-store'); assert.equal(options.body, undefined);
});
test('HTTP and response parsing errors do not become state objects', async () => {
 await assert.rejects(ui.getRequest('/status', { fetch: async () => ({ ok: false, status: 503 }) })(), { kind: 'http' });
 await assert.rejects(ui.getRequest('/status', { fetch: async () => ({ ok: true, json: async () => { throw new Error('bad JSON'); } }) })(), { kind: 'invalid_response' });
});
function scheduler() {
 let id = 0; const timers = new Map();
 return { timers, setTimer(fn, ms) { timers.set(++id, { fn, ms }); return id; }, clearTimer(key) { timers.delete(key); } };
}
function deferred() { let resolve, reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; }
test('polling prevents reentry and cancels scheduled work on exit', async () => {
 const clock = scheduler(), pending = deferred(); let calls = 0, updates = 0;
 const poller = ui.createPoller({ ...clock, request: () => { calls++; return pending.promise; }, onData: () => { updates++; }, onError: () => assert.fail('unexpected error') });
 const first = poller.start(); await Promise.resolve(); await poller.refresh();
 assert.equal(calls, 1); assert.equal(poller.busy(), true);
 pending.resolve(fresh()); await first;
 assert.equal(updates, 1); assert.equal(poller.busy(), false); assert.equal(clock.timers.size, 1);
 poller.stop(); assert.equal(clock.timers.size, 0);
});
test('exit aborts active request and ignores late responses', async () => {
 const clock = scheduler(), pending = deferred(); let signal, updates = 0;
 const poller = ui.createPoller({ ...clock, request: (value) => { signal = value; return pending.promise; }, onData: () => { updates++; }, onError: () => assert.fail('late error') });
 const first = poller.start(); await Promise.resolve(); poller.stop();
 assert.equal(signal.aborted, true); assert.equal(clock.timers.size, 0);
 pending.resolve(fresh()); await first; assert.equal(updates, 0);
});
test('page resume does not accept an older in-flight result', async () => {
 const clock = scheduler(), old = deferred(), next = deferred(); const updates = []; let count = 0;
 const poller = ui.createPoller({ ...clock, request: () => (++count === 1 ? old.promise : next.promise), onData: data => updates.push(data.id), onError: () => {} });
 const first = poller.start(); await Promise.resolve(); poller.stop(); const second = poller.start(); await Promise.resolve();
 old.resolve({ id: 'old' }); await first; next.resolve({ id: 'new' }); await second;
 assert.deepEqual(updates, ['new']); poller.stop();
});
test('request timeout aborts request and leaves a retry scheduled', async () => {
 const clock = scheduler(); let signal, error;
 const poller = ui.createPoller({ ...clock, request: value => { signal = value; return new Promise(() => {}); }, onData: () => assert.fail('timeout data'), onError: value => { error = value; } });
 const first = poller.start(); await Promise.resolve(); [...clock.timers.values()].find(t => t.ms === 15000).fn(); await first;
 assert.equal(signal.aborted, true); assert.equal(error.kind, 'timeout'); assert.equal(poller.busy(), false); poller.stop();
});
test('status and diagnostics templates are independent read-only views', () => {
 const base = path.join(__dirname, 'native-overlay/usr/lib/lua/luci');
 const controller = fs.readFileSync(path.join(base, 'controller/modem.lua'), 'utf8');
 const cbi = fs.readFileSync(path.join(base, 'model/cbi/myos_c8_modem.lua'), 'utf8');
 assert.match(controller, /template\("myos_c8_modem\/status"\)/); assert.match(controller, /template\("myos_c8_modem\/diagnostics"\)/);
 assert.match(controller, /post\("action_request"\)/); assert.match(controller, /request\.origin = "module_settings"/);
 assert.match(cbi, /REQUEST_METHOD.*POST/); assert.match(cbi, /count ~= 1/); assert.match(cbi, /request\.slot, request\.origin = tonumber\(data\.slot\), "module_settings"/);
 for (const name of ['status', 'diagnostics']) {
  const template = fs.readFileSync(path.join(base, 'view/myos_c8_modem', name + '.htm'), 'utf8');
  assert.match(template, /data-status-url=/); assert.doesNotMatch(template, /<form|method="post"|at_command/);
 }
});
test('settings retain saved UCI choices when cache is unavailable or stale', () => {
 const base = path.join(__dirname, 'native-overlay/usr/lib/lua/luci');
 const cbi = fs.readFileSync(path.join(base, 'model/cbi/myos_c8_modem.lua'), 'utf8');
 const intro = fs.readFileSync(path.join(base, 'view/myos_c8_modem/settings_intro.htm'), 'utf8');
 assert.match(cbi, /if not fresh then/);
 assert.match(cbi, /require\("luci\.model\.uci"\)\.cursor\(\)/);
 for (const name of ['mobility', 'mode', 'custom_freq', 'freq']) assert.match(cbi, new RegExp('uci:get\\("cpecfg", section, "' + name + '"\\)'));
 assert.match(cbi, /local physical = fresh and/); assert.match(cbi, /slot\.default = physical or settings\.slot/);
 assert.doesNotMatch(cbi, /uci:(set|save|commit|delete)\(/);
 for (const state of ['missing', 'read_error', 'invalid', 'stale']) assert.match(intro, new RegExp(state + ' ='));
});
test('controller rejects future samples and negative source age', () => {
 const controller = fs.readFileSync(path.join(__dirname, 'native-overlay/usr/lib/lua/luci/controller/modem.lua'), 'utf8');
 assert.match(controller, /sampled > now/); assert.match(controller, /source_age < 0/);
 assert.match(controller, /data\.age = now - sampled/); assert.doesNotMatch(controller, /math\.max\(0/);
 assert.match(controller, /missing_age and data\.stale == true/);
 assert.match(controller, /data\.stale = .*not source_age or/);
});
