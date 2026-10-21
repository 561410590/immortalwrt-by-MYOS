#!/usr/bin/env python3
"""Build an NB68 FIT and sysupgrade TAR using the current MT7981 kernel."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import lzma
import os
import re
import struct
import subprocess
import tarfile
import zlib

VERSION = '3.1.35-202610110100'
KERNEL = '6.6.110'
BOARD = 'nradio_wt9111_No2-manper_NB68'
COMPATIBLE = 'HCMT7981-emmc-NB68'


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def fdt_properties(data):
    assert data[:4] == b'\xd0\x0d\xfe\xed'
    _, size, structure, strings = struct.unpack_from('>4I', data)
    pos, stack, values = structure, [], {}
    while pos < size:
        token = struct.unpack_from('>I', data, pos)[0]
        pos += 4
        if token == 1:
            end = data.index(b'\0', pos)
            stack.append(data[pos:end].decode())
            pos = (end + 4) & ~3
        elif token == 2:
            stack.pop()
        elif token == 3:
            length, offset = struct.unpack_from('>II', data, pos)
            pos += 8
            end = data.index(b'\0', strings + offset)
            name = data[strings + offset:end].decode()
            values[('/'.join(stack), name)] = data[pos:pos + length]
            pos = (pos + length + 3) & ~3
        elif token == 4:
            continue
        elif token == 9:
            break
        else:
            raise ValueError('Bad FDT token')
    return values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path)
    parser.add_argument('--validate-only', action='store_true')
    parser.add_argument('--build-only', action='store_true',
                        help='Pack firmware for subsequent on-device verification')
    args = parser.parse_args()
    if args.build_only and args.validate_only:
        parser.error('--build-only and --validate-only are mutually exclusive')
    source = args.source or Path(__file__).resolve().parents[4]
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    kdir = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic'
    host = source / 'staging_dir/host/bin'
    output = source / 'bin/targets/mediatek/filogic' / ('MYOS-NBCPE-688-' + VERSION + '-6.6.110-multi-theme-sysupgrade.bin')
    fit = assets / 'nbcpe-kernel.itb'
    root = kdir / 'root-nbcpe-myos.squashfs'
    if not args.validate_only:
        linux = kdir / ('linux-' + KERNEL)
        dts = source / 'target/linux/mediatek/dts/mt7981-nradio-nbcpe-688.dts'
        preprocessed = assets / 'nbcpe-6.6.pp.dts'
        dtb = assets / 'nbcpe-6.6.dtb'
        run('gcc', '-E', '-nostdinc', '-undef', '-D__DTS__', '-x', 'assembler-with-cpp',
            '-I', source / 'target/linux/mediatek/dts', '-I', linux / 'include',
            '-I', linux / 'scripts/dtc/include-prefixes',
            '-I', linux / 'arch/arm64/boot/dts',
            '-I', linux / 'arch/arm64/boot/dts/mediatek', dts, '-o', preprocessed)
        run(linux / 'scripts/dtc/dtc', '-I', 'dts', '-O', 'dtb',
            '-o', dtb, preprocessed)
        image = (kdir / 'Image').read_bytes()
        if not args.build_only:
            assert image[56:60] == b'ARM\x64'
        text_offset = struct.unpack_from('<Q', image, 8)[0]
        if not args.build_only:
            assert text_offset == 0, 'FIT load address must follow the ARM64 Image header'
        compressed = assets / 'kernel-6.6.110.lzma'
        encoded = lzma.compress(image, format=lzma.FORMAT_ALONE, filters=[{
            'id': lzma.FILTER_LZMA1, 'dict_size': 8 * 1024 * 1024,
            'lc': 1, 'lp': 2, 'pb': 2, 'mode': lzma.MODE_NORMAL,
            'nice_len': 128, 'mf': lzma.MF_BT4}])
        encoded = encoded[:5] + struct.pack('<Q', len(image)) + encoded[13:]
        if not args.build_only:
            assert lzma.decompress(encoded, format=lzma.FORMAT_ALONE) == image
        compressed.write_bytes(encoded)
        if not args.build_only:
            assert dtb.is_file(), 'Compile the NB68 6.6 device tree first'
            metadata = fdt_properties(dtb.read_bytes())
            assert metadata[('', 'compatible')].split(b'\0')[0].decode() == COMPATIBLE
            assert metadata[('', 'model')].rstrip(b'\0') == b'Nradio-NB68-No2'
            assert b'root=PARTLABEL=rootfs_2nd' in metadata[('/chosen', 'bootargs')]
            ports = {node: struct.unpack('>I', value)[0]
                     for (node, key), value in metadata.items()
                     if key == 'reg' and '/ports/port@' in node and len(value) == 4}
            odu_ports = [node for node, number in ports.items() if number == 5]
            assert len(odu_ports) == 1 and metadata[(odu_ports[0], 'label')] == b'port5\0'
            power = next(node for (node, key), value in metadata.items()
                         if key == 'gpio-export,name' and value == b'cpe-pwr\0')
            assert struct.unpack('>3I', metadata[(power, 'gpios')])[1:] == (4, 0)
            assert metadata[(power, 'gpio-export,output')] == struct.pack('>I', 1)
        its = assets / 'nbcpe-kernel.its'
        its.write_text('''/dts-v1/;
/ {
  description = "ARM64 MYOS NBCPE-688 FIT";
  #address-cells = <1>;
  images {
    kernel-1 {
      description = "ARM64 OpenWrt Linux-6.6.110";
      data = /incbin/("''' + str(compressed) + '''");
      type = "kernel"; arch = "arm64"; os = "linux";
      compression = "lzma"; load = <0x48000000>; entry = <0x48000000>;
      hash-1 { algo = "crc32"; }; hash-2 { algo = "sha1"; };
    };
    fdt-1 {
      description = "ARM64 OpenWrt nradio_wt9111_No2-manper_NB68 device tree blob";
      data = /incbin/("''' + str(dtb) + '''");
      type = "flat_dt"; arch = "arm64"; compression = "none";
      hash-1 { algo = "crc32"; }; hash-2 { algo = "sha1"; };
    };
  };
  configurations {
    default = "config-1";
    config-1 { description = "OpenWrt"; kernel = "kernel-1"; fdt = "fdt-1"; };
  };
};
''')
        env = dict(os.environ, PATH=str(kdir / ('linux-' + KERNEL) / 'scripts/dtc') + ':' + os.environ['PATH'])
        run(host / 'mkimage', '-f', its, fit, env=env)
        run('sh', source / 'scripts/nradio-fit-rootfs-metadata.sh', fit, root, BOARD)
        control = ('BOARD=' + BOARD + '\n').encode()
        root_data = root.read_bytes()
        root_data += bytes((-len(root_data)) % 1024)
        with tarfile.open(output, 'w', format=tarfile.USTAR_FORMAT) as archive:
            prefix = 'sysupgrade-' + BOARD
            directory = tarfile.TarInfo(prefix)
            directory.type = tarfile.DIRTYPE
            directory.mode = 0o755
            archive.addfile(directory)
            for name, data in ((prefix + '/CONTROL', control),
                               (prefix + '/kernel', fit.read_bytes()),
                               (prefix + '/root', root_data),
                               ('sysupgrade-' + COMPATIBLE + '/CONTROL', control)):
                member = tarfile.TarInfo(name)
                member.size = len(data)
                member.mode = 0o644
                archive.addfile(member, io.BytesIO(data))
        manifest = {'metadata_version': '1.1', 'compat_version': '1.0',
                    'supported_devices': [COMPATIBLE], 'version': {
                        'dist': 'MYOS', 'version': VERSION, 'revision': 'r33578-c88458e977',
                        'target': 'mediatek/filogic', 'board': BOARD}}
        json_path = assets / 'firmware-metadata.json'
        json_path.write_text(json.dumps(manifest) + '\n')
        run(host / 'fwtool', '-I', json_path, output)

    if args.build_only:
        print(json.dumps({'file': str(output), 'bytes': output.stat().st_size,
                          'version': VERSION, 'built': True}), flush=True)
        return

    metadata = json.loads(run(host / 'fwtool', '-i', '-', output, stdout=subprocess.PIPE).stdout)
    assert metadata['supported_devices'] == [COMPATIBLE]
    assert metadata['version']['version'] == VERSION
    with tarfile.open(output, 'r:') as archive:
        kernels = [m for m in archive.getmembers() if m.name.endswith('/kernel')]
        roots = [m for m in archive.getmembers() if m.name.endswith('/root')]
        assert len(kernels) == len(roots) == 1
        assert archive.getmember('sysupgrade-' + COMPATIBLE + '/CONTROL').size == len('BOARD=' + BOARD + '\n')
        kernel_data = archive.extractfile(kernels[0]).read()
        root_data = archive.extractfile(roots[0]).read()
    fit_values = fdt_properties(kernel_data)
    squashfs_size = struct.unpack_from('<Q', root_data, 40)[0]
    root_data = root_data[:squashfs_size]
    assert root_data[:4] == b'hsqs'
    assert int(fit_values[('', 'rootfs_size')].rstrip(b'\0')) == squashfs_size
    assert fit_values[('', 'rootfs_crc32')].rstrip(b'\0').decode() == f'{zlib.crc32(root_data) & 0xffffffff:08x}'
    assert fit_values[('/rootfs/hash-2', 'value')] == hashlib.sha1(root_data).digest()
    for node in ('/images/kernel-1', '/images/fdt-1'):
        data = fit_values[(node, 'data')]
        assert fit_values[(node + '/hash-1', 'value')] == struct.pack('>I', zlib.crc32(data) & 0xffffffff)
        assert fit_values[(node + '/hash-2', 'value')] == hashlib.sha1(data).digest()
    embedded_dtb = fdt_properties(fit_values[('/images/fdt-1', 'data')])
    assert embedded_dtb[('', 'compatible')].split(b'\0')[0].decode() == COMPATIBLE
    assert root.read_bytes()[:squashfs_size] == root_data
    # Inspect permissions in the packed filesystem, not just the assembly tree.
    permission_check = run(host / 'unsquashfs4', '-lls', root,
                           'etc/board.d/02_network', 'etc/uci-defaults/98-nbcpe-network',
                           stdout=subprocess.PIPE).stdout.decode()
    for relative in ('etc/board.d/02_network', 'etc/uci-defaults/98-nbcpe-network'):
        entries = [line for line in permission_check.splitlines() if line.endswith('/' + relative)]
        assert len(entries) == 1 and entries[0].startswith('-rwxr-xr-x'), relative
    for relative, pattern in (
            ('etc/openwrt_release', r"^DISTRIB_RELEASE=['\"]([^'\"]+)['\"]$"),
            ('usr/lib/os-release', r'^VERSION_ID="([^"]+)"$'),
            ('etc/myos-version', r'^(.+)$')):
        text = run(host / 'unsquashfs4', '-cat', root, relative,
                   stdout=subprocess.PIPE).stdout.decode().strip()
        match = re.search(pattern, text, re.M)
        assert match and match[1] == VERSION, 'Packed version differs: ' + relative
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    report = {'file': str(output), 'bytes': output.stat().st_size, 'sha256': digest,
              'kernel': KERNEL, 'version': VERSION, 'board': COMPATIBLE,
              'rootfs_bytes': squashfs_size, 'fit_hashes_valid': True,
              'rootfs_hashes_valid': True, 'metadata_valid': True,
              'packed_version_consistent': True,
              'packed_network_and_reporting_permissions_valid': True}
    (assets / 'image-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    output.with_suffix(output.suffix + '.sha256').write_text(digest + '  ' + output.name + '\n')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
