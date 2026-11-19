"""Correct writable config, Lua and UI files in the N3000 build's local IPKs."""

from io import BytesIO
from pathlib import Path
import sys
import tarfile


def modem_payload(name, data):
    if name == 'etc/config/qmodem':
        return data.replace(b"option use_ubus '1'", b"option use_ubus '0'\n\toption enable_usb_scan '1'")
    if name == 'usr/share/qmodem/modem_scan.sh':
        return data.replace(b'.soft_reboot="1"', b'.soft_reboot="0"')
    if name == 'etc/uci-defaults/luci-app-qmodem':
        return data.replace(b'\tadd ucitrack qmodem\n\tset ucitrack.@qmodem[-1].init=qmodem_mwan\n', b'')
    if name in ('usr/share/qmodem/modem_util.sh', 'usr/share/qmodem/modem_ctrl.sh', 'usr/libexec/rpcd/qmodem'):
        data = data.replace(b'/usr/share/qmodem/vendor/generic.sh', b'/usr/share/qmodem/generic.sh')
        return data.replace(b'sms_tool_q -t 1 ', b'timeout 1 sms_tool ').replace(b'sms_tool_q ', b'sms_tool ')
    return data


def fix_copy(path):
    temporary = path.with_suffix('.ipk.tmp')
    with tarfile.open(path) as original, tarfile.open(temporary, 'w:gz') as output:
        for member in original:
            data = original.extractfile(member) if member.isfile() else None
            if member.name.removeprefix('./') == 'data.tar.gz':
                payload = BytesIO()
                with tarfile.open(fileobj=data) as files, tarfile.open(fileobj=payload, mode='w:gz') as fixed:
                    for entry in files:
                        name = entry.name.removeprefix('./').rstrip('/')
                        content = files.extractfile(entry) if entry.isfile() else None
                        shared_dirs = {'bin', 'sbin', 'lib', 'etc', 'etc/config', 'usr', 'usr/bin',
                                       'usr/sbin', 'usr/lib', 'usr/share', 'usr/libexec', 'usr/libexec/rpcd'}
                        public_dirs = ('www', 'usr/lib/ruby', 'usr/lib/lua', 'usr/share/luci', 'usr/share/rpcd')
                        if entry.isdir() and (name in shared_dirs or any(
                                name == p or name.startswith(p + '/') for p in public_dirs)):
                            entry.mode = 0o755
                        if entry.isfile() and entry.mode & 0o002:
                            entry.mode &= ~0o022
                        if content and path.name.startswith('qmodem_') and name in (
                                'etc/config/qmodem', 'etc/uci-defaults/luci-app-qmodem',
                                'usr/share/qmodem/modem_util.sh', 'usr/libexec/rpcd/qmodem',
                                'usr/share/qmodem/modem_scan.sh', 'usr/share/qmodem/modem_ctrl.sh'):
                            payload_bytes = modem_payload(name, content.read())
                            entry.size = len(payload_bytes)
                            content = BytesIO(payload_bytes)
                        fixed.addfile(entry, content)
                member.size = payload.tell()
                payload.seek(0)
                data = payload
            output.addfile(member, data)
    # Replace only the N3000 repo entry, which initially symlinks to the shared IPK.
    temporary.replace(path)


for ipk in sys.argv[1:]:
    fix_copy(Path(ipk))
