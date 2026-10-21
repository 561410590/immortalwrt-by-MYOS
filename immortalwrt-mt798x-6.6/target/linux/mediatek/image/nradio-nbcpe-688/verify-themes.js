'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const cp = require('child_process');
const assert = require('assert');
const root = path.join(__dirname, 'themes');
const read = p => fs.readFileSync(path.join(root, p), 'utf8');
function walk(dir) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(e => {
    const p = path.join(dir, e.name);
    return e.isDirectory() ? walk(p) : [p];
  });
}
let js = 0, json = 0, inline = 0, shell = 0;
for (const file of walk(root)) {
  if (file.endsWith('.js')) {
    new vm.Script('(function(){\n' + fs.readFileSync(file, 'utf8') + '\n})', { filename: file });
    js++;
  } else if (file.endsWith('.json')) {
    JSON.parse(fs.readFileSync(file, 'utf8')); json++;
  } else if (file.endsWith('.htm')) {
    const source = fs.readFileSync(file, 'utf8').replace(/<%=[\s\S]*?%>/g, 'null').replace(/<%[\s\S]*?%>/g, '');
    for (const match of source.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/gi)) {
      const script = match[1].replace(/<%=[\s\S]*?%>/g, 'null').replace(/<%[\s\S]*?%>/g, '');
      new vm.Script(script, { filename: file + ':inline-' + (++inline) });
    }
  }
}
const myos = 'luci-theme-c2000max-ui/';
const mwrt = 'luci-theme-mwrt/';
const menu = JSON.parse(read(myos + 'root/usr/share/luci/menu.d/luci-theme-c2000max-ui.json'));
assert.equal(menu['admin/status'].action.preferred, 'nbcpe');
assert(!menu['admin/status/nbcpe']);
const rpc = read(myos + 'root/usr/share/rpcd/ucode/c2000max-ui');
assert(rpc.includes('HCMT7981-emmc-NB68'));
assert(rpc.includes('/usr/libexec/myos-nbcpe-status'));
assert(!rpc.includes('infocd') && !rpc.includes('cpesel'));
const home = read(myos + 'htdocs/luci-static/resources/view/c2000max/home-nbcpe-20261009.js');
assert(home.includes("'modem-AK68','get_csq'"));
assert(!home.includes("object:'qmodem'"));
assert(home.includes('Date.now()-Math.max(0,...ages)*1000'));
assert(!fs.existsSync(path.join(root, myos + 'root/usr/libexec/myos-update')));
assert(!fs.existsSync(path.join(root, myos + 'root/usr/libexec/myos-usage')));
for (const prefix of [myos + 'htdocs/luci-static/c2000max-ui/assets/', mwrt + 'root/www/luci-static/router3d/']) {
  for (const name of ['nbcpe-688-front.jpg', 'nbcpe-688-back.jpg']) {
    const file = path.join(root, prefix, name);
    assert(fs.statSync(file).size > 1024);
    assert.equal(fs.readFileSync(file).readUInt16BE(0), 0xffd8);
  }
}
const bash = process.platform === 'win32' ? 'C:/Program Files/Git/bin/bash.exe' : '/bin/bash';
for (const relative of [
  myos + 'root/etc/uci-defaults/95-c2000max-ui',
  mwrt + 'root/etc/uci-defaults/96-mwrt-theme',
  mwrt + 'root/www/cgi-bin/router3d-data'
]) {
  const result = cp.spawnSync(bash, ['-n', path.join(root, relative).replaceAll('\\', '/')], { encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr); shell++;
}
console.log(JSON.stringify({ js, json, inline, shell, contract: 'passed', ucode_lua: 'Linux compiler check required' }));
