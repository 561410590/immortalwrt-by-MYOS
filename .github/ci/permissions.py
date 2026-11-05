"""Read actual IPK, assembly and final firmware permission metadata."""
from pathlib import Path
import argparse
import io
import json
import os
import re
import stat
import subprocess
import tarfile

def fields(text):
    return dict(re.findall(r'^([^:\n]+): (.*)$', text, re.M))

def member(m):
    return dict(path=m.name, mode=oct(m.mode), uid=m.uid, gid=m.gid,
                type=m.type.decode(), link=m.linkname)

def main():
    parser = argparse.ArgumentParser()
    for name in ('source', 'root', 'firmware', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    def save(name, value):
        (args.output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    installed = {}
    for text in (args.root / 'usr/lib/opkg/status').read_text().split('\n\n'):
        record = fields(text)
        if 'Package' in record:
            installed[record['Package']] = record
    selected = {}
    for directory in (args.source / 'bin',
            args.source / 'target/linux/mediatek/image/nradio-c8-688/packages',
            args.source / 'target/linux/mediatek/image/nradio-nbcpe-688/vantage/packages'):
        for path in directory.rglob('*.ipk'):
            with tarfile.open(path) as outer:
                cm = next(m for m in outer if Path(m.name).name.startswith('control.tar'))
                with tarfile.open(fileobj=io.BytesIO(outer.extractfile(cm).read())) as inner:
                    control = next(m for m in inner if Path(m.name).name == 'control')
                    record = fields(inner.extractfile(control).read().decode())
            name = record['Package']
            if name not in installed or record['Version'] != installed[name]['Version']:
                continue
            if name not in selected or path.stat().st_mtime > selected[name].stat().st_mtime:
                selected[name] = path
    package_rows = []
    for name, path in sorted(selected.items()):
        with tarfile.open(path) as outer:
            for part in ('control', 'data'):
                cm = next(m for m in outer.getmembers() if Path(m.name).name.startswith(part + '.tar'))
                with tarfile.open(fileobj=io.BytesIO(outer.extractfile(cm).read())) as inner:
                    package_rows.extend(member(m) | dict(package=name, part=part, archive=str(path.relative_to(args.source))) for m in inner)
    save('ipk-metadata.json', package_rows)
    assembly = []
    for path in [args.root, *args.root.rglob('*')]:
        metadata = path.lstat()
        assembly.append(dict(path='/' + str(path.relative_to(args.root)),
            mode=stat.filemode(metadata.st_mode), uid=metadata.st_uid, gid=metadata.st_gid,
            link=os.readlink(path) if path.is_symlink() else ''))
    save('assembled-metadata.json', assembly)
    image = args.output / 'final-root.squashfs'
    with tarfile.open(args.firmware) as archive:
        save('sysupgrade-metadata.json', [member(m) for m in archive])
        root = next(m for m in archive.getmembers() if m.name.endswith('/root'))
        with image.open('wb') as output:
            import shutil
            shutil.copyfileobj(archive.extractfile(root), output)
    tool = args.source / 'staging_dir/host/bin/unsquashfs4'
    listing = subprocess.check_output([tool, '-lln', image], text=True)
    (args.output / 'final-permissions.txt').write_text(listing)
    entries = []
    for line in listing.splitlines():
        match = re.match(r'^([-bcdlps][rwxstST-]{9})\s+(\d+)/(\d+)\s+.+?\s+\d{4}-\d\d-\d\d \d\d:\d\d squashfs-root(.*)$', line)
        if match:
            mode, uid, gid, tail = match.groups()
            path, _, link = tail.partition(' -> ')
            entries.append(dict(path=path or '/', mode=mode, uid=int(uid), gid=int(gid), type=mode[0], link=link))
    save('final-metadata.json', entries)
    by_path = {e['path']: e for e in entries}
    issues, runtime, sourced = [], [], []
    def resolve(path):
        pending, result, hops = path.split('/'), [], 0
        while pending:
            part = pending.pop(0)
            if part in ('', '.'):
                continue
            if part == '..':
                result = result[:-1]
                continue
            candidate = '/' + '/'.join(result + [part])
            entry = by_path.get(candidate)
            if entry and entry['type'] == 'l':
                hops += 1
                if hops > 50:
                    return candidate, 'link loop'
                if entry['link'].startswith('/'):
                    result = []
                pending = entry['link'].split('/') + pending
            else:
                result.append(part)
        return '/' + '/'.join(result), None
    for entry in entries:
        path, mode, kind = entry['path'], entry['mode'], entry['type']
        if kind == 'd' and mode[3] not in 'xs':
            issues.append([path, 'owner cannot traverse', mode])
        if kind == 'l':
            target, error = resolve(path)
            if error:
                issues.append([path, error])
            elif target not in by_path:
                if target.startswith(('/tmp/', '/proc/', '/sys/', '/dev/', '/var/lib/nginx/')):
                    runtime.append([path, target])
                else:
                    issues.append([path, 'unresolved link', target])
            continue
        if kind != '-':
            continue
        if mode[1] != 'r':
            issues.append([path, 'owner cannot read', mode])
        direct = path.startswith(('/bin/', '/sbin/', '/usr/bin/', '/usr/sbin/', '/etc/init.d/', '/usr/libexec/rpcd/', '/www/cgi-bin/'))
        if direct and mode[3] not in 'xs' and path != '/sbin/rsno-off.sh':
            issues.append([path, 'direct executable', mode])
        if path.startswith('/www/'):
            if mode[7] != 'r':
                issues.append([path, 'public Web read', mode])
            for parent in Path(path).parents:
                item = by_path.get(str(parent))
                if item and item['type'] == 'd' and item['mode'][9] not in 'xt':
                    issues.append([str(parent), 'public Web traversal', item['mode']])
        if path.startswith(('/etc/uci-defaults/', '/etc/hotplug.d/', '/lib/preinit/')):
            sourced.append(entry)
        if 'qmodem' in path and (path.endswith(('/modem_scan.sh', '/modem_hook.sh')) or '/vendor/' in path):
            if mode[3] not in 'xs':
                issues.append([path, 'QModem execution', mode])
    version = subprocess.check_output([tool, '-cat', image, 'etc/myos-version'], text=True).strip()
    if version.lstrip('Vv') != args.version:
        issues.append(['etc/myos-version', 'firmware version', version])
    save('sourced-metadata.json', sourced)
    save('permission-issues.json', issues)
    summary = dict(firmware=args.firmware.name, version=version, packages=len(selected),
        retained_packages=sorted(set(installed) - set(selected)), ipk_entries=len(package_rows),
        assembled_entries=len(assembly), final_entries=len(entries), runtime_links=runtime,
        final_owners=sorted({str(e['uid']) + ':' + str(e['gid']) for e in entries}), issues=issues)
    save('summary.json', summary)
    image.unlink()
    print(json.dumps(summary, ensure_ascii=False))
    if issues or not entries:
        raise SystemExit(1)

if __name__ == '__main__':
    main()
