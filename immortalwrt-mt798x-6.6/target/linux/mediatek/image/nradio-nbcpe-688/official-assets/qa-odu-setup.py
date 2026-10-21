#!/usr/bin/env python3
"""Execute the original ODU setup under a host ash+jshn fixture, without networking."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    args = parser.parse_args()
    assets = args.source / 'target/linux/mediatek/image/nradio-nbcpe-688/official-assets'
    (assets / 'qa/bin/cpetools.sh').chmod(0o755)
    subprocess.run(['/usr/bin/busybox', 'ash', str(assets / 'qa-odu-setup.sh'), str(args.source)], check=True)
    failures = []
    cases = []
    for trace in sorted((assets / 'qa-build/odu').glob('case-*.trace')):
        rows = [line.split('\t') for line in trace.read_text().splitlines()]
        _, pdp, bridge = rows[0]
        effective = pdp if pdp in ['ip', 'ipv6', 'ipv4v6'] else 'ipv4v6'
        children = [json.loads(row[1]) for row in rows if row[0] == 'dynamic']
        updates = [json.loads(row[1]) for row in rows if row[0] == 'notify']
        commands = [row[1] for row in rows if row[0] == 'cpetools']
        expected_names = (['cpe_4'] if effective in ['ip', 'ipv4v6'] else [])
        expected_names += (['cpe_6'] if effective in ['ipv6', 'ipv4v6'] else [])
        checks = [sorted(item['name'] for item in children) == expected_names,
                  any(item.get('link-up') is True and item.get('ifname') == 'port5' for item in updates),
                  any('-c pdp' in command and '3 ' + effective in command and 'fixture.apn' in command for command in commands)]
        for child in children:
            checks += [child['device'] == '@cpe', child['zone'] == 'wan', child['metric'] == 7,
                       child['defaultroute'] is True, child['peerdns'] is True,
                       child['proto'] == ('none' if bridge == '1' else 'dhcp' if child['name'] == 'cpe_4' else 'dhcpv6')]
            if child['name'] == 'cpe_6':
                checks.append(child['extendprefix'] == '1')
        if not all(checks):
            failures.append({'case': trace.name, 'checks': checks, 'rows': rows})
        cases.append({'pdptype': pdp, 'bridge': bridge, 'children': children, 'profile': 3,
                      'setup_return': next(row[1] for row in rows if row[0] == 'setup_return')})
    print(json.dumps({'executed': len(cases), 'passed': len(cases)-len(failures), 'failures': failures,
                      'cases': cases, 'real_AT': 0, 'network_requests': 0}))
    return bool(failures or len(cases) != 8)


if __name__ == '__main__':
    raise SystemExit(main())
