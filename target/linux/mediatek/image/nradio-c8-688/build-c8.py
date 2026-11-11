#!/usr/bin/env python3
"""Compile the C8 DTB and pack its independent second-system FIT/sysupgrade."""
import argparse
import hashlib
import io
import json
import lzma
import os
from pathlib import Path
import struct
import subprocess
import tarfile

VERSION = '3.1.41-202611061200'
KERNEL = '6.6.113'
BOARD = 'HCMT7981-688-emmc'


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--kernel-image', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    assets = source / 'target/linux/mediatek/image/nradio-c8-688'
    kdir = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic'
    linux = kdir / ('linux-' + KERNEL)
    host = source / 'staging_dir/host/bin'
    root = assets / ('root-c8-myos-' + VERSION + '.squashfs')
    dts = source / 'target/linux/mediatek/dts/mt7981-nradio-c8-688.dts'
    pp = assets / 'c8-6.6.pp.dts'
    dtb = assets / 'c8-6.6.dtb'
    run('gcc', '-E', '-nostdinc', '-undef', '-D__DTS__', '-x', 'assembler-with-cpp',
        '-I', source / 'target/linux/mediatek/dts', '-I', linux / 'include',
        '-I', linux / 'scripts/dtc/include-prefixes', '-I', linux / 'arch/arm64/boot/dts',
        '-I', linux / 'arch/arm64/boot/dts/mediatek', dts, '-o', pp)
    run(linux / 'scripts/dtc/dtc', '-I', 'dts', '-O', 'dtb', '-o', dtb, pp)
    kernel_image = args.kernel_image.resolve() if args.kernel_image else kdir / 'Image'
    image = kernel_image.read_bytes()
    if image[56:60] != b'ARM\x64' or struct.unpack_from('<Q', image, 8)[0] != 0:
        raise RuntimeError('C8 FIT load address requires this ARM64 Image with text_offset=0')
    encoded = lzma.compress(image, format=lzma.FORMAT_ALONE, filters=[{
        'id': lzma.FILTER_LZMA1, 'dict_size': 8 * 1024 * 1024,
        'lc': 1, 'lp': 2, 'pb': 2, 'mode': lzma.MODE_NORMAL,
        'nice_len': 128, 'mf': lzma.MF_BT4}])
    encoded = encoded[:5] + struct.pack('<Q', len(image)) + encoded[13:]
    if lzma.decompress(encoded, format=lzma.FORMAT_ALONE) != image:
        raise RuntimeError('C8 kernel compression check failed')
    compressed = assets / ('kernel-' + KERNEL + '.lzma')
    compressed.write_bytes(encoded)
    its = assets / 'c8-kernel.its'
    its.write_text('''/dts-v1/;
/ {
 description = "MYOS C8-688 ARM64 FIT";
 #address-cells = <1>;
 images {
  kernel-1 {
   description = "ARM64 OpenWrt Linux-''' + KERNEL + '''";
   data = /incbin/("''' + str(compressed) + '''");
   type = "kernel"; arch = "arm64"; os = "linux"; compression = "lzma";
   load = <0x48000000>; entry = <0x48000000>;
   hash-1 { algo = "crc32"; }; hash-2 { algo = "sha1"; };
  };
  fdt-1 {
   description = "NRadio-C8-New688 device tree";
   data = /incbin/("''' + str(dtb) + '''");
   type = "flat_dt"; arch = "arm64"; compression = "none";
   hash-1 { algo = "crc32"; }; hash-2 { algo = "sha1"; };
  };
 };
 configurations {
  default = "config-1";
  config-1 { description = "C8-688 second system"; kernel = "kernel-1"; fdt = "fdt-1"; };
 };
};
''', encoding='utf-8')
    fit = assets / 'c8-kernel.itb'
    env = dict(os.environ, PATH=str(linux / 'scripts/dtc') + ':' + os.environ['PATH'])
    run(host / 'mkimage', '-f', its, fit, env=env)
    run('sh', source / 'scripts/nradio-fit-rootfs-metadata.sh', fit, root, BOARD)
    output = args.output.resolve() if args.output else source / 'bin/targets/mediatek/filogic' / (
        'MYOS-C8-688-' + VERSION + '-' + KERNEL + '-sysupgrade.bin')
    root_data = root.read_bytes()
    root_data += bytes((-len(root_data)) % 1024)
    prefix = 'sysupgrade-' + BOARD
    with tarfile.open(output, 'w', format=tarfile.USTAR_FORMAT) as archive:
        directory = tarfile.TarInfo(prefix)
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o755
        archive.addfile(directory)
        for name, data in (('CONTROL', ('BOARD=' + BOARD + '\n').encode()),
                           ('kernel', fit.read_bytes()), ('root', root_data)):
            member = tarfile.TarInfo(prefix + '/' + name)
            member.size = len(data)
            member.mode = 0o644
            archive.addfile(member, io.BytesIO(data))
    metadata = {'metadata_version': '1.1', 'compat_version': '1.0',
                'supported_devices': [BOARD], 'version': {
                    'dist': 'MYOS', 'version': VERSION, 'revision': 'r33578-c88458e977',
                    'target': 'mediatek/filogic', 'board': BOARD}}
    manifest = assets / 'firmware-metadata.json'
    manifest.write_text(json.dumps(metadata) + '\n', encoding='utf-8')
    run(host / 'fwtool', '-I', manifest, output)
    report = {'file': str(output), 'bytes': output.stat().st_size,
              'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'kernel': KERNEL, 'version': VERSION, 'board': BOARD,
              'kernel_provenance': str(kernel_image), 'live_device_validation': 'pending'}
    (assets / 'build-result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
