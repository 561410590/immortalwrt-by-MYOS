"""Install universal sysupgrade support into the assembled MYOS 6.6 root."""
from pathlib import Path
import shutil
import sys


def install(source, root):
    upgrade = root / 'lib/upgrade'
    helper = upgrade / 'myos-688.sh'
    shutil.copyfile(source / 'scripts/myos-688-upgrade.sh', helper)
    helper.chmod(0o644)

    fwtool = upgrade / 'fwtool.sh'
    original = fwtool.read_text()
    fwtool.write_text(original.replace(
        'if [ "$dev" = "$device" ]; then',
        'if [ "$dev" = "$device" ] || {\n'
        '\t\t\t[ "$dev" = "myos,nradio-688-universal" ] &&\n'
        '\t\t\tmember="$(myos_688_kernel_member)" &&\n'
        '\t\t\ttar tf "$1" "$member" >/dev/null 2>&1 &&\n'
        '\t\t\ttar tf "$1" myos-688/root >/dev/null 2>&1\n'
        '\t\t}; then',
    ).replace('json_get_keys dev_keys\n', 'json_get_keys dev_keys\n\tlocal member\n', 1))

    emmc = upgrade / 'emmc.sh'
    original = emmc.read_text().replace('${board_dir}/kernel', '"$kernel_member"').replace(
        '${board_dir}/root', '"$root_member"',
    )
    emmc.write_text(original.replace(
        'local board_dir=$(tar tf "$tar_file" | grep -m 1 \'^sysupgrade-.*/$\')\n'
        '\tboard_dir=${board_dir%/}',
        'local board_dir kernel_member root_member\n'
        '\tboard_dir=$(tar tf "$tar_file" | grep -m 1 -E \'^(sysupgrade-.*/|myos-688/)$\')\n'
        '\tboard_dir=${board_dir%/}\n'
        '\tcase "$board_dir" in\n'
        '\t\tmyos-688)\n'
        '\t\t\tkernel_member="$(myos_688_kernel_member)" || return 1\n'
        '\t\t\troot_member=myos-688/root\n'
        '\t\t\t;;\n'
        '\t\t*)\n'
        '\t\t\tkernel_member="$board_dir/kernel"\n'
        '\t\t\troot_member="$board_dir/root"\n'
        '\t\t\t;;\n'
        '\tesac',
    ))


if __name__ == '__main__':
    install(*(Path(argument) for argument in sys.argv[1:]))

