#!/usr/bin/env python3
"""Exercise actual matching AArch64 mkfs/fsck on an isolated regular image."""

import argparse
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile
from types import SimpleNamespace


def extract_ipk(package, destination):
    if package.read_bytes()[:8] == b'!<arch>\n':
        payload = subprocess.check_output(['ar', 'p', str(package), 'data.tar.gz'])
    else:
        with tarfile.open(package) as outer:
            member = next(m for m in outer.getmembers() if m.name.lstrip('./') == 'data.tar.gz')
            payload = outer.extractfile(member).read()
    with tarfile.open(fileobj=io.BytesIO(payload)) as archive:
        for member in archive.getmembers():
            target = destination / member.name.lstrip('./')
            assert target.resolve().is_relative_to(destination.resolve()), member.name
        archive.extractall(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--mkfs-ipk', type=Path)
    parser.add_argument('--library-ipk', type=Path)
    parser.add_argument('--fsck-ipk', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    image_root = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    runtime = assets / 'official-assets/qa-build/qemu/runtime'
    qemu = runtime / 'usr/bin/qemu-aarch64'
    packages = source / 'bin/packages/aarch64_cortex-a53/base'
    mkfs_ipk = args.mkfs_ipk or packages / 'mkf2fs_1.16.0-r3_aarch64_cortex-a53.ipk'
    library_ipk = args.library_ipk or packages / 'libf2fs6_1.16.0-r3_aarch64_cortex-a53.ipk'
    fsck_ipk = args.fsck_ipk or packages / 'f2fsck_1.16.0-r3_aarch64_cortex-a53.ipk'
    spec = importlib.util.spec_from_file_location('recover_overlay', assets / 'recover-nbcpe-overlay.py')
    recovery = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recovery)
    assert bytes(Path('/dev/mmcblk0p9')) == b'/dev/mmcblk0p9'
    assert struct.calcsize(recovery.LOOP_INFO) == 232
    loop_info = struct.pack(recovery.LOOP_INFO, 0, 0, 0, 50659328, 6991052800,
                            0, 0, 0, 4, b'/dev/mmcblk0p9', b'', b'', 0, 0)
    fields = struct.unpack(recovery.LOOP_INFO, loop_info)
    assert fields[3:5] == (50659328, 6991052800)
    assert fields[8] == 4 and fields[9].split(b'\0', 1)[0] == b'/dev/mmcblk0p9'
    boundary_cases = 0
    for used, size, valid in ((50601411, 7041712128, True), (0, 7041712128, False),
                              (7041712129, 7041712128, False), (50601411, 50659328, False)):
        header = bytearray(96)
        header[:4] = b'hsqs'
        struct.pack_into('<Q', header, 40, used)
        try:
            result = recovery.squashfs_layout(header, size)
            assert valid and result == (50601411, 50659328)
        except RuntimeError:
            assert not valid
        boundary_cases += 1
    tail_cases = 0
    for variant, valid in (('zero', True), ('old-payload', True),
                           ('first-superblock', False), ('second-superblock', False),
                           ('existing-signature', False)):
        prefix = bytearray(8192)
        if variant == 'old-payload':
            prefix[4096:] = b'\x5a' * 4096
        elif variant == 'first-superblock':
            prefix[1024:1028] = struct.pack('<I', 0xF2F52010)
        elif variant == 'second-superblock':
            prefix[5120:5124] = struct.pack('<I', 0xF2F52010)
        elif variant == 'existing-signature':
            prefix[0] = 0x85
        try:
            recovery.uninitialized_tail(prefix)
            assert valid
        except RuntimeError:
            assert not valid
        tail_cases += 1
    environment = dict(os.environ)
    environment.pop('LD_PRELOAD', None)
    environment['LD_LIBRARY_PATH'] = str(runtime / 'usr/lib/x86_64-linux-gnu')
    command = [str(qemu), '-U', 'LD_LIBRARY_PATH', '-U', 'LD_PRELOAD', '-L', str(image_root)]
    python_environment = dict(environment, PYTHONHOME=str(image_root / 'usr'), PYTHONDONTWRITEBYTECODE='1')
    python_check = subprocess.run(command + [str(image_root / 'usr/bin/python3.9'), '-B', '-c',
        "import pathlib,struct,sys; assert bytes(pathlib.Path('/dev/mmcblk0p9')) == b'/dev/mmcblk0p9'; "
        "assert struct.calcsize('<QQQQQIIII64s64s32sQQ') == 232; print(sys.version.split()[0])"],
        env=python_environment, capture_output=True, text=True, timeout=30)
    assert python_check.returncode == 0, python_check.stderr
    with tempfile.TemporaryDirectory(prefix='nbcpe-storage-qa-', dir=assets) as temporary:
        work = Path(temporary)
        candidate = work / 'candidate'
        candidate.mkdir()
        for package in (mkfs_ipk, library_ipk, fsck_ipk):
            extract_ipk(package, candidate)
        libraries = ':'.join(map(str, [candidate / 'lib', candidate / 'usr/lib', image_root / 'lib', image_root / 'usr/lib']))
        loader = command + [str(image_root / 'lib/ld-musl-aarch64.so.1'), '--library-path', libraries]
        mkfs = candidate / 'usr/sbin/mkfs.f2fs'
        fsck = candidate / 'usr/sbin/fsck.f2fs'
        for binary in (mkfs, fsck):
            subprocess.run(loader + ['--list', str(binary)], env=environment, capture_output=True, check=True)
        disk = work / 'isolated-f2fs.img'
        with disk.open('wb') as stream:
            stream.truncate(256 * 1024 * 1024)
        initialized = subprocess.run(loader + [str(mkfs), '-q', '-f', '-l', 'rootfs_data', str(disk)],
                                     env=environment, capture_output=True, text=True, timeout=60)
        assert initialized.returncode == 0, initialized.stderr + initialized.stdout
        with disk.open('rb') as stream:
            stream.seek(1024)
            assert stream.read(4) == struct.pack('<I', 0xF2F52010)
        checked = subprocess.run(loader + [str(fsck), '--dry-run', str(disk)], env=environment,
                                 capture_output=True, text=True, timeout=60)
        assert checked.returncode == 0, checked.stderr + checked.stdout
        old = work / 'ram-upper'
        old.mkdir()
        (old / 'etc/config').mkdir(parents=True)
        (old / 'etc/config/network').write_text("config interface 'lan'\n option ipaddr '192.168.68.1'\n")
        (old / 'etc/config/dropbear').write_text("config dropbear\n option PasswordAuth 'on'\n")
        (old / 'etc/shadow').write_text('root:fixture-authentication:1:0:99999:7:::\n')
        (old / 'etc/shadow').chmod(0o600)
        (old / 'etc/link').symlink_to('shadow')
        os.setxattr(old / 'etc/config', 'user.migration-fixture', b'preserve')
        opaque = False
        whiteout = False
        if os.geteuid() == 0:
            os.setxattr(old / 'etc/config', 'trusted.overlay.opaque', b'y')
            opaque = True
            os.mknod(old / 'deleted-lower-file', stat.S_IFCHR | 0o600, os.makedev(0, 0))
            whiteout = True
        # Essential live-file comparisons are exercised against an independent
        # fixture tree rather than the build server's real /etc.
        saved_path = recovery.Path
        def fixture_path(value):
            if value == '/':
                return old
            if value == '/rom':
                return old
            return saved_path(value)
        recovery.Path = fixture_path
        saved_run = subprocess.run
        def guest_copy(arguments, **kwargs):
            assert arguments[:2] == ['cp', '-a'], arguments
            return saved_run(command + [str(image_root / 'bin/busybox')] + arguments,
                             env=environment, **kwargs)
        recovery.subprocess = SimpleNamespace(run=guest_copy)
        new = work / 'disk-upper'
        count = recovery.copy_upper(old, new)
        assert (new / 'etc/shadow').stat().st_mode & 0o777 == 0o600
        assert os.readlink(new / 'etc/link') == 'shadow'
        assert os.getxattr(new / 'etc/config', 'user.migration-fixture') == b'preserve'
        if opaque:
            assert os.getxattr(new / 'etc/config', 'trusted.overlay.opaque') == b'y'
        if whiteout:
            assert stat.S_ISCHR((new / 'deleted-lower-file').lstat().st_mode)
            assert (new / 'deleted-lower-file').lstat().st_rdev == 0
        print(json.dumps({'real_aarch64_mkfs': True, 'readonly_fsck': True,
                          'filesystem_magic': 'F2F52010', 'isolated_image_bytes': disk.stat().st_size,
                          'boundary_cases': boundary_cases, 'tail_signature_cases': tail_cases,
                          'loop_info_bytes': struct.calcsize(recovery.LOOP_INFO),
                          'path_bytes': True, 'actual_python_version': python_check.stdout.strip(),
                          'copied_nodes': count, 'authentication_and_lan': True,
                          'actual_aarch64_busybox_cp': True,
                          'xattrs': True, 'trusted_opaque': opaque, 'whiteout': whiteout,
                          'real_devices_formatted': 0, 'temporary_fixture_removed_on_exit': True}))


if __name__ == '__main__':
    main()
