#!/usr/bin/python3
"""One-time migration of the verified NBCPE 0800 RAM upper to its p9 tail.

Default is read-only. --migrate prepares the final persistent overlay and
preserves the running RAM root until the operator performs a separate reboot.
"""

import argparse
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess


DEVICE = Path('/dev/mmcblk0p9')
EXPECTED_SIZE = 7041712128
EXPECTED_BYTES_USED = 50601411
EXPECTED_OFFSET = 50659328
SOURCE = Path('/tmp/root/upper')
DESTINATION = Path('/tmp/nbcpe-overlay-migrate')
LOOP_SET_FD = 0x4C00
LOOP_CLR_FD = 0x4C01
LOOP_SET_STATUS64 = 0x4C04
LOOP_GET_STATUS64 = 0x4C05
LOOP_CTL_GET_FREE = 0x4C82
LOOP_INFO = '<QQQQQIIII64s64s32sQQ'
LIBC = ctypes.CDLL(None, use_errno=True)
for name, arguments, result in (
        ('llistxattr', [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t], ctypes.c_ssize_t),
        ('lgetxattr', [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t], ctypes.c_ssize_t),
        ('lsetxattr', [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int], ctypes.c_int),
        ('lremovexattr', [ctypes.c_char_p, ctypes.c_char_p], ctypes.c_int)):
    function = getattr(LIBC, name)
    function.argtypes = arguments
    function.restype = result


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def xattr_result(result, path):
    if result < 0:
        code = ctypes.get_errno()
        raise OSError(code, os.strerror(code), str(path))
    return result


def read_xattrs(path):
    # The stock Python 3.9 omits os.*xattr; musl exposes the no-follow libc API.
    encoded = os.fsencode(path)
    size = xattr_result(LIBC.llistxattr(encoded, None, 0), path)
    if not size:
        return {}
    names = ctypes.create_string_buffer(size)
    count = xattr_result(LIBC.llistxattr(encoded, names, size), path)
    attrs = {}
    for name in names.raw[:count].split(b'\0'):
        if not name:
            continue
        size = xattr_result(LIBC.lgetxattr(encoded, name, None, 0), path)
        value = ctypes.create_string_buffer(max(size, 1))
        count = xattr_result(LIBC.lgetxattr(encoded, name, value, size), path)
        attrs[name] = value.raw[:count]
    return attrs


def set_xattr(path, name, value):
    value_buffer = ctypes.create_string_buffer(value)
    xattr_result(LIBC.lsetxattr(os.fsencode(path), os.fsencode(name),
                              value_buffer, len(value), 0), path)


def remove_xattr(path, name):
    xattr_result(LIBC.lremovexattr(os.fsencode(path), os.fsencode(name)), path)


def squashfs_layout(header, partition_size):
    require(header[:4] == b'hsqs' and len(header) >= 48, 'Not a squashfs superblock')
    used = struct.unpack_from('<Q', header, 40)[0]
    offset = (used + 65535) & ~65535
    require(0 < used <= offset < partition_size, 'Invalid squashfs/overlay boundary')
    return used, offset


def uninitialized_tail(prefix):
    # This 0800 image leaves older compressed payload beyond the first block.
    # fstools sees FS_NONE: its first 4 KiB contain no superblock/signature,
    # and neither F2FS superblock copy contains the F2FS magic.
    magic = struct.pack('<I', 0xF2F52010)
    require(len(prefix) >= 8192, 'Cannot read both F2FS superblock blocks')
    require(not any(prefix[:4096]), 'Overlay first block already contains data/signature')
    require(prefix[1024:1028] != magic and prefix[5120:5124] != magic,
            'Overlay tail already contains an F2FS superblock')


def mounts():
    return [line.split() for line in Path('/proc/mounts').read_text().splitlines()]


def check_layout(resume=False):
    require(os.geteuid() == 0, 'Run as root')
    require(DEVICE.resolve() == DEVICE and stat.S_ISBLK(DEVICE.stat().st_mode), 'Unexpected p9 device')
    uevent = Path('/sys/class/block/mmcblk0p9/uevent').read_text()
    require('PARTNAME=rootfs_2nd\n' in uevent, 'p9 is not rootfs_2nd')
    require('root=PARTLABEL=rootfs_2nd' in Path('/proc/cmdline').read_text().split(), 'Wrong active system')
    mounted = mounts()
    rom = next((row for row in mounted if row[1] == '/rom'), None)
    require(rom is not None and rom[2] == 'squashfs', '/rom is not squashfs')
    require(os.stat('/rom').st_dev == DEVICE.stat().st_rdev, '/rom does not use p9')
    root = next((row for row in mounted if row[1] == '/'), None)
    require(root is not None and root[0] == 'overlayfs:/tmp/root' and root[2] == 'overlay', 'Root is not the expected RAM overlay')
    require('upperdir=/tmp/root/upper' in root[3].split(','), 'Unexpected live upper')
    require(not any(row[1] == '/overlay' for row in mounted), 'A persistent overlay is already mounted')
    require(SOURCE.is_dir() and not SOURCE.is_symlink(), 'RAM upper missing')
    size = int(Path('/sys/class/block/mmcblk0p9/size').read_text()) * 512
    with DEVICE.open('rb', buffering=0) as stream:
        used, offset = squashfs_layout(stream.read(96), size)
        stream.seek(offset)
        prefix = stream.read(65536)
    require(size == EXPECTED_SIZE, 'p9 partition size changed')
    require(used == EXPECTED_BYTES_USED and offset == EXPECTED_OFFSET, '0800 squashfs boundary changed')
    if resume:
        require(prefix[1024:1028] == struct.pack('<I', 0xF2F52010), 'Resume requires the initialized F2FS tail')
    else:
        uninitialized_tail(prefix)
    return {'device': str(DEVICE), 'partition_size': size, 'squashfs_bytes_used': used,
            'offset': offset, 'overlay_size': size - offset, 'current_upper': str(SOURCE)}


def metadata(path):
    st = path.lstat()
    attrs = read_xattrs(path)
    payload = None
    if stat.S_ISREG(st.st_mode):
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1048576), b''):
                digest.update(chunk)
        payload = (st.st_size, digest.digest())
    elif stat.S_ISLNK(st.st_mode):
        payload = os.readlink(path)
    elif stat.S_ISCHR(st.st_mode) or stat.S_ISBLK(st.st_mode):
        payload = st.st_rdev
    else:
        require(stat.S_ISDIR(st.st_mode) or stat.S_ISFIFO(st.st_mode), 'Unsupported upper node: ' + str(path))
    return (st.st_mode, st.st_uid, st.st_gid, payload, attrs)


def paths(root):
    yield root
    for directory, subdirs, files in os.walk(root, followlinks=False):
        for name in sorted(subdirs + files):
            yield Path(directory) / name


def copy_upper(source, target):
    require(not target.exists(), 'Migration destination upper is not empty')
    subprocess.run(['cp', '-a', str(source), str(target)], check=True)
    # BusyBox cp -a preserves whiteout nodes, ownership and symlinks but not
    # trusted.overlay.opaque. Carry every xattr explicitly, including dirs.
    for old in paths(source):
        new = target / old.relative_to(source)
        attrs = read_xattrs(old)
        for name in read_xattrs(new):
            if name not in attrs:
                remove_xattr(new, name)
        for name, value in attrs.items():
            set_xattr(new, name, value)
    old_names = {str(path.relative_to(source)) for path in paths(source)}
    new_names = {str(path.relative_to(target)) for path in paths(target)}
    require(old_names == new_names, 'Upper names changed during migration')
    for name in sorted(old_names):
        require(metadata(source / name) == metadata(target / name), 'Upper changed or metadata mismatch: ' + name)
    for essential in ('etc/config/network', 'etc/config/dropbear', 'etc/shadow'):
        live = Path('/') / essential
        final = target / essential
        if final.exists():
            require(live.read_bytes() == final.read_bytes(), 'Live configuration mismatch: ' + essential)
        else:
            require(live.read_bytes() == (Path('/rom') / essential).read_bytes(), 'Configuration absent from upper: ' + essential)
    return len(old_names)


def migrate(layout, resume=False):
    require(not DESTINATION.exists(), 'Migration mount point already exists')
    require(Path('/usr/sbin/mkfs.f2fs').is_file(), 'mkfs.f2fs missing')
    subprocess.run(['/lib/ld-musl-aarch64.so.1', '--list', '/usr/sbin/mkfs.f2fs'], check=True,
                   stdout=subprocess.DEVNULL)
    require(check_layout(resume) == layout, 'Device layout changed before migration')
    control = os.open('/dev/loop-control', os.O_RDWR)
    loop_fd = disk_fd = None
    mounted = False
    attached = False
    loop_path = None
    try:
        number = fcntl.ioctl(control, LOOP_CTL_GET_FREE)
        loop_path = '/dev/loop' + str(number)
        loop_fd = os.open(loop_path, os.O_RDWR)
        disk_fd = os.open(str(DEVICE), os.O_RDWR)
        fcntl.ioctl(loop_fd, LOOP_SET_FD, disk_fd)
        attached = True
        info = struct.pack(LOOP_INFO, 0, 0, 0, layout['offset'], layout['overlay_size'],
                           number, 0, 0, 4, bytes(DEVICE)[:63], b'', b'', 0, 0)
        fcntl.ioctl(loop_fd, LOOP_SET_STATUS64, info)
        actual = struct.unpack(LOOP_INFO, fcntl.ioctl(loop_fd, LOOP_GET_STATUS64, b'\0' * 232))
        require(actual[3:5] == (layout['offset'], layout['overlay_size']), 'Loop boundary differs from p9 tail')
        require(actual[9].split(b'\0', 1)[0] == bytes(DEVICE), 'Loop backing path differs')
        # This is the only formatting command. Its bound loop cannot address
        # the squashfs prefix or any neighbouring partition.
        if not resume:
            subprocess.run(['/usr/sbin/mkfs.f2fs', '-q', '-f', '-l', 'rootfs_data', loop_path], check=True)
        with DEVICE.open('rb', buffering=0) as stream:
            stream.seek(layout['offset'] + 1024)
            require(stream.read(4) == struct.pack('<I', 0xF2F52010), 'F2FS magic missing after initialization')
        DESTINATION.mkdir(mode=0o700)
        subprocess.run(['mount', '-t', 'f2fs', '-o', 'noatime', loop_path, str(DESTINATION)], check=True)
        mounted = True
        require(any(row[0] == loop_path and row[1] == str(DESTINATION) and row[2] == 'f2fs'
                    for row in mounts()), 'Migration target is not the bound F2FS loop')
        marker = DESTINATION / '.nbcpe-migration-pending'
        if resume:
            require(marker.is_file() and json.loads(marker.read_text()) == layout, 'Not this migration target')
            require(not os.path.lexists(DESTINATION / '.fs_state'), 'READY overlay must not be overwritten')
            previous = DESTINATION / 'upper'
            require(not previous.is_symlink(), 'Unexpected upper symlink')
            if previous.exists():
                shutil.rmtree(previous)
            work = DESTINATION / 'work'
            if work.exists():
                require(work.is_dir() and not work.is_symlink() and not any(work.iterdir()), 'Unexpected pending work directory')
                work.rmdir()
        else:
            marker.write_text(json.dumps(layout))
        count = copy_upper(SOURCE, DESTINATION / 'upper')
        (DESTINATION / 'work').mkdir(mode=0o755)
        os.symlink('2', DESTINATION / '.fs_state')
        marker.unlink()
        os.sync()
        subprocess.run(['umount', str(DESTINATION)], check=True)
        mounted = False
        DESTINATION.rmdir()
        print(json.dumps({'persistent_overlay_prepared': True, 'verified_upper_nodes': count,
                          'reboot_required': True, 'running_root_unchanged': True}))
    finally:
        if mounted:
            subprocess.run(['umount', str(DESTINATION)], check=False)
        if attached:
            try:
                fcntl.ioctl(loop_fd, LOOP_CLR_FD)
            except OSError:
                pass  # Autoclear still applies if the mount could not detach.
        if loop_fd is not None:
            os.close(loop_fd)
        if disk_fd is not None:
            os.close(disk_fd)
        os.close(control)
        if DESTINATION.exists() and not any(row[1] == str(DESTINATION) for row in mounts()):
            DESTINATION.rmdir()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--migrate', action='store_true')
    modes.add_argument('--resume', action='store_true', help='Recopy only a verified pending target; never reformat')
    args = parser.parse_args()
    layout = check_layout(args.resume)
    print(json.dumps(layout), flush=True)
    if args.migrate or args.resume:
        migrate(layout, args.resume)


if __name__ == '__main__':
    main()
