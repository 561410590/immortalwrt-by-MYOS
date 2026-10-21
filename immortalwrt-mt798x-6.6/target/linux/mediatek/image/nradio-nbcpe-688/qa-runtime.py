#!/usr/bin/env python3
"""Read-only ELF dependency, symbol and firewall migration audit."""

import argparse
import functools
import json
import os
import posixpath
import re
import shlex
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--compact", action="store_true")
    arguments = parser.parse_args()
    source = arguments.source
    root = source / "build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos"
    assets = source / "target/linux/mediatek/image/nradio-nbcpe-688"

    def virtual(path):
        path = "/" + str(path).lstrip("/")
        for _ in range(40):
            parts = Path(path).parts[1:]
            current = "/"
            changed = False
            for index, part in enumerate(parts):
                current = posixpath.join(current, part)
                candidate = root / current.lstrip("/")
                if candidate.is_symlink():
                    link = os.readlink(candidate)
                    target = link if link.startswith("/") else posixpath.join(posixpath.dirname(current), link)
                    path = posixpath.normpath(posixpath.join(target, *parts[index + 1:]))
                    changed = True
                    break
            if not changed:
                return root / path.lstrip("/")
        raise RuntimeError("Symlink loop: " + path)

    def output(*command):
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode:
            raise RuntimeError(" ".join(map(str, command)) + ": " + result.stderr.strip())
        return result.stdout

    @functools.lru_cache(maxsize=None)
    def elf(path):
        file = virtual(path)
        if not file.is_file():
            return None
        with file.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                return None
        dynamic = output("readelf", "-dW", str(file))
        # Stock shared libraries are sstrip'ed and have no section headers.
        # Read symbols through PT_DYNAMIC, not the absent .dynsym section.
        symbols = output("readelf", "-DsW", str(file))
        header = output("readelf", "-hW", str(file))
        needed = re.findall(r"\(NEEDED\).*?\[([^]]+)\]", dynamic)
        runpaths = re.findall(r"\((?:RPATH|RUNPATH)\).*?\[([^]]+)\]", dynamic)
        defined, undefined = set(), set()
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields) < 8 or not fields[0].endswith(":"):
                continue
            bind, visibility, section, name = fields[4:8]
            name = name.split("@", 1)[0]
            if section == "UND" and bind == "GLOBAL":
                undefined.add(name)
            elif section != "UND" and bind in ("GLOBAL", "WEAK") and visibility in ("DEFAULT", "PROTECTED"):
                defined.add(name)
        return {"needed": needed, "runpaths": runpaths,
                "defined": defined, "undefined": undefined,
                "machine": next((line.strip() for line in header.splitlines() if "Machine:" in line), "")}

    default_paths = ["/lib", "/usr/local/lib", "/usr/lib"]
    musl_path = root / "etc/ld-musl-aarch64.path"
    if musl_path.is_file():
        default_paths = re.split(r"[:\n]", musl_path.read_text().strip())

    def locate(name, owner):
        if "/" in name:
            return name if virtual(name).is_file() else None
        origin = posixpath.dirname(owner)
        search = []
        for value in elf(owner)["runpaths"]:
            search.extend(p.replace("${ORIGIN}", origin).replace("$ORIGIN", origin) for p in value.split(":"))
        for directory in search + default_paths:
            path = posixpath.join(directory, name)
            if virtual(path).is_file():
                return path
        return None

    def closure(paths):
        found, missing = set(), []
        pending = list(paths)
        while pending:
            path = pending.pop()
            if path in found:
                continue
            data = elf(path)
            if not data:
                continue
            found.add(path)
            for name in data["needed"]:
                dependency = locate(name, path)
                if dependency:
                    pending.append(dependency)
                else:
                    missing.append({"file": path, "needed": name})
        return found, missing

    cores = ["/sbin/rpcd", "/usr/sbin/uhttpd", "/usr/bin/ucode", "/sbin/netifd",
             "/sbin/uci", "/bin/ubus", "/usr/bin/python3.9", "/usr/bin/atsd_tools",
             "/usr/bin/atsd_tools_cli", "/usr/bin/lua5.1", "/lib/libc.so",
             "/usr/sbin/dnsmasq", "/usr/sbin/umdns", "/sbin/ujail", "/sbin/procd", "/sbin/ubusd", "/sbin/logd",
             "/usr/bin/jshn", "/sbin/mount_root", "/usr/bin/curl"]
    cores += ["/usr/sbin/mosquitto", "/usr/bin/pdu_decoder-AK68",
              "/usr/sbin/iptables", "/usr/sbin/ip6tables", "/usr/sbin/iptables-nft",
              "/usr/sbin/ip6tables-nft", "/usr/sbin/nft", "/bin/opkg",
              "/usr/sbin/mkfs.f2fs", "/usr/sbin/fsck.f2fs"]
    cores += ["/bin/bash", "/usr/sbin/dropbear", "/usr/lib/sftp-server",
              "/usr/libexec/myos-ubenv", "/usr/sbin/blkid"]
    cores += ['/usr/sbin/' + family + suffix
              for family in ('iptables', 'ip6tables') for suffix in ('-save', '-restore')]
    targets = set(cores)
    for relative in ("usr/lib/rpcd", "usr/lib/ucode", "usr/lib/lua", "usr/lib/python3.9"):
        targets.update("/" + p.relative_to(root).as_posix()
                       for p in (root / relative).rglob("*.so") if p.is_file())
    targets.update("/" + p.relative_to(root).as_posix()
                   for p in (root / "usr/lib").glob("uhttpd*.so"))
    results = []
    missing_files = []
    script_entry_points = []
    for path in sorted(targets):
        if not elf(path):
            if path in cores:
                file = virtual(path)
                if file.is_file() and file.read_bytes().startswith(b'#!') and file.stat().st_mode & 0o111:
                    script_entry_points.append(path)
                else:
                    missing_files.append(path)
            continue
        hosts = []
        if path.startswith("/usr/lib/rpcd/"):
            hosts = ["/sbin/rpcd"]
        elif path.startswith("/usr/lib/ucode/"):
            hosts = ["/usr/bin/ucode"]
        elif path.startswith("/usr/lib/lua/"):
            hosts = ["/usr/bin/lua5.1"]
        elif path.startswith("/usr/lib/python3.9/"):
            hosts = ["/usr/bin/python3.9"]
        elif path.startswith("/usr/lib/uhttpd"):
            hosts = ["/usr/sbin/uhttpd"]
        scope, missing = closure([path] + hosts)
        exports = set().union(*(elf(p)["defined"] for p in scope))
        # Check imports of every dependent ELF, not only the entry executable.
        # curl can resolve its own symbols while libcurl cannot resolve nghttp2;
        # xtables can likewise import an incompatible libiptext/libxtables pair.
        unresolved_by_file = {dependency: sorted(elf(dependency)["undefined"] - exports)
                              for dependency in scope
                              if elf(dependency)["undefined"] - exports}
        unresolved = sorted(set().union(*(set(symbols) for symbols in unresolved_by_file.values())))
        results.append({"path": path, "needed": elf(path)["needed"],
                        "machine": elf(path)["machine"], "missing_libraries": missing,
                        "unresolved_symbols": unresolved,
                        "unresolved_by_file": unresolved_by_file})

    runtime = assets / 'official-assets/qa-build/qemu/runtime'
    environment = dict(os.environ)
    environment.pop('LD_PRELOAD', None)
    environment['LD_LIBRARY_PATH'] = str(runtime / 'usr/lib/x86_64-linux-gnu')
    qemu = [str(runtime / 'usr/bin/qemu-aarch64'), '-U', 'LD_LIBRARY_PATH',
            '-U', 'LD_PRELOAD', '-L', str(root)]
    executions = []
    for path, option in [('/usr/sbin/iptables', '-V'), ('/usr/sbin/ip6tables', '-V'),
                         ('/usr/sbin/iptables-nft', '-V'), ('/usr/sbin/ip6tables-nft', '-V'),
                         ('/usr/sbin/iptables-save', '-V'), ('/usr/sbin/iptables-restore', '-V'),
                         ('/usr/sbin/ip6tables-save', '-V'), ('/usr/sbin/ip6tables-restore', '-V'),
                         ('/bin/opkg', '--version'),
                         ('/usr/bin/curl', '--version'), ('/usr/sbin/mkfs.f2fs', '-V'),
                         ('/usr/sbin/fsck.f2fs', '-V'), ('/bin/bash', '--version'),
                         ('/usr/sbin/dropbear', '-V'), ('/usr/sbin/blkid', '-V')]:
        result = subprocess.run(qemu + ['-0', Path(path).name, str(virtual(path)), option], env=environment,
                                capture_output=True, text=True, timeout=20)
        expected_output = 'nf_tables' if Path(path).name.startswith(('iptables', 'ip6tables')) else ''
        qemu_netlink_limit = (result.returncode == 1 and not result.stdout
                              and re.fullmatch(r'(?:ip6tables|iptables): Failed to initialize nft: Protocol not supported\n?',
                                               result.stderr) is not None)
        nft_backend = (not expected_output or virtual(path).name == 'xtables-nft-multi')
        executions.append({'path': path, 'return': result.returncode,
                           'stdout': result.stdout.strip(), 'stderr': result.stderr.strip(),
                           'expected_output_present': expected_output in result.stdout,
                           'qemu_netlink_limit': qemu_netlink_limit, 'nft_backend': nft_backend,
                           'image_libraries_only': True})

    # Exercise the complete installed shell API and firewall service_triggers,
    # capturing its ubus JSON in an isolated host fixture instead of sending it.
    with tempfile.TemporaryDirectory(prefix='nbcpe-procd-qa-') as temporary:
        fixture = Path(temporary)
        (fixture / 'usr/share/libubox').mkdir(parents=True)
        (fixture / 'var/lock').mkdir(parents=True)
        (fixture / 'usr/share/libubox/jshn.sh').symlink_to(root / 'usr/share/libubox/jshn.sh')
        api_environment = dict(os.environ, IPKG_INSTROOT=str(fixture),
                               PATH=str(source / 'staging_dir/host/bin') + ':' + os.environ['PATH'])
        api_script = ('initscript=/etc/init.d/firewall\n'
                      '. ' + shlex.quote(str(root / 'lib/functions/procd.sh')) + '\n'
                      '. ' + shlex.quote(str(root / 'etc/init.d/firewall')) + '\n'
                      '_procd_ubus_call() { json_dump; json_cleanup; }\n'
                      'service_data() { :; }\n'
                      'procd_open_service firewall\nprocd_close_service\n')
        api = subprocess.run(['busybox', 'ash', '-c', api_script], env=api_environment,
                             capture_output=True, text=True, timeout=20)
        try:
            api_json = json.loads(api.stdout)
        except ValueError:
            api_json = {}
        api_triggers = api_json.get('triggers', [])
        procd_check = {'passed': api.returncode == 0 and not api.stderr.strip()
                      and any(item[0] == 'config.change' for item in api_triggers)
                      and any(item[0] == 'service.data.update' for item in api_triggers)
                      and b'service.data.update' in virtual('/sbin/procd').read_bytes(),
                      'return': api.returncode, 'stderr': api.stderr.strip(),
                      'triggers': api_triggers, 'ubus_requests': 0,
                      'pid1_supports_data_update': b'service.data.update' in virtual('/sbin/procd').read_bytes()}
    package_files = ['/usr/libexec/package-manager-call',
                     '/usr/share/luci/menu.d/luci-app-package-manager.json',
                     '/usr/share/rpcd/acl.d/luci-app-package-manager.json',
                     '/www/luci-static/resources/view/package-manager.js']
    package_management = {'files': {path: virtual(path).is_file() for path in package_files}}
    feeds = (root / 'etc/opkg/distfeeds.conf').read_text()
    repository = re.search(r'^CONFIG_VERSION_REPO="([^"]+)"$', (source / '.config').read_text(), re.M)[1].rstrip('/')
    active_feeds = [line.split()[2] for line in feeds.splitlines() if line.startswith('src/gz ')]
    package_management['feeds'] = active_feeds
    trusted_keys = ['3fd9011f340b50dd', 'd310c6f2833e97f7']
    source_keys = source / 'staging_dir/target-aarch64_cortex-a53_musl/root-mediatek/etc/opkg/keys'
    package_management['trusted_source_keys'] = {
        name: (root / 'etc/opkg/keys' / name).is_file()
        and (root / 'etc/opkg/keys' / name).read_bytes() == (source_keys / name).read_bytes()
        for name in trusted_keys}
    package_management['signature_check_enabled'] = bool(re.search(
        r'^option\s+check_signature\s*$', (root / 'etc/opkg.conf').read_text(), re.M))
    package_management['passed'] = (all(package_management['files'].values()) and bool(active_feeds)
                                    and all(url.startswith(repository + '/')
                                            and '/targets/' not in url and '/kmods/' not in url
                                            and url.rstrip('/').rsplit('/', 1)[1] in {'base', 'luci', 'packages', 'routing', 'telephony'}
                                            for url in active_feeds))
    package_management['passed'] &= (all(package_management['trusted_source_keys'].values())
                                     and package_management['signature_check_enabled'])
    build_manifest = json.loads((assets / 'build-manifest.json').read_text())
    modern_names = set(build_manifest['required_packages'])
    modern_owned, native_ownership_collisions = {}, {}
    info = root / 'usr/lib/opkg/info'
    for name in sorted(modern_names):
        manifest = info / (name + '.list')
        for path in manifest.read_text().splitlines() if manifest.is_file() else []:
            relative = path.lstrip('/').rstrip('/')
            if (root / relative).is_file() or (root / relative).is_symlink():
                modern_owned[relative] = name
        control = info / (name + '.control')
        match = re.search(r'^Alternatives: (.+)$', control.read_text(), re.M) if control.is_file() else None
        for alternative in match[1].split(',') if match else []:
            _, link, _ = alternative.strip().split(':', 2)
            modern_owned[link.lstrip('/')] = name
    for manifest in info.glob('*.list'):
        if manifest.stem in modern_names:
            continue
        stale = {path: modern_owned[path.lstrip('/').rstrip('/')]
                 for path in manifest.read_text().splitlines()
                 if path.lstrip('/').rstrip('/') in modern_owned}
        if stale:
            native_ownership_collisions[manifest.stem] = stale
    native = ["usr/bin/python3.9", "usr/lib/libpython3.9.so.1.0", "usr/bin/atsd_tools",
              "usr/bin/atsd_tools_cli", "etc/init.d/atsd_tools", "etc/init.d/modeminit-AK68",
              "usr/bin/httpapi-AK68.py", "usr/bin/websocket_server-AK68.py",
              "usr/share/modem-AK68/MT5700-AK68.sh", "usr/share/modem-AK68/devck-AK68.sh",
              "usr/share/modem-AK68/zinfo5700-AK68.sh"]
    native_results = []
    unsquashfs = source / "staging_dir/host/bin/unsquashfs4"
    for relative in native:
        result = subprocess.run([str(unsquashfs), "-cat", str(assets / "root.squashfs"), relative],
                                capture_output=True, check=False)
        assembled = virtual(relative)
        native_results.append({"path": "/" + relative, "exists": assembled.is_file(),
                               "stock_readable": result.returncode == 0,
                               "identical_to_stock": result.returncode == 0 and assembled.is_file()
                               and result.stdout == assembled.read_bytes()})
    firewall = root / "etc/config/firewall"
    sections = []
    for number, line in enumerate(firewall.read_text().splitlines(), 1):
        tokens = shlex.split(line, comments=True)
        if not tokens:
            continue
        if tokens[0] == "config":
            sections.append({"type": tokens[1], "line": number, "options": {}})
        elif tokens[0] in ("option", "list") and len(tokens) >= 3 and sections:
            sections[-1]["options"].setdefault(tokens[1], []).append(" ".join(tokens[2:]))
    fw4 = root / "usr/share/ucode/fw4.uc"
    fw4_lines = fw4.read_text().splitlines() if fw4.is_file() else []
    fw4_evidence = [{"line": n, "text": line.strip()} for n, line in enumerate(fw4_lines, 1)
                    if re.search(r"fw4_compatible|Ignoring.*include|not.*compatible|syn_flood|fullcone", line)]
    firewall_user = root / "etc/firewall.user"
    user_rules = [{"line": n, "text": line} for n, line in enumerate(
        firewall_user.read_text().splitlines() if firewall_user.is_file() else [], 1)
                  if line.strip() and not line.lstrip().startswith("#")]
    legacy_hooks = []
    for relative in ("etc/init.d", "etc/hotplug.d", "etc/firewall.d", "usr/share/firewall4"):
        directory = root / relative
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if not path.is_file() or path.is_symlink() or path.stat().st_size > 262144:
                continue
            raw = path.read_bytes()
            if b"\0" in raw[:4096]:
                continue
            matches = [{"line": n, "text": line.strip()} for n, line in enumerate(
                raw.decode(errors="replace").splitlines(), 1)
                       if re.search(r"\b(?:iptables|ip6tables|fw3)\b", line)
                       and not line.lstrip().startswith("#")]
            if matches:
                legacy_hooks.append({"path": "/" + path.relative_to(root).as_posix(), "matches": matches[:16]})
    report = {
        "root": str(root), "elf_files_checked": len(results), "missing_core_files": missing_files,
        "script_entry_points": script_entry_points,
        "actual_aarch64_executions": executions, "procd_firewall_trigger_api": procd_check,
        "package_management": package_management,
        "native_filelist_ownership_collisions": native_ownership_collisions,
        "elf_failures": [r for r in results if r["missing_libraries"] or r["unresolved_symbols"]],
        "core_dependencies": [r for r in results if r["path"] in cores],
        "native_files": native_results,
        "firewall": {"sections": sections, "fw4_evidence": fw4_evidence,
                     "firewall_user_active_lines": user_rules, "legacy_hooks": legacy_hooks},
        "limits": ["Firmware executions use only read-only version flags; no daemon startup or formatting",
                   "nft and iptables-nft ELF closure is checked; their netlink startup cannot run in QEMU user mode",
                   "Symbol versions and semantic ABI compatibility are not inferred from symbol presence"],
    }
    if arguments.compact:
        report.pop("core_dependencies")
        report["firewall"]["sections"] = [s for s in sections if s["type"] in ("include", "defaults")]
        report["firewall"]["legacy_hooks"] = [h for h in legacy_hooks
                                                  if any("fw3" in m["text"] for m in h["matches"])]
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(1 if missing_files or report['elf_failures'] or not procd_check['passed']
                     or not package_management['passed']
                     or native_ownership_collisions
                     or any((item['return'] and not item['qemu_netlink_limit'])
                            or (not item['expected_output_present'] and not item['qemu_netlink_limit'])
                            or not item['nft_backend']
                            for item in executions) else 0)


if __name__ == "__main__":
    main()
