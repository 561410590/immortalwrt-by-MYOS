#!/usr/bin/env python3
"""Run only shell branches with fake procd/Python; no native process or AT."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def ash(code):
    return subprocess.run(['/usr/bin/busybox', 'ash', '-c', code],
                          capture_output=True, text=True, timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--overlay', type=Path, required=True)
    args = parser.parse_args()
    service = (args.overlay / 'etc/init.d/myos-nbcpe-native').read_text()
    worker = (args.overlay / 'usr/libexec/myos-nbcpe-sms-forward').read_text()
    checks = []
    with tempfile.TemporaryDirectory(prefix='nbcpe-native-qa-') as directory:
        token = Path(directory) / 'token'
        lock = Path(directory) / 'lock'
        service = service.replace('/usr/bin/smstrun-AK68.conf', str(token))
        worker = worker.replace('/usr/bin/smstrun-AK68.conf', str(token))
        worker = worker.replace('/tmp/smstrun-AK68.lock', str(lock))
        worker = worker.replace('exec /usr/bin/python3 /usr/bin/smstrun-AK68.py',
                                "printf '%s\\n' FAKE_PYTHON_START")
        procd = '''
procd_open_instance() { printf 'instance:%s\\n' "$1"; }
procd_set_param() { [ "$1" != command ] || printf 'command:%s\\n' "$*"; }
procd_close_instance() { :; }
'''
        for has_token in (False, True):
            if has_token:
                token.write_text('fixture-token')
            result = ash(procd + service + '\nstart_service\n')
            output = result.stdout
            passed = result.returncode == 0 and 'instance:at_websocket' in output
            passed &= ('instance:sms_forward' in output) == has_token
            checks.append({'case': 'procd-token-' + str(has_token).lower(), 'passed': passed})
        token.unlink()
        for name, value, expected_start, expected_lock in [
                ('missing-token', None, False, False),
                ('token-no-lock', None, True, False),
                ('dead-lock', '2147483647', True, False),
                ('live-lock', str(os.getpid()), False, True)]:
            if name != 'missing-token':
                token.write_text('fixture-token')
            if value is not None:
                lock.write_text(value + '\n')
            result = ash(worker)
            passed = result.returncode == 0
            passed &= ('FAKE_PYTHON_START' in result.stdout) == expected_start
            passed &= lock.exists() == expected_lock
            checks.append({'case': name, 'passed': passed})
    passed = all(check['passed'] for check in checks)
    print(json.dumps({'passed': passed, 'checks': checks, 'real_AT': 0,
                      'native_processes_started': 0, 'network_requests': 0}))
    return not passed


if __name__ == '__main__':
    raise SystemExit(main())
