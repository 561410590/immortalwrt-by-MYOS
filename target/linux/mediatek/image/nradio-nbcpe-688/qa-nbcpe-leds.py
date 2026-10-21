#!/usr/bin/env python3
"""Exercise the assembled NBCPE WiFi LED helper without accessing real sysfs."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def read_helper(root):
    source = root / 'lib/netifd/wireless/mtwifi.sh'
    text = source.read_text()
    match = re.search(r'^nbcpe_update_wifi_led\(\) \{\n.*?^\}', text, re.M | re.S)
    if not match:
        raise RuntimeError('WiFi LED helper missing from ' + str(source))
    return match.group(0)


def run_checks(root, busybox):
    helper = read_helper(root)
    cases = (
        ('both_down_initial', False, False, None, True, 0),
        ('only_ra_up', True, False, None, True, 1),
        ('only_rax_up', False, True, None, True, 1),
        ('both_up', True, True, None, True, 1),
        ('ra_down_rax_stays_up', False, True, None, True, 1),
        ('rax_down_ra_stays_up', True, False, None, True, 1),
        ('both_down_after_up', False, False, None, True, 0),
        ('temporary_dark_flag', True, True, 'temporary', True, 0),
        ('persistent_dark_flag', True, True, 'persistent', True, 0),
        ('led_node_missing', True, True, None, False, None),
    )
    passed = 0
    failures = []
    with tempfile.TemporaryDirectory(prefix='nbcpe-led-test-') as directory:
        fixture = Path(directory)
        led = fixture / 'led' / 'brightness'
        network = fixture / 'net'
        temporary_flag = fixture / 'ledflag.conf'
        persistent_flag = fixture / 'ledflagc.conf'
        replacements = (
            ('/sys/class/leds/hc:blue:wifi/brightness', str(led), 1),
            ('/sys/class/net', str(network), 2),
            ('/tmp/ledflag.conf', str(temporary_flag), 1),
            ('/usr/bin/ledflagc.conf', str(persistent_flag), 1),
        )
        for old, new, count in replacements:
            if helper.count(old) != count:
                raise RuntimeError('Unexpected helper path occurrences: ' + old)
            helper = helper.replace(old, new)
        if '/sys/' in helper or '/tmp/ledflag.conf' in helper or '/usr/bin/' in helper:
            raise RuntimeError('An absolute product path remains in test helper')
        led.parent.mkdir()
        for interface in ('ra0', 'rax0'):
            (network / interface).mkdir(parents=True)
        for name, ra_up, rax_up, dark_flag, led_exists, expected in cases:
            for flag in (temporary_flag, persistent_flag):
                flag.unlink(missing_ok=True)
            if dark_flag:
                flag = temporary_flag if dark_flag == 'temporary' else persistent_flag
                flag.write_text('0\n')
            for interface, enabled in (('ra0', ra_up), ('rax0', rax_up)):
                (network / interface / 'flags').write_text('0x1003\n' if enabled else '0x1002\n')
            if led_exists:
                led.write_text('9\n')
            else:
                led.unlink(missing_ok=True)
            result = subprocess.run(
                [busybox, 'ash', '-c', helper + '\nnbcpe_update_wifi_led\n'],
                text=True, capture_output=True, check=False,
            )
            actual = led.read_text().strip() if led.exists() else None
            expected_text = str(expected) if expected is not None else None
            if result.returncode == 0 and actual == expected_text and not result.stderr:
                passed += 1
            else:
                failures.append({
                    'case': name, 'expected': expected_text, 'actual': actual,
                    'exit_code': result.returncode, 'stderr': result.stderr.strip(),
                })
    return {'passed': passed, 'total': len(cases), 'failures': failures, 'fixture_cleaned': True}


def run_service_checks(service, busybox):
    match = re.search(r'^start_service\(\) \{\n.*?^\}', service.read_text(), re.M | re.S)
    if not match:
        raise RuntimeError('Actual LED start_service missing')
    passed, failures = 0, []
    with tempfile.TemporaryDirectory(prefix='nbcpe-led-service-') as temporary:
        for persistent in (False, True):
            fixture = Path(temporary) / str(persistent)
            fixture.mkdir()
            system = fixture / 'system.sh'
            system.write_text('board_name() { echo HCMT7981-emmc-NB68; }\n')
            cli = fixture / 'atsd_tools_cli'
            cli.write_text('#!/bin/sh\nexit 1\n')
            cli.chmod(0o755)
            permanent = fixture / 'ledflagc.conf'
            flag = fixture / 'ledflag.conf'
            if persistent:
                permanent.write_text('0\n')
            for name in ('status', 'wifi', 'cmode4', 'cmode5'):
                led = fixture / ('hc:blue:' + name) / 'brightness'
                led.parent.mkdir()
                led.write_text('9\n')
            body = match.group(0)
            for old, new in (('/lib/functions/system.sh', system),
                             ('/usr/bin/atsd_tools_cli', cli),
                             ('/usr/bin/ledflagc.conf', permanent), ('/tmp/ledflag.conf', flag),
                             ('/sys/class/leds/hc:blue:', str(fixture / 'hc:blue:'))):
                body = body.replace(old, str(new))
            assert '/sys/' not in body and '/tmp/ledflag.conf' not in body
            stub = 'procd_open_instance() { :; }\nprocd_close_instance() { :; }\n'
            stub += 'procd_set_param() { echo "$*" >> "' + str(fixture / 'procd.log') + '"; }\n'
            result = subprocess.run([busybox, 'ash', '-c', stub + body + '\nstart_service\n'],
                                    text=True, capture_output=True, check=False)
            try:
                assert result.returncode == 0 and not result.stderr, result.stderr
                expected = '0' if persistent else '9'
                for name in ('status', 'wifi', 'cmode4', 'cmode5'):
                    assert (fixture / ('hc:blue:' + name) / 'brightness').read_text().strip() == expected
                assert flag.exists() == persistent
                procd = (fixture / 'procd.log').read_text()
                assert 'command /bin/sh /usr/share/modem-AK68/netmodeled-AK68.sh' in procd
                assert 'respawn' in procd
                passed += 1
            except Exception as error:
                failures.append({'case': 'led_service_persistent_' + str(persistent), 'error': str(error)})
    return {'passed': passed, 'total': 2, 'failures': failures}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--busybox', default='busybox')
    parser.add_argument('--service', type=Path)
    args = parser.parse_args()
    result = run_checks(args.root, args.busybox)
    service = run_service_checks(args.service or args.root / 'etc/init.d/myos-nbcpe-leds', args.busybox)
    result['passed'] += service['passed']
    result['total'] += service['total']
    result['failures'].extend(service['failures'])
    print(json.dumps(result, ensure_ascii=False))
    return 1 if result['failures'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
