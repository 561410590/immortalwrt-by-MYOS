"""Restore the reference modes on corresponding N5000 build files."""

import os
from pathlib import Path
import stat
import sys


def reference_path(path):
    parts = path.parts
    if len(parts) > 2 and parts[:2] == ('lib', 'modules'):
        return '/'.join(('lib', 'modules', '@kernel', *parts[3:]))
    return path.as_posix()


def main(root):
    reference = Path(__file__).with_name('rootfs-modes.tsv')
    modes = {}
    for line in reference.read_text().splitlines():
        if line.startswith('#'):
            continue
        kind, mode, name = line.split('\t', 2)
        modes[reference_path(Path(name))] = (kind, int(mode, 8))

    for directory, _, files in os.walk(root):
        parent = Path(directory)
        for path in (parent, *(parent / name for name in files)):
            expected = modes.get(reference_path(path.relative_to(root)))
            if expected is None:
                continue
            actual = path.lstat().st_mode
            kind = 'd' if stat.S_ISDIR(actual) else '-' if stat.S_ISREG(actual) else None
            if kind == expected[0] and stat.S_IMODE(actual) != expected[1]:
                path.chmod(expected[1])


if __name__ == '__main__':
    main(Path(sys.argv[1]))
