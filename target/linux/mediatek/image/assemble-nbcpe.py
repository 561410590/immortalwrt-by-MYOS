#!/usr/bin/env python3
"""Assemble the NBCPE MYOS userspace around the native NB68 modem stack."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import os
import re
import runpy
import shutil
import subprocess
import tarfile

VERSION = '3.1.40-202611031200'
KERNEL = '6.6.110'
BOARD = 'HCMT7981-emmc-NB68'


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def install_remote_access(source, root):
    """Use current remote transport/session code, retaining model APP handlers."""
    assets = source / 'target/linux/mediatek/image/nradio-c5800-688'
    mapping = {
        'myos_nradio_app_session.lua': 'usr/lib/lua/myos_nradio_app_session.lua',
        'myos-nros-cloud-cbi.lua': 'usr/lib/lua/luci/model/cbi/myos_nros.lua',
        'myos-nros-cloud-status.htm': 'usr/lib/lua/luci/view/myos_nros/status.htm',
        'myos-nros-cloud-controller.lua': 'usr/lib/lua/luci/controller/myos_nros.lua',
    }
    for relative in ('usr/lib/lua/myos_nradio_ssh_authorization.lua',
                     'usr/share/myos-nros/id_rsa.db',
                     'etc/init.d/myos-nros-ssh',
                     'etc/uci-defaults/30-myos-nros-remote-access'):
        mapping['myos-remote-files/' + relative] = relative
    for origin, relative in mapping.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.is_file() or path.is_symlink():
            path.unlink()
        shutil.copy2(assets / origin, path)
        path.chmod(0o755 if relative.startswith(('usr/sbin/', 'etc/init.d/', 'etc/uci-defaults/')) else 0o644)
    (root / 'usr/share/myos-nros/id_rsa.db').chmod(0o400)
    config = root / 'etc/config/myos_nros'
    text = config.read_text()
    for name in ('enabled', 'remote_web_enabled', 'remote_ssh_enabled'):
        pattern = r'(?m)^\s*option\s+' + name + r'\s+[^\n]+'
        text, count = re.subn(pattern, "\toption " + name + " '1'", text)
        if not count:
            text = text.rstrip() + "\n\toption " + name + " '1'\n"
    config.write_text(text)


def read_ipk(path, data=False):
    with tarfile.open(path, 'r:*') as archive:
        names = {Path(m.name).name: m for m in archive.getmembers()}
        control = next(m for n, m in names.items() if n.startswith('control.tar'))
        with tarfile.open(fileobj=io.BytesIO(archive.extractfile(control).read()), mode='r:*') as inner:
            fields = {Path(m.name).name: (inner.extractfile(m).read(), m.mode)
                      for m in inner.getmembers() if m.isfile()}
        payload = archive.extractfile(next(m for n, m in names.items() if n.startswith('data.tar'))).read() if data else None
    record = fields['control'][0].decode()
    name = re.search(r'^Package: (.+)$', record, re.M)[1]
    return name, record, fields, payload


def records_in(text):
    result = {}
    for record in text.strip().split('\n\n'):
        match = re.search(r'^Package: (.+)$', record, re.M)
        if match:
            result[match[1]] = record
    return result


def provided_by(records):
    result = set(records)
    for record in records.values():
        match = re.search(r'^Provides: (.+)$', record, re.M)
        if match:
            result.update(x.strip().split()[0] for x in match[1].split(','))
    return result


def package_version(record):
    return re.search(r'^Version: (.+)$', record, re.M)[1]


def compare_versions(left, right):
    """Compare opkg/Debian epoch, upstream and revision components."""
    def parts(version):
        epoch, separator, rest = version.partition(':')
        epoch, version = (int(epoch), rest) if separator else (0, version)
        upstream, separator, revision = version.rpartition('-')
        return epoch, upstream if separator else version, revision if separator else '0'

    def order(char):
        if char == '~':
            return -1
        if not char:
            return 0
        return ord(char) if char.isalpha() else ord(char) + 256

    def component(a, b):
        while a or b:
            while (a and not a[0].isdigit()) or (b and not b[0].isdigit()):
                ca = a[0] if a and not a[0].isdigit() else ''
                cb = b[0] if b and not b[0].isdigit() else ''
                if order(ca) != order(cb):
                    return 1 if order(ca) > order(cb) else -1
                a, b = a[len(ca):], b[len(cb):]
            a, b = a.lstrip('0'), b.lstrip('0')
            na = re.match(r'\d*', a)[0]
            nb = re.match(r'\d*', b)[0]
            if len(na) != len(nb):
                return 1 if len(na) > len(nb) else -1
            if na != nb:
                return 1 if na > nb else -1
            a, b = a[len(na):], b[len(nb):]
        return 0

    le, lu, lr = parts(left)
    re_, ru, rr = parts(right)
    return (1 if le > re_ else -1) if le != re_ else component(lu, ru) or component(lr, rr)


def dependency_choices(expression):
    choices = []
    for choice in expression.split('|'):
        match = re.fullmatch(r'\s*([\w.+-]+)(?:\s*\((<<|>>|<=|>=|=|<|>)\s*([^)]+)\))?\s*', choice)
        if not match:
            raise RuntimeError('Unsupported dependency expression: ' + expression)
        choices.append(match.groups())
    return choices


def satisfies(record, choice):
    name, operator, version = choice
    package = re.search(r'^Package: (.+)$', record, re.M)[1]
    if name not in provided_by({package: record}):
        return False
    if not operator:
        return True
    difference = compare_versions(package_version(record), version.strip())
    return {'=': difference == 0, '<': difference < 0, '<<': difference < 0,
            '<=': difference <= 0, '>': difference > 0, '>>': difference > 0,
            '>=': difference >= 0}[operator]


def required_closure(roots, available):
    """Modern roots use only matching current IPKs, including virtual providers.

    Retained native packages are deliberately not candidates here. A stock
    package with the same name does not establish the ABI of a new dependency.
    """
    selected, missing, pending = set(), set(), list(sorted(roots))
    while pending:
        expression = pending.pop()
        choices = dependency_choices(expression)
        already = next((name for name in sorted(selected)
                        if any(satisfies(available[name][1], choice) for choice in choices)), None)
        if already:
            continue
        candidates = []
        for choice in choices:
            if choice[0] in available:
                candidates.append(choice[0])
            candidates.extend(name for name in sorted(available)
                              if name != choice[0] and satisfies(available[name][1], choice))
        name = next((name for name in candidates
                     if any(satisfies(available[name][1], choice) for choice in choices)), None)
        if name is None:
            missing.add(expression)
            continue
        selected.add(name)
        match = re.search(r'^Depends: (.+)$', available[name][1], re.M)
        pending.extend(match[1].split(',') if match else [])
    return selected, sorted(missing)


def restore_package_alternatives(root, records):
    """Apply offline IPK alternatives without executing package postinst scripts."""
    choices = {}
    for record in records.values():
        match = re.search(r'^Alternatives: (.+)$', record, re.M)
        for entry in match[1].split(',') if match else []:
            priority, link, target = entry.strip().split(':', 2)
            if link not in choices or int(priority) > choices[link][0]:
                choices[link] = (int(priority), target)
    for relative, (_, target) in choices.items():
        destination = root / target.lstrip('/')
        if not destination.is_file():
            raise RuntimeError('Missing alternative target: ' + target)
        link = root / relative.lstrip('/')
        link.parent.mkdir(parents=True, exist_ok=True)
        if link.is_symlink() or link.exists():
            link.unlink()
        link.symlink_to(target)


def restore_busybox_strings(root):
    """Keep board detection working after removing GNU binutils."""
    assert (root / 'bin/busybox').is_file(), 'BusyBox runtime required'
    link = root / 'usr/bin/strings'
    link.parent.mkdir(parents=True, exist_ok=True)
    if link.is_symlink() or link.exists():
        link.unlink()
    link.symlink_to('/bin/busybox')


def configured_feeds(source):
    feeds = source / 'staging_dir/target-aarch64_cortex-a53_musl/root-mediatek/etc/opkg/distfeeds.conf'
    config = (source / '.config').read_text()
    repository = re.search(r'^CONFIG_VERSION_REPO="([^"]+)"$', config, re.M)[1].rstrip('/')
    lines = []
    for line in feeds.read_text().splitlines():
        if line.startswith('src/gz '):
            assert line.split()[2].startswith(repository + '/'), 'Feed differs from source repository'
            if '/targets/' in line or '/kmods/' in line:
                line = '# NBCPE custom ' + KERNEL + ' ABI; install matching build IPKs only.\n# ' + line
            elif line.split()[2].rstrip('/').rsplit('/', 1)[1] not in {'base', 'luci', 'packages', 'routing', 'telephony'}:
                line = '# Source-only feed has no public repository at this URL.\n# ' + line
        lines.append(line)
    return '\n'.join(lines) + '\n'


def configured_opkg(source):
    config = (source / 'package/system/opkg/files/opkg.conf').read_text().rstrip() + '\n'
    # The opkg Makefile appends this option to its installed config. Preserve
    # the same signature policy when regenerating the source defaults here.
    if not re.search(r'^option\s+check_signature\s*$', config, re.M):
        config += 'option check_signature\n'
    return config


def reconcile_native_filelists(root, records, modern_names):
    """Remove stale stock ownership of files now supplied by current IPKs.

    Old monolithic libxtables/libip4tc packages listed modern split libiptext
    libraries. Leaving those paths behind lets a later opkg upgrade delete or
    reject a library even when its current package is already installed.
    """
    modern_owned = {}
    info = root / 'usr/lib/opkg/info'
    for name in sorted(modern_names):
        manifest = info / (name + '.list')
        for path in manifest.read_text().splitlines() if manifest.is_file() else []:
            relative = path.lstrip('/').rstrip('/')
            target = root / relative
            if target.is_file() or target.is_symlink():
                modern_owned[relative] = name
        match = re.search(r'^Alternatives: (.+)$', records[name], re.M)
        for alternative in match[1].split(',') if match else []:
            _, link, _ = alternative.strip().split(':', 2)
            modern_owned[link.lstrip('/')] = name
    corrections = {}
    for name in sorted(set(records) - modern_names):
        manifest = info / (name + '.list')
        if not manifest.is_file():
            continue
        paths = manifest.read_text().splitlines()
        stale = [path for path in paths if path.lstrip('/').rstrip('/') in modern_owned]
        if stale:
            corrections[name] = {path: modern_owned[path.lstrip('/').rstrip('/')] for path in stale}
            manifest.write_text('\n'.join(path for path in paths if path not in stale) + '\n')
    return corrections


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path)
    parser.add_argument('--inspect', action='store_true')
    parser.add_argument('--reconcile-filelists', action='store_true',
                        help='Reconcile ownership metadata in an already assembled current root')
    parser.add_argument('--pack', action='store_true', help='Create squashfs after assembly')
    args = parser.parse_args()
    source = args.source or Path(__file__).resolve().parents[4]
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    official_assets = assets / 'official-assets'
    official_manifest = json.loads((official_assets / 'install-manifest.json').read_text())
    kdir = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic'
    work = kdir / 'root-nbcpe-myos'
    host = source / 'staging_dir/host/bin'
    if not work.exists():
        run(host / 'unsquashfs4', '-no-progress', '-no-xattrs', '-excludes', '-d', work,
            assets / 'root.squashfs', 'dev/console', 'lib/modules')
    assert (work / 'etc/init.d/modeminit-AK68').is_file(), 'NBCPE stock rootfs required'
    assert (kdir / ('linux-' + KERNEL) / 'vmlinux').is_file(), 'Current 6.6 kernel build required'

    def write(relative, value, mode=0o644):
        path = work / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.is_symlink():
            path.unlink()
        path.write_text(value)
        path.chmod(mode)

    def copy(src, dst):
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.is_symlink():
            dst.unlink()
        shutil.copy2(src, dst)

    status = work / 'usr/lib/opkg/status'
    records = records_in(status.read_text())
    if args.reconcile_filelists:
        manifest = assets / 'build-manifest.json'
        plan = json.loads(manifest.read_text())
        assert plan['version'] == VERSION, 'Only the current assembled NBCPE root may be reconciled'
        corrected = reconcile_native_filelists(work, records, set(plan['required_packages']))
        plan['native_filelist_ownership_corrections'] = corrected
        manifest.write_text(json.dumps(plan, indent=2) + '\n')
        print(json.dumps(corrected, indent=2))
        return
    available = {}
    for directory in (source / 'bin/targets/mediatek/filogic/packages',
                      source / 'bin/packages/aarch64_cortex-a53', assets / 'vantage/packages'):
        for path in directory.rglob('*.ipk'):
            name, record, fields, _ = read_ipk(path)
            if name == 'kernel' or name.startswith('kmod-'):
                if not re.search(r'(^Version: 6\.6\.110[~+.\-]|kernel \(= 6\.6\.110)', record, re.M):
                    continue
            if name not in available or path.stat().st_mtime > available[name][0].stat().st_mtime:
                available[name] = (path, record, fields)

    # Keep native AK68 applications; replace the kernel ABI and the LuCI runtime.
    selected = set(re.findall(r'^CONFIG_PACKAGE_([^=]+)=y$', (source / '.config').read_text(), re.M))
    required = {'kernel'} | {n for n in selected if n.startswith('kmod-')}
    required |= {
        'libc', 'libgcc1', 'luci-base', 'luci-lib-base', 'luci-compat', 'luci-mod-admin-full',
        'luci-mod-network', 'luci-mod-system', 'luci-mod-status', 'luci-lib-jsonc',
        'luci-lib-nixio', 'luci-lib-ip', 'luci-theme-bootstrap', 'liblucihttp0',
        'rpcd', 'rpcd-mod-file', 'rpcd-mod-luci', 'rpcd-mod-iwinfo', 'rpcd-mod-ucode',
        'ucode', 'ucode-mod-fs', 'ucode-mod-ubus', 'ucode-mod-uci',
        'uci', 'libuci-lua', 'ubus', 'libubus-lua', 'netifd', 'procd', 'procd-ujail', 'ubox',
        'uhttpd', 'uhttpd-mod-ubus', 'firewall4', 'nftables-json',
        'luci-app-firewall', 'luci-app-store', 'luci-app-package-manager', 'opkg', 'openwrt-keyring',
        'iptables-nft', 'ip6tables-nft', 'tc-full',
        'mtwifi-cfg', 'luci-app-mtwifi-cfg', 'jq', 'bc', 'fstools', 'block-mount',
        'uclient-fetch', 'ca-bundle', 'curl', 'libcurl4', 'libnghttp2-14', 'mkf2fs', 'f2fsck',
        'luci-theme-myos-new', 'luci-app-myos-system',
        'lua-cjson', 'lua-md5', 'lua-mosquitto', 'libmosquitto-nossl', 'mosquitto-nossl',
        'libcares', 'luci-lua-runtime',
    }
    removed = {
        'firewall', 'luci-theme-argon', 'quickstart', 'luci-app-quickstart',
        'luci-theme-c2000max-ui', 'luci-theme-mwrt',
        'luci-theme-vantage', 'luci-app-vantage', 'luci-i18n-vantage-zh-cn',
        'luci-app-istorex', 'luci-app-c5800-688-wtmodem', 'luci-app-5700webui',
        'docker', 'dockerd', 'docker-compose', 'containerd', 'runc',
        'luci-app-dockerman', 'luci-lib-docker', 'tini', 'libseccomp', 'btrfs-progs',
        'ttyd', 'libwebsockets-full', 'cgroupfs-mount',
        'gcc', 'make', 'python3-dev', 'python3-pip', 'python3-setuptools',
        'python-pip-conf', 'binutils', 'objdump', 'ar', 'libopcodes', 'libctf', 'libbfd',
    }
    kept_apps = {'luci-app-store', 'luci-app-ModemATSD', 'luci-app-package-manager',
                 'luci-app-mtwifi-cfg', 'luci-app-firewall', 'luci-app-myos-system'}
    removed |= {name for name in records
                if name.startswith('luci-app-') and name not in kept_apps}
    translated_apps = {'base', 'store', 'ModemATSD', 'mtwifi-cfg', 'firewall', 'package-manager', 'vantage'}
    removed |= {name for name in records if name.startswith('luci-i18n-')
                and not any(name.startswith('luci-i18n-' + app + '-')
                            for app in translated_apps)}
    optional_daemons = re.compile(
        r'^(alist|bind-server|collectd(?:-.*)?|dawn|ddnsto|filebrowser|etherwake|'
        r'kcptun(?:-.*)?|microsocks|miniupnpd|mwan3|nlbwmon|qos-scripts|'
        r'samba4(?:-.*)?|smartd|smartmontools|strongswan(?:-.*)?|uugamebooster|'
        r'vsftpd|wsdd2|xl2tpd|xray-core|shadowsocks(?:r)?-.*|simple-obfs-client|'
        r'hysteria|dns2socks|dns2tcp|chinadns-ng|ipt2socks|pdnsd-alt|tcping|'
        r'vlmcsd|avahi-dbus-daemon|dbus|openvpn(?:-.*)?|libdevmapper|libnetwork|'
        r'iptables-zz-legacy|ip6tables-zz-legacy)$')
    removed |= {name for name in records if optional_daemons.fullmatch(name)}
    removed |= {
        'luci', 'zram-swap', 'ebtables', '6in4', 'ppp-mod-pppol2tp', 'proto-bonding',
        'blockd', 'ip6tables-extra', 'libfuse1', 'libopenssl-devcrypto',
        'libavahi-client', 'libavahi-dbus-support',
        'luci-proto-bonding', 'fuse-utils', 'libgnutls', 'ipv6helper',
        'luci-lib-fs', 'tc-tiny', 'tc-mod-iptables',
    }
    removed |= {name for name in records if name.startswith('iptables-mod-')
                and name not in required}
    old_kernel = {n for n in records if n == 'kernel' or n.startswith('kmod-')}
    required |= set(official_manifest['required_packages'])
    required_roots = sorted(required)
    required, missing = required_closure(required, available)
    kernel_abi = re.search(r'^Version: (.+)$', available['kernel'][1], re.M)[1]
    abi_mismatches = {}
    for name in required & available.keys():
        if not name.startswith('kmod-'):
            continue
        dependency = re.search(r'kernel \(= ([^)]+)\)', available[name][1])
        if dependency and dependency[1] != kernel_abi:
            abi_mismatches[name] = dependency[1]
    plan = {'version': VERSION, 'kernel': KERNEL, 'board': BOARD,
            'required_packages': sorted(required), 'missing_packages': missing,
            'remove_packages': sorted(removed), 'native_packages': len(records),
            'kernel_abi': kernel_abi, 'abi_mismatches': abi_mismatches,
            'modern_dependency_roots': required_roots,
            'dependency_policy': 'current-IPK closure; native private ABI and Python 3.9 retained'}
    final_records = {name: record for name, record in records.items()
                     if name not in old_kernel | removed}
    final_records.update({name: available[name][1] for name in required if name in available})
    dependency_gaps = {}
    for name, record in final_records.items():
        match = re.search(r'^Depends: (.+)$', record, re.M)
        for dependency in (match[1].split(',') if match else []):
            choices = dependency_choices(dependency)
            if not any(satisfies(candidate, choice)
                       for candidate in final_records.values() for choice in choices):
                dependency_gaps.setdefault(name, []).append(dependency.strip())
    plan['remaining_dependency_gaps'] = dependency_gaps
    plan['application_versions'] = {
        name: re.search(r'^Version: (.+)$', final_records[name], re.M)[1]
        for name in sorted(kept_apps)
        if name in final_records
    }
    plan['installed_package_count'] = len(final_records)
    if args.inspect:
        print(json.dumps(plan, indent=2))
        return
    if missing:
        raise RuntimeError('Missing build packages: ' + ', '.join(missing))
    if abi_mismatches:
        raise RuntimeError('Mismatched 6.6 kernel module ABI: ' + json.dumps(abi_mismatches))
    if dependency_gaps:
        raise RuntimeError('Unresolved installed-package dependencies: ' + json.dumps(dependency_gaps))
    if not (assets / 'python-runtime/usr/lib/python3.9/site-packages/bitarray/_bitarray.so').is_file():
        run('python3', assets / 'prepare-nbcpe-python.py', '--source', source)

    # Preserve calibration and board-specific control code across package overlays.
    preserved = {}
    for relative in ('lib/firmware/MT7981_iPAiLNA_EEPROM.bin', 'lib/firmware/MT7981_ePAeLNA_EEPROM.bin',
                     'etc/wireless/l1profile.dat', 'etc/wireless/mediatek/mt7981.dbdc.b0.dat',
                     'etc/wireless/mediatek/mt7981.dbdc.b1.dat', 'etc/config/firewall'):
        path = work / relative
        if path.is_file():
            preserved[relative] = (path.read_bytes(), path.stat().st_mode & 0o777)
    for name in old_kernel | removed:
        records.pop(name, None)
        manifest = work / ('usr/lib/opkg/info/' + name + '.list')
        if name in removed and manifest.exists():
            for relative in manifest.read_text().splitlines():
                relative = relative.lstrip('/')
                if name == 'firewall' and (relative.startswith('etc/config/')
                                           or relative == 'etc/firewall.user'):
                    continue
                path = work / relative
                if path.is_file() or path.is_symlink():
                    path.unlink()
        for path in (work / 'usr/lib/opkg/info').glob(name + '.*'):
            path.unlink()
    for relative in ('etc/modules.d', 'etc/modules-boot.d'):
        for path in (work / relative).glob('*'):
            if path.is_file() or path.is_symlink():
                path.unlink()
    for name in sorted(required):
        path = available[name][0]
        _, record, fields, data = read_ipk(path, data=True)
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:*') as payload:
            for member in payload.getmembers():
                clean = member.name.removeprefix('./')
                if clean.startswith('/') or '..' in Path(clean).parts:
                    raise RuntimeError('Unsafe package path: ' + member.name)
            payload.extractall(work)
            paths = ['/' + m.name.removeprefix('./').rstrip('/') for m in payload.getmembers()
                     if m.name not in ('.', './')]
        record = re.sub(r'^Status:.*\n?', '', record, flags=re.M).strip()
        records[name] = record + '\nStatus: install ok installed'
        write('usr/lib/opkg/info/' + name + '.list', '\n'.join(paths) + '\n')
        for key, (value, mode) in fields.items():
            target = work / ('usr/lib/opkg/info/' + name + '.' + key)
            target.write_bytes(value)
            target.chmod(mode)
    for relative, (data, mode) in preserved.items():
        path = work / relative
        path.write_bytes(data)
        path.chmod(mode)

    # Keep the APP protocol code in the NBCPE asset tree; shared model assets
    # remain reference-only. Theme files below supply the current service init.
    app_assets = assets / 'app-assets/root'
    assert (app_assets / 'usr/sbin/myos-nros-app-agent').is_file(), 'NBCPE APP assets required'
    copy(work / 'www/cgi-bin/luci', work / 'www/cgi-bin/luci.ucode')
    for path in app_assets.rglob('*'):
        if path.is_file():
            copy(path, work / path.relative_to(app_assets))
    for path in (assets / 'overlay').rglob('*'):
        if path.is_file():
            copy(path, work / path.relative_to(assets / 'overlay'))
    run('python3', assets / 'prepare-nbcpe-webui.py',
        '--archive', work / 'usr/share/modem-AK68/5700.tar.xz',
        '--www', work / 'www')
    # SMS decoding retains stock Python 3.9 and uses precompiled AArch64 modules.
    python_runtime = assets / 'python-runtime'
    assert (python_runtime / 'usr/lib/python3.9/site-packages/bitarray/_bitarray.so').is_file()
    for path in python_runtime.rglob('*'):
        if path.is_file():
            copy(path, work / path.relative_to(python_runtime))
    board_files = source / 'target/linux/mediatek/filogic/base-files'
    for relative in ('etc/board.d/02_network', 'lib/upgrade/platform.sh',
                     'lib/preinit/91_nbcpe_extract_caldata'):
        copy(board_files / relative, work / relative)
    # Match the modern netifd DHCP scripts, including the ipcalc shell helper.
    # Keep the existing lib/config/uci.sh and board-specific helper modules.
    copy(source / 'package/base-files/files/lib/functions.sh', work / 'lib/functions.sh')
    # The source-built procd package supplies PID1 and its matching shell API.
    # Feed configuration follows this source tree; custom-kernel modules must
    # come from the matching build, never a moving public target repository.
    write('etc/opkg/distfeeds.conf', configured_feeds(source))
    write('etc/opkg/customfeeds.conf', '# Add architecture-compatible custom feeds here.\n')
    write('etc/opkg.conf', configured_opkg(source))
    # The modern dispatcher contains the tested Lua-theme compatibility bridge.
    for relative in ('dispatcher.uc', 'template/header.ut'):
        copy(source / 'feeds/luci/modules/luci-base/ucode' / relative,
             work / 'usr/share/ucode/luci' / relative)

    # The original ODU daemons share their matching library ABI in a private
    # directory. Wrappers invoke musl directly without changing the environment
    # inherited by system uci/ubus commands.
    official_hashes = {}
    for path in (official_assets / 'root').rglob('*'):
        if path.is_file():
            relative = path.relative_to(official_assets / 'root')
            copy(path, work / relative)
            official_hashes['/' + relative.as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    for relative in official_manifest['exec_files']:
        (work / relative.lstrip('/')).chmod(0o755)
    for relative in official_manifest['shared_libraries']:
        (work / relative.lstrip('/')).chmod(0o644)
    for relative in official_manifest['required_runtime']:
        assert os.path.lexists(work / relative.lstrip('/')), 'Missing official ODU runtime: ' + relative

    # Shared eMMC dual-system controls use the original NRadio boot environment tool.
    system_switch = source / 'target/linux/mediatek/image/nradio-system-switch/root'
    for path in system_switch.rglob('*'):
        if path.is_file():
            copy(path, work / path.relative_to(system_switch))
    for relative in ('usr/libexec/myos-boot-system', 'usr/libexec/myos-ubenv'):
        (work / relative).chmod(0o755)

    description = 'MYOS by ImmortalWrt ' + VERSION
    write('etc/openwrt_release', "DISTRIB_ID='MYOS'\nDISTRIB_RELEASE='" + VERSION + "'\nDISTRIB_REVISION='r33578-c88458e977'\nDISTRIB_TARGET='mediatek/filogic'\nDISTRIB_ARCH='aarch64_cortex-a53'\nDISTRIB_DESCRIPTION='" + description + "'\nDISTRIB_TAINTS='no-all'\n")
    write('usr/lib/os-release', 'NAME="MYOS"\nVERSION="' + VERSION + '"\nID="immortalwrt"\nID_LIKE="openwrt"\nPRETTY_NAME="' + description + '"\nVERSION_ID="' + VERSION + '"\nBUILD_ID="r33578-c88458e977"\nOPENWRT_BOARD="mediatek/filogic"\nOPENWRT_ARCH="aarch64_cortex-a53"\nOPENWRT_RELEASE="' + description + '"\n')
    write('etc/banner', '\n' + description + '\nNRadio NBCPE-688 / Linux ' + KERNEL + '\n\n')
    write('etc/myos-version', VERSION + '\n')
    write('etc/openwrt_version', 'r33578-c88458e977\n')
    admin_hash = '$6$15Ns8v4UWR54WTfO$UTrY4tklhhav9UVA2j7ZoFZxnGjNgKBHwWphmHXN0grvHYalYJRRGFAU.fQ9l3.B2UB/vNfHRN.SdDMyL7NfC/'
    shadow = (work / 'etc/shadow').read_text()
    shadow, count = re.subn(r'(?m)^root:[^:]*:', 'root:' + admin_hash + ':', shadow)
    assert count == 1, 'Expected exactly one root password entry'
    write('etc/shadow', shadow, 0o600)
    write('usr/lib/opkg/status', '\n\n'.join(records.values()) + '\n\n')
    # Avoid Argon/quickstart files overriding the installed MWRT theme and homepage.
    for path in (work / 'etc/rc.d').glob('*quickstart*'):
        path.unlink()
    for path in (work / 'etc/rc.d').glob('*mosquitto'):
        path.unlink()
    for relative in ('usr/lib/lua/luci/controller/quickstart.lua',
                     'usr/share/luci/menu.d/luci-app-quickstart.json',
                     'usr/share/luci/menu.d/luci-base（复件）.json',
                     'etc/config/quickstart',
                     'etc/init.d/cklinkn3000',
                     'etc/uci-defaults/30_luci-theme-argon'):
        path = work / relative
        if path.is_file() or path.is_symlink():
            path.unlink()
    for link in (work / 'etc/rc.d').iterdir():
        if link.is_symlink():
            target = os.readlink(link)
            if Path(target).name == 'cklinkn3000' or not (link.parent / target).exists():
                link.unlink()
    # Install current remote and reporting files after all bundled theme overlays.
    install_remote_access(source, work)
    # Reuse the read-only MTK status API without replacing NBCPE acceleration settings.
    hnat = source / 'package/luci-app-c5800-hnat/root'
    for relative, mode in (('usr/libexec/rpcd/luci.turboacc', 0o755),
                           ('usr/share/rpcd/acl.d/luci-app-turboacc.json', 0o644)):
        copy(hnat / relative, work / relative)
        (work / relative).chmod(mode)
    disabled_init = set(official_manifest['disabled_init'])
    for link in (work / 'etc/rc.d').iterdir():
        if link.is_symlink() and Path(os.readlink(link)).name in disabled_init:
            link.unlink()
    for script in ('myos-nros-app', 'myos-nros-mqtt',
                   'myos-nbcpe-cache', 'myos-nbcpe-leds', 'myos-nbcpe-native',
                   'myos-nbcpe-webui', 'myos-factory-wifi'):
        target = work / ('etc/init.d/' + script)
        if target.exists():
            for key, prefix in (('START', 'S'), ('STOP', 'K')):
                match = re.search(r'^' + key + r'=(\d+)', target.read_text(errors='replace'), re.M)
                if match:
                    link = work / ('etc/rc.d/' + prefix + match[1].zfill(2) + script)
                    if not link.exists():
                        link.symlink_to('../init.d/' + script)
    for name, target in official_manifest['rc_links'].items():
        link = work / 'etc/rc.d' / name
        if link.is_symlink() or link.exists():
            link.unlink()
        link.symlink_to(target)
    runpy.run_path(str(source / 'target/linux/mediatek/image/myos-new-image.py'))['install'](work)
    # The stock strings alternative points at binutils-strings, which slimming
    # removes. Preinit 02_sysinfo must invoke the retained BusyBox applet.
    restore_package_alternatives(work, records)
    restore_busybox_strings(work)
    plan['native_filelist_ownership_corrections'] = reconcile_native_filelists(work, records, required)
    # A single module tree from the current kernel build, never stock 5.4 modules.
    module_root = work / 'lib/modules' / KERNEL
    for filename in ('modules.builtin', 'modules.builtin.modinfo', 'modules.order'):
        path = kdir / ('linux-' + KERNEL) / filename
        if path.exists():
            copy(path, module_root / filename)
    depmod = next((p for p in (host / 'depmod', host.parent / 'sbin/depmod',
                              Path('/sbin/depmod'), Path('/usr/sbin/depmod')) if p.is_file()), None)
    if depmod:
        run(depmod, '-b', work, KERNEL)
    elif shutil.which('busybox'):
        run('busybox', 'depmod', '-b', work, KERNEL)
    else:
        print('depmod unavailable; OpenWrt kmodloader uses /etc/modules.d', flush=True)
    for parent, dirs, files in os.walk(work):
        Path(parent).chmod(0o755)
        for filename in files:
            path = Path(parent) / filename
            if path.is_symlink():
                continue
            with path.open('rb') as stream:
                shebang = stream.read(2) == b'#!'
            executable = bool(path.stat().st_mode & 0o111) or shebang
            path.chmod(0o755 if executable else 0o644)
    run('python3', source / 'scripts/prepare-public-rootfs.py', work)
    (work / 'etc/shadow').chmod(0o600)
    (work / 'usr/share/myos-nros/id_rsa.db').chmod(0o400)
    for relative in ('etc/config/myos_nros', 'etc/config/mosquitto'):
        path = work / relative
        if path.is_file():
            path.chmod(0o600)
    # board_detect skips entries without execute permission, including sourced
    # board.d scripts with no shebang. Keep the NBCPE DSA setup executable.
    (work / 'etc/board.d/02_network').chmod(0o755)
    plan['preserved_native_files'] = {n: hashlib.sha256(data).hexdigest()
                                      for n, (data, _) in preserved.items()}
    plan['official_odu'] = {
        'source': official_manifest['source'], 'files_sha256': official_hashes,
        'disabled_init': sorted(disabled_init), 'rc_links': official_manifest['rc_links'],
    }
    (assets / 'build-manifest.json').write_text(json.dumps(plan, indent=2) + '\n')
    if args.pack:
        pseudo = kdir / 'nbcpe-myos-console.pseudo'
        pseudo.write_text('dev/console c 0600 0 0 5 1\n')
        output = kdir / 'root-nbcpe-myos.squashfs'
        run(host / 'mksquashfs4', work, output, '-nopad', '-noappend', '-root-owned',
            '-comp', 'xz', '-Xpreset', '9', '-Xe', '-Xlc', '0', '-Xlp', '2', '-Xpb', '2',
            '-b', '256k', '-no-xattrs', '-pf', pseudo, '-processors', '6')
        print('NBCPE MYOS rootfs:', output, output.stat().st_size, flush=True)
    else:
        print('NBCPE MYOS userspace assembled:', work, flush=True)


if __name__ == '__main__':
    main()
