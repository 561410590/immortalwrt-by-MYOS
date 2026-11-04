#!/usr/bin/env python3
"""Verify the built NB68 userspace and syntax of both theme runtimes."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

source = Path(__file__).resolve().parents[4]
assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
root = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
ucode = assets / 'ucode-host/ucode'
luac = source / 'staging_dir/hostpkg/bin/luac'
checks, failures = {}, []


def checked(command, path):
    result = subprocess.run([str(v) for v in command], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        failures.append({'file': str(path), 'error': result.stderr.decode(errors='replace')[-4000:]})


ucode_files = list((root / 'usr/share/ucode/luci').rglob('*.uc'))
ucode_files += list((root / 'usr/share/ucode/luci').rglob('*.ut'))
ucode_files += [root / 'usr/share/rpcd/ucode/c2000max-ui', root / 'usr/share/rpcd/ucode/myos-update']
for path in ucode_files:
    text = path.read_text()
    imports = set(re.findall(r'\bfrom\s+[\'"]([^\'"]+)[\'"]', text))
    options = '-c' + ''.join(',dynlink=' + name for name in sorted(imports))
    command = [ucode, '-L', root / 'usr/share/ucode', '-L', root / 'usr/lib/ucode',
               '-L', ucode.parent, options, '-o', assets / 'syntax-check.uc']
    if path.suffix == '.ut' or text.lstrip().startswith('{%'):
        checked(command + ['-T', path], path)
    else:
        # Export statements are valid when this source is imported as a module.
        # RPC scripts return an object and are compiled directly as scripts.
        if re.search(r'^export\b', text, re.M):
            checked(command + ['-e', 'import * as checked_module from ' + json.dumps(str(path)) + ';'], path)
        else:
            checked(command + [path], path)
checks['ucode_files'] = len(ucode_files)

lua_files = []
shell_files = []
for theme in (assets / 'themes').iterdir():
    if not theme.is_dir():
        continue
    for path in (theme / 'root').rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(theme / 'root')
        installed = root / relative
        if path.suffix == '.lua':
            lua_files.append(installed)
        elif path.read_bytes()[:2] == b'#!' and b'sh' in path.read_bytes().split(b'\n', 1)[0]:
            shell_files.append(installed)
lua_files.append(root / 'sbin/mtwifi_cfg')
for path in lua_files:
    checked([luac, '-p', path], path)
for relative in ('etc/board.d/02_network', 'lib/upgrade/platform.sh',
                 'lib/preinit/91_nbcpe_extract_caldata', 'etc/uci-defaults/99-myos-nbcpe-defaults',
                 'lib/netifd/wireless/mtwifi.sh', 'etc/init.d/modeminit-AK68',
                 'usr/share/modem-AK68/netmodeled-AK68.sh', 'usr/libexec/taskd',
                 'etc/init.d/tasks', 'etc/init.d/dockerd'):
    shell_files.append(root / relative)
    if (root / relative).read_bytes().startswith(b'#!'):
        assert (root / relative).stat().st_mode & 0o111, relative
for path in shell_files:
    command = ['bash'] if b'bash' in path.read_bytes().split(b'\n', 1)[0] else ['busybox', 'ash']
    checked(command + ['-n', path], path)
checks['lua_files'] = len(lua_files)
checks['shell_files'] = len(shell_files)

assert sorted(p.name for p in (root / 'lib/modules').iterdir() if p.is_dir()) == ['6.6.110']
assert '5.4.255' not in (root / 'usr/lib/opkg/status').read_text()
assert (root / 'lib/modules/6.6.110/modules.dep').is_file()
assert (root / 'usr/share/ucode/luci/template/themes/c2000max-ui/header.ut').is_file()
assert (root / 'usr/lib/lua/luci/view/themes/argon/header.htm').is_file()
assert (root / 'usr/lib/lua/luci/controller/modem-AK68.lua').is_file()
assert (root / 'usr/libexec/myos-nbcpe-status').stat().st_mode & 0o111
for view in ('front', 'back'):
    present = list((root / 'www').rglob('*nbcpe*' + view + '.jpg'))
    assert len(present) >= 2, view
    assert len({hashlib.sha256(p.read_bytes()).hexdigest() for p in present}) == 1
checks['themes_and_modem_assets'] = True
checks['kernel_abi_and_module_tree'] = True
assert (root / 'etc/myos-version').read_text().strip() == '2.1.32-202610080100'
installed = set(re.findall(r'^Package: (.+)$', (root / 'usr/lib/opkg/status').read_text(), re.M))
expected_apps = {'luci-app-store', 'luci-app-dockerman', 'luci-app-ModemATSD',
                 'luci-app-mtwifi-cfg', 'luci-app-firewall'}
assert {name for name in installed if name.startswith('luci-app-')} == expected_apps
assert not any('quickstart' in name.lower() or 'istorex' in name.lower() for name in installed)
assert not (root / 'etc/config/quickstart').exists()
assert not list((root / 'etc/rc.d').glob('*quickstart*'))
for relative in ('usr/bin/docker', 'usr/bin/dockerd', 'usr/bin/containerd', 'usr/sbin/runc',
                 'usr/lib/lua/luci/controller/store.lua', 'etc/init.d/dockerd'):
    assert (root / relative).is_file(), relative
checks['version_and_applications'] = True
report = {'checks': checks, 'failures': failures}
(assets / 'rootfs-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
raise SystemExit(1 if failures else 0)
