"""Install the shared MYOS LuCI presentation into assembled firmware."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile


ADGUARDHOME_DIR = Path(__file__).resolve().parent / 'nradio-c5800-688/adguardhome'


def add_adguard_packages(available, read_ipk):
    packages = (
        'luci-app-adguardhome_git-24.182.27136-d3d2083_all.ipk',
        'luci-i18n-adguardhome-zh-cn_git-24.182.25496-1d4b053_all.ipk',
    )
    if 'wget-ssl' not in available:
        packages += ('wget-ssl_1.24.5-r2_aarch64_cortex-a53.ipk',)
    names = []
    for filename in packages:
        path = ADGUARDHOME_DIR / 'packages' / filename
        name, record, fields, _ = read_ipk(path)
        available[name] = path, record, fields
        names.append(name)
    return names


def configure_adguard(root):
    subprocess.run(['patch', '--batch', '--no-backup-if-mismatch', '-p1',
                    '-i', ADGUARDHOME_DIR / 'myos-adguardhome.patch'],
                   cwd=root, check=True)

    # The .run ships these programs as 0644 and fixes them in opkg postinst.
    share = root / 'usr/share/AdGuardHome'
    for name in ('addhost.sh', 'firewall.start', 'getsyslog.sh', 'gfw2adg.sh',
                 'tailto.sh', 'update_core.sh', 'waitnet.sh', 'watchconfig.sh'):
        (share / name).chmod(0o755)
    (share / 'AdGuardHome_template.yaml').unlink()
    for relative in ('etc/init.d/AdGuardHome', 'etc/uci-defaults/40_luci-AdGuardHome'):
        (root / relative).chmod(0o755)

    for name in ('S95AdGuardHome', 'K01AdGuardHome'):
        link = root / 'etc/rc.d' / name
        link.unlink(missing_ok=True)
        link.symlink_to('../init.d/AdGuardHome')

    # Official release with the native AdGuard Home web interface.
    archive = ADGUARDHOME_DIR / 'AdGuardHome_linux_arm64_v1.0.0.tar.gz'
    with tarfile.open(archive) as release:
        for member, relative, mode in (
            ('AdGuardHome', 'usr/bin/AdGuardHome/AdGuardHome', 0o755),
            ('LICENSE.txt', 'usr/share/licenses/AdGuardHome/LICENSE.txt', 0o644),
        ):
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True, mode=0o755)
            with release.extractfile('./AdGuardHome/' + member) as data, target.open('wb') as stream:
                shutil.copyfileobj(data, stream)
            target.chmod(mode)

    # Keep the package manifest accurate after removing the vendor quick setup.
    manifest = root / 'usr/lib/opkg/info/luci-app-adguardhome.list'
    files = manifest.read_text().splitlines()
    files.remove('/usr/share/AdGuardHome/AdGuardHome_template.yaml')
    manifest.write_text('\n'.join(files) + '\n')


def install_nros_app(root):
    source = Path(__file__).resolve().parents[4]
    files = Path(__file__).resolve().parent / 'nradio-c5800-688/nros-app-2.3.9/root'
    shutil.copy2(source / 'feeds/luci/modules/luci-base/htdocs/cgi-bin/luci',
                 root / 'www/cgi-bin/luci.ucode')
    (root / 'www/cgi-bin/luci.ucode').chmod(0o755)
    programs = {
        'www/cgi-bin/luci', 'usr/lib/nros-app-2.3.9/cgi',
        'usr/lib/nros-app-2.3.9/bin/nradio_crypto', 'usr/sbin/mosquitto',
        'usr/local/bin/mosquitto_collect_sysinfo', 'etc/init.d/mosquitto',
        'etc/init.d/myos-nros-mqtt',
    }
    for item in sorted(files.rglob('*')):
        relative = item.relative_to(files)
        target = root / relative
        if item.is_symlink():
            target.unlink(missing_ok=True)
            target.symlink_to(item.readlink())
        elif item.is_dir():
            if not target.is_dir():
                target.mkdir()
                target.chmod(0o755)
        else:
            if target.is_symlink():
                target.unlink()
            shutil.copyfile(item, target)
            target.chmod(0o755 if relative.as_posix() in programs else 0o644)

def add_vantage_packages(source, available, read_ipk):
    packages = ('luci-app-vantage', 'luci-i18n-vantage-zh-cn')
    directory = source / 'target/linux/mediatek/image/nradio-c5800-688/vantage/packages'
    for package in packages:
        path = directory / f'{package}_1.0.20-r1_all.ipk'
        name, record, fields, _ = read_ipk(path)
        available[name] = path, record, fields
    return packages


KUNPENG_DIR = Path(__file__).resolve().parent / 'nradio-c5800-688/kunpeng-ai'


def kunpeng_packages():
    return sorted((KUNPENG_DIR / 'packages/native').glob('*.ipk'))


def configure_kunpeng(root, run):
    run('patch', '--batch', '--no-backup-if-mismatch', '-d', root, '-p1',
        '-i', KUNPENG_DIR / 'myos-kunpeng.patch')
    for relative in ('lib/zeroclaw/zeroclaw.sh', 'usr/lib/lua/luci/controller/nradio/ai.lua',
                     'usr/lib/lua/luci/view/nradio_ai/index.htm'):
        (root / relative).chmod(0o644)
    target = root / 'www/luci-static/nradio/js/jquery.min.js'
    shutil.copyfile(KUNPENG_DIR / 'jquery.min.js', target)
    target.chmod(0o644)
    license_path = root / 'usr/share/licenses/myos-kunpeng/jquery'
    license_path.parent.mkdir(parents=True, exist_ok=True, mode=0o755)
    shutil.copyfile(KUNPENG_DIR / 'jquery.copyright', license_path)
    license_path.chmod(0o644)


def configure_vantage(root):
    menu = root / 'usr/share/luci/menu.d/zz-luci-app-vantage.json'
    items = json.loads(menu.read_text(encoding='utf-8'))
    items['admin/dashboard'].update(title='Vantage 仪表盘', order=2)
    menu.write_text(json.dumps(items, ensure_ascii=False, indent='\t') + '\n', encoding='utf-8')
    menu.chmod(0o644)

    unlock_readers = (
        'usr/lib/lua/myos_plugins.lua',
        'usr/lib/lua/luci/controller/openclash.lua',
        'usr/lib/lua/luci/controller/passwall.lua',
        'usr/lib/lua/luci/controller/passwall2.lua',
        'usr/lib/lua/luci/controller/zz_mwrt_features.lua',
    )
    ssr_controller = 'usr/lib/lua/luci/controller/shadowsocksr.lua'
    if (root / ssr_controller).is_file():
        unlock_readers += (ssr_controller,)
    for relative in unlock_readers:
        path = root / relative
        text = path.read_text(encoding='utf-8')
        path.write_text(text.replace('/etc/config/Outofnetwork', '/etc/Outofnetwork'), encoding='utf-8')

    migration = root / 'etc/uci-defaults/00-myos-plugin-unlock'
    shutil.copyfile(Path(__file__).with_name('myos-plugin-unlock.defaults'), migration)
    migration.chmod(0o755)
    keep = root / 'lib/upgrade/keep.d/myos-plugin-unlock'
    keep.write_text('/etc/Outofnetwork\n', encoding='utf-8')
    keep.chmod(0o644)


def install_upgrade_preservation(root):
    migration = root / 'etc/uci-defaults/99-myos-restore-adguard-dns'
    shutil.copyfile(Path(__file__).with_name('myos-restore-adguard-dns.defaults'), migration)
    migration.chmod(0o755)

    keep = root / 'lib/upgrade/keep.d/myos-plugin-data'
    keep.write_text('/etc/openclash/config/\n/etc/AdGuardHome.yaml\n'
                    '/usr/bin/AdGuardHome/AdGuardHome\n', encoding='utf-8')
    keep.chmod(0o644)


def install(root):
    tree = Path(__file__).resolve().parents[4]
    luci = tree / 'feeds/luci/modules/luci-base'
    network = tree / 'feeds/luci/modules/luci-mod-network/htdocs'
    theme = tree / 'package/luci-theme-myos-new/root'
    for source, target in (
        (luci / 'ucode/template/footer.ut', 'usr/share/ucode/luci/template/footer.ut'),
        (luci / 'ucode/controller/admin/myos_disclaimer.uc',
         'usr/share/ucode/luci/controller/admin/myos_disclaimer.uc'),
        (luci / 'htdocs/luci-static/resources/myos-disclaimer.js',
         'www/luci-static/resources/myos-disclaimer.js'),
        (luci / 'root/usr/share/luci/menu.d/myos-disclaimer.json',
         'usr/share/luci/menu.d/myos-disclaimer.json'),
        (luci / 'root/lib/upgrade/keep.d/myos-disclaimer', 'lib/upgrade/keep.d/myos-disclaimer'),
        (network / 'luci-static/resources/view/network/interfaces.js',
         'www/luci-static/resources/view/network/interfaces.js'),
        (theme / 'www/luci-static/myos-new/layout.js', 'www/luci-static/myos-new/layout.js'),
        (theme / 'usr/share/ucode/luci/template/themes/myos-new/footer.ut',
         'usr/share/ucode/luci/template/themes/myos-new/footer.ut'),
    ):
        destination = root / target
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        destination.chmod(0o644)

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

