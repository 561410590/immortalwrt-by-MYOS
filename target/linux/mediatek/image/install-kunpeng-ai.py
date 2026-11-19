"""Install the original Kunpeng IPKs into a board rootfs before packing."""
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys


def install_device_status(root):
    source = Path(__file__).resolve().with_name('myos-kunpeng-status')
    rpc = root / 'usr/libexec/rpcd/infocd'
    skill = root / 'etc/skills/infocd-tool/SKILL.md'
    shutil.copyfile(source / 'infocd', rpc)
    rpc.chmod(0o755)
    (root / 'usr/bin/myos-device-status').symlink_to('../libexec/rpcd/infocd')
    shutil.copyfile(source / 'SKILL.md', skill)
    skill.chmod(0o644)


def main(root, opkg, myos_runtime=False):
    image = runpy.run_path(str(Path(__file__).with_name('myos-new-image.py')))
    env = dict(os.environ, IPKG_NO_SCRIPT='1', IPKG_INSTROOT=str(root),
               TMPDIR=str(root / 'tmp'))
    subprocess.run([
        opkg, '--conf', '/dev/null', '--offline-root', root, '--force-postinstall',
        '--add-dest', 'root:/', '--add-arch', 'all:100',
        '--add-arch', 'aarch64_cortex-a53:200', 'install', *image['kunpeng_packages'](),
    ], env=env, check=True)
    image['configure_kunpeng'](root, lambda *args: subprocess.run(args, check=True))
    if myos_runtime:
        install_device_status(root)
        subprocess.run([
            'patch', '--batch', '--no-backup-if-mismatch', '-d', root, '-p1',
            '-i', Path(__file__).resolve().with_name('myos-kunpeng-siliconflow.patch'),
        ], check=True)
    (root / 'etc/rc.d/S99zeroclaw').symlink_to('../init.d/zeroclaw')


if __name__ == '__main__':
    main(Path(sys.argv[1]).resolve(), sys.argv[2], '--myos-runtime' in sys.argv[3:])
