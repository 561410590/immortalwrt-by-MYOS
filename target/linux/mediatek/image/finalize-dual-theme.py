#!/usr/bin/env python3
"""Install shared LuCI compatibility files into the freshly built ROM."""
from pathlib import Path
import re
import shutil
import sys

root, src = map(Path, sys.argv[1:3])

# The shipped WebUI already serializes commands, but its one-second deadline
# releases the queue before the AT backend's ten-second deadline. A late reply
# can then complete the next command. Keep the backend and repair that contract.
bundle = root / 'www/5700/umi.ec9b4b52.js'
if bundle.exists():
    at_js = bundle.read_text()
    replacements = [
        ('I()(this,"commandTimeout",1e3)', 'I()(this,"commandTimeout",12e3)'),
        ('setTimeout(Ie,20)', 'setTimeout(Ie,100)'),
        ('L.pendingCommands.delete(J),Ie({success:!1,error:',
         'L.pendingCommands.delete(J),L.connected=!1,L.authenticated=!1,L.ws&&L.ws.close(),Ie({success:!1,error:'),
        ('key:"setupWebSocket",value:function(){var S=this;',
         'key:"setupWebSocket",value:function(){var S=this,W0=this.ws;'),
        ('this.ws.onopen=function(){if(console.log(',
         'this.ws.onopen=function(){if(S.ws!==W0)return;if(console.log('),
        ('this.ws.onclose=function(){console.log(',
         'this.ws.onclose=function(){if(S.ws!==W0)return;console.log('),
        ('this.ws.onmessage=function(C){try{var N=C.data;',
         'this.ws.onmessage=function(C){if(S.ws!==W0||!S.connected)return;try{var N=C.data;'),
    ]
    for before, after in replacements:
        if at_js.count(before) != 1:
            raise SystemExit('Unexpected AT WebUI transport: ' + before)
        at_js = at_js.replace(before, after, 1)
    bundle.write_text(at_js)
    bundle.chmod(0o644)
    page = root / 'www/5700/index.html'
    page_text = page.read_text()
    if page_text.count('/5700/umi.ec9b4b52.js') != 1:
        raise SystemExit('Missing AT WebUI entry script')
    page.write_text(page_text.replace('/5700/umi.ec9b4b52.js', '/5700/umi.ec9b4b52.js?v=202610180245'))

for rel in ('dispatcher.uc', 'template/header.ut'):
    dst = root / 'usr/share/ucode/luci' / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src / 'feeds/luci/modules/luci-base/ucode' / rel, dst)
    dst.chmod(0o644)

# Carry the rebuilt shared JavaScript into MAX's custom root filesystem.
dst = root / 'www/luci-static/resources/luci.js'
shutil.copyfile(src / 'staging_dir/target-aarch64_cortex-a53_musl/root-mediatek/www/luci-static/resources/luci.js', dst)
dst.chmod(0o644)

# Install the system page without the retired theme selector.
dst = root / 'www/luci-static/resources/view/system/system.js'
shutil.copyfile(src / 'staging_dir/target-aarch64_cortex-a53_musl/root-mediatek/www/luci-static/resources/view/system/system.js', dst)
dst.chmod(0o644)

# Only presentation is retired. Shared device services come from
# luci-app-myos-system, with no legacy theme package in the image.
for relative in (
    'www/luci-static/c2000max-ui', 'www/luci-static/argon',
    'www/luci-static/bootstrap', 'www/luci-static/resources/view/c2000max',
    'www/luci-static/bootstrap-dark', 'www/luci-static/bootstrap-light',
    'usr/share/ucode/luci/template/themes/c2000max-ui',
    'usr/share/ucode/luci/template/themes/argon',
    'usr/share/ucode/luci/template/themes/bootstrap',
    'usr/share/ucode/luci/template/themes/bootstrap-dark',
    'usr/share/ucode/luci/template/themes/bootstrap-light',
    'usr/lib/lua/luci/view/themes/argon',
    'usr/lib/lua/luci/view/themes/c2000max-ui',
    'usr/lib/lua/luci/view/themes/bootstrap',
):
    path = root / relative
    if path.is_symlink():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)
for relative in (
    'etc/config/c2000max_ui', 'etc/init.d/c2000max-wallpaper',
    'etc/uci-defaults/95-c2000max-ui', 'etc/uci-defaults/96-mwrt-theme',
    'etc/uci-defaults/30_luci-theme-argon', 'etc/uci-defaults/30_luci-theme-bootstrap',
    'usr/libexec/c2000max-bing', 'www/cgi-bin/c2000max-wallpaper',
    'usr/share/luci/menu.d/luci-theme-c2000max-ui.json',
    'usr/share/luci/menu.d/myos-theme.json',
    'usr/share/rpcd/acl.d/myos-theme.json',
    'usr/share/rpcd/acl.d/luci-theme-c2000max-ui.json',
    'usr/share/rpcd/ucode/c2000max-ui',
    'www/luci-static/resources/view/myos/theme.js',
    'www/luci-static/resources/view/myos/max.js',
):
    path = root / relative
    if path.is_file() or path.is_symlink():
        path.unlink()
for path in (root / 'etc/rc.d').iterdir():
    if path.is_symlink() and path.readlink().name == 'c2000max-wallpaper':
        path.unlink()
update_link = root / 'etc/rc.d/S99myos-update'
if update_link.is_symlink() or update_link.exists():
    update_link.unlink()
update_link.symlink_to('../init.d/myos-update')

# Match the sole installed theme, including clean-ROM UCI registration.
cfg = root / 'etc/config/luci'
text = cfg.read_text()
text = re.sub(r"(?m)^\s*option\s+mediaurlbase\s+[^\n]+", "\toption mediaurlbase '/luci-static/myos-new'", text)
text = re.sub(r"(?ms)^config\s+internal\s+['\"]?themes['\"]?[^\n]*\n.*?(?=^config\s|\Z)", "", text)
text = text.rstrip() + "\n\nconfig internal 'themes'\n\toption MYOS_NEW '/luci-static/myos-new'\n"
cfg.write_text(text)
cfg.chmod(0o644)
print('MYOS-NEW is the sole ROM theme; shared device services retained')
