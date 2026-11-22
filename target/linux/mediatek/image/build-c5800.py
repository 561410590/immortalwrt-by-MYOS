#!/usr/bin/env python3
"""Pack a C5800 sysupgrade image with three top-level files."""
from pathlib import Path
import argparse
import io
import json
import os
import subprocess
import tarfile

VERSION = os.environ.get('MYOS_BUILD_VERSION', '3.1.41-202611061200')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--kernel', type=Path)
    parser.add_argument('--rootfs', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    kdir = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic'
    root = args.rootfs or kdir / 'root-c5800-myos.squashfs'
    kernel = args.kernel or kdir / 'nradio_c5800-688-kernel.bin'
    version = (kdir / 'root-c5800-myos/etc/myos-version').read_text().strip()
    if version.lstrip('V') != VERSION:
        raise RuntimeError('C5800 rootfs version differs from the requested build')
    kernel_data = kernel.read_bytes()
    if kernel_data[:4] != bytes.fromhex('d00dfeed'):
        raise RuntimeError('Expected the source-built C5800 FIT kernel')
    root_data = root.read_bytes()
    if root_data[:4] != b'hsqs':
        raise RuntimeError('Expected the assembled C5800 squashfs')
    root_data += bytes((-len(root_data)) % 1024)
    output = args.output or source / 'bin/targets/mediatek/filogic' / (
        'MYOS-C5800-688-' + VERSION + '-6.6.113-sysupgrade.bin')
    board = 'nradio_c5800-688'
    control = ('BOARD=' + board + '\n').encode()
    with tarfile.open(output, 'w', format=tarfile.USTAR_FORMAT) as archive:
        for name, data in (('CONTROL', control), ('kernel', kernel_data), ('root', root_data)):
            member = tarfile.TarInfo(name)
            member.mode = 0o644
            member.mtime = 1772464398
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
    metadata = {'metadata_version': '1.1', 'compat_version': '1.0',
                'supported_devices': ['nradio,c5800-688', 'HCMT7981-emmc'],
                'version': {'dist': 'MYOS', 'version': VERSION,
                            'revision': 'r33578-c88458e977',
                            'target': 'mediatek/filogic', 'board': board}}
    subprocess.run([str(source / 'staging_dir/host/bin/fwtool'), '-I', '-', str(output)],
                   input=json.dumps(metadata) + '\n', text=True, check=True)
    print(json.dumps({'file': str(output), 'bytes': output.stat().st_size,
                      'version': VERSION, 'kernel_provenance': str(kernel)}), flush=True)


if __name__ == '__main__':
    main()
