"""Bundle the supplied MWRT proxy packages into the C5800 build root.

Only package-owned files are copied. Kernel/modules and existing system
libraries stay from the C5800 build; no donor hardware or boot setup is used.
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

def install(source, work, records):
    image = Path(os.environ.get('MWRT_REFERENCE',
        '/mnt/f/Download/Mwrt-c2000ultra-tf-1153-24-20261028.bin'))
    unpack = work.parent / 'mwrt-1153-24-packages'
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
                raise RuntimeError('Build the C5800 kernel package: ' + choice)
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
