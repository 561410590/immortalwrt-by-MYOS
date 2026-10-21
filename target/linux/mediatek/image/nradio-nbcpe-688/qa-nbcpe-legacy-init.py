#!/usr/bin/env python3
"""Check legacy-init guards with real AArch64 ash and isolated service stubs."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    assets = args.source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    root = args.source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    runtime = assets / 'official-assets/qa-build/qemu/runtime'
    environment = dict(os.environ)
    environment.pop('LD_PRELOAD', None)
    environment['LD_LIBRARY_PATH'] = str(runtime / 'usr/lib/x86_64-linux-gnu')
    ash = [str(runtime / 'usr/bin/qemu-aarch64'), '-U', 'LD_LIBRARY_PATH', '-U', 'LD_PRELOAD',
           '-L', str(root), str(root / 'bin/busybox'), 'ash']
    switch = assets / 'overlay/etc/init.d/switch_ctl'
    huawei = assets / 'official-assets/root/etc/cpetools/huawei.sh'
    for script in (switch, huawei):
        subprocess.run(ash + ['-n', str(script)], env=environment, check=True)
    blocked = '\n'.join(name + '() { exit 99; }' for name in
                        ('logger', 'sleep', 'sw_poweroff_ports', 'sw_poweron_ports'))
    switch_check = subprocess.run(ash + ['-c', blocked + '\n. ' + shlex.quote(str(switch)) +
        '\nstart_service\n! type service_triggers >/dev/null 2>&1'],
        env=environment, capture_output=True, text=True, check=True)
    assert not switch_check.stdout and not switch_check.stderr
    block = re.search(r'\tcommand_generic_imsreport "\$1"\n.*?\n\tcommand_generic_imsfmt "\$1"',
                      huawei.read_text(), re.S).group(0)
    cases = 0
    with tempfile.TemporaryDirectory(prefix='nbcpe-legacy-init-qa-', dir=assets) as directory:
        service = Path(directory) / 'smsd'
        block = block.replace('/etc/init.d/smsd', shlex.quote(str(service)))
        for installed in (False, True):
            if installed:
                service.write_text('#!/bin/sh\nprintf "restart\\n"\n')
                service.chmod(0o755)
            for storage_status in (0, 1):
                body = ('command_generic_imsreport() { printf "report\\n"; }\n'
                        'command_huawei_imssetstorage() { printf "storage\\n"; return "$1"; }\n'
                        'command_generic_imsfmt() { printf "format\\n"; }\n'
                        'set -- ' + str(storage_status) + '\n' + block)
                result = subprocess.run(ash + ['-c', body], env=environment,
                                        capture_output=True, text=True, check=True)
                expected = ['report', 'storage']
                if installed and storage_status == 0:
                    expected.append('restart')
                expected.append('format')
                assert result.stdout.splitlines() == expected, result.stdout
                assert not result.stderr, result.stderr
                cases += 1
    print(json.dumps({'aarch64_ash_syntax_files': 2, 'switch_no_legacy_io': True,
                      'switch_no_lan_reload_trigger': True, 'sms_guard_cases': cases,
                      'modem_commands_executed': 0, 'temporary_fixture_removed_on_exit': True}))


if __name__ == '__main__':
    main()
