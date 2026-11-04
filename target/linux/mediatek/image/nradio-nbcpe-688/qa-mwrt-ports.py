#!/usr/bin/env python3
"""Exercise the actual MWRT physical-port section against isolated sysfs data."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--assets', type=Path, required=True)
    arguments = parser.parse_args()
    product = arguments.assets / 'themes/luci-theme-mwrt/root/www/cgi-bin/router3d-data'
    text = product.read_text()
    section = text.split('WANDEV=$(uci -q get network.wan.device)', 1)[1]
    section = 'WANDEV=$(uci -q get network.wan.device)' + section.split('# --- wan traffic', 1)[0]
    with tempfile.TemporaryDirectory(prefix='nbcpe-mwrt-ports-') as directory:
        network = Path(directory) / 'net'
        for name, carrier, speed in [('port1', 0, -1), ('port2', 0, -1),
                                      ('port3', 1, 1000), ('port5', 1, 2500),
                                      ('lan1', 0, -1), ('lan2', 0, -1), ('lan3', 1, 1000)]:
            device = network / name
            device.mkdir(parents=True)
            for key, value in [('carrier', carrier), ('speed', speed),
                               ('operstate', 'up' if carrier else 'down')]:
                (device / key).write_text(str(value) + '\n')
        section = section.replace('/sys/class/net', str(network))
        helpers = '''rd() { cat "$1" 2>/dev/null; }
is_num() { case "$1" in ''|*[!0-9]*) return 1;; *) return 0;; esac; }
uci() { printf 'port5\\n'; }
ip() { printf '198.51.100.1 dev port5 src 192.0.2.10\\n'; }
'''
        for model in ('NBCPE-688', 'C5800-688'):
            result = subprocess.run(['busybox', 'ash', '-c',
                helpers + '\nMODEL=' + model + '\n' + section + '\nprintf "[%s]" "$port_json"'],
                capture_output=True, text=True, check=True)
            ports = json.loads(result.stdout)
            prefix = 'port' if model == 'NBCPE-688' else 'lan'
            assert {port['id'] for port in ports} == {prefix + '1', prefix + '2', prefix + '3', 'port5'}, ports
            by_name = {port['id']: port for port in ports}
            assert by_name['port5']['carrier'] == 1 and by_name['port5']['speed'] == 2500
            assert by_name['port5']['role'] == 'WAN'
            assert by_name[prefix + '3']['carrier'] == 1 and by_name[prefix + '3']['speed'] == 1000
            assert by_name[prefix + '1']['carrier'] == 0 and by_name[prefix + '1']['speed'] == -1
    print(json.dumps({'passed': 2, 'physical_ports': 4, 'wan_speed': 2500}))


if __name__ == '__main__':
    main()
