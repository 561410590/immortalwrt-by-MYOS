#!/usr/bin/env python3
"""Check the actual curl dependency closure and run its AArch64 binary."""

import argparse
import hashlib
import io
import json
import os
import posixpath
import re
import subprocess
import tarfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--nghttp2', type=Path,
                        help='Isolated replacement; does not change the image')
    args = parser.parse_args()
    source = args.source.resolve()
    root = args.root or source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    runtime = assets / 'official-assets/qa-build/qemu/runtime'
    readelf = source / 'staging_dir/toolchain-aarch64_cortex-a53_gcc-13.3.0_musl/bin/aarch64-openwrt-linux-musl-readelf'
    replacement = args.nghttp2.resolve() if args.nghttp2 else None

    def virtual(name):
        if replacement and posixpath.basename(name) == 'libnghttp2.so.14':
            return replacement
        name = '/' + name.lstrip('/')
        for _ in range(40):
            current = '/'
            parts = Path(name).parts[1:]
            for index, part in enumerate(parts):
                current = posixpath.join(current, part)
                candidate = root / current.lstrip('/')
                if candidate.is_symlink():
                    target = os.readlink(candidate)
                    if not target.startswith('/'):
                        target = posixpath.join(posixpath.dirname(current), target)
                    name = posixpath.normpath(posixpath.join(target, *parts[index + 1:]))
                    break
            else:
                return root / name.lstrip('/')
        raise AssertionError('Symlink loop: ' + name)

    def output(*command):
        return subprocess.run(command, capture_output=True, text=True,
                              check=True).stdout

    data = {}

    def elf(name):
        if name in data:
            return data[name]
        path = virtual(name)
        dynamic = output(str(readelf), '-dW', str(path))
        symbols = output(str(readelf), '-DsW', str(path))
        defined, undefined = set(), set()
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields) < 8 or not fields[0].endswith(':'):
                continue
            bind, visibility, section, symbol = fields[4:8]
            symbol = symbol.split('@', 1)[0]
            if section == 'UND' and bind == 'GLOBAL':
                undefined.add(symbol)
            elif section != 'UND' and bind in ('GLOBAL', 'WEAK') and visibility in ('DEFAULT', 'PROTECTED'):
                defined.add(symbol)
        data[name] = {'needed': re.findall(r'\(NEEDED\).*?\[([^]]+)\]', dynamic),
                      'defined': defined, 'undefined': undefined}
        return data[name]

    search = ['/lib', '/usr/local/lib', '/usr/lib']
    musl_path = root / 'etc/ld-musl-aarch64.path'
    if musl_path.is_file():
        search = re.split(r'[:\n]', musl_path.read_text().strip())
    pending = ['/usr/bin/curl']
    missing_libraries = []
    while pending:
        name = pending.pop()
        if name in data:
            continue
        for needed in elf(name)['needed']:
            candidates = [needed] if '/' in needed else [posixpath.join(directory, needed) for directory in search]
            located = next((path for path in candidates if virtual(path).is_file()), None)
            if located:
                pending.append(located)
            else:
                missing_libraries.append({'file': name, 'needed': needed})
    exports = set().union(*(entry['defined'] for entry in data.values()))
    unresolved = {name: sorted(entry['undefined'] - exports)
                  for name, entry in data.items() if entry['undefined'] - exports}

    package = source / 'bin/packages/aarch64_cortex-a53/packages/libnghttp2-14_1.63.0-r1_aarch64_cortex-a53.ipk'
    package_match = None
    if package.is_file():
        nghttp2 = virtual('/usr/lib/libnghttp2.so.14').read_bytes()
        with tarfile.open(package, 'r:*') as outer:
            payload = next(member for member in outer if Path(member.name).name.startswith('data.tar'))
            with tarfile.open(fileobj=io.BytesIO(outer.extractfile(payload).read()), mode='r:*') as inner:
                member = next(member for member in inner
                              if member.isfile() and Path(member.name).name == 'libnghttp2.so.14.28.2')
                package_match = inner.extractfile(member).read() == nghttp2

    environment = os.environ.copy()
    environment.pop('LD_PRELOAD', None)
    environment['LD_LIBRARY_PATH'] = str(runtime / 'usr/lib/x86_64-linux-gnu')
    command = [str(runtime / 'usr/bin/qemu-aarch64'), '-U', 'LD_LIBRARY_PATH',
               '-U', 'LD_PRELOAD', '-L', str(root)]
    if replacement:
        command += ['-E', 'LD_PRELOAD=' + str(replacement)]
    command += [str(root / 'usr/bin/curl'), '--version']
    executed = subprocess.run(command, env=environment, capture_output=True,
                              text=True, timeout=20)
    result = {
        'passed': not missing_libraries and not unresolved and executed.returncode == 0,
        'isolated_nghttp2_replacement': str(replacement) if replacement else None,
        'closure': sorted(data), 'missing_libraries': missing_libraries,
        'unresolved_symbols': unresolved, 'qemu_returncode': executed.returncode,
        'curl_version': executed.stdout.strip(), 'stderr': executed.stderr.strip(),
        'nghttp2_matches_compiled_ipk': package_match,
        'nghttp2_sha256': hashlib.sha256(virtual('/usr/lib/libnghttp2.so.14').read_bytes()).hexdigest(),
        'image_writes': 0, 'network_requests': 0,
    }
    print(json.dumps(result))
    raise SystemExit(0 if result['passed'] else 1)


if __name__ == '__main__':
    main()
