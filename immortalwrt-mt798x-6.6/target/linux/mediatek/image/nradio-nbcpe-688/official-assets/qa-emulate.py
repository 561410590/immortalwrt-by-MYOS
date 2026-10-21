#!/usr/bin/env python3
"""Build-only qemu extraction and safe loader/BusyBox checks; no daemons or AT."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    assets = args.source / 'target/linux/mediatek/image/nradio-nbcpe-688/official-assets'
    work = assets / 'qa-build/qemu'
    work.mkdir(parents=True, exist_ok=True)
    runtime = work / 'runtime'
    qemu = runtime / 'usr/bin/qemu-aarch64'
    if args.download:
        subprocess.run(['apt', 'download', 'qemu-user', 'libcapstone4', 'liburing2', 'libnuma1'],
                       cwd=work, check=True, timeout=180)
        for package in sorted(work.glob('*.deb')):
            subprocess.run(['dpkg-deb', '-x', str(package), str(runtime)], check=True)
    if not qemu.is_file():
        raise SystemExit('Run once with --download to extract build-only qemu-user')
    environment = dict(os.environ)
    environment.pop('LD_PRELOAD', None)
    environment['LD_LIBRARY_PATH'] = str(runtime / 'usr/lib/x86_64-linux-gnu')
    root = args.source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    command = [str(qemu), '-U', 'LD_LIBRARY_PATH', '-U', 'LD_PRELOAD', '-L', str(root)]
    libraries = ':'.join(map(str, [assets / 'root/usr/lib/myos-nbcpe-official', root / 'lib', root / 'usr/lib']))
    checks = []
    for name in ['atsd', 'atsd_cli', 'infocd', 'infocd_consumer', 'atserver-sniffer', 'bdinfo', 'system-blkid']:
        target = root / 'usr/sbin/blkid' if name == 'system-blkid' else assets / 'root/usr/libexec/myos-nbcpe-official' / name
        result = subprocess.run(command + [str(root / 'lib/ld-musl-aarch64.so.1'),
                                          '--library-path', libraries, '--list', str(target)],
                                env=environment, capture_output=True, text=True, timeout=20)
        checks.append({'name': name, 'loader_return': result.returncode,
                       'diagnostic': result.stderr.strip(), 'library_count': len(result.stdout.splitlines())})
    busybox_command = command + ['-0', 'busybox', str(root / 'bin/busybox')]
    busybox = subprocess.run(busybox_command + ['--list'], env=environment,
                             capture_output=True, text=True, timeout=20)
    applets = busybox.stdout.splitlines()
    if busybox.returncode != 0:
        # Stock BusyBox omitted FEATURE_INSTALLER/--list; --help lists applets.
        busybox = subprocess.run(busybox_command + ['--help'], env=environment,
                                 capture_output=True, text=True, timeout=20)
        applets = (busybox.stdout + busybox.stderr).replace(',', ' ').split()
    if 'flock' not in applets:
        # Verify the installed symlink dispatch even when the busybox applet is disabled.
        busybox = subprocess.run(command + ['-0', 'flock', str(root / 'bin/busybox'), '--help'],
                                 env=environment, capture_output=True, text=True, timeout=20)
        help_text = busybox.stdout + busybox.stderr
        if 'flock' in help_text and 'Usage:' in help_text and 'applet not found' not in help_text:
            applets = ['flock']
    checks.append({'name': 'BusyBox applet listing', 'return': busybox.returncode,
                   'flock': 'flock' in applets, 'diagnostic': '' if 'flock' in applets else busybox.stderr.strip()})
    # Run curl with only the image's own libraries. A loader check on the curl
    # executable alone misses unresolved symbols imported by libcurl itself.
    curl = subprocess.run(command + [str(root / 'usr/bin/curl'), '--version'],
                          env=environment, capture_output=True, text=True, timeout=20)
    checks.append({'name': 'curl actual AArch64 execution', 'return': curl.returncode,
                   'diagnostic': curl.stderr.strip(), 'result': curl.stdout.strip(),
                   'image_libraries_only': True, 'LD_PRELOAD': False})
    python_environment = dict(environment)
    python_environment['PYTHONHOME'] = str(root / 'usr')
    python_environment['PYTHONPATH'] = str(root / 'usr/lib/python3.9/site-packages')
    python_environment['PYTHONDONTWRITEBYTECODE'] = '1'
    import_code = ("import json,ssl,asyncio,requests,websockets,bitarray; "
                   "from smspdudecoder.fields import SMSDeliver; "
                   "assert bitarray.bitarray('101').to01()=='101'; "
                   "print(json.dumps({'requests':requests.__version__, 'websockets':websockets.__version__, "
                   "'bitarray':bitarray.__version__, 'ssl':ssl.OPENSSL_VERSION}))")
    python = subprocess.run(command + [str(root / 'usr/bin/python3.9'), '-B', '-c', import_code],
                            env=python_environment, capture_output=True, text=True, timeout=60)
    checks.append({'name': 'AK68 Python offline imports', 'return': python.returncode,
                   'diagnostic': python.stderr.strip(), 'result': python.stdout.strip()})
    ok = all(item['loader_return'] == 0 for item in checks if 'loader_return' in item)
    ok = ok and 'flock' in applets and curl.returncode == 0 and python.returncode == 0
    print(json.dumps({'passed': ok, 'checks': checks, 'real_AT': 0, 'network_requests': 0}))
    return not ok


if __name__ == '__main__':
    raise SystemExit(main())
