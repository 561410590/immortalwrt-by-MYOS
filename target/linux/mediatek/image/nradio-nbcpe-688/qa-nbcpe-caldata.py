#!/usr/bin/env python3
"""Exercise the actual NBCPE extractor with synthetic files and native ash."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--root', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    script = (args.root / 'lib/preinit/91_nbcpe_extract_caldata') if args.root else source / 'target/linux/mediatek/filogic/base-files/lib/preinit/91_nbcpe_extract_caldata'
    text = script.read_text()
    helper = (args.root / 'lib/functions/nbcpe.sh') if args.root else source / 'target/linux/mediatek/image/nradio-nbcpe-688/overlay/lib/functions/nbcpe.sh'
    identity = helper.read_text()
    busybox = shutil.which('busybox')
    if not busybox:
        raise RuntimeError('Native BusyBox required')
    subprocess.run([busybox, 'ash', '-n', str(script)], check=True)
    passed, failures = [], []
    cases = [
        ('factory_odd_mac_bytes', 'factory', 20480, b'\x81\x79', 'HCMT7981-emmc-NB68', True, ''),
        ('Factory_label', 'Factory', 20480, b'\x81\x79', 'HCMT7981-emmc-NB68', True, ''),
        ('valid_board_mac_only_changes_six_bytes', 'factory', 20480, b'\x81\x79', 'HCMT7981-emmc-NB68', True, '02:11:22:33:44:50'),
        ('invalid_board_mac_keeps_calibration', 'factory', 20480, b'\x81\x79', 'HCMT7981-emmc-NB68', True, '01:11:22:33:44:50'),
        ('invalid_chip_id', 'factory', 20480, b'\xff\xff', 'HCMT7981-emmc-NB68', False, ''),
        ('short_eeprom', 'factory', 4096, b'\x81\x79', 'HCMT7981-emmc-NB68', False, ''),
        ('missing_factory', None, 0, b'', 'HCMT7981-emmc-NB68', False, ''),
        ('other_board', 'factory', 20480, b'\x81\x79', 'other,board', False, ''),
    ]
    with tempfile.TemporaryDirectory(prefix='nbcpe-caldata-') as temporary:
        base = Path(temporary)
        for name, label, size, header, board, expected, mac in cases:
            try:
                root = base / name
                (root / 'tmp').mkdir(parents=True)
                (root / 'dev').mkdir()
                data = bytearray(size)
                data[:len(header)] = header
                if size > 10:
                    # Deliberately odd first byte: this is not a board-MAC assertion.
                    data[4:10] = bytes.fromhex('1d0043266050')
                if label:
                    (root / label).write_bytes(data)
                body = re.sub(r'^\s*\. /lib/functions(?:/(?:system|nbcpe))?\.sh\n', '', text, flags=re.M)
                body = body.replace('[ -b "$factory" ]', '[ -f "$factory" ]')
                body = body.replace('/tmp/nbcpe-e2p.bin', str(root / 'tmp/e2p.bin'))
                body = body.replace('/lib/firmware', str(root / 'firmware'))
                body = body.replace('/dev/kmsg', str(root / 'dev/kmsg'))
                prelude = 'board_name() { echo ' + board + '; }\n'
                prelude += 'find_mmc_part() { [ -f "' + str(root) + '/$1" ] && echo "' + str(root) + '/$1"; }\n'
                prelude += 'boot_hook_add() { :; }\n'
                prelude += 'bdinfo() { echo "' + mac + '"; }\n' + identity + '\n'
                run_script = root / 'test.sh'
                run_script.write_text(prelude + body + '\nnbcpe_extract_caldata\n')
                result = subprocess.run([busybox, 'ash', str(run_script)], capture_output=True, text=True)
                output = root / 'firmware/e2p'
                assert output.is_file() == expected, result.stderr
                if expected:
                    assert result.returncode == 0, result.stderr
                    expected_data = bytearray(data)
                    if mac.startswith('02:'):
                        expected_data[4:10] = bytes.fromhex(mac.replace(':', ''))
                    assert output.read_bytes() == expected_data, 'Unexpected EEPROM byte change'
                    assert (root / label).read_bytes() == data, 'Source factory fixture changed'
                elif board == 'HCMT7981-emmc-NB68':
                    assert result.returncode != 0, 'Invalid EEPROM accepted'
                    assert 'EEPROM extraction failed' in result.stderr, result.stderr
                assert not (root / 'tmp/e2p.bin').exists(), 'Temporary EEPROM leaked'
                passed.append(name)
            except Exception as error:
                failures.append({'case': name, 'error': str(error)})
    print(json.dumps({'passed': len(passed), 'total': len(cases), 'cases_passed': passed,
                      'failures': failures, 'synthetic_calibration_only': True,
                      'fixtures_cleaned': True}))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
