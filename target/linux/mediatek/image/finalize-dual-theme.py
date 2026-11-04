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
        at_js = at_js.replace(before, after, 1)
    bundle.write_text(at_js)
    bundle.chmod(0o644)
    page = root / 'www/5700/index.html'
    page_text = page.read_text()
    page.write_text(page_text.replace('/5700/umi.ec9b4b52.js', '/5700/umi.ec9b4b52.js?v=myos-at2'))

for rel in ('dispatcher.uc', 'template/header.ut', 'template/footer.ut'):
    dst = root / 'usr/share/ucode/luci' / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src / 'feeds/luci/modules/luci-base/ucode' / rel, dst)
    dst.chmod(0o644)

# Carry the rebuilt shared JavaScript into Ultra's custom root filesystem.
dst = root / 'www/luci-static/resources/luci.js'
shutil.copyfile(src / 'staging_dir/target-aarch64_cortex-a53_musl/root-mediatek/www/luci-static/resources/luci.js', dst)
dst.chmod(0o644)

# Shared public resources must remain readable by the WebUI service.
for rel in ("www/luci-static", "www/luci-static/resources"):
    (root / rel).chmod(0o755)

# Only the new ROM defaults are set here. sysupgrade retains the user's overlay.
cfg = root / 'etc/config/luci'
text = cfg.read_text()
text, count = re.subn(r"(?m)^\s*option\s+mediaurlbase\s+[^\n]+", "\toption mediaurlbase '/luci-static/myos-new'", text)
if count != 1:
    raise SystemExit('Expected one luci.main.mediaurlbase in ROM')
cfg.write_text(text)
print('Ultra ROM default: MYOS-NEW only; legacy theme packages and switch menu excluded')
