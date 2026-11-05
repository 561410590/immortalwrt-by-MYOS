"""Bundle the supplied MWRT proxy packages into a MYOS build root.

Only package-owned files are copied. Kernel/modules and existing system
libraries stay from the target build; no donor hardware or boot setup is used.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tarfile

PACKAGES = {
    'luci-app-openclash', 'luci-app-passwall', 'luci-app-passwall2',
    'luci-app-ssr-plus', 'luci-i18n-passwall-zh-cn',
    'luci-i18n-passwall2-zh-cn', 'luci-i18n-ssr-plus-zh-cn',
    'sing-box', 'xray-core', 'xray-plugin', 'hysteria', 'tuic-client',
    'naiveproxy', 'shadowsocks-rust-sslocal', 'shadowsocks-rust-ssserver',
    'shadowsocksr-libev-ssr-local', 'shadowsocksr-libev-ssr-redir',
    'shadowsocksr-libev-ssr-server', 'chinadns-ng', 'dns2socks', 'dns2tcp',
    'haproxy', 'v2ray-plugin', 'v2ray-geoip', 'v2ray-geosite', 'smartdns',
    'microsocks', 'ipt2socks', 'shadow-tls', 'simple-obfs-client',
}

# Resolve these against the target's current IPKs, including its kernel ABI.
SYSTEM_PACKAGES = {
    'coreutils-base64', 'coreutils-nohup', 'coreutils-timeout', 'lyaml',
    'libyaml', 'libudns', 'resolveip', 'libev', 'libsodium', 'ip-full',
    'libuci-lua', 'libbpf1', 'libelf1', 'iptables-mod-tproxy', 'libpcap1',
    'ipset', 'libipset13', 'tc-full', 'libltdl7', 'liblua5.3-5.3', 'dnsmasq-full',
}

def install(source, work, records):
    image = Path(os.environ.get('MWRT_REFERENCE',
        '/mnt/f/Download/Mwrt-c2000ultra-tf-1153-24-20261028.bin'))
    unpack = source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/mwrt-1153-24-packages'
    tool = source / 'staging_dir/host/bin/unsquashfs4'
    # Reuse the existing extracted input when the original BIN is no longer present.
    if image.is_file():
        with tarfile.open(image) as archive:
            member = next(m for m in archive.getmembers() if m.name.endswith('/root'))
            offset = member.offset_data
        # This is a build input directory, never an executable donor environment.
        subprocess.run([str(tool), '-no-xattrs', '-force', '-o', str(offset),
                        '-d', str(unpack), str(image), 'usr', 'etc', 'lib', 'www', 'bin', 'sbin'],
                       check=True)
    donor_records = {}
    for record in (unpack / 'usr/lib/opkg/status').read_text().strip().split('\n\n'):
        match = re.search(r'^Package: (.+)$', record, re.M)
        if match:
            donor_records[match[1]] = record
    provided = set(records)
    for record in records.values():
        match = re.search(r'^Provides: (.+)$', record, re.M)
        if match:
            provided.update(x.strip().split()[0] for x in match[1].split(','))
    wanted = set(PACKAGES)
    pending = list(wanted)
    while pending:
        name = pending.pop()
        record = donor_records[name]
        match = re.search(r'^Depends: (.+)$', record, re.M)
        for dep in match[1].split(',') if match else []:
            choices = [x.strip().split()[0] for x in dep.split('|')]
            if any(x in provided or x in wanted for x in choices):
                continue
            choice = next((x for x in choices if x in donor_records), choices[0])
            if choice == 'kernel' or choice.startswith('kmod-'):
                raise RuntimeError('Build the target kernel package: ' + choice)
            wanted.add(choice)
            pending.append(choice)
    for name in sorted(wanted):
        manifest = unpack / ('usr/lib/opkg/info/' + name + '.list')
        installed = []
        for line in manifest.read_text().splitlines():
            relative = line.lstrip('/')
            if not relative or relative.startswith('etc/rc.d/'):
                continue
            origin = unpack / relative
            target = work / relative
            if origin.is_dir() and not origin.is_symlink():
                target.mkdir(parents=True, exist_ok=True)
                continue
            if not origin.exists() and not origin.is_symlink():
                # First-boot generated configs are installed by the vendor defaults.
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_symlink() or target.is_file():
                target.unlink()
            if origin.is_symlink():
                target.symlink_to(os.readlink(origin))
            else:
                shutil.copy2(origin, target)
            installed.append('/' + relative)
        for metadata in (unpack / 'usr/lib/opkg/info').glob(name + '.*'):
            if metadata.suffix != '.list':
                shutil.copy2(metadata, work / 'usr/lib/opkg/info' / metadata.name)
        (work / 'usr/lib/opkg/info' / (name + '.list')).write_text('\n'.join(installed) + '\n')
        records[name] = donor_records[name]
        print('MWRT package:', name, re.search(r'^Version: (.+)$', records[name], re.M)[1], flush=True)
    # Use the shipped pristine defaults; no node credentials are carried over.
    for name in ('passwall', 'passwall2'):
        default = unpack / ('usr/share/' + name + '/0_default_config')
        if default.is_file():
            shutil.copy2(default, work / 'etc/config' / name)
            manifest = work / ('usr/lib/opkg/info/luci-app-' + name + '.list')
            with manifest.open('a') as stream:
                stream.write('/etc/config/' + name + '\n')
    (work / 'usr/lib/opkg/status').write_text('\n\n'.join(records.values()) + '\n\n')
    return {name: re.search(r'^Version: (.+)$', records[name], re.M)[1]
            for name in sorted(wanted)}


def install_bundle(source, work, records):
    versions = install(source, work, records)
    assets = source / 'target/linux/mediatek/image/nradio-c5800-688'
    for name in ('ASN.mmdb', 'GeoIP.dat'):
        shutil.copy2(assets / ('openclash-' + name), work / 'etc/openclash' / name)
        with (work / 'usr/lib/opkg/info/luci-app-openclash.list').open('a') as stream:
            stream.write('/etc/openclash/' + name + '\n')
    shutil.copy2(work / 'usr/share/shadowsocksr/shadowsocksr.config',
                 work / 'etc/config/shadowsocksr')

    def defaults(name, section, options):
        path = work / 'etc/config' / name
        pattern = r'(?ms)^(config[ \t]+' + re.escape(section) + r'(?:[ \t]+[^\n]*)?\n)(.*?)(?=^config[ \t]|\Z)'
        def update(match):
            body = match[2]
            for key, value in options.items():
                body = re.sub(r'(?m)^[ \t]*option[ \t]+' + re.escape(key) + r'[ \t]+[^\n]*(?:\n|$)', '', body)
                if value is not None:
                    body = "\toption " + key + " '" + str(value) + "'\n" + body
            return match[1] + body
        path.write_text(re.sub(pattern, update, path.read_text(), count=1))

    for name in ('passwall', 'passwall2'):
        defaults(name, 'global', {'enabled': '0', 'socks_enabled': '0', 'acl_enable': '0'})
        defaults(name, 'global_haproxy', {'balancing_enable': '0'})
        defaults(name, 'global_delay', {'start_week_mode': None, 'stop_week_mode': None, 'restart_week_mode': None})
        defaults(name, 'global_rules', {'update_week_mode': None})
        defaults(name + '_server', 'global', {'enable': '0'})
    defaults('shadowsocksr', 'global', {'global_server': 'nil', 'enable_switch': '0'})
    defaults('shadowsocksr', 'socks5_proxy', {'server': 'nil'})
    defaults('shadowsocksr', 'http_proxy', {'server': 'nil'})
    defaults('shadowsocksr', 'server_global', {'enable_server': '0'})
    defaults('shadowsocksr', 'server_subscribe', {'auto_update': '0'})
    defaults('openclash', 'openclash', {
        'enable': '0', 'enable_redirect_dns': '0', 'interface_name': '0',
        'core_type': 'Smart', 'smart_enable': '1', 'enable_geoip_dat': '1',
        'small_flash_memory': '0', 'router_self_proxy': '1', 'proxy_mode': 'rule',
        'en_mode': 'fake-ip', 'enable_meta_sniffer': '1', 'enable_respect_rules': '0',
        'enable_rule_proxy': '0', 'geo_auto_update': '1', 'geoip_auto_update': '1',
        'geosite_auto_update': '1', 'geoasn_auto_update': '1',
    })
    # Keep normal init registration; each plugin retains its disabled ROM default.
    for name in ('openclash', 'passwall', 'passwall2', 'passwall_server',
                 'passwall2_server', 'shadowsocksr'):
        text = (work / 'etc/init.d' / name).read_text()
        for key, prefix in (('START', 'S'), ('STOP', 'K')):
            match = re.search(r'^' + key + r'=(\d+)', text, re.M)
            if match:
                link = work / 'etc/rc.d' / (prefix + match[1].zfill(2) + name)
                if not link.is_symlink():
                    link.symlink_to('../init.d/' + name)
    return versions
