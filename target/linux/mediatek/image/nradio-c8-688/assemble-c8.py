#!/usr/bin/env python3
"""Assemble the C8 rootfs from the verified C8 reference and current IPKs."""
import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tarfile

VERSION = '3.1.35-202610161200'
KERNEL = '6.6.110'
BOARD = 'HCMT7981-688-emmc'
REFERENCE_SHA256 = '4b8e62131bb5be2926273103e0d5dad030c8a72c7de847e3ce4407219a70b6a1'


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def package_helpers(source):
    # Only the existing pure IPK/dependency helpers are reused; main is not run.
    spec = importlib.util.spec_from_file_location(
        'myos_package_helpers', source / 'target/linux/mediatek/image/assemble-nbcpe.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def copy_file(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink():
        target.unlink()
    shutil.copy2(source, target)


def write(root, relative, text, mode=0o644):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink():
        path.unlink()
    path.write_text(text, encoding='utf-8')
    path.chmod(mode)


def root_path(root, relative):
    """Resolve archive ancestors inside the offline root, including /var -> /tmp."""
    pending = list(Path(relative).parts)
    result = root
    seen = 0
    while pending:
        component = pending.pop(0)
        if component in ('', '.', '/'):
            continue
        if component == '..':
            if result == root:
                raise ValueError('Archive path escapes the C8 root: ' + relative)
            result = result.parent
            continue
        candidate = result / component
        if pending and candidate.is_symlink():
            seen += 1
            if seen > 40:
                raise ValueError('Archive symlink loop: ' + relative)
            link = os.readlink(candidate)
            if link.startswith('/'):
                result = root
            pending = list(Path(link).parts) + pending
        else:
            result = candidate
    return result


def extract_ipk(data, root):
    entries = []
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:*') as archive:
        for member in archive:
            relative = member.name.removeprefix('./').rstrip('/')
            if not relative or relative == '.':
                continue
            if relative.startswith('/') or '..' in Path(relative).parts:
                raise ValueError('Unsafe IPK path: ' + member.name)
            path = root_path(root, relative)
            entries.append('/' + relative)
            if member.isdir():
                if path.is_symlink():
                    continue
                path.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                if path.is_symlink():
                    path.unlink()
                with archive.extractfile(member) as stream, path.open('wb') as target:
                    shutil.copyfileobj(stream, target)
                path.chmod(member.mode)
            elif member.issym():
                path.parent.mkdir(parents=True, exist_ok=True)
                if path.is_symlink() or path.is_file():
                    path.unlink()
                path.symlink_to(member.linkname)
            elif member.islnk():
                link = root_path(root, member.linkname.removeprefix('./'))
                path.parent.mkdir(parents=True, exist_ok=True)
                if path.is_symlink() or path.is_file():
                    path.unlink()
                os.link(link, path)
            else:
                raise ValueError('Unexpected IPK special file: ' + member.name)
    return entries


def overlay_tree(directory, root, excluded=()):
    if not directory.is_dir():
        raise RuntimeError('Missing C8 input directory: ' + str(directory))
    for path in directory.rglob('*'):
        relative = path.relative_to(directory)
        if '__pycache__' in relative.parts or path.suffix in ('.pyc', '.pyo'):
            continue
        if path.is_file() and relative.as_posix() not in excluded:
            copy_file(path, root / relative)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--inspect', action='store_true')
    parser.add_argument('--pack', action='store_true')
    args = parser.parse_args()
    source = args.source.resolve()
    assets = source / 'target/linux/mediatek/image/nradio-c8-688'
    common = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    host = source / 'staging_dir/host/bin'
    kdir = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic'
    work = assets / ('root-c8-myos-' + VERSION)
    stock = assets / 'stock-root'
    helpers = package_helpers(source)
    reference = assets / 'reference.bin'
    if hashlib.sha256(reference.read_bytes()).hexdigest() != REFERENCE_SHA256:
        raise RuntimeError('C8 reference SHA256 differs')
    if not (kdir / ('linux-' + KERNEL) / 'vmlinux').is_file():
        raise RuntimeError('Matching source-built kernel is required')
    records = helpers.records_in((stock / 'usr/lib/opkg/status').read_text(encoding='utf-8'))
    available = {}
    for directory in (source / 'bin/targets/mediatek/filogic/packages',
                      source / 'bin/packages/aarch64_cortex-a53', common / 'vantage/packages'):
        for path in directory.rglob('*.ipk'):
            name, record, fields, _ = helpers.read_ipk(path)
            if name == 'kernel' or name.startswith('kmod-'):
                if not re.search(r'(^Version: 6\.6\.110[~+.\-]|kernel \(= 6\.6\.110)', record, re.M):
                    continue
            if name not in available or path.stat().st_mtime > available[name][0].stat().st_mtime:
                available[name] = path, record, fields
    config = (source / '.config').read_text(encoding='utf-8')
    selected = set(re.findall(r'^CONFIG_PACKAGE_([^=]+)=y$', config, re.M))
    roots = {'kernel'} | {name for name in selected if name.startswith('kmod-')}
    roots |= {
        'base-files', 'busybox', 'libc', 'libgcc1', 'libpthread', 'dropbear',
        'luci-base', 'luci-lib-base', 'luci-compat', 'luci-mod-admin-full',
        'luci-mod-network', 'luci-mod-system', 'luci-mod-status', 'luci-lib-jsonc',
        'luci-i18n-base-zh-cn', 'luci-i18n-firewall-zh-cn',
        'luci-lib-nixio', 'luci-lib-ip', 'luci-theme-bootstrap', 'liblucihttp0',
        'rpcd', 'rpcd-mod-file', 'rpcd-mod-luci', 'rpcd-mod-iwinfo', 'rpcd-mod-ucode',
        'ucode', 'ucode-mod-fs', 'ucode-mod-ubus', 'ucode-mod-uci',
        'uci', 'libuci-lua', 'ubus', 'libubus-lua', 'ubusd',
        'netifd', 'procd', 'procd-ujail', 'ubox', 'logd',
        'uhttpd', 'uhttpd-mod-ubus', 'firewall4', 'nftables-json',
        'luci-app-firewall', 'luci-app-store', 'luci-proto-ipv6', 'kmod-nft-fullcone',
        'opkg', 'openwrt-keyring', 'iptables-nft', 'ip6tables-nft', 'iptables-mod-extra',
        'mtwifi-cfg', 'luci-app-mtwifi-cfg', 'wifi-dats', 'wifi-profile', 'mtk-smp', 'ethtool',
        'luci-app-turboacc', 'luci-i18n-turboacc-zh-cn',
        'dnsmasq-full', 'odhcp6c', 'odhcpd-ipv6only',
        'jq', 'bc', 'fstools', 'block-mount', 'blkid', 'lsblk', 'ip-full', 'tc-full',
        'uclient-fetch', 'libustream-openssl', 'ca-bundle', 'ca-certificates',
        'curl', 'libcurl4', 'libnghttp2-14',
        'losetup', 'kmod-loop', 'kmod-fs-ext4', 'e2fsprogs',
        'mkf2fs', 'f2fsck', 'usbutils', 'usb-modeswitch',
        'luci-theme-c2000max-ui', 'luci-theme-mwrt',
        'luci-theme-vantage', 'luci-app-vantage', 'luci-i18n-vantage-zh-cn',
        'lua-cjson', 'lua-md5', 'lua-mosquitto', 'libmosquitto-nossl',
        'mosquitto-nossl', 'libcares', 'luci-lua-runtime',
    }
    required, missing = helpers.required_closure(roots, available)
    unwanted = {name for name in required if 'quickstart' in name.lower()
                or name == 'luci-app-package-manager'}
    if unwanted:
        raise RuntimeError('Unwanted dashboard/store dependency: ' + repr(sorted(unwanted)))
    native = {'python3-light', 'python3-urllib'}
    if not native <= records.keys():
        raise RuntimeError('The C8 native adapter requires reference Python light/urllib')
    final = {name: records[name] for name in native}
    final.update({name: available[name][1] for name in required})
    # Retain only native Python's own dependency closure when a modern provider is absent.
    pending = list(native)
    while pending:
        name = pending.pop()
        dependencies = re.search(r'^Depends: (.+)$', final[name], re.M)
        for expression in dependencies[1].split(',') if dependencies else []:
            choices = helpers.dependency_choices(expression)
            if any(helpers.satisfies(record, choice) for record in final.values() for choice in choices):
                continue
            dependency = next((candidate for candidate, record in records.items()
                               if candidate not in final and not candidate.startswith('kmod-')
                               and any(helpers.satisfies(record, choice) for choice in choices)), None)
            if dependency is not None:
                native.add(dependency)
                final[dependency] = records[dependency]
                pending.append(dependency)
    kernel_abi = helpers.package_version(available['kernel'][1])
    mismatches = {}
    gaps = {}
    for name, record in final.items():
        dependency = re.search(r'kernel\s*\(=\s*([^)]+)\)', record)
        if name.startswith('kmod-') and (not dependency or dependency[1].strip() != kernel_abi):
            mismatches[name] = dependency[1] if dependency else 'missing kernel ABI'
        dependencies = re.search(r'^Depends: (.+)$', record, re.M)
        for expression in dependencies[1].split(',') if dependencies else []:
            choices = helpers.dependency_choices(expression)
            if not any(helpers.satisfies(candidate, choice) for candidate in final.values() for choice in choices):
                gaps.setdefault(name, []).append(expression)
    plan = {'version': VERSION, 'kernel': KERNEL, 'board': BOARD,
            'reference_sha256': REFERENCE_SHA256, 'required_packages': sorted(required),
            'native_packages': sorted(native), 'missing_packages': missing,
            'kernel_abi': kernel_abi, 'abi_mismatches': mismatches,
            'dependency_gaps': gaps, 'source_config_sha256': hashlib.sha256(config.encode()).hexdigest(),
            'live_device_validation': 'pending'}
    print(json.dumps(plan, indent=2), flush=True)
    if args.inspect:
        return
    if missing or mismatches or gaps:
        raise RuntimeError('C8 package closure failed; see inspection result')
    owner = work / '.c8-build-owner'
    if work.exists():
        if not owner.is_file() or owner.read_text(encoding='utf-8').strip() != VERSION:
            raise RuntimeError('Existing root is not owned by this C8 build')
    else:
        run(host / 'unsquashfs4', '-no-progress', '-no-xattrs', '-excludes', '-d', work,
            assets / 'reference-tar/sysupgrade-HCMT7981-688-emmc/root', 'dev/console', 'lib/modules')
        write(work, '.c8-build-owner', VERSION + '\n')
    info = work / 'usr/lib/opkg/info'
    removed = set(records) - native - required
    plan['removed_reference_packages'] = sorted(removed)
    for name in removed:
        listing = info / (name + '.list')
        for relative in listing.read_text(encoding='utf-8').splitlines() if listing.is_file() else []:
            path = root_path(work, relative.lstrip('/'))
            if path.is_file() or path.is_symlink():
                path.unlink()
        for path in info.glob(name + '.*'):
            path.unlink()
    for directory in ('etc/modules.d', 'etc/modules-boot.d'):
        for path in (work / directory).glob('*'):
            if path.is_file() or path.is_symlink():
                path.unlink()
    for name in sorted(required):
        _, record, fields, payload = helpers.read_ipk(available[name][0], data=True)
        entries = extract_ipk(payload, work)
        record = re.sub(r'^Status:.*\n?', '', record, flags=re.M).strip()
        final[name] = record + '\nStatus: install ok installed'
        write(work, 'usr/lib/opkg/info/' + name + '.list', '\n'.join(entries) + '\n')
        for key, (data, mode) in fields.items():
            path = info / (name + '.' + key)
            path.write_bytes(data)
            path.chmod(mode)
    app = common / 'app-assets/root'
    copy_file(work / 'www/cgi-bin/luci', work / 'www/cgi-bin/luci.ucode')
    overlay_tree(app, work)
    copy_file(work / 'usr/lib/lua/myos_nradio_data.lua',
              work / 'usr/lib/lua/myos_nradio_data_generic.lua')
    for name in ('luci-theme-c2000max-ui', 'luci-theme-mwrt'):
        theme = common / 'themes' / name
        for part, destination in (('root', work), ('htdocs', work / 'www'),
                                  ('ucode', work / 'usr/share/ucode/luci')):
            directory = theme / part
            if directory.exists():
                overlay_tree(directory, destination)
    overlay_tree(common / 'vantage/root', work)
    overlay_tree(source / 'target/linux/mediatek/image/nradio-system-switch/root', work)
    board_files = source / 'target/linux/mediatek/filogic/base-files'
    for relative in ('etc/board.d/02_network', 'lib/upgrade/platform.sh'):
        copy_file(board_files / relative, work / relative)
    # The shared 04_defaults installs a C5800 lan1/eth1/BLUE4 topology on C8.
    # Keep the C8 reference defaults; 02_network and 93-myos-c8 own its ports.
    copy_file(stock / 'etc/board.d/04_defaults', work / 'etc/board.d/04_defaults')
    copy_file(source / 'package/base-files/files/lib/functions.sh', work / 'lib/functions.sh')
    for relative in ('dispatcher.uc', 'template/header.ut'):
        copy_file(source / 'feeds/luci/modules/luci-base/ucode' / relative,
                  work / 'usr/share/ucode/luci' / relative)
    # Offline image builds must not execute package postinst or any device commands.
    helpers.restore_package_alternatives(work, final)
    helpers.restore_busybox_strings(work)
    copy_file(work / 'sbin/wifi', work / 'usr/libexec/myos-c8-wifi-native')
    # The native C8 stack owns AT access; earlier standalone modem drafts stay out.
    excluded = {
        'etc/config/myos_c8_modem', 'etc/init.d/myos-c8-modem',
        'usr/sbin/myos-c8-modemd', 'usr/sbin/myos-c8-modemctl',
        'usr/lib/lua/myos_c8_modem.lua', 'usr/lib/lua/luci/controller/modem.lua',
    }
    for relative in excluded:
        path = work / relative
        if path.is_file() or path.is_symlink():
            path.unlink()
    overlay_tree(assets / 'overlay', work, excluded)
    spec = importlib.util.spec_from_file_location('c8_official_runtime', assets / 'install-c8-official.py')
    official = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(official)
    plan['official_runtime'] = official.install_official(source, assets, work)
    overlay_tree(assets / 'native-overlay', work)
    for relative in (
        'etc/uci-defaults/94-myos-factory-wifi',
        'etc/init.d/myos-factory-wifi',
        'lib/preinit/82_myos_factory_wifi',
        'lib/functions/myos-factory-wifi.sh',
        'usr/lib/lua/luci/controller/zz_mwrt_modem.lua',
        'usr/lib/lua/luci/controller/zz_mwrt_qmodem.lua',
        'usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua',
    ):
        path = work / relative
        if path.is_file() or path.is_symlink():
            path.unlink()
    for path in sorted(work.rglob('*quickstart*'), key=lambda item: len(item.parts), reverse=True):
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)
    for path in (work / 'etc/wireless').rglob('*.dat'):
        lines = path.read_text(encoding='utf-8').splitlines()
        lines = [line for line in lines if not re.fullmatch(
            r'(?:MacAddress|ApcliMacAddress)\d*', line.partition('=')[0].strip())]
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    wireless = work / 'etc/config/wireless'
    if wireless.is_file():
        lines = wireless.read_text(encoding='utf-8').splitlines()
        lines = [line for line in lines if shlex.split(line, comments=True)[:2] != ['option', 'macaddr']]
        wireless.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    description = 'MYOS C8-688 ' + VERSION
    write(work, 'etc/openwrt_release', "DISTRIB_ID='MYOS'\nDISTRIB_RELEASE='" + VERSION +
          "'\nDISTRIB_REVISION='r33578-c88458e977'\nDISTRIB_TARGET='mediatek/filogic'\n"
          "DISTRIB_ARCH='aarch64_cortex-a53'\nDISTRIB_DESCRIPTION='" + description + "'\n")
    write(work, 'usr/lib/os-release', 'NAME="MYOS"\nID="immortalwrt"\nVERSION="' + VERSION +
          '"\nVERSION_ID="' + VERSION + '"\nPRETTY_NAME="' + description +
          '"\nOPENWRT_BOARD="mediatek/filogic"\nOPENWRT_ARCH="aarch64_cortex-a53"\n')
    write(work, 'etc/banner', '\n' + description + '\nLinux ' + KERNEL + '\n\n')
    write(work, 'etc/myos-version', VERSION + '\n')
    write(work, 'etc/openwrt_version', 'r33578-c88458e977\n')
    write(work, 'etc/opkg/distfeeds.conf', helpers.configured_feeds(source).replace('NBCPE', 'C8'))
    write(work, 'etc/opkg/customfeeds.conf', '')
    write(work, 'etc/opkg.conf', helpers.configured_opkg(source))
    write(work, 'usr/lib/opkg/status', '\n\n'.join(final.values()) + '\n\n')
    # The public source uses the upstream default instead of a reference login hash.
    copy_file(source / 'package/base-files/files/etc/shadow', work / 'etc/shadow')
    core_services = {
        'sysfixtime', 'urngd', 'boot', 'system', 'fstab', 'sysctl', 'log', 'rpcd',
        'openssl', 'dnsmasq', 'dropbear', 'firewall', 'wpad', 'network', 'usbmode',
        'odhcpd', 'istore', 'tasks', 'cron', 'uhttpd', 'ucitrack', 'turboacc',
        'done', 'sysntpd', 'mtk_smp', 'urandom_seed',
        'c2000max-wallpaper', 'myos-nros-app', 'myos-nros-mqtt',
        'myos-c8-identity', 'myos-c8-cellular', 'myos-c8-led', 'myos-c8-fan', 'myos-c8-factory-wifi',
        'myos-c8-calibration', 'atsd', 'infocd', 'smsd',
    }
    rc = work / 'etc/rc.d'
    rc.mkdir(exist_ok=True)
    for path in rc.iterdir():
        if path.is_symlink():
            name = Path(os.readlink(path)).name
            if name not in core_services or not (work / 'etc/init.d' / name).is_file():
                path.unlink()
    for name in core_services:
        init = work / 'etc/init.d' / name
        if not init.is_file():
            continue
        text = init.read_text(encoding='utf-8')
        for key, prefix in (('START', 'S'), ('STOP', 'K')):
            match = re.search(r'^' + key + r'=(\d+)', text, re.M)
            if match:
                path = rc / (prefix + match[1].zfill(2) + name)
                if not path.is_symlink():
                    path.symlink_to('../init.d/' + name)
    for relative in ('etc/hotplug.d/usb/50-map-at', 'etc/uci-defaults/30_luci-theme-argon'):
        path = work / relative
        if path.is_file() or path.is_symlink():
            path.unlink()
    for directory in ('etc/modules.d', 'etc/modules-boot.d'):
        for path in (work / directory).glob('*'):
            if path.is_file() and any(re.match(r'^\s*mt_wifi(?:\s|$)', line)
                                      for line in path.read_text(encoding='utf-8').splitlines()):
                path.unlink()
    for path in (work / 'etc/uci-defaults').iterdir():
        if path.is_file() and any(token in path.name.lower() for token in ('nbcpe', 'c5800', 'quickstart', 'modematsd', 'wtmodem')):
            path.unlink()
    # Install current remote and reporting files after all model/theme overlays.
    helpers.install_remote_access(source, work)
    module_root = work / 'lib/modules' / KERNEL
    for filename in ('modules.builtin', 'modules.builtin.modinfo', 'modules.order'):
        path = kdir / ('linux-' + KERNEL) / filename
        if path.is_file():
            copy_file(path, module_root / filename)
    depmod = next((path for path in (host / 'depmod', Path('/sbin/depmod'), Path('/usr/sbin/depmod'))
                   if path.is_file()), None)
    if not depmod:
        raise RuntimeError('Host depmod required')
    run(depmod, '-b', work, KERNEL)
    for directory, _, files in os.walk(work):
        Path(directory).chmod(0o755)
        for name in files:
            path = Path(directory) / name
            if path.is_symlink():
                continue
            with path.open('rb') as stream:
                shebang = stream.read(2) == b'#!'
            path.chmod(0o755 if shebang or path.stat().st_mode & 0o111 else 0o644)
    for relative in ('etc/board.d/02_network', 'usr/libexec/myos-boot-system', 'usr/libexec/myos-ubenv'):
        (work / relative).chmod(0o755)
    run('python3', source / 'scripts/prepare-public-rootfs.py', work)
    (work / 'usr/share/myos-nros/id_rsa.db').chmod(0o400)
    for relative in ('etc/shadow', 'etc/config/myos_nros', 'etc/config/mosquitto'):
        path = work / relative
        if path.is_file():
            path.chmod(0o600)
    plan['native_filelist_ownership_corrections'] = helpers.reconcile_native_filelists(work, final, required)
    (assets / 'build-manifest.json').write_text(json.dumps(plan, indent=2) + '\n', encoding='utf-8')
    if args.pack:
        output = assets / ('root-c8-myos-' + VERSION + '.squashfs')
        pseudo = assets / 'c8-console.pseudo'
        pseudo.write_text('dev/console c 0600 0 0 5 1\n', encoding='utf-8')
        run(host / 'mksquashfs4', work, output, '-nopad', '-noappend', '-root-owned',
            '-comp', 'xz', '-b', '256k', '-no-xattrs', '-pf', pseudo, '-processors', '6')
        print('C8 squashfs:', output, output.stat().st_size, flush=True)


if __name__ == '__main__':
    main()
