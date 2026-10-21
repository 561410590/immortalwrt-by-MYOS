#!/usr/bin/env python3
"""Exercise the packaged platform/emmc scripts against disposable file devices.

The partition sizes are the FC83C618430C official 2.3.5 observation. GPT status,
blkid and jsonfilter are fixtures; shell, tar and dd are real host BusyBox.
This verifies upgrade routing, payload bytes and config/reset offsets. It does
not emulate ARM boot, the kernel block driver, or F2FS mount/format behavior.
No host block devices or router connection are exposed to the chroot.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile

from check_boot_identity import put


def digest(path, length):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        while length:
            data = stream.read(min(length, 1024 * 1024))
            assert data, (path, "short payload")
            result.update(data)
            length -= len(data)
    return result.hexdigest()


def run(image, firmware_root, keep, expected, offset):
    with tempfile.TemporaryDirectory(prefix="ultra-sd-upgrade-") as directory:
        root = Path(directory)
        busybox = Path(shutil.which("busybox")).resolve()
        put(root, "/bin/busybox", busybox.read_bytes(), 0o755)
        linked = subprocess.check_output(["ldd", str(busybox)], text=True)
        for library in set(re.findall(r"/[^\s()]+", linked)):
            put(root, library, Path(library).read_bytes(), 0o755)
        for app in ("sh", "dd", "tar", "grep", "cut", "wc", "cat", "sync"):
            (root / "bin" / app).symlink_to("busybox")
        for name in ("dev", "tmp", "lib/upgrade", "usr/libexec"):
            (root / name).mkdir(parents=True, exist_ok=True)
        os.mknod(root / "dev/null", stat.S_IFCHR | 0o666, os.makedev(1, 3))
        os.mknod(root / "dev/zero", stat.S_IFCHR | 0o666, os.makedev(1, 5))
        guards = {}
        for part in ("mmcblk0", "mmcblk0p1", "mmcblk0p2"):
            guards[part] = ("preserve-" + part).encode() * 1024
            put(root, "/dev/" + part, guards[part])
        for number, sectors in ((3, 262144), (4, 64090079)):
            put(root, f"/sys/class/block/mmcblk0p{number}/size", str(sectors))
            put(root, f"/dev/mmcblk0p{number}", b"old-data" * 1024)
        # The exact delivered files execute unchanged; only external probes are fixtures.
        for name in ("platform.sh", "emmc.sh"):
            put(root, "/lib/upgrade/" + name,
                (firmware_root / "lib/upgrade" / name).read_bytes())
        put(root, "/lib/functions.sh", "")
        put(root, "/usr/libexec/myos-ultra-gpt", """#!/bin/sh
[ "$1" = status ] || exit 1
echo '{"ok":true,"layout":"official","backup_gpt_valid":true}'
""", 0o755)
        put(root, "/bin/jsonfilter", """#!/bin/sh
cat >/dev/null
case "$*" in
 *backup_gpt_valid*) echo true ;;
 *layout*) echo official ;;
 *) exit 1 ;;
esac
""", 0o755)
        put(root, "/bin/blkid", """#!/bin/sh
case "$*" in
 *mmcblk0p3) echo kernel ;;
 *mmcblk0p4) echo rootfs ;;
 *) exit 1 ;;
esac
""", 0o755)
        shutil.copyfile(image, root / "image.bin")
        backup = b"synthetic-config-backup\n" * 53
        put(root, "/backup.tgz", backup)
        put(root, "/run-upgrade", """#!/bin/sh
. /lib/upgrade/emmc.sh
. /lib/upgrade/platform.sh
get_magic_long() { echo fixture-ustar; }
identify_magic_long() { echo tar; }
platform_check_image /image.bin || exit 41
[ "$1" = keep ] && export UPGRADE_BACKUP=/backup.tgz
platform_do_upgrade /image.bin || exit 42
[ "$1" != keep ] || platform_copy_config || exit 43
printf '%s\n' "$EMMC_KERN_DEV" "$EMMC_ROOT_DEV" "$EMMC_ROOTFS_BLOCKS" > /result
""", 0o755)
        result = subprocess.run([shutil.which("chroot"), str(root), "/bin/sh",
                                 "/run-upgrade", "keep" if keep else "reset"],
                                env={"PATH": "/bin", "LC_ALL": "C"},
                                capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stderr
        values = (root / "result").read_text().splitlines()
        assert values == ["/dev/mmcblk0p3", "/dev/mmcblk0p4", str(offset // 512)], values
        for part, guard in guards.items():
            assert (root / "dev" / part).read_bytes() == guard, (part, "modified")
        for name, number in (("kernel", 3), ("root", 4)):
            size, sha256 = expected[name]
            assert digest(root / f"dev/mmcblk0p{number}", size) == sha256, name
        with (root / "dev/mmcblk0p4").open("rb") as stream:
            stream.seek(offset)
            marker = backup if keep else bytes(4096)
            assert stream.read(len(marker)) == marker, "wrong overlay config/reset bytes"
        print(f"{'keep config' if keep else 'reset config'}: actual p3/p4 payloads match; "
              f"GPT/p1/p2 fixtures preserved; overlay write offset={offset}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("extracted_root", type=Path)
    args = parser.parse_args()
    if os.geteuid() != 0:
        parser.error("requires root for the isolated chroot")
    expected = {}
    with tarfile.open(args.image) as archive:
        directories = [member.name for member in archive if member.isdir()
                       and member.name.startswith("sysupgrade-")]
        assert len(directories) == 1, directories
        for name in ("kernel", "root"):
            member = archive.getmember(directories[0].rstrip("/") + "/" + name)
            with archive.extractfile(member) as stream:
                digest_value = hashlib.sha256()
                if name == "root":
                    superblock = stream.read(96)
                    assert superblock[:4] == b"hsqs"
                    used = struct.unpack_from("<Q", superblock, 40)[0]
                    digest_value.update(superblock)
                    offset = (used + 65535) & ~65535
                    # OEM installer uses TAR size; fstools uses SquashFS bytes_used.
                    assert offset == (member.size + 65535) & ~65535
                    assert 96 < used <= member.size
                    assert offset + 65536 < 32814120448
                else:
                    assert 0 < member.size < 134217728 - 65536
                while data := stream.read(1024 * 1024):
                    digest_value.update(data)
                expected[name] = member.size, digest_value.hexdigest()
    print(f"OEM TAR length and fstools SquashFS length agree at offset={offset}")
    for keep in (False, True):
        run(args.image.resolve(), args.extracted_root.resolve(), keep, expected, offset)


if __name__ == "__main__":
    main()
