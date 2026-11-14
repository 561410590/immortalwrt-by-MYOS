#!/usr/bin/env python3
"""Restore recorded C2000-Ultra permissions on files staged by this build."""

import os
from pathlib import Path
import stat
import sys


def read_modes(package):
    name = 'ipk-modes.tsv' if package else 'rootfs-modes.tsv'
    manifest = Path(__file__).with_name(name)
    modes = {}
    with manifest.open(encoding='utf-8') as rows:
        for row in rows:
            if row.startswith('#'):
                continue
            key, mode, relative = row.rstrip('\n').split('\t', 2)
            if package and key != package:
                continue
            path = Path(relative)
            # Image assembly already sets the public Web tree's permissions.
            if not package and path.parts[:1] == ('www',):
                continue
            modes[path] = (None if package else key, int(mode, 8))
    return modes


def apply_modes(root, modes):
    if not modes:
        return

    for directory, _, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        for path in (parent, *(parent / name for name in files)):
            entry = modes.get(path.relative_to(root))
            if entry is None:
                continue
            expected_type, mode = entry
            current = path.lstat().st_mode
            if stat.S_ISDIR(current):
                file_type = 'd'
            elif stat.S_ISREG(current):
                file_type = '-'
            else:
                continue
            if expected_type in (None, file_type) and stat.S_IMODE(current) != mode:
                path.chmod(mode)


if __name__ == '__main__':
    target = Path(sys.argv[1])
    package = sys.argv[2] if len(sys.argv) > 2 else None
    apply_modes(target, read_modes(package))
