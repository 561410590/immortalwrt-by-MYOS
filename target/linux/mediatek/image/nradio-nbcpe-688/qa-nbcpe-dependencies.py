#!/usr/bin/env python3
"""Verify current-IPK dependency resolution and inspect the NBCPE hot package set."""
import argparse
import hashlib
import importlib.util
import io
import json
import re
import tarfile
import tempfile
from pathlib import Path


def load_assembler():
    path = Path(__file__).with_name('assemble-nbcpe.py')
    spec = importlib.util.spec_from_file_location('nbcpe_assembler', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--hot-bundle', action='store_true',
                        help='Create the explicitly requested NBCPE hot IPK transfer archive')
    parser.add_argument('--feed-assets', action='store_true',
                        help='Export current-source repository configuration and trusted public keys')
    args = parser.parse_args()
    assembly = load_assembler()
    checks = []

    def record(name, version='1', depends='', provides=''):
        return ('Package: ' + name + '\nVersion: ' + version + '\n'
                + ('Depends: ' + depends + '\n' if depends else '')
                + ('Provides: ' + provides + '\n' if provides else ''))

    def pool(*records):
        return {re.search(r'^Package: (.+)$', item, re.M)[1]: (Path('fixture.ipk'), item, {})
                for item in records}

    available = pool(record('iptables-nft', '1.8.10-r1', 'libxtables12'),
                     record('libxtables12', '1.8.10-r1'),
                     record('mkf2fs', '1.16.0-r3', 'libf2fs6'),
                     record('libf2fs6', '1.16.0-r3'))
    selected, missing = assembly.required_closure({'iptables-nft', 'mkf2fs'}, available)
    assert selected == set(available) and not missing
    checks.append('Same-name native libraries cannot satisfy a modern dependency closure')

    available = pool(record('procd', '2', 'libubox (>= 2024), optional-a | optional-b'),
                     record('libubox20240329', '2024.12.19-r1', 'libc', 'libubox'),
                     record('libc'), record('optional-b', '3'))
    selected, missing = assembly.required_closure({'procd'}, available)
    assert selected == set(available) and not missing
    checks.append('Virtual providers, version constraints, alternatives and transitive dependencies resolve')
    _, missing = assembly.required_closure({'procd (>= 3)'}, available)
    assert missing == ['procd (>= 3)']
    checks.append('Unsatisfied dependency versions fail closed')
    available = pool(record('a', depends='b'), record('b', depends='a'))
    assert assembly.required_closure({'a'}, available) == ({'a', 'b'}, [])
    checks.append('Dependency cycles terminate')
    for left, right, expected in [('1.8.10-r1', '1.8.7-2', 1), ('1.01', '1.1', 0),
                                  ('1.0~rc1', '1.0', -1), ('1:1.0', '9.0', 1),
                                  ('1.2-r10', '1.2-r2', 1), ('1.2', '1.2-0', 0)]:
        assert assembly.compare_versions(left, right) == expected, (left, right)
        assert assembly.compare_versions(right, left) == -expected, (right, left)
    checks.append('Epoch, tilde, numeric and revision version ordering passes')
    with tempfile.TemporaryDirectory(prefix='nbcpe-ownership-qa-') as temporary:
        fixture = Path(temporary)
        info = fixture / 'usr/lib/opkg/info'
        info.mkdir(parents=True)
        library = fixture / 'usr/lib/libiptext4.so'
        library.write_bytes(b'fixture')
        (info / 'libiptext0.list').write_text('/usr/lib/libiptext4.so\n')
        (info / 'libip4tc2.list').write_text('/usr/lib/libiptext4.so\n/usr/lib/libip4tc.so.2\n/usr/sbin/iptables\n')
        records = {'libiptext0': record('libiptext0') + 'Alternatives: 300:/usr/sbin/iptables:/usr/sbin/xtables-nft-multi\n',
                   'libip4tc2': record('libip4tc2')}
        corrected = assembly.reconcile_native_filelists(fixture, records, {'libiptext0'})
        assert (info / 'libip4tc2.list').read_text() == '/usr/lib/libip4tc.so.2\n'
        assert set(corrected['libip4tc2']) == {'/usr/lib/libiptext4.so', '/usr/sbin/iptables'}
    checks.append('Native split-library and alternatives ownership is reconciled without removing native-only files')
    report = {'checks': checks, 'passed': True}
    if args.source:
        source = args.source
        available = {}
        for directory in (source / 'bin/targets/mediatek/filogic/packages',
                          source / 'bin/packages/aarch64_cortex-a53'):
            for path in directory.rglob('*.ipk'):
                name, control, fields, _ = assembly.read_ipk(path)
                if name == 'kernel' or name.startswith('kmod-'):
                    if not re.search(r'(^Version: 6\.6\.110[~+.\-]|kernel \(= 6\.6\.110)', control, re.M):
                        continue
                if name not in available or path.stat().st_mtime > available[name][0].stat().st_mtime:
                    available[name] = (path, control, fields)
        roots = {'procd', 'procd-ujail', 'ubox', 'iptables-nft', 'ip6tables-nft', 'nftables-json',
                 'opkg', 'openwrt-keyring', 'luci-app-package-manager', 'mkf2fs', 'f2fsck'}
        selected, missing = assembly.required_closure(roots, available)
        assert not missing, missing
        root = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
        installed = assembly.records_in((root / 'usr/lib/opkg/status').read_text())
        packages = []
        for name in sorted(selected):
            path, control, fields = available[name]
            previous = assembly.package_version(installed[name]) if name in installed else None
            version = assembly.package_version(control)
            if previous == version:
                continue
            _, _, _, data = assembly.read_ipk(path, data=True)
            with tarfile.open(fileobj=io.BytesIO(data), mode='r:*') as archive:
                payload = [member.name.removeprefix('./') for member in archive.getmembers()
                           if member.isfile() or member.issym()]
            packages.append({'name': name, 'version': version, 'previous': previous,
                             'ipk': str(path), 'depends': re.findall(r'^Depends: (.+)$', control, re.M),
                             'alternatives': re.findall(r'^Alternatives: (.+)$', control, re.M),
                             'payload': payload,
                             'maintainer_scripts': {key: value[0].decode(errors='replace')
                                                    for key, value in fields.items()
                                                    if key in ('preinst', 'postinst', 'prerm', 'postrm')}})
        report.update({'hot_roots': sorted(roots), 'current_ipk_closure': sorted(selected),
                       'changed_packages': packages})
        if args.feed_assets:
            assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
            keyring, _, _, data = assembly.read_ipk(available['openwrt-keyring'][0], data=True)
            keys = assets / 'nbcpe-feed-keys'
            keys.mkdir(exist_ok=True)
            public_keys = []
            with tarfile.open(fileobj=io.BytesIO(data), mode='r:*') as archive:
                for member in archive.getmembers():
                    relative = member.name.removeprefix('./')
                    if member.isfile() and relative.startswith('etc/opkg/keys/'):
                        destination = keys / Path(relative).name
                        destination.write_bytes(archive.extractfile(member).read())
                        public_keys.append(str(destination))
            assert len(public_keys) == 2, public_keys
            (assets / 'nbcpe-distfeeds.conf').write_text(assembly.configured_feeds(source))
            (assets / 'nbcpe-opkg.conf').write_text(assembly.configured_opkg(source))
            report['feed_assets'] = {'distfeeds': str(assets / 'nbcpe-distfeeds.conf'),
                                     'opkg_config': str(assets / 'nbcpe-opkg.conf'),
                                     'keys': public_keys, 'source_ipk': str(available[keyring][0])}
        if args.hot_bundle:
            assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
            repairs = {'libiptext0', 'libiptext6-0', 'libiptext-nft0', 'xtables-nft',
                       'iptables-nft', 'ip6tables-nft'}
            names = {item['name'] for item in packages} | repairs
            manifest = {'version': assembly.VERSION,
                        'install_first': [Path(item['ipk']).name for item in packages],
                        'force_reinstall_after': [available[name][0].name for name in sorted(repairs)],
                        'package_count': len(names), 'packages': []}
            archive = assets / 'nbcpe-hot-update-ipks.tar'
            with tarfile.open(archive, 'w') as output:
                for name in sorted(names):
                    path, control, _ = available[name]
                    output.add(path, arcname=path.name, recursive=False)
                    manifest['packages'].append({'name': name, 'file': path.name,
                                                 'source': str(path), 'control': control,
                                                 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
                metadata = (json.dumps(manifest, indent=2) + '\n').encode()
                entry = tarfile.TarInfo('manifest.json')
                entry.size = len(metadata)
                output.addfile(entry, io.BytesIO(metadata))
                feeds = assembly.configured_feeds(source).encode()
                entry = tarfile.TarInfo('nbcpe-distfeeds.conf')
                entry.size = len(feeds)
                output.addfile(entry, io.BytesIO(feeds))
            (assets / 'nbcpe-distfeeds.conf').write_text(assembly.configured_feeds(source))
            report['hot_bundle'] = {'archive': str(archive), 'package_count': len(names),
                                    'bytes': archive.stat().st_size,
                                    'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
                                    'feeds': str(assets / 'nbcpe-distfeeds.conf')}
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
