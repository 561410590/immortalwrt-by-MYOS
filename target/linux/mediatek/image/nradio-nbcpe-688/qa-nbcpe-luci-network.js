#!/usr/bin/env node
'use strict';

// Execute the installed LuCI firewall/network code and the NBCPE modules.
// RPC and UCI writes are blocked; only fixture interface dumps are returned.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const argv = process.argv.slice(2);
if (argv.includes('--help')) {
  console.log('Usage: node qa-nbcpe-luci-network.js --root ROOT [--overlay OVERLAY]');
  process.exit(0);
}
const value = name => { const i = argv.indexOf(name); return i < 0 ? null : argv[i + 1]; };
const root = value('--root');
assert(root, '--root required');
const overlay = value('--overlay') || root;
const resource = (base, name) => path.join(base, 'www/luci-static/resources', name + '.js');

function extend(methods) {
  const Parent = typeof this === 'function' ? this : function() {};
  function Class(...args) { if (this.__init__) this.__init__(...args); }
  Class.prototype = Object.create(Parent.prototype);
  Object.assign(Class.prototype, methods);
  Class.extend = extend;
  return Class;
}
function fixture() {
  const data = {
    network: {
      globals: { '.type': 'globals', '.name': 'globals', nbcpe: '1' },
      cpe: { '.type': 'interface', '.name': 'cpe', proto: 'wwan', mode: 'odu',
        device: 'port5', phyifname: 'port5', odu_model: 'MT5700M-CN', auto_sniffer: '192.168.8.1' }
    },
    firewall: {
      lan: { '.type': 'zone', '.name': 'lan', name: 'lan', network: ['lan'] },
      wan: { '.type': 'zone', '.name': 'wan', name: 'wan', network: ['wan', 'wan6', 'cpe'] }
    }
  };
  let dumps = 0, interfaces = [], rejectDump = false;
  const forbidden = () => { throw Error('UCI write or unexpected RPC requested'); };
  const uci = {
    get: (c, s, o) => o == null ? data[c]?.[s] : data[c]?.[s]?.[o],
    sections: (c, t) => Object.values(data[c] || {}).filter(s => !t || s['.type'] === t),
    load: () => Promise.resolve(), set: forbidden, unset: forbidden, add: forbidden,
    remove: forbidden, save: forbidden, apply: forbidden
  };
  // The real core declares other RPCs but must not call them in this fixture.
  const rpc = { declare: spec => (...args) => {
    if (spec.object === 'network.interface' && spec.method === 'dump') {
      assert.equal(args.length, 0); dumps++;
      return rejectDump ? Promise.reject(Error('fixture RPC unavailable')) : Promise.resolve(interfaces);
    }
    return forbidden();
  } };
  const L = { Class: { extend }, isObject: x => x !== null && typeof x === 'object',
    toArray: x => Array.isArray(x) ? x : x == null ? [] : String(x).split(/\s+/),
    resolveDefault: (p, fallback) => Promise.resolve(p).catch(() => fallback),
    bind: (fn, self) => fn.bind(self) };
  const context = vm.createContext({ L, uci, rpc, baseclass: { extend }, _: s => s,
    form: { DummyValue: {} }, validation: {}, random: { derive_color: () => '#123456' },
    console, Promise });
  function load(base, name) {
    const result = vm.runInContext('(function(){\n' + fs.readFileSync(resource(base, name), 'utf8') + '\n})()', context,
      { filename: resource(base, name) });
    return typeof result === 'function' ? new result() : result;
  }
  context.firewall = load(root, 'firewall');
  context.network = load(root, 'network');
  context.nbcpe = load(overlay, 'tools/nbcpe-network');
  load(overlay, 'protocol/wwan');
  return { context, data, setInterfaces: v => { interfaces = v; },
    setReject: v => { rejectDump = v; }, dumps: () => dumps };
}

(async function() {
  const f = fixture(), { firewall, network, nbcpe } = f.context;
  const proto = network.getProtocol('wwan', 'cpe');
  assert(proto); assert.equal(proto.getProtocol(), 'wwan');
  assert.equal(proto.getI18n(), 'Cellular (WWAN / ODU)');
  assert.equal(proto.isFloating(), false); assert.equal(proto.isVirtual(), false);
  const options = [];
  proto.renderFormOptions({ taboption: (tab, type, name, title) => {
    const o = { tab, type, name, title }; options.push(o); return o;
  } });
  assert.deepEqual(options.map(o => o.cfgvalue('cpe')), ['port5', 'odu', 'MT5700M-CN', '192.168.8.1']);
  const before = JSON.stringify(f.data);
  options.forEach(o => { o.write('cpe', 'bad'); o.remove('cpe'); });
  assert.equal(JSON.stringify(f.data), before);

  f.setInterfaces([
    { interface: 'cpe_4', dynamic: true, proto: 'dhcp', data: { zone: 'wan' } },
    { interface: 'cpe_6', dynamic: true, proto: 'dhcpv6', data: { zone: 'wan' } }
  ]);
  assert.equal((await firewall.getZoneByNetwork('cpe')).getName(), 'wan');
  assert.equal(f.dumps(), 0);
  const originalWrapper = firewall.getZoneByNetwork; nbcpe.install();
  assert.equal(firewall.getZoneByNetwork, originalWrapper);
  const zones = await Promise.all(['cpe_4', 'cpe_6'].map(n => firewall.getZoneByNetwork(n)));
  assert.deepEqual(zones.map(z => z.getName()), ['wan', 'wan']); assert.equal(f.dumps(), 1);

  f.data.firewall.lan.network.push('cpe_4');
  assert.equal((await firewall.getZoneByNetwork('cpe_4')).getName(), 'lan');
  assert.equal(f.dumps(), 1); f.data.firewall.lan.network.pop();
  f.setInterfaces([{ interface: 'cpe_4', dynamic: true, data: { zone: 'missing' } }]);
  assert.equal(await firewall.getZoneByNetwork('cpe_4'), null);
  f.setInterfaces([{ interface: 'cpe_4', dynamic: false, data: { zone: 'wan' } }]);
  assert.equal(await firewall.getZoneByNetwork('cpe_4'), null);
  f.setInterfaces([{ interface: 'cpe_4', dynamic: true, data: {} }]);
  assert.equal(await firewall.getZoneByNetwork('cpe_4'), null);
  f.setReject(true); assert.equal(await firewall.getZoneByNetwork('cpe_4'), null); f.setReject(false);
  const count = f.dumps();
  assert.equal(await firewall.getZoneByNetwork('other_4'), null);
  f.data.network.cpe.mode = 'usb'; assert.equal(await firewall.getZoneByNetwork('cpe_4'), null);
  f.data.network.cpe.mode = 'odu'; f.data.network.globals.nbcpe = '0';
  assert.equal(await firewall.getZoneByNetwork('cpe_4'), null); assert.equal(f.dumps(), count);
  console.log(JSON.stringify({ result: 'PASS', protocol: 'wwan', readonly_odu_fields: options.length,
    dynamic_zones: zones.map(z => z.getName()), static_precedence: true, missing_zone: null,
    rpc_failure: null, scoped_to_nbcpe_odu: true, fixture: 'installed LuCI firewall/network + NBCPE modules; RPC fixtures, UCI writes blocked' }));
})().catch(error => { console.error(error.stack); process.exitCode = 1; });
