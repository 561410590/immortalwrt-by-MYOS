#!/usr/bin/env python3
"""Run final-root sysinfo hooks with its actual AArch64 strings applet."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


def shell_function(text, name):
    match = re.search(r'^' + re.escape(name) + r'\(\)\s*\{.*?^\}', text, re.M | re.S)
    if not match:
        raise RuntimeError('Actual shell function missing: ' + name)
    return match.group(0)


def image_link_target(root, relative):
    path = root / relative
    for _ in range(16):
        if not path.is_symlink():
            if not path.is_file():
                raise RuntimeError('Missing image executable: ' + str(path))
            return path
        target = Path(os.readlink(path))
        path = root / str(target).lstrip('/') if target.is_absolute() else path.parent / target
        if root not in path.absolute().parents:
            raise RuntimeError('Image link escapes root: ' + str(path))
    raise RuntimeError('Image symlink loop: ' + relative)


def checks(root, qemu, environment, busybox):
    # Resolve links within the firmware root, never against the build host /bin.
    strings = image_link_target(root, 'usr/bin/strings')
    assert strings == root / 'bin/busybox', 'strings must use the retained BusyBox applet'
    assert os.readlink(root / 'usr/bin/strings') == '/bin/busybox'
    hook_code = (root / 'lib/functions/preinit.sh').read_text()
    sysinfo_code = (root / 'lib/preinit/02_sysinfo').read_text()
    board_name = shell_function((root / 'lib/functions.sh').read_text(), 'board_name')
    qemu_command = [str(qemu), '-U', 'LD_LIBRARY_PATH', '-L', str(root),
                    '-0', 'strings', str(strings)]
    passed = []

    with tempfile.TemporaryDirectory(prefix='nbcpe-preinit-test-') as temporary:
        parent = Path(temporary)

        def run_case(name, compatible, expected, broken=False):
            fixture = parent / name
            for path in ('bin', 'proc/device-tree', 'tmp'):
                (fixture / path).mkdir(parents=True, exist_ok=True)
            (fixture / 'proc/device-tree/compatible').write_bytes(compatible)
            (fixture / 'proc/device-tree/model').write_text('NBCPE real preinit fixture')
            for applet in ('mkdir', 'head', 'cat'):
                (fixture / 'bin' / applet).symlink_to(busybox)
            strings_entry = fixture / 'bin/strings'
            if broken:
                strings_entry.symlink_to('missing-binutils-strings')
            else:
                # argv0 and binary come from the installed strings link. Its
                # actual target applet parses bytes; no strings output is mocked.
                strings_entry.write_text('#!/bin/sh\nexec ' + shlex.join(qemu_command) + ' "$@"\n')
                strings_entry.chmod(0o755)

            def relocate(text):
                return text.replace('/proc/device-tree', str(fixture / 'proc/device-tree')).replace(
                    '/tmp/sysinfo', str(fixture / 'tmp/sysinfo'))

            script = hook_code + '\nboot_hook_init preinit_main\n'
            script += relocate(sysinfo_code) + '\nboot_run_hook preinit_main\n'
            script += relocate(board_name) + '\nprintf "board=%s\\n" "$(board_name)"\n'
            env = dict(environment, PATH=str(fixture / 'bin'))
            result = subprocess.run([str(busybox), 'ash', '-c', script], env=env,
                                    capture_output=True, text=True, timeout=30)
            actual = (fixture / 'tmp/sysinfo/board_name').read_text().strip()
            assert actual == expected, (name, actual, expected, result.stderr)
            assert 'board=' + expected + '\n' in result.stdout, result.stdout
            if broken:
                assert 'strings' in result.stderr and actual == '', result.stderr
            else:
                assert result.returncode == 0 and not result.stderr, result.stderr
                assert (fixture / 'tmp/sysinfo/model').read_text().strip() == 'NBCPE real preinit fixture'
            passed.append(name)

        compatible = b'HCMT7981-emmc-NB68\0mediatek,mt7981-emmc-rfb\0'
        run_case('actual_root_strings_preinit_and_board_name', compatible, 'HCMT7981-emmc-NB68')
        run_case('first_compatible_selected', b'other,board\0HCMT7981-emmc-NB68\0', 'other,board')
        run_case('old_dangling_strings_reproduces_empty_board', compatible, '', broken=True)
    return {'passed': len(passed), 'total': len(passed), 'cases_passed': passed,
            'strings_target': '/bin/busybox', 'real_target_strings': True,
            'actual_preinit_hooks': True, 'real_AT': 0, 'network_requests': 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--qemu', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    root = (args.root or source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos').resolve()
    runtime = source / 'target/linux/mediatek/image/nradio-nbcpe-688/official-assets/qa-build/qemu/runtime'
    qemu = (args.qemu or runtime / 'usr/bin/qemu-aarch64').resolve()
    busybox = shutil.which('busybox')
    if not qemu.is_file() or not busybox:
        raise SystemExit('Existing build-only QEMU and host BusyBox are required')
    environment = dict(os.environ, LD_LIBRARY_PATH=str(runtime / 'usr/lib/x86_64-linux-gnu'))
    print(json.dumps(checks(root, qemu, environment, busybox), ensure_ascii=False))


if __name__ == '__main__':
    main()
