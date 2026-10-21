#!/usr/bin/env python3
"""Remove excluded private services from an offline public-build root filesystem."""

from pathlib import Path
import shutil
import sys


SOURCE = Path(__file__).resolve().parents[1]
FILES = (
    'usr/libexec/myos-usage', 'etc/config/myos_usage',
    'usr/libexec/myos-update', 'etc/init.d/myos-update',
    'usr/share/luci/menu.d/myos-update.json',
    'usr/share/rpcd/acl.d/myos-update.json',
    'usr/share/rpcd/ucode/myos-update',
    'www/luci-static/resources/view/myos/update.js',
)


def clean(path):
    root = Path(path).resolve(strict=True)
    if root == SOURCE or not root.is_relative_to(SOURCE):
        raise SystemExit('An offline root filesystem inside this source checkout is required')
    if not (root / 'etc/openwrt_release').is_file():
        raise SystemExit('The target is not an assembled OpenWrt root filesystem')
    candidates = [root / relative for relative in FILES]
    candidates.append(root / 'usr/share/myos-update')
    candidates.extend((root / 'etc/rc.d').glob('*myos-update*'))
    for target in candidates:
        if not target.parent.resolve().is_relative_to(root):
            raise SystemExit('Refusing a path outside the offline root filesystem')
        if target.is_symlink() or target.is_file():
            target.unlink()
        elif target.is_dir():
            shutil.rmtree(target)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: prepare-public-rootfs.py OFFLINE_ROOTFS')
    clean(sys.argv[1])
