#!/usr/bin/env python3
"""Verify the built NB68 userspace and syntax of both theme runtimes."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

source = Path(__file__).resolve().parents[5]
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
ucode_files += [root / 'usr/share/rpcd/ucode/c2000max-ui',
                root / 'usr/share/rpcd/ucode/luci.vantage', root / 'www/cgi-bin/luci.ucode']
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
for directory in (assets / 'app-assets/root', assets / 'overlay', assets / 'official-assets/root'):
    for path in directory.rglob('*'):
        if not path.is_file():
            continue
        installed = root / path.relative_to(directory)
        first_line = path.read_bytes().split(b'\n', 1)[0]
        if path.suffix == '.lua' or first_line.startswith(b'#!') and b'lua' in first_line:
            lua_files.append(installed)
        elif first_line.startswith(b'#!') and b'sh' in first_line:
            shell_files.append(installed)
for path in lua_files:
    checked([luac, '-p', path], path)
for relative in ('etc/board.d/02_network', 'lib/upgrade/platform.sh',
                 'lib/preinit/91_nbcpe_extract_caldata', 'etc/uci-defaults/99-myos-nbcpe-defaults',
                 'lib/netifd/wireless/mtwifi.sh', 'etc/init.d/modeminit-AK68',
                 'usr/share/modem-AK68/netmodeled-AK68.sh', 'usr/libexec/taskd',
                 'etc/init.d/tasks', 'etc/uci-defaults/98-nbcpe-network',
                 'usr/bin/BLUE4WAN-AK68.sh', 'usr/libexec/myos-nbcpe-cache',
                 'etc/init.d/myos-nbcpe-cache'):
    shell_files.append(root / relative)
    if relative.startswith('etc/board.d/') or (root / relative).read_bytes().startswith(b'#!'):
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
assert (root / 'etc/myos-version').read_text().strip() == '3.1.34-202610092000'
installed = set(re.findall(r'^Package: (.+)$', (root / 'usr/lib/opkg/status').read_text(), re.M))
expected_apps = {'luci-app-store', 'luci-app-ModemATSD',
                 'luci-app-mtwifi-cfg', 'luci-app-firewall', 'luci-app-package-manager',
                 'luci-app-vantage'}
assert {'luci-theme-vantage', 'luci-app-vantage', 'luci-i18n-vantage-zh-cn'} <= installed
for relative in ('usr/share/ucode/luci/template/themes/vantage/header.ut',
                 'usr/share/ucode/luci/template/themes/vantage/footer.ut',
                 'usr/share/ucode/luci/template/themes/vantage/sysauth.ut',
                 'usr/share/rpcd/ucode/luci.vantage',
                 'www/luci-static/vantage/vantage.css',
                 'www/luci-static/resources/menu-vantage.js',
                 'www/luci-static/resources/view/vantage/overview.js',
                 'usr/lib/lua/luci/i18n/vantage.zh-cn.lmo', 'etc/config/vantage'):
    assert (root / relative).is_file(), relative
for relative in ('usr/share/luci/menu.d/luci-app-vantage.json',
                 'usr/share/rpcd/acl.d/luci-app-vantage.json'):
    assert isinstance(json.loads((root / relative).read_text()), dict), relative
theme_registration = (root / 'etc/uci-defaults/30_luci-theme-vantage').read_text()
assert 'luci.themes.Vantage' in theme_registration and '/luci-static/vantage' in theme_registration
assert not re.search(r'\bset\s+[\'"]?luci\.main\.mediaurlbase\s*=', theme_registration)
checks['vantage_theme_dashboard_and_translation'] = True
assert {name for name in installed if name.startswith('luci-app-')} == expected_apps
assert not any('quickstart' in name.lower() or 'istorex' in name.lower() for name in installed)
assert not (root / 'etc/config/quickstart').exists()
assert not list((root / 'etc/rc.d').glob('*quickstart*'))
for relative in ('usr/lib/lua/luci/controller/store.lua',):
    assert (root / relative).is_file(), relative
assert not {'docker', 'dockerd', 'docker-compose', 'containerd', 'runc', 'luci-app-dockerman'} & installed
assert not {'gcc', 'python3-dev', 'python3-pip', 'python3-setuptools'} & installed
for name in ('bitarray/_bitarray.so', 'bitarray/_util.so', 'bitstring/__init__.py', 'smspdudecoder/__init__.py'):
    assert (root / 'usr/lib/python3.9/site-packages' / name).is_file(), name
assert not (root / 'usr/libexec/myos-usage').exists()
assert not (root / 'usr/libexec/myos-update').exists()
for relative in ('usr/sbin/myos-nradio-app-fast',
                 'usr/sbin/myos-nradio-discovery', 'usr/sbin/myos-nros-app-agent',
                 'usr/sbin/myos-nros-cloudctl', 'etc/init.d/myos-nros-app'):
    assert (root / relative).is_file() and (root / relative).stat().st_mode & 0o111, relative
assert (root / 'etc/rc.d/S32myos-nros-app').is_symlink()
assert (root / 'etc/rc.d/S99myos-nbcpe-cache').is_symlink()
assert (root / 'etc/rc.d/K15myos-nbcpe-cache').is_symlink()
assert (root / 'etc/rc.d/S99myos-nbcpe-leds').is_symlink()
assert (root / 'etc/rc.d/K15myos-nbcpe-leds').is_symlink()
assert (root / 'etc/rc.d/S99myos-nbcpe-native').is_symlink()
assert (root / 'etc/rc.d/K15myos-nbcpe-native').is_symlink()
for relative in ('usr/bin/httpapi-AK68.py', 'usr/bin/smstrun-AK68.py',
                 'usr/libexec/myos-nbcpe-sms-forward'):
    assert (root / relative).is_file(), relative
for relative in ('lib/functions/nbcpe.sh',
                 'usr/lib/lua/luci/model/cbi/modem5700-AK68.lua',
                 'usr/lib/lua/luci/model/cbi/modem-AK68.lua'):
    assert (root / relative).is_file(), relative
cgi = (root / 'www/cgi-bin/luci.ucode').read_text()
assert re.match(r'^#!/usr/bin/(?:env )?ucode(?:\s|$)', cgi)
assert "import dispatch from 'luci.dispatcher';" in cgi
assert (root / 'www/cgi-bin/luci.ucode').stat().st_mode & 0o111
assert (root / 'usr/share/myos-update/ca.pem').is_file()
checks['phone_app_and_server_reporting'] = True
checks['version_and_applications'] = True
official = json.loads((assets / 'official-assets/install-manifest.json').read_text())
for relative in official['exec_files']:
    path = root / relative.lstrip('/')
    assert path.is_file() and path.stat().st_mode & 0o111, relative
for relative in official['shared_libraries']:
    assert (root / relative.lstrip('/')).is_file(), relative
for name, target in official['rc_links'].items():
    link = root / 'etc/rc.d' / name
    assert link.is_symlink() and str(link.readlink()) == target, name
for link in (root / 'etc/rc.d').iterdir():
    assert not (link.is_symlink() and link.readlink().name in official['disabled_init']), str(link)
checks['official_odu_and_single_at_transport'] = True
for relative in ('bin/bash', 'usr/sbin/dropbear', 'usr/lib/sftp-server',
                 'usr/libexec/myos-boot-system', 'usr/libexec/myos-ubenv',
                 'usr/bin/ucode', 'usr/lib/ucode/fs.so', 'usr/sbin/blkid'):
    path = root / relative
    assert path.is_file() and path.stat().st_mode & 0o111, relative
for relative, target in (('usr/bin/ssh', '/usr/sbin/dropbear'),
                         ('usr/bin/scp', '/usr/sbin/dropbear'),
                         ('usr/libexec/sftp-server', '../lib/sftp-server')):
    path = root / relative
    assert path.is_symlink() and str(path.readlink()) == target, relative
assert (root / 'usr/libexec/myos-boot-system').read_text().startswith('#!/usr/bin/ucode\n')
checks['ssh_bash_and_system_switch_files'] = True
report = {'checks': checks, 'failures': failures}
(assets / 'rootfs-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
raise SystemExit(1 if failures else 0)
