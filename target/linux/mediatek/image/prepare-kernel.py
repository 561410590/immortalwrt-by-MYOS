#!/usr/bin/env python3
"""Prepare the authorized C5800 6.6.110 build; preserve source contents and caches."""
from pathlib import Path
import os
import shutil
import stat
import subprocess
import sys

root = Path(__file__).resolve().parents[4]
assert (root / 'include/toplevel.mk').is_file(), root
# FNOS ACL exports left executable tools at mode 000. Repair owner access only.
for base in (() if '--skip-permissions' in sys.argv else ('include', 'scripts', 'staging_dir', 'build_dir', 'tmp')):
    directory = root / base
    if not directory.exists():
        continue
    for parent, dirs, files in os.walk(directory):
        os.chmod(parent, os.stat(parent).st_mode | stat.S_IRWXU)
        for name in files:
            path = Path(parent) / name
            if path.is_symlink():
                continue
            mode = path.stat().st_mode | stat.S_IRUSR | stat.S_IWUSR
            with path.open('rb') as stream:
                signature = stream.read(4)
            if signature.startswith((b'#!', b'\x7fELF')):
                mode |= stat.S_IXUSR
            os.chmod(path, mode)
tracked = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=root)
for record in tracked.split(b'\0'):
    if record.startswith(b'100755 '):
        path = root / os.fsdecode(record.split(b'\t', 1)[1])
        if path.is_file() and not path.is_symlink():
            os.chmod(path, path.stat().st_mode | stat.S_IRWXU)
# These two patches are already included in stable Linux 6.6.110.
for relative in (
    'target/linux/generic/backport-6.6/001-powerpc-kernel-Fix-pcc_save_regs-inclusion.patch',
    'target/linux/generic/pending-6.6/650-Revert-ipv6-save-dontfrag-in-cork.patch',
    'target/linux/mediatek/patches-6.6/999-ephy-en8811h-01-update-driver-to-v2.0.7.patch',
    'target/linux/mediatek/patches-6.6/999-ephy-en8811h-02-led-activity.patch',
):
    path = root / relative
    legacy = path.with_suffix('.included-6.6.110')
    source = path if path.exists() else legacy
    if source.exists():
        included = path.parent / 'included-6.6.110'
        included.mkdir(exist_ok=True)
        source.rename(included / path.name)
feed = root / 'feeds/base'
if feed.is_symlink() and not feed.exists():
    feed.unlink()
    feed.symlink_to(root / 'package', target_is_directory=True)
archive = root / 'dl/linux-6.6.110.tar.xz'
if not archive.exists():
    shutil.copyfile(root.with_name(root.name + '-c2000ultra') / 'dl' / archive.name, archive)
print('C5800 build tools, feed link and Linux 6.6.110 source ready', flush=True)
