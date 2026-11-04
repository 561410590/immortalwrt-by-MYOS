#!/usr/bin/env python3
"""Check supplied Vantage IPKs and the NBCPE read-only cache adapter."""
import argparse
import io
import json
import subprocess
import tarfile
import tempfile
from pathlib import Path


def run(*command):
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        raise AssertionError(result.stderr or result.stdout)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--ucode', type=Path, required=True)
    args = parser.parse_args()
    expected = {'luci-app-vantage': '1.0.1-r4', 'luci-theme-vantage': '1.0.1-r3',
                'luci-i18n-vantage-zh-cn': '1.0.1-r4'}
    packages = {}
    with tempfile.TemporaryDirectory(prefix='nbcpe-vantage-qa-') as directory:
        temporary = Path(directory)
        for path in sorted((args.assets / 'packages').glob('*.ipk')):
            with tarfile.open(path, 'r:*') as archive:
                control = next(m for m in archive.getmembers() if Path(m.name).name.startswith('control.tar'))
                with tarfile.open(fileobj=io.BytesIO(archive.extractfile(control).read()), mode='r:*') as inner:
                    member = next(m for m in inner.getmembers() if Path(m.name).name == 'control')
                    record = dict(line.split(': ', 1) for line in inner.extractfile(member).read().decode().splitlines()
                                  if ': ' in line and not line.startswith(' '))
                packages[record['Package']] = record['Version']
                assert record['Architecture'] == 'all', record
                data = next(m for m in archive.getmembers() if Path(m.name).name.startswith('data.tar'))
                with tarfile.open(fileobj=io.BytesIO(archive.extractfile(data).read()), mode='r:*') as payload:
                    for member in payload.getmembers():
                        if member.isfile() and member.name.endswith('.ut'):
                            template = temporary / Path(member.name).name
                            template.write_bytes(payload.extractfile(member).read())
                            run(str(args.ucode), '-T', '-c', '-o', '/dev/null', str(template))
        assert packages == expected, packages
        registration = args.assets / 'root/etc/uci-defaults/30_luci-theme-vantage'
        script = registration.read_text()
        assert 'set luci.themes.Vantage=/luci-static/vantage' in script
        assert 'set luci.main.mediaurlbase' not in script
        run('sh', '-n', str(registration))

        plugin = args.assets / 'root/usr/share/rpcd/ucode/luci.vantage'
        run(str(args.ucode), '-c,dynlink=fs,dynlink=uci,dynlink=ubus', '-o', '/dev/null', str(plugin))
        text = plugin.read_text()
        text = text.replace("import { cursor } from 'uci';", 'const cursor = () => null;')
        text = text.replace("import { connect } from 'ubus';", 'const connect = () => null;')
        text = text.replace("import { readfile, stat, lsdir, popen } from 'fs';", '''
let fixture = {}, calls = 0;
const readfile = () => null, stat = () => null, lsdir = () => [];
function popen(command, mode) {
    if (command != '/usr/libexec/myos-nbcpe-status status' || mode != 'r')
        die('Unexpected process or modem command');
    calls++;
    return { read: () => sprintf('%J', fixture), close: () => true };
}
''')
        assert "return { 'luci.vantage': methods };" in text
        text = text.replace("return { 'luci.vantage': methods };", r'''
function check(value, label) { if (!value) die(label); }
function reset(value) { fixture = value; calls = 0; nbcpe_snapshot = null; nbcpe_sampled = 0; }
const fresh = {
    cache_state: 'complete', cache: { age: 5, stale: false }, radio_age: 6, registration: { registered: true },
    summary: { model: 'MT5700M-CN', manufacturer: 'TD Tech Ltd.', revision: 'V200R001C20B024', operator: '中国移动' },
    connect_status: 'Yes', serving_cell: { mode: 'NR-5G', mcc: '460', mnc: '00', cellid: '0000000138DF9003',
        pci: '125', tac: '64451C', arfcn: '504990', band: 'n41', rsrp: -117, rsrq: -12, sinr: 2 },
    sources: [{ entries: [{ key: 'temperature', value: 22 }, { key: 'IMSI', value: 'private' }] }]
};
reset(fresh);
let cell = methods.cellular.call(), sensors = methods.sensors.call();
check(calls == 1, 'Cellular/sensors must share one read-only snapshot');
check(cell.present === true && cell.connected === true && cell.sim_ready === true, 'Fresh connected state');
check(cell.module == 'MT5700M-CN' && cell.operator == '中国移动', 'Model/operator mapping');
check(cell.rsrp == -117 && cell.sinr == 2 && cell.band == 'n41', 'Serving cell mapping');
check(sensors.modem_temp_mc == 22000 && sensors.cpu_temp_mc === null, 'Temperature units/source');
check(!('IMSI' in cell) && !('IMEI' in cell) && !('ICCID' in cell), 'Subscriber identifiers must remain private');
check(cell.updated <= time() && cell.updated >= time() - 6, 'Cache timestamp');

fresh.cache = { age: 121, stale: true }; reset(fresh);
cell = methods.cellular.call(); sensors = methods.sensors.call();
check(cell.connected === null && cell.rsrp === null && cell.operator === null && cell.updated === null, 'Stale cache must be unknown');
check(sensors.modem_temp_mc === null, 'Stale modem temperature must be unknown');

fresh.cache = { age: 5, stale: false }; fresh.radio_age = 121; reset(fresh);
cell = methods.cellular.call();
check(cell.connected === null && cell.rsrp == -117, 'Stale dial evidence must be unknown');

fresh.radio_age = 5; fresh.connect_status = 'No'; reset(fresh);
check(methods.cellular.call().connected === false, 'Known disconnected state');
fresh.registration.registered = false; reset(fresh);
check(methods.cellular.call().sim_ready === null, 'Complete collection does not prove SIM ready');

reset({cache_state:'no_sim',cache:{age:1,stale:false},radio_age:null,connect_status:'No'});
cell = methods.cellular.call();
check(cell.present === true && cell.connected === false && cell.sim_ready === false, 'Fresh no-SIM state');

reset({}); cell = methods.cellular.call(); sensors = methods.sensors.call();
check(cell.present === false && cell.connected === null && cell.rsrp === null && cell.sim_ready === null, 'Missing cache state');
check(sensors.modem_temp_mc === null, 'Missing modem temperature');

fresh.cache_state = 'partial'; reset(fresh); cell = methods.cellular.call();
check(cell.present === false && cell.connected === null && cell.rsrp === null && cell.sim_ready === null, 'Partial cache state');
check(methods.sensors.call().modem_temp_mc === null, 'Partial cache temperature');
fresh.cache_state = 'complete';

fresh.sources[0].entries[0].value = 6553.5; reset(fresh);
check(methods.sensors.call().modem_temp_mc === null, 'Invalid modem temperature');
print('Vantage NBCPE cache fixtures: passed\n');
''')
        fixture = temporary / 'adapter-test.uc'
        fixture.write_text(text)
        output = run(str(args.ucode), str(fixture))
        assert 'Vantage NBCPE cache fixtures: passed' in output, output
        print(output.strip())
    print('Vantage 3 IPKs, templates, RPC syntax and default-theme registration: passed')


if __name__ == '__main__':
    main()
