#!/usr/bin/env python3
"""Install shared LuCI compatibility files into the freshly built ROM."""
from pathlib import Path
import re
import shutil
import sys

root, src = map(Path, sys.argv[1:3])
for rel in ('dispatcher.uc', 'template/header.ut'):
    dst = root / 'usr/share/ucode/luci' / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src / 'feeds/luci/modules/luci-base/ucode' / rel, dst)
    dst.chmod(0o644)

# The stock ucode Argon shell would otherwise take precedence over MWRT's Lua shell.
old_shell = root / 'usr/share/ucode/luci/template/themes/argon'
if old_shell.exists():
    shutil.rmtree(old_shell)

# Only the new ROM defaults are set here. sysupgrade retains the user's overlay.
cfg = root / 'etc/config/luci'
text = cfg.read_text()
text, count = re.subn(r"(?m)^\s*option\s+mediaurlbase\s+[^\n]+", "\toption mediaurlbase '/luci-static/c2000max-ui'", text)
if count != 1:
    raise SystemExit('Expected one luci.main.mediaurlbase in ROM')
cfg.write_text(text)
print('Dual-theme ROM defaults: MYOS; MWRT native homepage installed')
