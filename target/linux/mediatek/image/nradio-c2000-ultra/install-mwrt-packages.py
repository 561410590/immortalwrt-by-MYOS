#!/usr/bin/env python3
"""Install the selected MWRT packages and their runtime dependency closure."""
import io
import re
import sys
import tarfile
from pathlib import Path

root, tree = map(Path, sys.argv[1:3])
requested = sys.argv[3:]
info = root / 'usr/lib/opkg/info'
status = root / 'usr/lib/opkg/status'

def fields(text):
    return dict(line.split(': ', 1) for line in text.splitlines() if ': ' in line and not line.startswith(' '))

def records(text):
    return {fields(p)['Package']: p for p in re.split(r'\n\s*\n', text.strip()) if 'Package' in fields(p)}

def provides(meta):
    return [meta['Package']] + [re.split(r'[ (]', p.strip())[0] for p in meta.get('Provides', '').split(',') if p.strip()]

installed = records(status.read_text())
known = {alias for rec in installed.values() for alias in provides(fields(rec))}
catalog = {}

def ipk_parts(path):
    data = path.read_bytes()
    if data.startswith(b'!<arch>\n'):
        parts, pos = {}, 8
        while pos + 60 <= len(data):
            h = data[pos:pos + 60]
            size = int(h[48:58])
            parts[h[:16].decode().strip().rstrip('/')] = data[pos + 60:pos + 60 + size]
            pos += 60 + size + size % 2
        return parts
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:*') as archive:
        return {m.name.lstrip('./'): archive.extractfile(m).read() for m in archive.getmembers() if m.isfile()}

for path in sorted((tree / 'bin').glob('**/*.ipk'), key=lambda p: p.stat().st_mtime):
    parts = ipk_parts(path)
    control_name = next(k for k in parts if k.startswith('control.tar.'))
    with tarfile.open(fileobj=io.BytesIO(parts[control_name]), mode='r:*') as archive:
        control = {Path(m.name).name: archive.extractfile(m).read() for m in archive.getmembers() if m.isfile()}
    meta = fields(control['control'].decode())
    for alias in provides(meta):
        catalog[alias] = (path, meta, control)

active = set()
def install(name):
    if name in known:
        return
    if name in active:
        return
    if name not in catalog:
        raise SystemExit('C2000ULTRA missing compiled dependency: ' + name)
    path, meta, control = catalog[name]
    active.add(name)
    for dep in meta.get('Depends', '').split(','):
        alternatives = [re.split(r'[ (]', p.strip())[0] for p in dep.split('|') if p.strip()]
        if alternatives and not any(p in known for p in alternatives):
            install(next((p for p in alternatives if p in catalog), alternatives[0]))
    parts = ipk_parts(path)
    payload = next(v for k, v in parts.items() if k.startswith('data.tar.'))
    with tarfile.open(fileobj=io.BytesIO(payload), mode='r:*') as archive:
        for member in archive.getmembers():
            if member.name.startswith('/') or '..' in Path(member.name).parts:
                raise SystemExit('Invalid package path: ' + member.name)
            if member.mode == 0 and (member.isdir() or member.isfile()):
                member.mode = 0o755 if member.isdir() else 0o644
            destination = root / member.name
            # BusyBox applets are symlinks. Never write a replacement through them.
            if not member.isdir() and destination.is_symlink():
                destination.unlink()
        archive.extractall(root)
        listing = ['/' + m.name.lstrip('./') for m in archive.getmembers() if not m.isdir()]
    info.mkdir(parents=True, exist_ok=True)
    for key, value in control.items():
        if key != 'control':
            (info / (meta['Package'] + '.' + key)).write_bytes(value)
    (info / (meta['Package'] + '.control')).write_bytes(control['control'])
    (info / (meta['Package'] + '.list')).write_text('\n'.join(listing) + '\n')
    rec = control['control'].decode().rstrip()
    if 'Status:' not in rec:
        rec += '\nStatus: install ok installed'
    installed[meta['Package']] = rec
    known.update(provides(meta))
    for alternative in meta.get('Alternatives', '').split(','):
        if not alternative.strip():
            continue
        priority, link, target = alternative.strip().split(':', 2)
        selected = (int(priority), target)
        for record in installed.values():
            for other in fields(record).get('Alternatives', '').split(','):
                if not other.strip():
                    continue
                rank, other_link, other_target = other.strip().split(':', 2)
                if other_link == link and int(rank) > selected[0]:
                    selected = (int(rank), other_target)
        destination = root / link.lstrip('/')
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() or destination.is_symlink():
            destination.unlink()
        destination.symlink_to(selected[1])
    print('C2000ULTRA added', meta['Package'], meta['Version'])

for package in requested:
    install(package)
for path in ('', 'etc', 'usr', 'usr/bin', 'usr/sbin', 'usr/lib', 'usr/lib/lua',
             'usr/lib/lua/luci', 'usr/share', 'usr/share/rpcd', 'usr/share/rpcd/acl.d', 'www'):
    (root / path).chmod(0o755)
status.write_text('\n\n'.join(installed.values()) + '\n\n')
