#!/usr/bin/env python3
"""Exercise boot-time Wi-Fi defaults with in-memory UCI mocks, never a router."""
import importlib.util
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

WORK = Path(__file__).resolve().parent
HELPER = WORK / 'overlay/lib/functions/myos-c8-factory-wifi.sh'
BASH = shutil.which('bash') or str(Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/usr/bin/bash.exe')
FIXTURE = {
    'radio0.band': '2g', 'radio1.band': '5g',
    'ap0.mode': 'ap', 'ap0.device': 'radio0', 'ap0.network': 'lan', 'ap0.ssid': 'NRadio_C5800-688_2.4G',
    'ap1.mode': 'ap', 'ap1.device': 'radio1', 'ap1.network': 'lan', 'ap1.ssid': 'NRadio_C5800-688_5G',
}
MOCKS = r'''
config_load() { return 0; }
config_get() {
    local target="$1" entry="$2.$3" fallback="${4:-}" answer
    answer="${CFG[$entry]-$fallback}"
    printf -v "$target" '%s' "$answer"
}
config_foreach() {
    local callback="$1" item
    for item in $SECTIONS; do "$callback" "$item"; done
}
uci() {
    printf 'UCI %s\n' "$*"
    if [ "$FAIL_COMMIT" = 1 ] && [ "${2:-}" = commit ]; then return 1; fi
    return 0
}
myos_c8_board() { [ "$BOARD" = 1 ]; }
myos_c8_wifi_mac() { myos_c8_wifi_validate_mac "$TEST_MAC"; }
'''


class FactoryWifiTests(unittest.TestCase):
    def run_defaults(self, overrides=None, *, fresh=False, mac='02:11:22:AA:BB:CC',
                     sections='ap0 ap1', board=True, fail_commit=False):
        config = dict(FIXTURE)
        config.update(overrides or {})
        with tempfile.TemporaryDirectory(prefix='c8-wifi-') as directory:
            root = Path(directory)
            (root / 'tmp').mkdir()
            marker = root / 'tmp/myos-c8-factory-wifi-init'
            if fresh:
                marker.touch()
            assignments = '\n'.join('CFG[' + shlex.quote(key) + ']=' + shlex.quote(value)
                                    for key, value in config.items())
            script = '\n'.join((
                'PATH=/usr/bin:/bin:$PATH', '. "$1"', 'declare -A CFG', assignments, MOCKS,
                'ROOT=' + shlex.quote(root.as_posix()), 'TEST_MAC=' + shlex.quote(mac),
                'SECTIONS=' + shlex.quote(sections), 'BOARD=' + str(int(board)),
                'FAIL_COMMIT=' + str(int(fail_commit)), 'myos_c8_wifi_apply "$ROOT"'))
            result = subprocess.run([BASH, '-c', script, '--', HELPER.as_posix()],
                                    capture_output=True, text=True, encoding='utf-8')
            self.assertEqual(result.stderr, '', result.stderr)
            return result.returncode, result.stdout.splitlines(), marker.exists()

    def test_fresh_both_bands(self):
        code, writes, marker = self.run_defaults(fresh=True)
        self.assertEqual(code, 0)
        self.assertFalse(marker)
        for section, band in (('ap0', '2.4G'), ('ap1', '5G')):
            self.assertIn(f'UCI -q set wireless.{section}.ssid=@NRadio-BBCC-{band}', writes)
            self.assertIn(f'UCI -q set wireless.{section}.key=22AABBCC', writes)
            self.assertIn(f'UCI -q set wireless.{section}.encryption=psk2', writes)

    def test_legacy_defaults_migrate_without_network_marker(self):
        code, writes, _ = self.run_defaults()
        self.assertEqual(code, 0)
        self.assertIn('UCI -q set wireless.ap0.key=22AABBCC', writes)
        self.assertIn('UCI -q set wireless.ap1.ssid=@NRadio-BBCC-5G', writes)

    def test_hwmode_fallback_distinguishes_bands(self):
        _, writes, _ = self.run_defaults({'radio0.band': '', 'radio1.band': '',
                                         'radio0.hwmode': '11g', 'radio1.hwmode': '11a'})
        self.assertIn('UCI -q set wireless.ap1.ssid=@NRadio-BBCC-5G', writes)

    def test_custom_password_on_default_ssid_is_preserved(self):
        _, writes, _ = self.run_defaults({'ap0.key': 'user saved password', 'ap0.encryption': 'sae-mixed'})
        self.assertTrue(any('wireless.ap0.ssid=' in line for line in writes))
        self.assertFalse(any('wireless.ap0.key=' in line or 'wireless.ap0.encryption=' in line for line in writes))

    def test_custom_ssid_with_empty_password_is_preserved(self):
        _, writes, _ = self.run_defaults({'ap0.ssid': 'My network', 'ap1.ssid': 'My network 5G'})
        self.assertEqual(writes, [])

    def test_generated_names_are_idempotent(self):
        _, writes, _ = self.run_defaults({'ap0.ssid': '@NRadio-BBCC-2.4G', 'ap1.ssid': '@NRadio-BBCC-5G',
                                         'ap0.key': '22AABBCC', 'ap1.key': '22AABBCC'})
        self.assertEqual(writes, [])

    def test_guest_and_station_are_untouched(self):
        fields = {'guest.mode': 'ap', 'guest.device': 'radio0', 'guest.network': 'lan', 'guest.ssid': 'MYOS',
                  'sta.mode': 'sta', 'sta.device': 'radio1', 'sta.network': 'lan', 'sta.ssid': 'MYOS'}
        _, writes, _ = self.run_defaults(fields, fresh=True, sections='guest sta ap0 ap1')
        self.assertFalse(any('wireless.guest.' in line or 'wireless.sta.' in line for line in writes))

    def test_guest_flags_and_non_lan_ap_are_untouched(self):
        fields = {'ap0.guest': '1', 'ap1.network': 'wan'}
        _, writes, _ = self.run_defaults(fields)
        self.assertEqual(writes, [])
        _, writes, _ = self.run_defaults({'ap0.is_guest': '1', 'ap1.is_guest': '1'})
        self.assertEqual(writes, [])

    def test_only_first_primary_ap_per_device(self):
        fields = {'ap2.mode': 'ap', 'ap2.device': 'radio0', 'ap2.network': 'lan', 'ap2.ssid': 'MYOS'}
        _, writes, _ = self.run_defaults(fields, fresh=True, sections='ap0 ap2 ap1')
        self.assertFalse(any('wireless.ap2.' in line for line in writes))

    def test_custom_primary_does_not_promote_secondary_ap(self):
        fields = {'ap0.ssid': 'Custom primary', 'ap2.mode': 'ap', 'ap2.device': 'radio0',
                  'ap2.network': 'lan', 'ap2.ssid': 'MYOS', 'ap1.ssid': 'Custom 5G'}
        _, writes, _ = self.run_defaults(fields, sections='ap0 ap2 ap1')
        self.assertEqual(writes, [])

    def test_invalid_mac_never_writes(self):
        for mac in ('', 'bad', '001122334455', '00:00:00:00:00:00', 'FF:FF:FF:FF:FF:FF',
                    '00:66:88:00:00:00', '01:11:22:33:44:55'):
            with self.subTest(mac=mac):
                code, writes, marker = self.run_defaults(mac=mac, fresh=True)
                self.assertNotEqual(code, 0)
                self.assertEqual(writes, [])
                self.assertTrue(marker)

    def test_lowercase_mac_normalizes(self):
        _, writes, _ = self.run_defaults(mac='02:11:22:aa:bb:cc')
        self.assertIn('UCI -q set wireless.ap0.key=22AABBCC', writes)

    def test_fresh_incomplete_bands_keep_marker_without_writes(self):
        code, writes, marker = self.run_defaults(fresh=True, sections='ap0')
        self.assertNotEqual(code, 0)
        self.assertEqual(writes, [])
        self.assertTrue(marker)

    def test_other_board_never_writes(self):
        code, writes, marker = self.run_defaults(fresh=True, board=False)
        self.assertEqual(code, 0)
        self.assertEqual(writes, [])
        self.assertTrue(marker)

    def test_commit_failure_does_not_consume_marker(self):
        code, _, marker = self.run_defaults(fresh=True, fail_commit=True)
        self.assertNotEqual(code, 0)
        self.assertTrue(marker)

    def test_preinit_preserves_saved_overlay(self):
        for saved in ('', 'overlay/etc/config/wireless', 'overlay/upper/etc/config/wireless'):
            with self.subTest(saved=saved), tempfile.TemporaryDirectory(prefix='c8-wifi-mark-') as directory:
                root = Path(directory)
                (root / 'tmp').mkdir()
                if saved:
                    path = root / saved
                    path.parent.mkdir(parents=True)
                    path.touch()
                script = '. "$1"; myos_c8_board() { return 0; }; myos_c8_wifi_mark_if_new "$2"'
                result = subprocess.run([BASH, '-c', script, '--', HELPER.as_posix(), root.as_posix()],
                                        capture_output=True, text=True, encoding='utf-8')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual((root / 'tmp/myos-c8-factory-wifi-init').exists(), not bool(saved))

    def test_packed_wifi_requires_boot_hook_and_no_legacy_initializer(self):
        spec = importlib.util.spec_from_file_location('verify_c8', WORK / 'verify-c8.py')
        verifier = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(verifier)
        with tempfile.TemporaryDirectory(prefix='c8-wifi-pack-') as directory:
            root = Path(directory)
            for relative in ('lib/functions/myos-c8-factory-wifi.sh', 'usr/libexec/myos-c8-identity',
                             'lib/preinit/82_myos_c8_factory_wifi', 'etc/init.d/myos-c8-factory-wifi',
                             'etc/uci-defaults/94-myos-c8-factory-wifi', 'etc/uci-defaults/93-myos-c8'):
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(WORK / 'overlay' / relative, target)
            start = root / 'etc/rc.d/S19myos-c8-factory-wifi'
            start.parent.mkdir(parents=True)
            start.touch()
            # Model the Linux init link without requiring Windows symlink privileges.
            with mock.patch.object(Path, 'is_symlink', lambda path: path == start and path.exists()), \
                 mock.patch.object(Path, 'readlink', return_value=Path('../init.d/myos-c8-factory-wifi')):
                self.assertTrue(verifier.check_factory_wifi(root)['hardware_mac'])
                legacy = root / 'etc/init.d/myos-factory-wifi'
                legacy.touch()
                with self.assertRaisesRegex(ValueError, 'Another board'):
                    verifier.check_factory_wifi(root)
                legacy.unlink()
                start.unlink()
                with self.assertRaisesRegex(ValueError, 'S19'):
                    verifier.check_factory_wifi(root)


if __name__ == '__main__':
    unittest.main()
