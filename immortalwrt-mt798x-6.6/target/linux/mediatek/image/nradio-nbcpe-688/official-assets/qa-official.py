#!/usr/bin/env python3
"""Offline NBCPE official-ODU installation and private ELF closure checks.

Never runs a cellular binary/client, opens a socket, or touches router state.
Symbol/loader checks establish linkage only, not complete runtime ABI semantics.
"""
import argparse
import functools
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--system-root', type=Path, required=True)
    args = parser.parse_args()
    assets, system = args.assets, args.system_root
    overlay = assets / 'root'
    manifest = json.loads((assets / 'install-manifest.json').read_text())
    failures = []
    checked = []

    def require(condition, label):
        checked.append(label)
        if not condition:
            failures.append(label)

    for name in manifest['exec_files'] + manifest['shared_libraries']:
        require((overlay / name.lstrip('/')).is_file(), 'installed ' + name)

    bins = ['atsd', 'atsd_cli', 'infocd', 'infocd_consumer', 'atserver-sniffer', 'bdinfo']
    for name in bins:
        entry = overlay / ('usr/bin' if name == 'bdinfo' else 'usr/sbin') / name
        text = entry.read_text()
        require('--library-path /usr/lib/myos-nbcpe-official:/lib:/usr/lib' in text
                and 'LD_LIBRARY_PATH' not in text, 'isolated loader ' + name)
        require((overlay / 'usr/libexec/myos-nbcpe-official' / name).read_bytes()
                == (assets / 'rom' / ('usr/bin' if name == 'bdinfo' else 'usr/sbin') / name).read_bytes(), 'original ELF ' + name)
    require('modeminit-AK68' in manifest['disabled_init']
            and 'atsd_tools' in manifest['disabled_init'], 'one transport owner')
    busybox = system / 'bin/busybox'
    require((system / 'usr/bin/flock').is_file() and busybox.is_file()
            and b'\0flock\0' in busybox.read_bytes(), 'existing BusyBox flock applet')

    freq = json.loads((overlay / 'etc/freq.json').read_text())['MT5700M-CN']
    require(freq['odu']['atlocal'] == '192.168.8.254'
            and freq['odu']['atserver'] == '192.168.8.1'
            and freq['odu']['atport'] == '20249', 'official ODU management profile')
    # Official ROM uses a trailing comma, accepted by json-c/jshn.
    ncm_source = (overlay / 'etc/gcom/ncm.json').read_text()
    ncm = json.loads(re.sub(r',\s*([}\]])', r'\1', ncm_source))['tdtech_mt5700_odu']
    require(ncm['connect'] == 'AT' and ncm['disconnect'] == 'AT', 'external self dial')
    odu = (overlay / 'lib/netifd/proto/odu.sh').read_text()
    require('json_add_string name "${interface}_4"' in odu
            and 'json_add_string proto "dhcp"' in odu
            and 'json_add_string name "${interface}_6"' in odu
            and 'json_add_string proto "dhcpv6"' in odu
            and 'json_add_string extendprefix 1' in odu
            and odu.count('json_add_string device "@$interface"') == 2,
            'modern DHCPv4 + DHCPv6/PD child contract')
    require('fw4 -q network' in odu and 'fw3 -q network' not in odu, 'fw4 zone lookup')
    initializer = (overlay / 'usr/sbin/cellular_init').read_text()
    require('inet $atlocal/32' in initializer and 'route show table 250' in initializer
            and initializer.index('route show table 250') < initializer.index('touch /var/run/cellular_init/cpe/ready'),
            'readiness follows native management address and route')

    for name in manifest['exec_files']:
        path = overlay / name.lstrip('/')
        if path.read_bytes().startswith(b'#!'):
            result = subprocess.run(['bash', '-n', str(path)], capture_output=True, text=True)
            require(result.returncode == 0, 'shell syntax ' + name + ': ' + result.stderr.strip())

    @functools.lru_cache(maxsize=None)
    def elf(path):
        data = path.read_bytes()
        if data[:4] != b'\x7fELF':
            return None
        dynamic = subprocess.check_output(['readelf', '-dW', str(path)], text=True)
        symbols = subprocess.check_output(['readelf', '-DsW', str(path)], text=True)
        defined, undefined = set(), set()
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields) < 8 or not fields[0].endswith(':'):
                continue
            bind, visibility, section, name = fields[4:8]
            name = name.split('@', 1)[0]
            if section == 'UND' and bind == 'GLOBAL':
                undefined.add(name)
            elif section != 'UND' and bind in ('GLOBAL', 'WEAK') and visibility in ('DEFAULT', 'PROTECTED'):
                defined.add(name)
        return re.findall(r'\(NEEDED\).*?\[([^]]+)\]', dynamic), defined, undefined

    private = overlay / 'usr/lib/myos-nbcpe-official'

    def locate(name):
        for directory in [private, system / 'lib', system / 'usr/lib']:
            candidate = directory / name
            if candidate.is_file():
                return candidate
        return None

    linkage = []
    for name in bins + ['system-blkid']:
        target = system / 'usr/sbin/blkid' if name == 'system-blkid' else overlay / 'usr/libexec/myos-nbcpe-official' / name
        todo, scope, missing = [target], set(), set()
        while todo:
            path = todo.pop()
            if path in scope:
                continue
            scope.add(path)
            for needed in elf(path)[0]:
                dependency = locate(needed)
                if dependency:
                    todo.append(dependency)
                else:
                    missing.add(needed)
        exports = set().union(*(elf(path)[1] for path in scope))
        unresolved = {str(path): sorted(elf(path)[2] - exports) for path in scope
                      if elf(path)[2] - exports}
        require(not missing and not unresolved, 'private ELF closure ' + name)
        linkage.append({'name': name, 'elfs': len(scope), 'missing': sorted(missing),
                        'unresolved': unresolved})

    identity_primitives = {name: (system / name.lstrip('/')).is_file() for name in
                           ['/usr/bin/bdinfo', '/usr/sbin/bdinfo', '/lib/preinit/81_mount_app_data']}
    print(json.dumps({'checks': len(checked), 'passed': len(checked)-len(failures),
                      'failures': failures, 'linkage': linkage,
                      'identity_primitives': identity_primitives,
                      'real_AT': 0, 'network_requests': 0}, ensure_ascii=False))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
