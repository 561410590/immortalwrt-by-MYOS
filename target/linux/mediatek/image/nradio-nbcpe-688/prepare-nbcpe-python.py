#!/usr/bin/env python3
"""Prebuild the stock AK68 SMS dependencies instead of shipping a compiler."""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import tarfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    source = args.source
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    root = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    bundles = root / 'usr/share/modem-AK68'
    output = assets / 'python-runtime/usr/lib/python3.9/site-packages'
    build = assets / 'bitarray-build'
    output.mkdir(parents=True, exist_ok=True)
    build.mkdir(parents=True, exist_ok=True)
    with tarfile.open(bundles / 'bitarray-3.0.0.tar.gz') as archive:
        for member in archive.getmembers():
            assert not member.name.startswith('/') and '..' not in Path(member.name).parts
            assert member.isfile() or member.isdir(), member.name
        archive.extractall(build)
    package = build / 'bitarray-3.0.0/bitarray'
    target = output / 'bitarray'
    target.mkdir(exist_ok=True)
    for path in package.iterdir():
        if path.suffix in ('.py', '.pyi', '.pickle') or path.name == 'py.typed':
            shutil.copy2(path, target / path.name)
    gcc = next((source / 'staging_dir').glob('toolchain-aarch64_cortex-a53_*/bin/aarch64-openwrt-linux-musl-gcc'))
    strip = gcc.with_name('aarch64-openwrt-linux-musl-strip')
    headers = root / 'usr/include/python3.9'
    if not (headers / 'Python.h').is_file():
        stock_headers = build / 'stock-headers'
        subprocess.run([str(source / 'staging_dir/host/bin/unsquashfs4'),
                        '-no-progress', '-no-xattrs', '-f', '-d', str(stock_headers),
                        str(assets / 'root.squashfs'), 'usr/include/python3.9'], check=True)
        headers = stock_headers / 'usr/include/python3.9'
    version = (headers / 'patchlevel.h').read_text()
    assert re.search(r'#define\s+PY_MAJOR_VERSION\s+3\b', version)
    assert re.search(r'#define\s+PY_MINOR_VERSION\s+9\b', version)
    for name in ('_bitarray', '_util'):
        extension = target / (name + '.so')
        subprocess.run([str(gcc), '-shared', '-fPIC', '-Os', '-DNDEBUG',
                        '-I' + str(headers), str(package / (name + '.c')),
                        '-o', str(extension)], check=True)
        subprocess.run([str(strip), '--strip-unneeded', str(extension)], check=True)
    for name in ('bitstring-4.3.0-py3-none-any.whl', 'smspdudecoder-2.1.1-py3-none-any.whl'):
        with zipfile.ZipFile(bundles / name) as archive:
            for entry in archive.infolist():
                assert not entry.filename.startswith('/') and '..' not in Path(entry.filename).parts
            archive.extractall(output)
    metadata = output / 'bitarray-3.0.0.dist-info'
    metadata.mkdir(exist_ok=True)
    shutil.copy2(build / 'bitarray-3.0.0/PKG-INFO', metadata / 'METADATA')
    (metadata / 'INSTALLER').write_text('MYOS cross-build\n')
    print('AK68 SMS Python 3.9 dependencies prebuilt:', output, flush=True)


if __name__ == '__main__':
    main()
