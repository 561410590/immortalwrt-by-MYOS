#!/usr/bin/env python3
"""Install MYOS remote support into a model's assembled root filesystem."""
from pathlib import Path
import shutil
import sys


def install(root, assets, updater=None):
    files = {
        assets / 'myos-nros-cloud-controller.lua': ('usr/lib/lua/luci/controller/myos_nros.lua', 0o644),
        assets / 'myos-nros-cloud-cbi.lua': ('usr/lib/lua/luci/model/cbi/myos_nros.lua', 0o644),
        assets / 'myos-nros-cloud-status.htm': ('usr/lib/lua/luci/view/myos_nros/status.htm', 0o644),
        assets / 'myos_nradio_app_session.lua': ('usr/lib/lua/myos_nradio_app_session.lua', 0o644),
    }
    if updater is not None:
        files[updater] = ('usr/libexec/myos-update', 0o755)
    for relative, mode in {
        'usr/sbin/myos-support': 0o755,
        'etc/init.d/myos-support': 0o755,
        'usr/libexec/myos-support-session': 0o755,
        'usr/libexec/myos-mayebano-aaaa': 0o755,
        'usr/lib/lua/luci/view/myos_nros/support.htm': 0o644,
        'lib/upgrade/keep.d/myos-support': 0o644,
    }.items():
        files[assets / 'myos-support' / relative] = (relative, mode)
    for source, (relative, mode) in files.items():
        target = root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_symlink():
            target.unlink()
        shutil.copyfile(source, target)
        target.chmod(mode)


if __name__ == '__main__':
    install(*map(Path, sys.argv[1:]))
