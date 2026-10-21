#!/usr/bin/env python3
"""Exercise the real firmware overlay installer in a fresh build-only root."""
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile

assets = Path(sys.argv[1]).resolve()
factory = Path(sys.argv[2]).resolve()
tree = assets.parents[4]
build_dir = tree / "build_dir"
output = Path(tempfile.mkdtemp(prefix="remote-api-smoke-", dir=build_dir))
assert output.is_relative_to(build_dir.resolve())
root = output / "rootfs"
(root / "www/cgi-bin").mkdir(parents=True)
shutil.copyfile(factory / "www/cgi-bin/luci.ucode", root / "www/cgi-bin/luci")
(root / "etc/config").mkdir(parents=True)
shutil.copyfile(factory / "etc/config/uhttpd", root / "etc/config/uhttpd")
environment = os.environ.copy()
environment.pop("MYOS_NROS_IDENTITY_FILE", None)
subprocess.run(["sh", str(assets / "MYOS-nros-app-patch.sh"), str(root), str(factory)],
               env=environment, check=True)
installed = {
    "usr/lib/lua/myos_nradio_app_session.lua": "myos_nradio_app_session.lua",
    "usr/lib/lua/luci/controller/nradio/app.lua": "myos-nradio-app.lua",
    "usr/sbin/myos-nradio-app-fast": "myos-nradio-app-fast.lua",
    "usr/sbin/myos-nros-app-agent": "myos-remote-files/usr/sbin/myos-nros-app-agent",
    "etc/init.d/myos-nros-ssh": "myos-remote-files/etc/init.d/myos-nros-ssh",
    "etc/uci-defaults/30-myos-nros-remote-access":
        "myos-remote-files/etc/uci-defaults/30-myos-nros-remote-access",
}
for destination, source in installed.items():
    assert (root / destination).read_bytes() == (assets / source).read_bytes(), destination
for path in ("etc/init.d/myos-nros-ssh", "etc/uci-defaults/30-myos-nros-remote-access",
             "usr/sbin/myos-nros-app-agent", "usr/sbin/myos-nradio-app-fast"):
    assert stat.S_IMODE((root / path).stat().st_mode) == 0o755, path
config = (root / "etc/config/myos_nros").read_text()
for option in ("remote_web_enabled", "remote_ssh_enabled", "remote_access_version"):
    assert f"option {option} '1'" in config, option
wrapper = (root / "www/cgi-bin/luci").read_text()
assert "/usr/sbin/myos-nradio-app-fast" in wrapper
assert "exec /www/cgi-bin/luci.ucode" in wrapper
assert (root / "www/cgi-bin/luci.ucode").read_bytes() == (factory / "www/cgi-bin/luci.ucode").read_bytes()
for path in ("www/cgi-bin/luci", "etc/init.d/myos-nros-ssh",
             "etc/uci-defaults/30-myos-nros-remote-access"):
    subprocess.run(["busybox", "ash", "-n", str(root / path)], check=True)
print("PASS: real overlay install, runtime files, modes, defaults, LuCI dispatch")
print("Build test root:", root)
