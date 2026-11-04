#!/usr/bin/env python3
"""Run real NBCPE board/network generation against native host UCI and jshn."""

import argparse
import fnmatch
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tarfile
import tempfile


# Actual overlay is loaded at runtime; all execution stays in disposable fixtures.
MIGRATION_SCRIPT = None
BOARD_NETWORK_SCRIPT = None
IDENTITY_SCRIPT = None


def execute(command, env=None):
    result = subprocess.run([str(arg) for arg in command], env=env,
                            text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(' '.join(map(str, command)) + '\n' + result.stderr)
    return result.stdout


def shell_function(text, name):
    match = re.search(r'^' + re.escape(name) + r'\s*\(\)\s*\{.*?^\}', text, re.M | re.S)
    if not match:
        raise RuntimeError('Actual shell function missing: ' + name)
    return match.group(0)


def native_busybox_awk(source, assets):
    tool = assets / 'busybox-host'
    build = tool / 'build'
    binary = build / 'busybox'
    if binary.is_file():
        return binary
    archives = sorted((source / 'dl').glob('busybox-*.tar.bz2'))
    if not archives:
        raise RuntimeError('Existing BusyBox source archive unavailable')
    extracted = tool / 'source'
    extracted.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archives[-1]) as archive:
        for member in archive.getmembers():
            path = (extracted / member.name).resolve()
            if extracted.resolve() not in path.parents:
                raise RuntimeError('Unsafe BusyBox source archive path')
        archive.extractall(extracted)
    busybox_source = next(extracted.glob('busybox-*'))
    build.mkdir(parents=True, exist_ok=True)
    command = ['make', '-C', busybox_source, 'O=' + str(build)]
    execute([*command, 'allnoconfig'])
    config = build / '.config'
    text = config.read_text()
    for name in ('CONFIG_AWK', 'CONFIG_FEATURE_AWK_LIBM', 'CONFIG_FEATURE_AWK_GNU_EXTENSIONS'):
        pattern = r'^(?:# ' + name + r' is not set|' + name + r'=.*)$'
        text, count = re.subn(pattern, name + '=y', text, flags=re.M)
        if count != 1:
            raise RuntimeError('BusyBox option unavailable: ' + name)
    config.write_text(text)
    execute([*command, 'silentoldconfig'])
    execute([*command, '-j2'])
    return binary


def host_tools(source, assets):
    busybox = shutil.which('busybox')
    jshn = source / 'staging_dir/host/bin/jshn'
    if not busybox or not jshn.is_file():
        raise RuntimeError('Native host busybox or jshn unavailable')
    native_uci = assets / 'uci-host/uci'
    if not native_uci.is_file():
        candidates = sorted((source / 'build_dir/target-aarch64_cortex-a53_musl').glob('uci-*'))
        if not candidates or not shutil.which('gcc'):
            raise RuntimeError('Native host UCI missing, and existing source/gcc unavailable')
        uci_source = candidates[-1]
        host = source / 'staging_dir/host'
        library = host / 'lib/libubox.a'
        if not library.is_file() or not (uci_source / 'uci_config.h').is_file():
            raise RuntimeError('Existing UCI headers or native libubox.a unavailable')
        native_uci.parent.mkdir(parents=True, exist_ok=True)
        sources = ('cli.c', 'libuci.c', 'file.c', 'util.c', 'delta.c', 'parse.c', 'blob.c')
        execute(['gcc', '-O2', '-std=gnu99', '-I', uci_source, '-I', host / 'include',
                 *[uci_source / name for name in sources], library, '-o', native_uci])
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = str(source / 'staging_dir/host/lib') + ':' + env.get('LD_LIBRARY_PATH', '')
    env['NBCPE_TEST_AWK'] = str(native_busybox_awk(source, assets))
    execute([jshn, '-r', '{}'], env)
    execute([native_uci, '-h'], env)
    return busybox, native_uci, jshn, env


class Fixture:
    def __init__(self, directory, root, busybox, uci, jshn, env, mode=0o755,
                 board='HCMT7981-emmc-NB68'):
        self.directory = directory
        self.root = root
        self.busybox = busybox
        self.uci_binary = uci
        self.env = env.copy()
        self.env['PATH'] = str(directory / 'bin') + ':' + str(jshn.parent) + ':' + self.env.get('PATH', '')
        for name in ('bin', 'lib/functions', 'etc/board.d', 'etc/config', 'tmp/.uci',
                     'tmp/sysinfo', 'sys/class/net/eth0', 'sys/class/net/eth1',
                     'usr/share/libubox', 'proc/sys/net', 'dev'):
            (directory / name).mkdir(parents=True, exist_ok=True)
        self.write('tmp/sysinfo/board_name', board + '\n')
        self.write('tmp/sysinfo/model', 'NBCPE network fixture\n')
        self.write('sys/class/net/eth0/address', '02:11:22:33:44:50\n')
        scripts = ('bin/board_detect', 'bin/config_generate', 'etc/board.d/01_leds',
                   'etc/board.d/02_network', 'etc/board.d/99-default_network',
                   'lib/functions/uci-defaults.sh', 'usr/share/libubox/jshn.sh')
        for relative in scripts:
            path = BOARD_NETWORK_SCRIPT if relative == 'etc/board.d/02_network' and BOARD_NETWORK_SCRIPT else root / relative
            text = self.relocate(path.read_text())
            if text.startswith('#!/bin/sh'):
                text = text.replace('#!/bin/sh', '#!' + busybox + ' ash', 1)
            self.write(relative, text, path.stat().st_mode & 0o777)
        (directory / 'etc/board.d/02_network').chmod(mode)
        # Board identity and calibration inputs are fixtures; JSON and UCI remain real.
        self.write('lib/functions.sh',
                   'board_name() { cat "' + str(directory / 'tmp/sysinfo/board_name') + '"; }\n'
                   'l1dat() { printf "%s\\n" "' + str(directory / 'missing-calibration-file') + '"; }\n')
        for relative in ('lib/functions/system.sh', 'lib/functions/leds.sh'):
            self.write(relative, '. "' + str(directory / 'lib/functions.sh') + '"\n')
        with (directory / 'lib/functions/system.sh').open('a') as helper:
            helper.write(shell_function((root / 'lib/functions/system.sh').read_text(), 'macaddr_add') + '\n')
        self.write('lib/functions/nbcpe.sh', IDENTITY_SCRIPT)
        self.write('bin/bdinfo', '#!' + busybox + ' ash\n'
                   'printf "%s\\n" "02:11:22:33:44:50"\n', 0o755)
        self.write('bin/uci', '#!' + busybox + ' ash\nexec "' + str(uci) + '" -c "'
                   + str(directory / 'etc/config') + '" -P "' + str(directory / 'tmp/.uci') + '" "$@"\n', 0o755)
        dhcp = root / 'etc/config/dhcp'
        if not dhcp.is_file():
            raise RuntimeError('Assembled DHCP configuration missing')
        self.write('etc/config/dhcp', dhcp.read_text())
        self.write('etc/config/firewall', (root / 'etc/config/firewall').read_text())
        self.write('migration.sh', self.relocate(MIGRATION_SCRIPT), 0o755)

    def relocate(self, text):
        text = re.sub(r'/(?:usr/share|etc|lib|tmp|sys|proc|var|sbin)(?=/)',
                      lambda match: str(self.directory) + match.group(0), text)
        return text.replace('/dev/kmsg', str(self.directory / 'dev/kmsg'))

    def write(self, relative, content, mode=0o644):
        path = self.directory / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        path.chmod(mode)

    def uci(self, *arguments, missing_ok=False):
        command = [self.uci_binary, '-c', self.directory / 'etc/config',
                   '-P', self.directory / 'tmp/.uci', '-q', *arguments]
        result = subprocess.run([str(arg) for arg in command], env=self.env,
                                text=True, capture_output=True, check=False)
        if result.returncode and not missing_ok:
            raise RuntimeError(' '.join(map(str, command)) + '\n' + result.stderr)
        return result.stdout.strip()

    def generate(self):
        execute([self.busybox, 'ash', self.directory / 'bin/board_detect'], self.env)
        execute([self.busybox, 'ash', self.directory / 'bin/config_generate'], self.env)

    def migrate(self):
        execute([self.busybox, 'ash', self.directory / 'migration.sh'], self.env)

    def bridge_ports(self):
        index = 0
        while self.uci('get', 'network.@device[' + str(index) + ']', missing_ok=True) == 'device':
            section = 'network.@device[' + str(index) + ']'
            if self.uci('get', section + '.name', missing_ok=True) == 'br-lan':
                return self.uci('get', section + '.ports', missing_ok=True).split()
            index += 1
        return []

    def assert_network(self, ip='192.168.66.1', mask='255.255.255.0'):
        assert self.bridge_ports() == ['port1', 'port2', 'port3'], self.bridge_ports()
        for option, expected in (('device', 'br-lan'), ('proto', 'static'),
                                 ('ipaddr', ip), ('netmask', mask)):
            actual = self.uci('get', 'network.lan.' + option)
            assert actual == expected, (option, actual, expected)
        assert self.uci('get', 'network.wan.device') == 'port5'
        for option, expected in {'proto': 'wwan', 'device': 'port5', 'phyifname': 'port5', 'ifname': 'port5',
                                 'phycap': '2500', 'odu_interface': 'nbdevice', 'mode': 'odu',
                                 'forcephy': '1', 'defaultroute': '1', 'auto_sniffer': '192.168.8.1',
                                 'odu_model': 'MT5700M-CN', 'ippass': '1', 'sms': '1'}.items():
            assert self.uci('get', 'network.cpe.' + option) == expected, option
        for interface in ('wan', 'wan6', 'nbdevice'):
            assert self.uci('get', 'network.' + interface + '.device') == 'port5'
            assert self.uci('get', 'network.' + interface + '.disabled') == '1'
        wan_zone = next(section for section in self.uci('show', 'firewall').splitlines()
                        if section.endswith(".name='wan'"))
        section = wan_zone.split('.name=')[0]
        assert 'cpe' in self.uci('get', section + '.network').split()
        assert self.uci('get', section + '.masq') == '1'
        assert self.uci('get', 'cpesel.sim.max') == '2'
        assert self.uci('get', 'cpesel.sim.gval') == '1-1,1-2'
        assert self.uci('get', 'cpesel.sim.stype') == '0,4'
        assert self.uci('get', 'network.nrswitch.dsa') == '1'
        assert self.uci('get', 'network.nrswitch.nvlan') == 'NLLLNWC'
        assert self.uci('get', 'network.nrswitch.ovlan') == 'NmLLNMC'
        assert self.uci('get', 'network.lan.macaddr', missing_ok=True) in ('', '02:11:22:33:44:50')
        for name, expected in (('br-lan', '02:11:22:33:44:50'),
                               ('port1', '02:11:22:33:44:50'), ('port2', '02:11:22:33:44:50'),
                               ('port3', '02:11:22:33:44:50'), ('port5', '02:11:22:33:44:51')):
            devices = self.uci('show', 'network').splitlines()
            section = next(line.split('.name=')[0] for line in devices if line.endswith(".name='" + name + "'"))
            assert self.uci('get', section + '.macaddr') == expected, name
        for slot in (1, 2):
            assert self.uci('get', 'cpecfg.cpesim' + str(slot) + '.ippass') == '1'

    def assert_dhcp(self, migrated=False):
        assert self.uci('get', 'dhcp.lan.interface') == 'lan'
        assert self.uci('get', 'dhcp.lan.ignore', missing_ok=True) != '1'
        assert self.uci('get', 'dhcp.lan.dynamicdhcp', missing_ok=True) != '0'
        assert int(self.uci('get', 'dhcp.lan.start')) > 0
        assert int(self.uci('get', 'dhcp.lan.limit')) > 0
        if migrated:
            assert self.uci('get', 'dhcp.lan.dhcpv4') == 'server'

    def assert_ndp(self):
        for option, expected in {'interface': 'cpe', 'ignore': '1',
                                 'ndp': 'relay', 'master': '1'}.items():
            assert self.uci('get', 'dhcp.cpe.' + option) == expected, option
        assert self.uci('get', 'dhcp.lan.ndp') == 'hybrid'
        assert self.uci('get', 'dhcp.lan.ra_default') == '1'

    def config_bytes(self):
        return tuple((self.directory / 'etc/config' / name).read_bytes()
                     for name in ('network', 'dhcp', 'firewall', 'cpesel', 'cpecfg')
                     if (self.directory / 'etc/config' / name).is_file())


    def assert_ipv4_chain(self):
        # Run the real shell/UCI loader, not a Python replacement for UCI parsing.
        for relative in ('lib/functions.sh', 'lib/config/uci.sh', 'lib/wifi/mtwifi.sh',
                         'bin/ipcalc.sh'):
            self.write(relative, self.relocate((self.root / relative).read_text()), 0o755)
        self.write('sbin/uci', (self.directory / 'bin/uci').read_text(), 0o755)
        (self.directory / 'var/state').mkdir(parents=True, exist_ok=True)
        native_awk = self.env['NBCPE_TEST_AWK']
        self.write('bin/awk', '#!' + self.busybox + ' ash\nexec "' + native_awk + '" awk "$@"\n', 0o755)

        # The installed netifd DHCP handler calls the lib/functions.sh ipcalc
        # function, rather than executing a standalone ipcalc binary.
        dhcp_setup = shell_function((self.root / 'lib/netifd/dhcp.script').read_text(), 'setup_interface')
        dhcp_prelude = '. ' + shlex.quote(str(self.directory / 'lib/functions.sh')) + '\n'
        for helper in ('proto_init_update', 'proto_add_data', 'proto_close_data',
                       'proto_send_update', 'proto_add_dns_server', 'proto_add_dns_search'):
            dhcp_prelude += helper + '() { :; }\n'
        dhcp_prelude += 'proto_add_ipv4_address() { printf "address:%s|%s\\n" "$1" "$2"; }\n'
        dhcp_prelude += 'proto_add_ipv4_route() { printf "route:%s|%s|%s|%s\\n" "$1" "$2" "$3" "$4"; }\n'
        dhcp_prelude += dhcp_setup + '\nIFACE6RD=0\nip=10.131.137.239\nmask=8\nsubnet=255.0.0.0\n'
        dhcp_gateway_checks = []
        for gateway, same_subnet in (('10.0.0.1', True), ('192.168.8.1', False)):
            command = dhcp_prelude + 'router=' + gateway + '\nsetup_interface\nprintf "done\\n"\n'
            result = subprocess.run([self.busybox, 'ash', '-c', command], env=self.env,
                                    text=True, capture_output=True, check=False)
            assert result.returncode == 0 and not result.stderr, result.stderr
            routes = result.stdout.splitlines()
            assert 'address:10.131.137.239|255.0.0.0' in routes, routes
            assert 'route:0.0.0.0|0|' + gateway + '|10.131.137.239' in routes, routes
            host_route = 'route:' + gateway + '|32||10.131.137.239'
            assert (host_route in routes) == (not same_subnet), routes
            dhcp_gateway_checks.append({'gateway': gateway, 'same_subnet': same_subnet,
                                        'host_route': not same_subnet})

        profile = dict(line.split('=', 1) for line in
                       (self.root / 'etc/wireless/l1profile.dat').read_text().splitlines()
                       if '=' in line)
        main_names = profile['INDEX0_main_ifname'].split(';')
        prefixes = profile['INDEX0_ext_ifname'].split(';')
        assert main_names == ['ra0', 'rax0'], main_names
        assert prefixes == ['ra', 'rax'], prefixes
        devices = [profile['INDEX0'] + '_1_' + str(index) for index in (1, 2)]
        # l1util represents two physical radios from the actual NBCPE l1profile.
        body = '#!' + self.busybox + ' ash\ncase "$1" in\nlist) printf "%s\\n" '
        body += shlex.quote(' '.join(devices)) + ' ;;\nget) case "$2:$3" in\n'
        for index, (device, ifname) in enumerate(zip(devices, main_names), 1):
            body += shlex.quote(device + ':main_ifname') + ') printf "%s\\n" ' + shlex.quote(ifname) + ' ;;\n'
            body += shlex.quote(device + ':subidx') + ') printf "%s\\n" ' + str(index) + ' ;;\n'
        body += '*) exit 1 ;;\nesac ;;\n*) exit 1 ;;\nesac\n'
        self.write('bin/l1util', body, 0o755)
        (self.directory / 'sys/module/mt_wifi').mkdir(parents=True, exist_ok=True)
        self.write('etc/config/wireless', '')
        detect = '. ' + shlex.quote(str(self.directory / 'lib/functions.sh')) + '\n'
        detect += 'config_load wireless\n. ' + shlex.quote(str(self.directory / 'lib/wifi/mtwifi.sh'))
        detect += '\ndetect_mtwifi\n'
        execute([self.busybox, 'ash', '-c', detect], self.env)

        driver = (self.root / 'lib/netifd/wireless/mtwifi.sh').read_text()
        naming = shell_function(driver, 'mtwifi_vif_ap_set_data')
        associations = []
        for device, ifname, prefix, band in zip(devices, main_names, prefixes, ('2g', '5g')):
            section = 'wireless.default_' + device
            assert self.uci('get', 'wireless.' + device + '.phy') == ifname
            assert self.uci('get', 'wireless.' + device + '.band') == band
            assert self.uci('get', section + '.mode') == 'ap'
            assert self.uci('get', section + '.network') == 'lan'
            assert self.uci('get', section + '.disabled', missing_ok=True) != '1'
            command = '. ' + shlex.quote(str(self.directory / 'usr/share/libubox/jshn.sh')) + '\n'
            command += naming + '\njson_init\nMTWIFI_MAX_AP_IDX=15\nAP_IDX=0\n'
            command += 'MTWIFI_CFG_IFNAME_KEY=mtwifi_ifname\nMTWIFI_AP_IF_PREFIX=' + prefix
            command += '\nmtwifi_vif_ap_set_data\njson_dump\n'
            actual = json.loads(execute([self.busybox, 'ash', '-c', command], self.env))
            assert actual['mtwifi_ifname'] == ifname, actual
            associations.append({'band': band, 'ap': ifname, 'network': 'lan',
                                 'bridge': self.uci('get', 'network.lan.device')})
        assert {item['bridge'] for item in associations} == {'br-lan'}

        binary = (self.root / 'usr/sbin/dnsmasq').read_bytes()
        features = next((item.decode() for item in re.findall(rb'[\x20-\x7e]{12,}', binary)
                         if b'GNU-getopt' in item and 'DHCP' in item.decode().split()), None)
        assert features, 'Assembled dnsmasq binary lacks DHCP compile feature'
        assert (self.root / 'etc/rc.d/S19dnsmasq').is_symlink(), 'dnsmasq startup disabled'
        assert self.uci('get', 'dhcp.@dnsmasq[0].disabled', missing_ok=True) not in ('1', 'true')
        dnsmasq = (self.root / 'etc/init.d/dnsmasq').read_text()
        helpers = ('dnsmasq_ignore_opt', 'xappend', 'dhcp_calc', 'append_bool',
                   'append_interface', 'append_notinterface', 'append_listenaddress',
                   'dhcp_option_append', 'dhcp_option_add', 'dhcp_add', 'add_interface_trigger')
        script = '. ' + shlex.quote(str(self.directory / 'lib/functions.sh')) + '\nconfig_load dhcp\n'
        script += '\n'.join(shell_function(dnsmasq, name) for name in helpers) + '\n'
        ip = self.uci('get', 'network.lan.ipaddr')
        mask = self.uci('get', 'network.lan.netmask')
        subnet = str(ipaddress.IPv4Interface(ip + '/' + mask))
        config = self.directory / 'dnsmasq-fixture.conf'
        trigger = self.directory / 'dnsmasq-trigger.txt'
        script += 'CONFIGFILE_TMP=' + shlex.quote(str(config)) + '\n: > "$CONFIGFILE_TMP"\n'
        script += 'dnsmasq_features=' + shlex.quote(features + ' ') + '\ndnsmasq_has_dhcp=1\n'
        script += 'DNSMASQ_DHCP_VER=4\ncachelocal=1\nADD_LOCAL_FQDN=""\n'
        # These three functions model netifd status from the generated fixture UCI.
        script += 'network_get_device() { local v; v="$(uci -q get "network.$2.device")"; '
        script += '[ -n "$v" ] || return 1; export "$1=$v"; }\n'
        script += 'network_get_protocol() { export "$1=$(uci -q get "network.$2.proto")"; }\n'
        script += 'network_get_subnet() { export "$1=' + subnet + '"; }\n'
        script += 'dhcp_check() { return 0; }\ndhcp_this_host_add() { :; }\n'
        script += 'procd_add_interface_trigger() { printf "%s\\n" "$*" >> '
        script += shlex.quote(str(trigger)) + '; }\n'
        script += 'dhcp_add lan\nadd_interface_trigger lan\n'
        script += 'fixture_instance() { config_list_foreach "$1" interface append_interface; '
        script += 'config_list_foreach "$1" notinterface append_notinterface; '
        script += 'config_list_foreach "$1" listen_address append_listenaddress; }\n'
        script += 'config_foreach fixture_instance dnsmasq\n'
        generated = subprocess.run([self.busybox, 'ash', '-c', script], env=self.env,
                                   text=True, capture_output=True, check=False)
        if generated.returncode or generated.stderr:
            raise RuntimeError('dnsmasq fixture generation: ' + generated.stderr.strip())
        options = config.read_text().splitlines()
        ranges = [item for item in options if item.startswith('dhcp-range=')]
        assert len(ranges) == 1 and ranges[0].startswith('dhcp-range=set:lan,'), options
        assert mask in ranges[0] and ',static,' not in ranges[0], ranges
        assert 'no-dhcp-interface=br-lan' not in options, options
        for item in options:
            if item.startswith('except-interface='):
                assert not fnmatch.fnmatch('br-lan', item.split('=', 1)[1]), options
        whitelist = [item.split('=', 1)[1] for item in options if item.startswith('interface=')]
        assert not whitelist or any(fnmatch.fnmatch('br-lan', value) for value in whitelist), options
        listeners = [item.split('=', 1)[1] for item in options if item.startswith('listen-address=')]
        assert not listeners or ip in listeners or '0.0.0.0' in listeners, options
        assert 'interface.* lan ' in trigger.read_text(), trigger.read_text()

        self.write('etc/config/firewall', (self.root / 'etc/config/firewall').read_text())
        index = 0
        lan_accepts = False
        while self.uci('get', 'firewall.@zone[' + str(index) + ']', missing_ok=True) == 'zone':
            section = 'firewall.@zone[' + str(index) + ']'
            if 'lan' in self.uci('get', section + '.network', missing_ok=True).split():
                assert self.uci('get', section + '.input') == 'ACCEPT'
                lan_accepts = True
            index += 1
        assert lan_accepts, 'LAN not assigned to an accepting firewall input zone'
        index = 0
        while self.uci('get', 'firewall.@rule[' + str(index) + ']', missing_ok=True) == 'rule':
            section = 'firewall.@rule[' + str(index) + ']'
            if self.uci('get', section + '.src', missing_ok=True) == 'lan':
                assert self.uci('get', section + '.target', missing_ok=True) not in ('DROP', 'REJECT')
            index += 1
        return {'wireless': associations, 'dhcp_range': ranges[0],
                'dnsmasq_startup': True, 'lan_trigger_reload': True, 'lan_input': 'ACCEPT',
                'dhcp_gateway_checks': dhcp_gateway_checks}


def checks(root, busybox, uci, jshn, env):
    passed = []
    failures = []
    ipv4_chains = []
    with tempfile.TemporaryDirectory(prefix='nbcpe-network-test-') as temporary:
        parent = Path(temporary)

        def case(name, action, mode=0o755, board='HCMT7981-emmc-NB68'):
            try:
                fixture = Fixture(parent / name, root, busybox, uci, jshn, env, mode, board)
                action(fixture)
                passed.append(name)
            except Exception as error:
                failures.append({'case': name, 'error': str(error)})

        def clean(fixture):
            fixture.generate()
            board = json.loads((fixture.directory / 'etc/board.json').read_text())
            assert board['network']['lan']['ports'] == ['port1', 'port2', 'port3']
            assert board['network']['wan']['device'] == 'port5'
            fixture.migrate()
            fixture.assert_network()
            fixture.assert_dhcp()
            fixture.assert_ndp()

        def regression(fixture):
            fixture.generate()
            assert fixture.bridge_ports() == ['eth0'], fixture.bridge_ports()
            assert fixture.uci('get', 'network.wan.device') == 'eth1'
            fixture.assert_dhcp()

        def migrate_old(fixture):
            regression(fixture)
            fixture.uci('set', 'network.lan.ipaddr=192.168.88.1')
            fixture.uci('set', 'network.lan.netmask=255.255.254.0')
            fixture.uci('set', 'dhcp.lan.ignore=1')
            fixture.uci('set', 'dhcp.lan.dynamicdhcp=0')
            fixture.uci('set', 'network.BLUE4WAN=interface')
            fixture.uci('set', 'network.BLUE4WAN.proto=dhcp')
            fixture.uci('set', 'network.BLUE4WAN.ifname=BLUE4')
            fixture.uci('commit')
            fixture.migrate()
            fixture.assert_network('192.168.88.1', '255.255.254.0')
            fixture.assert_dhcp(migrated=True)
            fixture.assert_ndp()
            assert fixture.uci('get', 'network.BLUE4WAN.disabled') == '1'
            before = fixture.config_bytes()
            fixture.migrate()
            assert fixture.config_bytes() == before, 'Migration is not idempotent'

        def preserve_correct(fixture):
            clean(fixture)
            fixture.uci('set', 'network.lan.ipaddr=10.28.0.1')
            fixture.uci('set', 'network.lan.netmask=255.255.254.0')
            fixture.uci('commit', 'network')
            before = fixture.config_bytes()
            fixture.migrate()
            assert fixture.config_bytes() == before, 'Correct user configuration was rewritten'
            fixture.assert_network('10.28.0.1', '255.255.254.0')
            fixture.assert_dhcp()

        def missing_lan(fixture):
            fixture.generate()
            fixture.uci('delete', 'network.lan')
            fixture.uci('delete', 'dhcp.lan')
            fixture.uci('commit')
            fixture.migrate()
            fixture.assert_network()
            fixture.assert_dhcp(migrated=True)

        def missing_cellular_packages(fixture):
            fixture.generate()
            for name in ('cpesel', 'cpecfg'):
                assert not (fixture.directory / 'etc/config' / name).exists(), name
                assert fixture.uci('show', name, missing_ok=True) == '', name
            fixture.migrate()
            fixture.assert_network()
            fixture.assert_dhcp()
            for name in ('cpesel', 'cpecfg'):
                assert (fixture.directory / 'etc/config' / name).is_file(), name
                assert fixture.uci('show', name), name

        def preserve_user_mac(fixture):
            clean(fixture)
            fixture.uci('set', 'network.lan.macaddr=02:ab:cd:ef:12:34')
            fixture.uci('set', 'network.@device[0].macaddr=02:ab:cd:ef:12:34')
            fixture.uci('commit', 'network')
            before = fixture.config_bytes()
            fixture.migrate()
            assert fixture.config_bytes() == before, 'Valid manual LAN MAC was overwritten'

        def invalid_identity(fixture):
            fixture.write('bin/bdinfo', '#!' + fixture.busybox + ' ash\necho "01:11:22:33:44:50"\n', 0o755)
            fixture.generate()
            fixture.migrate()
            assert fixture.uci('get', 'network.lan.macaddr', missing_ok=True) == ''
            assert fixture.uci('get', 'network.@device[0].macaddr', missing_ok=True) == ''
            assert 'no valid bdinfo MAC' in (fixture.directory / 'dev/kmsg').read_text()

        def old_dsa_names(fixture, ports=('lan1', 'lan2', 'lan3')):
            clean(fixture)
            fixture.uci('set', 'network.lan.ipaddr=192.168.77.1')
            fixture.uci('set', 'network.lan.netmask=255.255.254.0')
            fixture.uci('delete', 'network.@device[0].ports')
            for port in ports:
                fixture.uci('add_list', 'network.@device[0].ports=' + port)
            fixture.uci('set', 'cpesel.sim.cur=2')
            fixture.uci('commit')
            fixture.migrate()
            fixture.assert_network('192.168.77.1', '255.255.254.0')
            fixture.assert_dhcp(migrated=True)
            assert fixture.uci('get', 'cpesel.sim.cur') == '2'

        def empty_board_dt_fallback(fixture):
            fixture.generate()
            fixture.uci('set', 'network.lan.ipaddr=192.168.68.1')
            fixture.uci('delete', 'network.@device[0].ports')
            for port in ('lan1', 'lan2', 'lan3', 'lan4'):
                fixture.uci('add_list', 'network.@device[0].ports=' + port)
            fixture.uci('commit')
            fixture.write('tmp/sysinfo/board_name', '\n')
            fixture.write('proc/device-tree/compatible',
                          'HCMT7981-emmc-NB68\x00mediatek,mt7981-emmc-rfb\x00')
            fixture.migrate()
            fixture.assert_network('192.168.68.1')
            fixture.assert_dhcp(migrated=True)
            before = fixture.config_bytes()
            fixture.migrate()
            assert fixture.config_bytes() == before

        def empty_other_board(fixture):
            fixture.generate()
            fixture.write('tmp/sysinfo/board_name', '\n')
            fixture.write('proc/device-tree/compatible', 'other,board\x00mediatek,mt7981\x00')
            before = fixture.config_bytes()
            fixture.migrate()
            assert fixture.config_bytes() == before, 'Unmatched DT configuration was rewritten'

        def wrong_board(fixture):
            regression(fixture)
            before = fixture.config_bytes()
            fixture.migrate()
            assert fixture.config_bytes() == before, 'Other board configuration was rewritten'

        def clean_ipv4(fixture):
            clean(fixture)
            ipv4_chains.append(fixture.assert_ipv4_chain())

        def migrated_ipv4(fixture):
            migrate_old(fixture)
            ipv4_chains.append(fixture.assert_ipv4_chain())

        case('clean_0755', clean)
        case('regression_0644', regression, 0o644)
        case('migrate_old_eth0_and_repeat', migrate_old, 0o644)
        case('preserve_correct_user_ip', preserve_correct)
        case('missing_lan_defaults', missing_lan)
        case('create_missing_cpesel_cpecfg', missing_cellular_packages)
        case('preserve_valid_manual_mac', preserve_user_mac)
        case('invalid_board_identity_no_mac_invention', invalid_identity)
        case('migrate_old_lan123_and_keep_slot', old_dsa_names)
        case('migrate_real_lan1234_and_keep_slot',
             lambda fixture: old_dsa_names(fixture, ('lan1', 'lan2', 'lan3', 'lan4')))
        case('empty_board_dt_fallback_preserves_68_1', empty_board_dt_fallback)
        case('empty_board_other_dt_untouched', empty_other_board)
        case('other_board_untouched', wrong_board, 0o644, 'other,board')
        case('clean_dual_band_dnsmasq_ipv4', clean_ipv4)
        case('migrated_dual_band_dnsmasq_ipv4', migrated_ipv4, 0o644)
    return {'passed': len(passed), 'total': len(passed) + len(failures),
            'cases_passed': passed, 'failures': failures, 'fixtures_cleaned': True,
            'ipv4_chains': ipv4_chains}


def main():
    global MIGRATION_SCRIPT, BOARD_NETWORK_SCRIPT, IDENTITY_SCRIPT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--migration-script', type=Path)
    parser.add_argument('--board-network', type=Path)
    arguments = parser.parse_args()
    source = arguments.source.resolve()
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    root = arguments.root or source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    migration = arguments.migration_script or assets / 'overlay/etc/uci-defaults/98-nbcpe-network'
    MIGRATION_SCRIPT = migration.read_text()
    identity = (root / 'lib/functions/nbcpe.sh') if arguments.root else assets / 'overlay/lib/functions/nbcpe.sh'
    IDENTITY_SCRIPT = identity.read_text()
    BOARD_NETWORK_SCRIPT = arguments.board_network
    busybox, uci, jshn, env = host_tools(source, assets)
    result = checks(root, busybox, uci, jshn, env)
    result['tools'] = {'busybox': str(busybox), 'uci': str(uci), 'jshn': str(jshn),
                       'awk_busybox': env['NBCPE_TEST_AWK']}
    result['root_02_network_mode'] = oct((root / 'etc/board.d/02_network').stat().st_mode & 0o777)
    result['migration_sha256'] = hashlib.sha256(MIGRATION_SCRIPT.encode()).hexdigest()
    result['board_network_input'] = str(BOARD_NETWORK_SCRIPT or root / 'etc/board.d/02_network')
    print(json.dumps(result, ensure_ascii=False))
    return 1 if result['failures'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
