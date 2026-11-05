#!/usr/bin/env python3
"""Carry the shared LuCI fixes and MYOS-NEW defaults into N5000 ROM."""
from pathlib import Path
import re
import shutil
import sys

root, src = map(Path, sys.argv[1:3])
for relative in ('dispatcher.uc', 'template/header.ut'):
    destination = root / 'usr/share/ucode/luci' / relative
    shutil.copyfile(src / 'feeds/luci/modules/luci-base/ucode' / relative, destination)
    destination.chmod(0o644)

destination = root / 'www/luci-static/resources/luci.js'
shutil.copyfile(src / 'staging_dir/target-aarch64_cortex-a53_musl/root-mediatek/www/luci-static/resources/luci.js', destination)
destination.chmod(0o644)
config = root / 'etc/config/luci'
text = re.sub(r'(?m)^\s*option\s+mediaurlbase\s+[^\n]+',
              "\toption mediaurlbase '/luci-static/myos-new'", config.read_text())
text = re.sub(r'(?ms)^config internal themes\n.*?(?=^config |\Z)',
              'config internal themes\n\toption MYOS_NEW /luci-static/myos-new\n\n', text)
config.write_text(text)
config.chmod(0o644)
print('N5000 ROM theme: MYOS-NEW')
