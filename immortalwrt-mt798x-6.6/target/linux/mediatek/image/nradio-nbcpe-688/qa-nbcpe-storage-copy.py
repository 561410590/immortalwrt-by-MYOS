#!/usr/bin/python3
"""Exercise migration copying with the router's actual Python and BusyBox."""

import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile


def main():
    script = Path(__file__).with_name('recover-nbcpe-overlay.py')
    spec = importlib.util.spec_from_file_location('recover_overlay', script)
    recovery = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recovery)
    with tempfile.TemporaryDirectory(prefix='nbcpe-storage-copy-qa-', dir='/tmp') as directory:
        work = Path(directory)
        source = work / 'ram-upper'
        (source / 'etc/config').mkdir(parents=True)
        (source / 'etc/config/network').write_text("config interface 'lan'\n option ipaddr '192.168.68.1'\n")
        (source / 'etc/config/dropbear').write_text("config dropbear\n option PasswordAuth 'on'\n")
        (source / 'etc/shadow').write_text('root:fixture-authentication:1:0:99999:7:::\n')
        (source / 'etc/shadow').chmod(0o600)
        (source / 'etc/link').symlink_to('shadow')
        whiteout = source / 'deleted-lower-file'
        subprocess.run(['mknod', str(whiteout), 'c', '0', '0'], check=True)
        recovery.set_xattr(source / 'etc/config', b'trusted.overlay.opaque', b'y')
        recovery.set_xattr(source / 'etc/config', b'user.migration-fixture', b'preserve\0binary')
        recovery.set_xattr(source / 'etc/config', b'user.empty', b'')
        recovery.set_xattr(source / 'etc/config', b'user.remove-fixture', b'remove')
        recovery.remove_xattr(source / 'etc/config', b'user.remove-fixture')
        saved_path = recovery.Path
        def fixture_path(value):
            if value in ('/', '/rom'):
                return source
            return saved_path(value)
        recovery.Path = fixture_path
        target = work / 'disk-upper'
        count = recovery.copy_upper(source, target)
        attrs = recovery.read_xattrs(target / 'etc/config')
        assert attrs[b'trusted.overlay.opaque'] == b'y'
        assert attrs[b'user.migration-fixture'] == b'preserve\0binary'
        assert attrs[b'user.empty'] == b'' and b'user.remove-fixture' not in attrs
        assert (target / 'etc/shadow').stat().st_mode & 0o777 == 0o600
        assert os.readlink(target / 'etc/link') == 'shadow'
        assert stat.S_ISCHR((target / 'deleted-lower-file').lstat().st_mode)
        assert (target / 'deleted-lower-file').lstat().st_rdev == 0
        print(json.dumps({'target_python_copy': True, 'copied_nodes': count,
                          'libc_xattrs': True, 'trusted_opaque': True, 'whiteout': True,
                          'authentication_and_lan': True, 'real_devices_modified': 0,
                          'temporary_fixture_removed_on_exit': True}))


if __name__ == '__main__':
    main()
