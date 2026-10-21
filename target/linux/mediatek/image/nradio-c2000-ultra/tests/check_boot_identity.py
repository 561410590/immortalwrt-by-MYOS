#!/usr/bin/env python3
"""Run the real first-boot identity script in a disposable host chroot.

Only UCI/service operations and block probing are fixtures. BusyBox executes
the unchanged script and reads a bounded synthetic SPI bdinfo record. A dummy
block node catches any attempt to scan the rootfs, without exposing host disks.
This checks first-boot identity initialization, not hardware boot or networking.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile
import time


def put(root, path, data, mode=0o644):
    target = root / path.lstrip("/")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data if isinstance(data, bytes) else data.encode())
    target.chmod(mode)


def check(source, case, factory, values, expected_mac, expected_code, expect_scan):
    with tempfile.TemporaryDirectory(prefix="ultra-boot-identity-") as directory:
        root = Path(directory)
        busybox = Path(shutil.which("busybox")).resolve()
        put(root, "/bin/busybox", busybox.read_bytes(), 0o755)
        linked = subprocess.check_output(["ldd", str(busybox)], text=True)
        for library in set(re.findall(r"/[^\s()]+", linked)):
            put(root, library, Path(library).read_bytes(), 0o755)
        for app in ("sh", "awk", "grep", "tr", "head", "dd", "hexdump", "sed",
                    "rm", "dirname", "basename", "cat", "mkdir", "true", "readlink"):
            (root / "bin" / app).symlink_to("busybox")
        for name in ("dev", "tmp", "etc/config", "etc/rc.d", "proc"):
            (root / name).mkdir(parents=True, exist_ok=True)
        os.mknod(root / "dev/null", stat.S_IFCHR | 0o666, os.makedev(1, 3))
        os.mknod(root / "dev/mmcblk0p4", stat.S_IFBLK | 0o600, os.makedev(0, 0))
        put(root, "/proc/mtd", 'mtd3: 00080000 00010000 "bdinfo"\n' if factory else "")
        if factory:
            record = b"\x00fac_mac=02:11:22:33:44:55\xffdevice_code=fixture-device\x00"
            put(root, "/dev/mtd3", record.ljust(65536, b"\xff"))
        put(root, "/uci-values", "".join(f"{key} {value}\n" for key, value in values.items()))
        put(root, "/bin/uci", """#!/bin/sh
[ "$1" = -q ] && shift
case "$1" in
 get) awk -v key="$2" '$1 == key { print $2; found=1; exit } END { if (!found) exit 1 }' /uci-values ;;
 batch) cat >> /uci-batch ;;
 set|commit) exit 0 ;;
 *) exit 1 ;;
esac
""", 0o755)
        put(root, "/bin/blkid", """#!/bin/sh
case "$*" in *PARTLABEL=rootfs*) echo /dev/mmcblk0p4 ;; *) exit 1 ;; esac
""", 0o755)
        put(root, "/bin/strings", """#!/bin/sh
echo "$*" >> /raw-rootfs-access
exit 1
""", 0o755)
        for service in ("mosquitto", "myos-nros-mqtt", "myos-nros-app"):
            put(root, f"/etc/init.d/{service}", "#!/bin/sh\nexit 0\n", 0o755)
        put(root, "/identity", source.read_bytes(), 0o755)
        start = time.monotonic()
        result = subprocess.run([shutil.which("chroot"), str(root), "/bin/sh", "/identity"],
                                env={"PATH": "/bin", "LC_ALL": "C"},
                                capture_output=True, text=True, timeout=5)
        assert result.returncode == 0, (case, result.stderr)
        batch = (root / "uci-batch").read_text()
        assert f"set oem.board.id='{expected_mac}'" in batch, case
        assert f"set oem.board.device_code='{expected_code}'" in batch, case
        scanned = (root / "raw-rootfs-access").exists()
        assert scanned == expect_scan, (case, "raw rootfs read", scanned)
        print(f"{case}: factory/current identity preserved; raw_rootfs_scan={scanned}; "
              f"elapsed={time.monotonic() - start:.3f}s")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("script", type=Path)
    parser.add_argument("--expect-raw-scan", action="store_true")
    args = parser.parse_args()
    if os.geteuid() != 0:
        parser.error("requires root for the isolated chroot")
    check(args.script, "SPI bdinfo on official SD", True, {},
          "02:11:22:33:44:55", "fixture-device", args.expect_raw_scan)
    check(args.script, "preserved UCI without bdinfo", False,
          {"oem.board.id": "02:22:33:44:55:66", "oem.board.device_code": "existing-device"},
          "02:22:33:44:55:66", "existing-device", args.expect_raw_scan)
    check(args.script, "manual identity override", True,
          {"myos_nros.main.manual_mac": "023344556677"},
          "02:33:44:55:66:77", "fixture-device", args.expect_raw_scan)


if __name__ == "__main__":
    main()
