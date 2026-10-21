#!/usr/bin/env python3
"""Inspect the final C8 archive, including the Ethernet PHY and UI fixes."""
import argparse
import ast
import hashlib
import importlib.util
import json
import lzma
from pathlib import Path
import re
import struct
import tarfile
import tempfile
import zlib


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--image', required=True, type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    assets = source / 'target/linux/mediatek/image/nradio-c8-688'
    host = source / 'staging_dir/host/bin'
    spec = importlib.util.spec_from_file_location('c8_format', assets / 'verify-c8.py')
    v = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(v)
    require = v.require
    image = args.image.resolve()
    expected_name = 'MYOS-C8-688-' + args.version + '-' + v.KERNEL + '-sysupgrade.bin'
    require(image.name == expected_name, 'Final filename mismatch')
    metadata = json.loads(v.run_bytes(host / 'fwtool', '-i', '-', image))
    require(metadata.get('metadata_version') == '1.1' and metadata.get('compat_version') == '1.0',
            'Invalid metadata compatibility')
    require(metadata.get('supported_devices') == [v.BOARD], 'Wrong supported device')
    require(metadata.get('version', {}) == {
        'dist': 'MYOS', 'version': args.version, 'revision': 'r33578-c88458e977',
        'target': 'mediatek/filogic', 'board': v.BOARD}, 'Wrong firmware version/target')
    prefix = 'sysupgrade-' + v.BOARD
    with tarfile.open(image, 'r:') as archive:
        members = [m.name for m in archive if m.isfile()]
        require(sorted(members) == sorted(prefix + '/' + name for name in ('CONTROL', 'kernel', 'root')),
                'Invalid sysupgrade members')
        require(archive.extractfile(prefix + '/CONTROL').read() == ('BOARD=' + v.BOARD + '\n').encode(),
                'Wrong CONTROL board')
        kernel = archive.extractfile(prefix + '/kernel').read()
        root = archive.extractfile(prefix + '/root').read()
    require(root[:4] == b'hsqs' and struct.unpack_from('<HH', root, 28) == (4, 0), 'Invalid SquashFS')
    size = struct.unpack_from('<Q', root, 40)[0]
    require(96 <= size <= len(root) and not any(root[size:]), 'Invalid SquashFS length/padding')
    root = root[:size]
    assembled = assets / ('root-c8-myos-' + args.version + '.squashfs')
    require(assembled.read_bytes()[:size] == root, 'Packed root differs from final assembled root')
    fit = v.fdt_properties(kernel)
    require(v.string(fit['/configurations', 'default']) == 'config-1', 'Wrong FIT configuration')
    require(v.string(fit['/configurations/config-1', 'kernel']) == 'kernel-1'
            and v.string(fit['/configurations/config-1', 'fdt']) == 'fdt-1', 'Wrong FIT image references')
    for node in ('/images/kernel-1', '/images/fdt-1'):
        require(v.string(fit[node, 'arch']) == 'arm64', 'Wrong FIT architecture')
        v.check_hashes(fit, node, fit[node, 'data'])
    node = '/images/kernel-1'
    require(v.string(fit[node, 'compression']) == 'lzma' and v.string(fit[node, 'type']) == 'kernel'
            and v.string(fit[node, 'os']) == 'linux', 'Wrong kernel format')
    require(v.cells(fit[node, 'load']) == (v.LOAD_ADDRESS,)
            and v.cells(fit[node, 'entry']) == (v.LOAD_ADDRESS,), 'Wrong FIT load/entry')
    decoded = lzma.decompress(fit[node, 'data'], format=lzma.FORMAT_ALONE)
    require(decoded[56:60] == b'ARM\x64' and struct.unpack_from('<Q', decoded, 8)[0] == 0,
            'Wrong ARM64 kernel header')
    require(re.search(rb'Linux version ' + re.escape(v.KERNEL.encode()) + rb'(?:\s|\0)', decoded),
            'Wrong Linux version')
    require(decoded == (source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/Image').read_bytes(),
            'Packed kernel differs from source Image')
    require(int(v.string(fit['/', 'rootfs_size'])) == size
            and v.string(fit['/', 'rootfs_crc32']) == format(zlib.crc32(root) & 0xffffffff, '08x'),
            'FIT rootfs metadata mismatch')
    v.check_hashes(fit, '/rootfs', root)
    hardware = v.check_device_tree(fit['/images/fdt-1', 'data'])
    paths = [
        'usr/libexec/myos-c8-native', 'lib/netifd/proto/ncm.sh', 'lib/netifd/proto/wwan.sh',
        'usr/bin/cpetools.sh', 'opt/myos-c8-official/share/cpetools.sh',
        'usr/libexec/myos-boot-system',
        'etc/cpetools/huawei.sh',
        'etc/config/cpesel', 'etc/config/cpecfg',
        'usr/lib/lua/luci/view/myos_c8_modem/menu.htm',
        'usr/lib/lua/myos_c8_modem.lua', 'usr/lib/lua/luci/controller/modem.lua',
        'usr/lib/lua/luci/model/cbi/myos_c8_modem.lua',
        'usr/share/ucode/luci/template/themes/c2000max-ui/shell.ut',
        'usr/share/ucode/luci/template/themes/c2000max-ui/footer.ut',
        'www/luci-static/c2000max-ui/menu.js',
        'www/luci-static/myos/c8-cellular.js', 'www/luci-static/resources/view/c2000max/home-c8.js',
        'usr/lib/lua/luci/view/myos_c8_modem/status.htm', 'www/luci-static/resources/protocol/wwan.js',
        'www/luci-static/resources/view/network/interfaces.js', 'etc/uci-defaults/93-myos-c8',
        'usr/libexec/myos-c8-fan', 'etc/hotplug.d/iface/32-myos-c8-dsa-mtu', 'sbin/mtwifi_cfg',
        'usr/lib/lua/luci/controller/zz_mwrt_c8.lua', 'usr/share/luci/menu.d/luci-theme-c2000max-ui.json',
        'usr/lib/lua/luci/controller/myos_c8_status.lua', 'etc/uci-defaults/95-c2000max-ui']
    digests = {}
    with tempfile.TemporaryDirectory(prefix='inspect-final-c8-', dir=assets) as temporary:
        directory = Path(temporary)
        squashfs = directory / 'root.squashfs'
        squashfs.write_bytes(root)
        console = v.run_bytes(host / 'unsquashfs4', '-lln', squashfs, 'dev/console').decode()
        require(re.search(r'^crw-------\s+0/0\s+5,\s*1\s+.*\s+[^\s]*/dev/console$', console, re.M),
                'Invalid console device ownership/mode')
        unpacked = directory / 'root'
        v.run_bytes(host / 'unsquashfs4', '-no-progress', '-d', unpacked, '-excludes', squashfs, 'dev/console')
        for path in paths:
            overlay = 'overlay' if path in (
                'etc/uci-defaults/93-myos-c8', 'usr/libexec/myos-c8-fan',
                'etc/hotplug.d/iface/32-myos-c8-dsa-mtu', 'usr/lib/lua/luci/controller/myos_c8_status.lua',
                'etc/uci-defaults/95-c2000max-ui') else 'native-overlay'
            content = (unpacked / path).read_bytes()
            require(content == (assets / overlay / path).read_bytes(), 'Final fix omitted or changed: ' + path)
            digests[path] = hashlib.sha256(content).hexdigest()
        for path in ('usr/libexec/myos-c8-fan', 'etc/hotplug.d/iface/32-myos-c8-dsa-mtu',
                     'sbin/mtwifi_cfg'):
            require((unpacked / path).stat().st_mode & 0o111, 'Fix is not executable: ' + path)
        def text(path):
            return (unpacked / path).read_text()
        require(not (unpacked / 'usr/libexec/myos-update').exists()
                and not (unpacked / 'usr/libexec/myos-usage').exists(),
                'Private services entered the public source build')
        controller = text('usr/lib/lua/luci/controller/zz_mwrt_c8.lua')
        require('firmware = firmware' in text('usr/lib/lua/luci/controller/myos_c8_status.lua')
                and '/rom/etc/myos-version' in text('usr/lib/lua/luci/controller/myos_c8_status.lua'),
                'C8 identity endpoint does not read the actual ROM version')
        require(controller.count('entry({"admin", "home"}') == 2
                and controller.count('_("C8-688 首页"), 0)') == 2
                and 'build_url("admin", "home")' in controller,
                'C8 first-level device homepage/legacy redirect omitted')
        menu = json.loads(text('usr/share/luci/menu.d/luci-theme-c2000max-ui.json'))
        require(menu['admin/status']['action']['preferred'] == 'overview', 'Status does not default to system overview')
        for path, pattern in (
            ('etc/openwrt_release', r"^DISTRIB_RELEASE=['\"]([^'\"]+)['\"]$"),
            ('usr/lib/os-release', r'^VERSION_ID="([^"]+)"$'),
            ('etc/myos-version', r'^(.+)$')):
            match = re.search(pattern, text(path).strip(), re.M)
            require(match and match[1] == args.version, 'Wrong embedded version: ' + path)
        case = v.board_case_body(text('lib/upgrade/platform.sh'), v.BOARD)
        require(case and re.search(r'CI_KERNPART=[\"\']kernel_2nd[\"\']', case)
                and re.search(r'CI_ROOTPART=[\"\']rootfs_2nd[\"\']', case)
                and 'emmc_do_upgrade' in case, 'Wrong C8 upgrade partitions')
        case = v.board_case_body(text('etc/board.d/02_network'), v.BOARD)
        require(case and re.search(r'ucidef_set_interfaces_lan_wan\s+[\"\']port1 port2 port3[\"\']\s+[\"\']?port5', case),
                'Wrong C8 LAN/WAN mapping')
        defaults = text('etc/uci-defaults/93-myos-c8')
        require('c8_lan_mac="$(myos_c8_identity mac)"' in defaults
                and 'network.$1.macaddr=$c8_lan_mac' in defaults
                and 'network.lan.macaddr=$c8_lan_mac' in defaults,
                'Retained LAN bridge factory MAC migration omitted')
        adapter = ast.parse(text('usr/libexec/myos-c8-native'))
        constants = {t.id: ast.literal_eval(n.value) for n in adapter.body if isinstance(n, ast.Assign)
                     for t in n.targets if isinstance(t, ast.Name) and t.id in ('DATA_DEVICE', 'DATA_CID')}
        require(constants == {'DATA_DEVICE': 'eth1', 'DATA_CID': 8}, 'Wrong Ethernet data device/CID')
        require((unpacked / 'usr/libexec/myos-c8-native').stat().st_mode & 0o111, 'Collector is not executable')
        packages = {r['Package'] for r in v.package_records(text('usr/lib/opkg/status'))}
        require({'kmod-usb-serial-option', 'python3-light', 'python3-base', 'kmod-nft-fullcone', 'mount-utils',
                 'python3-urllib', 'python3-email', 'ethtool', 'luci-proto-ipv6'} <= packages,
                'AT/collector/FullCone runtime package missing')
        require((unpacked / 'etc/board.d/04_defaults').read_bytes()
                == (assets / 'stock-root/etc/board.d/04_defaults').read_bytes(),
                'C5800 BLUE4 topology was reintroduced into C8 defaults')
        require((unpacked / 'www/luci-static/resources/protocol/dhcpv6.js').is_file()
                and (unpacked / 'usr/sbin/ethtool').is_file(),
                'IPv6 protocol view or SMP ethtool omitted')
        for relative in ('usr/lib/python3.11/pathlib.pyc', 'usr/lib/python3.11/urllib/parse.pyc',
                         'usr/lib/python3.11/email/__init__.pyc'):
            require((unpacked / relative).read_bytes() == (assets / 'stock-root' / relative).read_bytes(),
                    'Matching reference Python runtime omitted or changed: ' + relative)
        modules = v.check_modules(unpacked, source)
        official = v.check_official_runtime(unpacked, source)
        wifi_helper = text('lib/functions/myos-c8-factory-wifi.sh')
        require((unpacked / 'lib/functions/myos-c8-factory-wifi.sh').read_bytes()
                == (assets / 'overlay/lib/functions/myos-c8-factory-wifi.sh').read_bytes(),
                'Factory Wi-Fi helper differs from current C8 source')
        require('/usr/libexec/myos-c8-identity factory-mac' in wifi_helper
                and 'ssid=@NRadio-$suffix-$band' in wifi_helper
                and 'if [ "$myos_fw_fresh" = 1 ] || [ -z "$key" ]; then' in wifi_helper,
                'Factory Wi-Fi identity/retained-password handling mismatch')
        start = unpacked / 'etc/rc.d/S19myos-c8-factory-wifi'
        require(start.is_symlink() and start.readlink().as_posix() == '../init.d/myos-c8-factory-wifi',
                'Factory Wi-Fi service is not enabled')
        factory = {'source_identical': True, 'hardware_mac_ssid': True, 'retained_password_preserved': True}
    report = {'file': str(image), 'bytes': image.stat().st_size,
              'sha256': hashlib.sha256(image.read_bytes()).hexdigest(), 'version': args.version,
              'board': v.BOARD, 'kernel': v.KERNEL, 'rootfs_bytes': size,
              'upgrade_partitions': ['kernel_2nd', 'rootfs_2nd'], 'data_device': 'eth1', 'data_cid': 8,
              'embedded_fixes': digests, 'hardware': hardware, 'modules': modules,
              'official_runtime': official, 'factory_wifi': factory, 'new_image_live_flash': 'not_performed'}
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
