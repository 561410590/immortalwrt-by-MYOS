"""Remove retired theme presentation files from assembled firmware only."""
import os
import re
import shutil

def install(root):
    directories = (
        'www/luci-static/c2000max-ui', 'www/luci-static/argon',
        'www/luci-static/vantage', 'www/luci-static/bootstrap',
        'www/luci-static/material', 'www/luci-static/openwrt',
        'www/luci-static/openwrt2020',
        'usr/share/ucode/luci/template/themes/c2000max-ui',
        'usr/share/ucode/luci/template/themes/argon',
        'usr/share/ucode/luci/template/themes/vantage',
        'usr/lib/lua/luci/view/themes/argon',
        'usr/lib/lua/luci/view/themes/c2000max-ui',
    )
    for relative in directories:
        path = root / relative
        if path.is_symlink():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)
    for relative in ('usr/share/ucode/luci/template/themes', 'usr/lib/lua/luci/view/themes'):
        directory = root / relative
        if directory.is_dir():
            for path in directory.iterdir():
                if path.name != 'myos-new':
                    if path.is_symlink() or path.is_file():
                        path.unlink()
                    elif path.is_dir():
                        shutil.rmtree(path)
    for relative in (
        'etc/config/c2000max_ui', 'etc/init.d/c2000max-wallpaper',
        'etc/uci-defaults/95-c2000max-ui', 'etc/uci-defaults/96-mwrt',
        'etc/uci-defaults/30_luci-theme-vantage',
        'etc/uci-defaults/30_luci-theme-bootstrap',
        'usr/libexec/c2000max-bing', 'www/cgi-bin/c2000max-wallpaper',
        'usr/share/luci/menu.d/luci-theme-c2000max-ui.json',
        'usr/share/luci/menu.d/myos-theme.json',
        'www/luci-static/resources/view/myos/theme.js',
        'www/luci-static/resources/view/c2000max/home-c8.js',
    ):
        path = root / relative
        if path.is_file() or path.is_symlink():
            path.unlink()
    for path in (root / 'etc/rc.d').iterdir():
        if path.is_symlink() and os.path.basename(os.readlink(path)) == 'c2000max-wallpaper':
            path.unlink()
    path = root / 'etc/config/luci'
    text = path.read_text()
    text = re.sub(r"(?m)^\s*option\s+mediaurlbase\s+[^\n]+",
                  "\toption mediaurlbase '/luci-static/myos-new'", text)
    # Theme entries live in this section only; core.resourcebase is not a theme.
    text = re.sub(r"(?ms)^(config internal ['\"]?themes['\"]?[^\n]*\n).*?(?=^config\s|\Z)",
                  r"\1\toption MYOS_NEW '/luci-static/myos-new'\n\n", text)
    # Restore the core resource URL in roots processed by the former broad rule.
    def core_resources(match):
        body = re.sub(r"(?m)^[ \t]*option\s+resourcebase\s+[^\n]*(?:\n|$)", "", match[2])
        return match[1] + "\toption resourcebase '/luci-static/resources'\n" + body
    text = re.sub(r"(?ms)^(config core ['\"]?main['\"]?[^\n]*\n)(.*?)(?=^config\s|\Z)",
                  core_resources, text)
    path.write_text(text)
    path.chmod(0o644)

