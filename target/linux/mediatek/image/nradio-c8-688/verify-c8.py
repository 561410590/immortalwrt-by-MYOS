#!/usr/bin/env python3
"""Verify a packed C8-688 MYOS image without contacting a device."""

from pathlib import Path
import argparse
import ast
import gzip
import hashlib
import json
import lzma
import re
import shlex
import struct
import subprocess
import tarfile
import tempfile
import zlib


BOARD = "HCMT7981-688-emmc"
MODEL = "NRadio-C8-New688"
VERSION = "3.1.35-202610161200"
KERNEL = "6.6.110"
KERNEL_ABI = "7b6e534b726d3806dc7bdd177cff3504"
LOAD_ADDRESS = 0x48000000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def run_bytes(*args):
    result = subprocess.run([str(arg) for arg in args], check=False,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError("Command failed (" + str(result.returncode) + "): "
                           + shlex.join(str(arg) for arg in args) + "\n"
                           + result.stderr.decode("utf-8", errors="replace"))
    return result.stdout


def fdt_properties(data):
    require(len(data) >= 40, "Truncated FDT header")
    header = struct.unpack_from(">10I", data)
    magic, size, structure, strings = header[:4]
    require(magic == 0xD00DFEED, "Invalid FDT magic")
    require(40 <= size <= len(data), "Invalid FDT total size")
    strings_end = strings + header[8]
    structure_end = structure + header[9]
    require(40 <= structure < structure_end <= size, "Invalid FDT structure bounds")
    require(40 <= strings <= strings_end <= size, "Invalid FDT strings bounds")
    pos, stack, values = structure, [], {}
    ended = False
    while pos + 4 <= structure_end:
        token = struct.unpack_from(">I", data, pos)[0]
        pos += 4
        if token == 1:
            end = data.index(b"\0", pos, structure_end)
            stack.append(data[pos:end].decode("utf-8"))
            pos = (end + 4) & ~3
        elif token == 2:
            require(bool(stack), "Unbalanced FDT end node")
            stack.pop()
        elif token == 3:
            require(stack and pos + 8 <= structure_end, "Invalid FDT property")
            length, offset = struct.unpack_from(">II", data, pos)
            pos += 8
            require(pos + length <= structure_end, "FDT property exceeds structure")
            require(strings + offset < strings_end, "Invalid FDT property name offset")
            end = data.index(b"\0", strings + offset, strings_end)
            name = data[strings + offset:end].decode("utf-8")
            node = "/" + "/".join(part for part in stack if part)
            require((node, name) not in values, "Duplicate FDT property: " + node + "/" + name)
            values[node, name] = data[pos:pos + length]
            pos = (pos + length + 3) & ~3
        elif token == 4:
            continue
        elif token == 9:
            require(not stack, "Unbalanced FDT tree")
            ended = True
            break
        else:
            raise ValueError("Invalid FDT token: " + str(token))
    require(ended, "FDT has no end token")
    return values


def string(value):
    require(value.endswith(b"\0"), "FDT string lacks terminator")
    return value.rstrip(b"\0").decode("utf-8")


def cells(value):
    require(len(value) % 4 == 0, "Invalid FDT cell length")
    return struct.unpack(">" + "I" * (len(value) // 4), value)


def check_hashes(values, node, data):
    algorithms = set()
    for (path, name), raw in values.items():
        if name != "algo" or not path.startswith(node + "/hash"):
            continue
        require("/" not in path[len(node) + 1:], "Unexpected nested FIT hash")
        algorithm = string(raw)
        expected = values.get((path, "value"))
        if algorithm == "crc32":
            actual = struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)
        elif algorithm in ("sha1", "sha256"):
            actual = hashlib.new(algorithm, data).digest()
        else:
            raise ValueError("Unsupported FIT hash algorithm: " + algorithm)
        require(expected == actual, "FIT hash mismatch: " + path)
        algorithms.add(algorithm)
    require({"crc32", "sha1"} <= algorithms, "Missing FIT hashes: " + node)


def check_device_tree(data):
    values = fdt_properties(data)
    compatibles = values["/", "compatible"].rstrip(b"\0").decode("utf-8").split("\0")
    require(compatibles == [BOARD, "mediatek,mt7981"], "C8 compatible mismatch")
    require(string(values["/", "model"]) == MODEL, "C8 model mismatch")
    bootargs = string(values["/chosen", "bootargs"]).split()
    require("root=PARTLABEL=rootfs_2nd" in bootargs, "C8 must boot rootfs_2nd")
    require("rootwait" in bootargs, "Missing rootwait")
    require("rootfstype=squashfs,f2fs" in bootargs, "Unexpected C8 rootfs types")
    require(values["/memory", "reg"] == struct.pack(">4I", 0, 0x40000000, 0, 0x40000000),
            "C8 memory declaration differs from reference")

    mmc = "/soc/mmc@11230000"
    require(string(values[mmc, "status"]) == "okay", "C8 eMMC is disabled")
    require(cells(values[mmc, "bus-width"]) == (8,), "C8 eMMC bus width mismatch")
    require(cells(values[mmc, "max-frequency"]) == (52000000,), "C8 eMMC frequency mismatch")
    require((mmc, "non-removable") in values, "C8 eMMC must be non-removable")

    switch = "/soc/ethernet@15100000/mdio-bus/switch@0"
    require(string(values[switch, "compatible"]) == "mediatek,mt7531", "C8 switch mismatch")
    require(cells(values[switch, "reset-gpios"])[1:] == (39, 0), "C8 switch reset GPIO mismatch")
    ports = {number: switch + "/ports/port@" + format(number, "x")
             for number in (1, 2, 3, 5, 6)}
    for number, label in ((1, "port1"), (2, "port2"), (3, "port3"), (5, "port5")):
        require(string(values[ports[number], "label"]) == label, "C8 port label mismatch: " + label)
        require(cells(values[ports[number], "reg"]) == (number,), "C8 switch port number mismatch")
    require(string(values[ports[5], "phy-mode"]) == "2500base-x", "C8 port5 PHY mode mismatch")
    require(string(values[ports[6], "phy-mode"]) == "2500base-x", "C8 CPU port mode mismatch")
    require(cells(values[ports[6] + "/fixed-link", "speed"]) == (2500,), "C8 CPU link speed mismatch")
    phy5 = switch.rsplit("/", 1)[0] + "/phy@5"
    phy21 = switch.rsplit("/", 1)[0] + "/phy@15"
    require(values[ports[5], "phy-handle"] == values[phy5, "phandle"], "C8 port5 does not use PHY5")
    require(cells(values[phy5, "reg"]) == (5,), "C8 PHY5 address mismatch")
    require(cells(values[phy5, "reset-gpios"])[1:] == (7, 1), "C8 PHY5 reset GPIO mismatch")
    require(cells(values[phy21, "reg"]) == (21,), "C8 PHY21 address mismatch")
    require(cells(values[phy21, "reset-gpios"])[1:] == (8, 1), "C8 PHY21 reset GPIO mismatch")
    for number in (0, 1):
        mac = "/soc/ethernet@15100000/mac@" + str(number)
        require(string(values[mac, "phy-mode"]) == "2500base-x", "C8 GMAC mode mismatch")
        if number == 0:
            require((mac, "phy-handle") not in values, "C8 switch GMAC must retain its fixed link")
            require(cells(values[mac + "/fixed-link", "speed"]) == (2500,), "C8 switch GMAC speed mismatch")
        else:
            require(values.get((mac, "phy-handle")) == values[phy21, "phandle"],
                    "C8 cellular GMAC must manage PHY21")
            require(not any(node == mac + "/fixed-link" for node, key in values),
                    "C8 cellular GMAC must use PHY negotiation")

    gpio_report = {}
    for name, gpio in (("status", 10), ("cmode5", 11), ("cmode4", 12), ("wifi", 34)):
        node = "/gpio-leds/" + name
        require(string(values[node, "label"]) == "hc:blue:" + name, "C8 LED label mismatch")
        require(cells(values[node, "gpios"])[1:] == (gpio, 1), "C8 LED GPIO mismatch: " + name)
        gpio_report[name] = gpio
    for name, gpio in (("reset", 1), ("wps", 9)):
        require(cells(values["/gpio-keys/" + name, "gpios"])[1:] == (gpio, 1),
                "C8 button GPIO mismatch: " + name)
    exports = {string(value): node for (node, key), value in values.items()
               if key == "gpio-export,name"}
    for name, gpio, active_low, output in (("cpe-pwr", 31, 1, 0), ("fan-hw", 27, 0, 1),
                                           ("fan-fg", 28, 0, 1)):
        node = exports[name]
        require(cells(values[node, "gpios"])[1:] == (gpio, active_low), "C8 export GPIO mismatch: " + name)
        require(cells(values[node, "gpio-export,output"]) == (output,), "C8 GPIO initial state mismatch: " + name)
        gpio_report[name] = gpio
    require(not {"cpe-sel0", "cpe-sel1"} & set(exports), "DTB must not initialize SIM selector GPIOs")
    for (node, key), value in values.items():
        if key == "gpios" and node.startswith("/gpio-export/"):
            require(cells(value)[1] not in (29, 30), "DTB exports a SIM selector under another name")
    require(cells(values["/pwm-fan", "pwms"])[1:] == (0, 40000, 0), "C8 PWM fan timing mismatch")
    return {"model": MODEL, "compatible": compatibles, "ports": ["port1", "port2", "port3", "port5", "eth1"],
            "gpio": gpio_report, "manual_sim_slots": 4, "root_partition": "rootfs_2nd",
            "cellular_phy": {"device": "eth1", "mdio_address": 21, "fixed_link": False}}


def elf_sections(data):
    require(data[:6] == b"\x7fELF\x02\x01", "Expected little-endian ELF64 kernel module")
    require(struct.unpack_from("<H", data, 18)[0] == 183, "Kernel module is not AArch64")
    offset = struct.unpack_from("<Q", data, 40)[0]
    size, count, names_index = struct.unpack_from("<HHH", data, 58)
    require(size >= 64 and count and names_index < count, "Invalid module section table")
    require(offset + size * count <= len(data), "Module section table is truncated")
    headers = [struct.unpack_from("<IIQQQQIIQQ", data, offset + index * size)
               for index in range(count)]
    names_header = headers[names_index]
    names = data[names_header[4]:names_header[4] + names_header[5]]
    result = {}
    for header in headers:
        end = names.index(b"\0", header[0])
        name = names[header[0]:end].decode("ascii")
        if header[1] != 8:
            require(header[4] + header[5] <= len(data), "Module section is truncated: " + name)
            result[name] = data[header[4]:header[4] + header[5]]
    return result


def package_records(text):
    records = []
    for paragraph in text.strip().split("\n\n"):
        fields = {}
        for line in paragraph.splitlines():
            if line[:1].isspace():
                continue
            key, separator, value = line.partition(":")
            if separator:
                fields[key] = value.strip()
        if fields:
            records.append(fields)
    return records


def check_modules(root, source):
    linux = source / "build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic" / ("linux-" + KERNEL)
    abi = (linux / ".vermagic").read_text(encoding="utf-8").strip()
    require(abi == KERNEL_ABI, "Source kernel package ABI differs from the C8 build baseline")
    module_root = root / "lib/modules"
    directories = sorted(path.name for path in module_root.iterdir() if path.is_dir())
    require(directories == [KERNEL], "Packed module kernel directories mismatch: " + repr(directories))
    records = package_records((root / "usr/lib/opkg/status").read_text(encoding="utf-8"))
    kernel = [record for record in records if record.get("Package") == "kernel"]
    require(len(kernel) == 1, "Packed package database needs exactly one kernel")
    kernel_version = kernel[0].get("Version", "")
    require(kernel_version.startswith(KERNEL + "~") and abi in kernel_version,
            "Packed kernel package ABI differs from source: " + kernel_version)
    kmods = [record for record in records if record.get("Package", "").startswith("kmod-")]
    require(bool(kmods), "No kernel module packages in packed package database")
    module_packages, builtin_packages = 0, 0
    for record in kmods:
        listing = root / "usr/lib/opkg/info" / (record["Package"] + ".list")
        require(listing.is_file(), "Missing kmod package file list: " + record["Package"])
        contains_module = any(re.search(r"\.ko(?:\.xz|\.gz)?$", relative)
                              and (root / relative.lstrip("/")).is_file()
                              for relative in listing.read_text(encoding="utf-8").splitlines())
        dependencies = [value.strip() for value in re.findall(
            r"(?:^|,\s*)kernel\s*\(=\s*([^\)]+)\)", record.get("Depends", ""))]
        if contains_module:
            require(dependencies == [kernel_version], "Kernel package ABI dependency mismatch: " + record["Package"])
            module_packages += 1
        else:
            require(not dependencies or dependencies == [kernel_version],
                    "Built-in kernel package ABI dependency mismatch: " + record["Package"])
            builtin_packages += 1

    config = (linux / ".config").read_text(encoding="utf-8")
    modversions = bool(re.search(r"^CONFIG_MODVERSIONS=y$", config, re.M))
    symvers = {}
    if modversions:
        for line in (linux / "Module.symvers").read_text(encoding="utf-8").splitlines():
            parts = line.split()
            if len(parts) >= 2:
                symvers[parts[1]] = int(parts[0], 16)
    vermagics, count, crc_count = set(), 0, 0
    for path in sorted((module_root / KERNEL).rglob("*")):
        if not path.is_file() or not re.search(r"\.ko(?:\.xz|\.gz)?$", path.name):
            continue
        data = path.read_bytes()
        if path.suffix == ".xz":
            data = lzma.decompress(data)
        elif path.suffix == ".gz":
            data = gzip.decompress(data)
        sections = elf_sections(data)
        tags = [item[len(b"vermagic="):].decode("ascii")
                for item in sections.get(".modinfo", b"").split(b"\0") if item.startswith(b"vermagic=")]
        require(len(tags) == 1 and tags[0].startswith(KERNEL + " ") and "aarch64" in tags[0],
                "Kernel module vermagic mismatch: " + str(path.relative_to(root)))
        vermagics.add(tags[0])
        if modversions:
            versions = sections.get("__versions", b"")
            require(versions and len(versions) % 64 == 0, "Missing module symbol versions: " + path.name)
            for offset in range(0, len(versions), 64):
                crc = struct.unpack_from("<Q", versions, offset)[0]
                name = versions[offset + 8:offset + 64].split(b"\0", 1)[0].decode("ascii")
                require(symvers.get(name) == crc, "Module symbol ABI mismatch: " + path.name + ":" + name)
                crc_count += 1
        count += 1
    require(count > 0 and len(vermagics) == 1, "No modules or inconsistent module vermagic")
    return {"kernel": KERNEL, "package_abi": abi, "kernel_package_version": kernel_version,
            "module_count": count, "kmod_package_count": len(kmods), "vermagic": next(iter(vermagics)),
            "module_package_count": module_packages, "builtin_package_count": builtin_packages,
            "symbol_versions_enabled": modversions, "symbol_crcs_checked": crc_count}


def uci_named_section(text, name):
    active, options = False, {}
    for line in text.splitlines():
        words = shlex.split(line, comments=True)
        if not words:
            continue
        if words[0] == "config":
            active = len(words) >= 3 and words[2] == name
        elif active and words[0] == "option" and len(words) == 3:
            options[words[1]] = words[2]
    return options


def board_case_body(text, board):
    cases = re.finditer(r"^[ \t]*([A-Za-z0-9_,*?@+./:!|\\ \t\n-]+)\)[ \t]*\n(.*?)^[ \t]*;;",
                        text, re.M | re.S)
    for case in cases:
        patterns = re.sub(r"\\\n|\s", "", case[1]).split("|")
        if board in patterns:
            return case[2]
    return ""


def check_official_runtime(root, source):
    prefix = root / "opt/myos-c8-official"
    manifest = json.loads((prefix / "manifest.json").read_text(encoding="utf-8"))
    require(manifest.get("prefix") == "/opt/myos-c8-official"
            and manifest.get("global_library_replacement") is False
            and manifest.get("kernel_modules_imported") is False,
            "Official userspace isolation manifest mismatch")
    programs = {"atsd", "atsd_cli", "infocd", "infocd_consumer", "smsd", "cellular_init", "cellular_record"}
    libraries = {"libuci.so", "libubox.so.20210516", "libblobmsg_json.so.20210516",
                 "libiwinfo.so.20210430", "libjson-c.so.5", "libnradio-utils.so.1.0.0",
                 "libcrypto.so.1.1", "libgcc_s.so.1", "libc.so", "ld-musl-aarch64.so.1",
                 "libbdinfo.so.3.3", "libnl-tiny.so"}
    records = manifest.get("files", [])
    require(len(records) == 19 and {Path(record["installed"]).name for record in records} == programs | libraries,
            "Official runtime file closure mismatch")
    reference = source / "target/linux/mediatek/image/nradio-c8-688/official-assets/root"
    for record in records:
        name = Path(record["installed"]).name
        directory = "bin" if name in programs else "lib"
        require(record["installed"] == "/opt/myos-c8-official/" + directory + "/" + name,
                "Official runtime destination escapes private directory")
        relative = Path(record["source"].lstrip("/"))
        require(".." not in relative.parts and relative.name == name, "Invalid official source path")
        data = (prefix / directory / name).read_bytes()
        require(len(data) == record["size"] and hashlib.sha256(data).hexdigest() == record["sha256"],
                "Packed official ELF hash mismatch: " + name)
        require(data == (reference / relative).read_bytes(), "Official ELF differs from inspected source: " + name)
        require(data[:6] == b"\x7fELF\x02\x01" and struct.unpack_from("<H", data, 18)[0] == 183,
                "Official runtime architecture mismatch: " + name)
        dynamic = run_bytes("readelf", "-d", prefix / directory / name).decode("utf-8")
        needed = re.findall(r"\(NEEDED\).*Shared library:\s*\[([^\]]+)\]", dynamic)
        require(needed == record["needed"] and set(needed) <= libraries,
                "Official dependency differs from the packed ELF: " + name)
        if name in programs:
            require(record["interpreter"] == "/lib/ld-musl-aarch64.so.1", "Official interpreter mismatch")
            headers = run_bytes("readelf", "-l", prefix / directory / name).decode("utf-8")
            require("[Requesting program interpreter: /lib/ld-musl-aarch64.so.1]" in headers,
                    "Official executable interpreter differs from its manifest: " + name)
            expected = '#!/bin/sh\nexec /usr/libexec/myos-c8-official ' + name + ' "$@"\n'
            require((root / "usr/sbin" / name).read_text(encoding="utf-8") == expected,
                    "Official system entry must be a private-runtime wrapper: " + name)
    wrapper = (root / "usr/libexec/myos-c8-official").read_text(encoding="utf-8")
    require('runtime=/opt/myos-c8-official' in wrapper and '--library-path "$runtime/lib"' in wrapper
            and 'exec "$runtime/lib/ld-musl-aarch64.so.1"' in wrapper
            and 'unset LD_PRELOAD LD_LIBRARY_PATH' in wrapper, "Official loader wrapper is not isolated")
    for name in ("libc.so", "libuci.so"):
        system = list((root / "lib").glob(name + "*"))
        require(system and all(path.is_file() and path.read_bytes() != (prefix / "lib" / name).read_bytes()
                               for path in system),
                "Old official library replaced or removed the system library: " + name)
    factory_switch = (reference / "usr/bin/cpetools.sh").read_text(encoding="utf-8")
    factory_switch = factory_switch.replace('readonly CPE_SIM_CTRL_DIR="$GPIO_EXPORT_DIR"',
        'readonly CPE_SIM_CTRL_DIR="${MYOS_C8_SIM_CTRL_DIR:-$GPIO_EXPORT_DIR}"', 1)
    factory_switch = factory_switch.replace('if ! chk_sim_num "$_gval"; then',
        'if ! chk_sim_num "$_gval" || [ "$MYOS_C8_SIM_REINIT" = 1 ]; then', 1)
    require((prefix / "share/cpetools.sh").read_text(encoding="utf-8") == factory_switch,
            "Factory cpetools differs beyond C8 GPIO compatibility and explicit same-slot reinitialization")
    factory_huawei = (reference / "etc/cpetools/huawei.sh").read_text(encoding="utf-8")
    rstsim = re.search(r"(?ms)^command_huawei_rstsim\(\) \{.*?^\}", factory_huawei)
    require(rstsim is not None, "Factory SIM activation function is missing")
    fixed_rstsim = rstsim[0].replace('local _ctl="$1"', 'local _ctl="$1"\n\tlocal _res', 1)
    fixed_rstsim = fixed_rstsim.replace(
        '\t\t_command_exec_raw "$1" "${AT_PRIVATE_PREFIX}HVSST=1,1" "5"',
        '\t\t_res=$(_command_exec_raw "$1" "${AT_PRIVATE_PREFIX}HVSST=1,1" "5")\n'
        '\t\techo "$_res"\n\t\techo "$_res" | grep -qsw "OK" || return 1', 1)
    require((root / "etc/cpetools/huawei.sh").read_text(encoding="utf-8")
            == factory_huawei.replace(rstsim[0], fixed_rstsim, 1),
            "Factory Huawei script differs beyond SIM activation error propagation")
    scripts = manifest.get("reference_scripts", [])
    require(len(scripts) == 19 and len({record["path"] for record in scripts}) == 19,
            "Official script/data provenance list is incomplete")
    overlay = source / "target/linux/mediatek/image/nradio-c8-688/native-overlay"
    for record in scripts:
        relative = Path(record["path"].lstrip("/"))
        origin = Path(record["source"].lstrip("/"))
        require(".." not in relative.parts and ".." not in origin.parts,
                "Official script provenance escapes the C8 input/root")
        reference_data = (reference / origin).read_bytes()
        installed_data = (root / relative).read_bytes()
        expected = overlay / relative
        require(record["reference_size"] == len(reference_data)
                and record["reference_sha256"] == hashlib.sha256(reference_data).hexdigest(),
                "Official reference script/data hash mismatch: " + str(origin))
        require(record["installed_size"] == len(installed_data)
                and record["installed_sha256"] == hashlib.sha256(installed_data).hexdigest(),
                "Installed script/data differs from its manifest: " + str(relative))
        require(record["native_overlay"] == expected.is_file()
                and installed_data == (expected.read_bytes() if expected.is_file() else reference_data),
                "Installed script/data differs from its declared C8 overlay: " + str(relative))
        if relative.suffix == ".json":
            require(record["json_validation"] in ("python-json", "host-jshn-json-c"),
                    "Official JSON lacks a structural-parser validation: " + str(relative))
    require((root / "lib/network/wwan/3466:3301").is_file(), "Native USB descriptor name mismatch")
    for relative in ("etc/freq.json", "usr/lib/lua/luci/luapdu.lua",
                     "usr/lib/lua/luci/luapdu/string.lua", "usr/lib/lua/luci/luapdu/smsobject.lua"):
        require((root / relative).read_bytes() == (reference / relative).read_bytes(),
                "Official frequency/PDU support differs from its source: " + relative)
    cellular = (root / "etc/config/cellular_init").read_text(encoding="utf-8")
    require(not re.search(r"\boption\b", cellular), "Live cellular identity values entered the image")
    return {"files": 19, "architecture": "aarch64", "private_loader": True,
            "global_libraries_preserved": True, "live_cellular_identity_excluded": True,
            "script_provenance_verified": len(scripts)}


def check_root_files(root, version):
    def text(relative):
        return (root / relative).read_text(encoding="utf-8")

    for relative, pattern in (("etc/openwrt_release", r"^DISTRIB_RELEASE=['\"]([^'\"]+)['\"]$"),
                              ("usr/lib/os-release", r'^VERSION_ID="([^"]+)"$'),
                              ("etc/myos-version", r"^(.+)$")):
        match = re.search(pattern, text(relative).strip(), re.M)
        require(match and match[1] == version, "Packed version differs: " + relative)
    network = text("etc/board.d/02_network")
    case = board_case_body(network, BOARD)
    require(case and re.search(r'ucidef_set_interfaces_lan_wan\s+[\"\']port1 port2 port3[\"\']\s+[\"\']?port5', case),
            "Packed C8 board network mapping mismatch")
    upgrade = text("lib/upgrade/platform.sh")
    case = board_case_body(upgrade, BOARD)
    require(case and re.search(r'CI_KERNPART=[\"\']kernel_2nd[\"\']', case)
            and re.search(r'CI_ROOTPART=[\"\']rootfs_2nd[\"\']', case)
            and "emmc_do_upgrade" in case, "Packed C8 upgrade partitions mismatch")

    for section in ("cpesim1", "cpesim2", "cpesim3", "cpesim4"):
        modem = uci_named_section(text("etc/config/cpecfg"), section)
        require(modem.get("mobility") == "2" and modem.get("mode") == "auto",
                "All native C8 SIM policies must default to no preference (2)")
    selector = uci_named_section(text("etc/config/cpesel"), "sim")
    require(selector.get("max") == "4" and selector.get("gval") == "10-1,11-1,11-2,01-2"
            and selector.get("stype") == "0,0,4,4" and selector.get("mode") == "1",
            "C8 external/internal manual SIM routing mismatch")
    for relative in ("etc/config/myos_c8_modem", "usr/sbin/myos-c8-modemd", "usr/sbin/myos-c8-modemctl",
                     "etc/init.d/myos-c8-modem"):
        require(not (root / relative).exists(), "Standalone modem draft entered the native image: " + relative)
    for relative in ("usr/libexec/myos-c8-native", "etc/init.d/myos-c8-cellular", "usr/libexec/myos-c8-official",
                     "lib/functions/myos-c8-identity.sh", "usr/libexec/myos-boot-system", "usr/libexec/myos-ubenv"):
        path = root / relative
        require(path.is_file() and path.stat().st_mode & 0o111, "Missing executable C8 component: " + relative)
    rc = list((root / "etc/rc.d").iterdir())
    for service in ("myos-c8-cellular", "atsd", "infocd", "smsd"):
        require(any(re.fullmatch(r"S\d+" + re.escape(service), path.name) for path in rc),
                "Native C8 service is not enabled: " + service)
    legacy = re.compile(r"modeminit|atsd_tools|websocket_server|5700netchek|smstrun|httpapi|"
                        r"myos-c8-modem|cellular_init|cellular_power|cpesel|wanchk", re.I)
    require(not [path.name for path in rc if path.name.startswith("S") and legacy.search(path.name)],
            "Legacy modem service remains enabled")
    require(not (root / "etc/hotplug.d/usb/50-map-at").exists(), "Legacy USB node remapper remains active")
    for relative in ("etc/rc.local", "etc/crontabs/root"):
        path = root / relative
        if path.exists():
            active = "\n".join(line for line in text(relative).splitlines() if not line.lstrip().startswith("#"))
            require(not legacy.search(active), "Legacy modem command remains active: " + relative)
    identity = uci_named_section(text("etc/config/myos_nros"), "main")
    require(not identity.get("manual_mac", "").strip(), "C8 image contains a fixed remote identity MAC")
    profile = dict(line.split("=", 1) for line in text("etc/wireless/l1profile.dat").splitlines() if "=" in line)
    require(profile.get("INDEX0_EEPROM_offset") == "0x0" and profile.get("INDEX0_EEPROM_size") == "0x1000"
            and profile.get("INDEX0_EEPROM_name") == "myos-c8-eeprom.bin", "C8 local EEPROM profile mismatch")
    defaults = text("etc/uci-defaults/93-myos-c8")
    require("if [ -f /tmp/myos-c8-new-network ]; then" in defaults
            and "network.cpe.device=eth1" in defaults and "network.cpe.phyifname=eth1" in defaults
            and "network.cpe.mobility=2" in defaults and "blacklist_band" not in defaults,
            "Fresh C8 network defaults must retain native Ethernet data and no frequency preference")
    eeprom = root / "lib/firmware/myos-c8-eeprom.bin"
    require(not eeprom.exists() and not eeprom.is_symlink(), "A reference-device EEPROM entered the image")
    calibration = text("usr/libexec/myos-c8-calibration")
    require("PARTLABEL=factory" in calibration and "bs=4096 count=1" in calibration
            and "8179" in calibration and "myos-c8-identity calibrate" in calibration,
            "C8 calibration must validate this device's factory bytes and local identity")
    require("myos-c8-calibration check" in text("sbin/wifi"), "Wi-Fi lacks the local calibration guard")
    for directory in ("etc/modules.d", "etc/modules-boot.d"):
        for path in (root / directory).glob("*"):
            if path.is_file():
                require(not re.search(r"^\s*mt_wifi(?:\s|$)", path.read_text(encoding="utf-8"), re.M),
                        "MTK Wi-Fi loads before local calibration is ready")
    collector = text("etc/init.d/myos-c8-cellular")
    require(re.search(r"^START=16$", collector, re.M)
            and "/usr/libexec/myos-c8-native run" in collector
            and not re.search(r"select_sim|apply-saved|cellular_init|cpesel", collector),
            "Native collector startup must only collect cached status")
    adapter = text("usr/libexec/myos-c8-native")
    tree = ast.parse(adapter)
    sim_calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call)
                 and isinstance(node.func, ast.Name) and node.func.id == "select_sim"]
    require(len(sim_calls) == 1, "SIM selection must have one explicit request dispatch")
    dispatch = [node for node in ast.walk(tree) if isinstance(node, ast.If)
                and isinstance(node.test, ast.Compare)
                and any(isinstance(value, ast.Constant) and value.value == "select_sim"
                        for value in node.test.comparators)]
    require(any(sim_calls[0] in list(ast.walk(node)) for node in dispatch), "SIM selection is not request-gated")
    functions = {node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)}
    data_constants = {target.id: ast.literal_eval(node.value) for node in tree.body
                      if isinstance(node, ast.Assign) for target in node.targets
                      if isinstance(target, ast.Name) and target.id in ("DATA_DEVICE", "DATA_CID")}
    require(data_constants == {"DATA_DEVICE": "eth1", "DATA_CID": 8},
            "Native data path must be Ethernet eth1/CID8")
    selection = ast.get_source_segment(adapter, functions["select_sim"])
    require('request.get("origin") != "module_settings"' in selection,
            "Native SIM change lacks explicit module-settings origin")
    configure = ast.get_source_segment(adapter, functions["configure_slot"])
    require('"/usr/bin/cpetools.sh", "-i", "cpe", "-s", str(slot)' in configure
            and not re.search(r"SCICHG|HVSST|CFUN|write_text", configure),
            "Manual SIM selection must delegate to factory cpetools -s")
    reconnect = ast.get_source_segment(adapter, functions["reconnect"]).replace("{DATA_CID}", "8")
    require("NDISDUP=8,0" in reconnect and "NDISDUP=8,1" in reconnect
            and not re.search(r"SCICHG|HVSST|CFUN|select_sim|write_text", reconnect),
            "Native reconnect must affect the data session only")
    for name in ("netifd_connect", "netifd_disconnect"):
        body = ast.get_source_segment(adapter, functions[name])
        require('interface != "cpe"' in body and 'require_modem()' in body
                and not re.search(r"SCICHG|HVSST|CFUN|select_sim|write_text|FREQLOCK", body),
                "C8 netifd path changes something beyond CID8 data: " + name)
    connect = ast.get_source_segment(adapter, functions["netifd_connect"]).replace("{DATA_CID}", "8")
    require('AT^NDISSTATQRY=8' in connect and 'AT^NDISDUP=8,1' in connect
            and 'wait_session(True)' in connect and 'AT^SETAUTODIAL=1,2' in connect
            and 'Ethernet autodial profile readback failed' in connect,
            "C8 netifd connect lacks Ethernet autodial/CID8 verification")
    disconnect = ast.get_source_segment(adapter, functions["netifd_disconnect"])
    require(not any(isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                    and node.func.id in ("at", "at_response", "run", "wait_session")
                    for node in ast.walk(functions["netifd_disconnect"])),
            "Host DHCP teardown must preserve the modem's automatic session")
    constants = {target.id: ast.literal_eval(node.value) for node in tree.body
                 if isinstance(node, ast.Assign) and isinstance(node.value, ast.Dict)
                 for target in node.targets if isinstance(target, ast.Name) and target.id == "SIM_MAP"}
    require(constants.get("SIM_MAP") == {
        1: {"sel0": 0, "sel1": 1, "usim": 1},
        2: {"sel0": 1, "sel1": 1, "usim": 1},
        3: {"sel0": 1, "sel1": 1, "usim": 2},
        4: {"sel0": 1, "sel1": 0, "usim": 2}},
        "Native C8 SIM routes differ from the board select table")
    lua = text("usr/lib/lua/myos_c8_modem.lua")
    require("/tmp/myos-cellular.json" in lua and "/usr/libexec/myos-c8-native request" in lua,
            "Native LuCI adapter is not using the shared collector/request endpoints")
    for name in ("apply_native_cpesel", "sim_swap"):
        body = re.search(r"function M\." + name + r"\([^)]*\)\s+return false\s+end", lua)
        require(body,
                "Legacy application SIM trigger remains active: " + name)
    wrapper = text("usr/lib/lua/myos_nradio_data.lua")
    require('require("myos_nradio_data_generic")' in wrapper
            and 'require("myos_c8_modem")' in wrapper
            and "M.is_native_nbcpe = function() return false end" in wrapper
            and "M.is_native_c5800 = function() return false end" in wrapper
            and "return modem.apply_native_cpesel(slot, mode)" in wrapper,
            "APP does not gate the shared legacy SIM implementation through the C8 adapter")
    for relative in ("usr/lib/lua/luci/controller/nradio/app.lua", "usr/sbin/myos-nradio-app-fast"):
        caller = text(relative)
        require(re.search(r"local function apply_cpesel\(req\)\s*"
                          r"-- C8 SIM writes belong exclusively to explicit module settings\.\s*"
                          r"return false\s*end", caller),
                "APP's local SIM selection fallback is still active: " + relative)
    for path in (root / "usr/lib/lua").rglob("*.lua"):
        if path == root / "usr/lib/lua/myos_nradio_data.lua":
            continue
        require(not re.search(r'require\s*\(?\s*["\']myos_nradio_data_generic["\']',
                              path.read_text(encoding="utf-8")),
                "APP bypasses the C8 adapter: " + str(path.relative_to(root)))
    wwan = text("lib/netifd/proto/wwan.sh")
    require(not re.search(r"ubus\s+call\s+cpesel[^\s]*\s+set\b", wwan)
            and not re.search(r"cpetools\.sh[^\n]*\s-r(?:\s|$)", wwan),
            "Native WWAN still automatically selects a SIM or resets it after a missing-card check")
    require('cpetools.sh -i "$interface" -c prepare' in wwan
            and '/usr/libexec/myos-c8-native netifd-prepare "$interface"' in wwan
            and "proto_wwan_init_pin" in wwan,
            "C8 WWAN override removed native discovery, preparation or PIN handling")
    ncm = text("lib/netifd/proto/ncm.sh")
    require('[ "$interface" = cpe ] && [ "$vendor" = tdtech_mt5700 ] && [ "$ifname" = eth1 ]' in ncm
            and '/usr/libexec/myos-c8-native netifd-connect -- "$interface"' in ncm
            and '/usr/libexec/myos-c8-native netifd-disconnect "$interface"' in ncm
            and 'profile=8' in ncm and 'proto_init_update "$ifname" 1' in ncm
            and 'json_add_string name "${interface}_4"' in ncm
            and 'json_add_string name "${interface}_6"' in ncm
            and 'json_add_string proto dhcp' in ncm and 'json_add_string proto dhcpv6' in ncm,
            "C8 NCM must retain eth1 addressing around explicit CID8 data-session control")
    active_ncm = "\n".join(line for line in ncm.splitlines() if not line.lstrip().startswith("#"))
    require(not re.search(r"-c\s+(?:preinit|pdp)|cpetools\.sh[^\n]*\s-r(?:\s|$)|SCICHG|HVSST|CFUN|FREQLOCK", active_ncm),
            "C8 NCM enters legacy reset, SIM or frequency-policy paths")
    gate = text("usr/bin/cpetools.sh")
    require('while getopts "i:rRdkc:t:m:s:l:L:a:f:uhS" option;' in gate
            and 'S|r|R|d|k|l|L) exit 77' in gate
            and 's) sim="$OPTARG"' in gate and 'case "$sim" in 1|2|3|4)' in gate
            and 'MYOS_C8_FACTORY_CHILD' in gate
            and 'myos-c8-native prepare-sim-gpio' in gate
            and 'MYOS_C8_SIM_CTRL_DIR=/var/run/myos-c8-sim-gpio' in gate
            and 'cfun_o) [ "$interface" = cpe ] || exit 64 ;;' in gate
            and 'usim_set|usim_reset|rstsim|reboot|cfun_c|reset|preinit|pdp)' in gate
            and '*SCICHG=*|*SIMSWITCH=*|*HVSST=*) exit 77' in gate,
            "C8 cpetools must allow explicit four-slot selection and retain other operation gates")
    automatic = re.compile(r"(?:SCICHG|HVSST|SIMSWITCH)\s*=|"
                           r"ubus\s+call\s+cpesel[^\s]*\s+(?:set|reload)\b|"
                           r"/etc/init\.d/cpesel[^\s]*\s+(?:start|restart|reload)\b|"
                           r"cpetools\.sh[^\n]*\s-[sS](?:\s|[0-9])")
    surfaces = [root / "etc/rc.local"]
    for relative in ("etc/hotplug.d", "etc/crontabs", "etc/uci-defaults", "lib/netifd/proto"):
        surfaces.extend(path for path in (root / relative).rglob("*") if path.is_file())
    for path in rc:
        if path.name.startswith("S"):
            name = re.sub(r"^S\d+", "", path.name)
            surfaces.append(root / "etc/init.d" / name)
    for path in surfaces:
        if not path.is_file():
            continue
        active = "\n".join(line for line in path.read_text(encoding="utf-8").splitlines()
                           if not line.lstrip().startswith("#"))
        require(not automatic.search(active),
                "Automatic SIM operation remains on an active launch surface: " + str(path.relative_to(root)))
    records = package_records(text("usr/lib/opkg/status"))
    packages = {record["Package"] for record in records}
    require({"python3-light", "python3-base", "libpython3-3.11"} <= packages,
            "Native collector standard-library runtime is incomplete")
    require(not any(re.search(r"aiohttp|requests|pyserial", name, re.I) for name in packages),
            "Unused third-party Python modem dependencies remain")
    require(not list(root.rglob("*cpython-312*.pyc"))
            and not (root / "usr/libexec/__pycache__").exists(),
            "Host Python 3.12 development cache entered the Python 3.11 image")
    require(not (root / 'usr/libexec/myos-update').exists()
            and not (root / 'usr/libexec/myos-usage').exists(),
            'Private services entered the public source build')
    for relative in ("etc/board.d/02_network", "etc/init.d/myos-c8-cellular", "usr/libexec/myos-c8-native"):
        require((root / relative).stat().st_mode & 0o111, "Packed script is not executable: " + relative)
    return {"packed_version": version, "frequency_policy": "none", "automatic_sim_switch": False,
            "native_collector": True, "manual_sim_slots": 4, "data_only_reconnect": True,
            "legacy_modem_startup_removed": True, "remote_manual_mac_empty": True,
            "wwan_auto_sim_removed": True, "app_sim_gate": True,
            "netifd_cid8_data_session": True,
            "python_runtime": "light/base/3.11", "unused_python_dependencies_removed": True,
            "update_channel_configured": False}


def check_factory_wifi(root):
    def text(relative):
        return (root / relative).read_text(encoding="utf-8")

    helper = text("lib/functions/myos-c8-factory-wifi.sh")
    identity = text("usr/libexec/myos-c8-identity")
    require('command == "factory-mac"' in identity and "identity.hardware()" in identity
            and "/usr/libexec/myos-c8-identity factory-mac" in helper,
            "Wi-Fi defaults must derive from the C8 hardware identity")
    require('ssid=@NRadio-$suffix-$band' in helper
            and 'suffix="$(printf' in helper and 'cut -c 9-' in helper
            and 'password=12345678' in helper
            and 'if [ "$myos_fw_fresh" = 1 ] || [ -z "$key" ]; then' in helper
            and 'wireless.$section.key=$password' in helper
            and 'wireless.$section.encryption=psk2' in helper,
            "Missing C8 Wi-Fi identity/default-password or retained-password handling")
    require("myos_c8_wifi_mark_if_new" in text("lib/preinit/82_myos_c8_factory_wifi")
            and "myos_c8_wifi_apply" in text("etc/init.d/myos-c8-factory-wifi")
            and re.search(r"^START=19$", text("etc/init.d/myos-c8-factory-wifi"), re.M)
            and "/etc/init.d/myos-c8-factory-wifi enable" in text("etc/uci-defaults/94-myos-c8-factory-wifi"),
            "C8 Wi-Fi boot initialization is incomplete")
    start = root / "etc/rc.d/S19myos-c8-factory-wifi"
    require(start.is_symlink() and start.readlink().as_posix() == "../init.d/myos-c8-factory-wifi",
            "C8 Wi-Fi initialization is not enabled at S19")
    for relative in ("etc/uci-defaults/94-myos-factory-wifi", "etc/init.d/myos-factory-wifi",
                     "lib/preinit/82_myos_factory_wifi", "lib/functions/myos-factory-wifi.sh"):
        require(not (root / relative).exists() and not (root / relative).is_symlink(),
                "Another board's Wi-Fi initializer remains: " + relative)
    require("wireless.$section.ssid" not in text("etc/uci-defaults/93-myos-c8"),
            "Legacy Wi-Fi defaults still depend on the network initialization marker")
    return {"ssid": "@NRadio-<MAC-last4>-2.4G/5G", "password": "12345678",
            "retained_password_preserved": True, "hardware_mac": True, "boot_order": 19}


def check_ui(root):
    quickstart = [path.relative_to(root).as_posix() for path in root.rglob("*")
                  if "quickstart" in path.relative_to(root).as_posix().lower()]
    require(not quickstart, "QuickStart files/resources remain: " + repr(quickstart[:12]))
    records = package_records((root / "usr/lib/opkg/status").read_text(encoding="utf-8"))
    packages = {record["Package"] for record in records}
    require(not [name for name in packages if "quickstart" in name.lower()], "QuickStart packages remain")
    require("luci-app-store" in packages, "The iStore package is missing")
    require(not {"luci-app-package-manager", "luci-app-opkg"} & packages and "opkg" in packages,
            "Only the iStore frontend may be installed; its opkg backend must remain")
    for relative in ("www/luci-static/resources/view/system/package-manager.js",
                     "www/luci-static/resources/view/system/opkg.js"):
        require(not (root / relative).exists(), "Unused software frontend remains: " + relative)
    for relative in ("usr/lib/lua/luci/controller/store.lua", "usr/lib/lua/luci/view/store/main.htm",
                     "www/luci-static/istore/index.js", "etc/init.d/istore", "etc/init.d/tasks"):
        require((root / relative).is_file(), "Missing iStore component: " + relative)
    store = (root / "usr/lib/lua/luci/controller/store.lua").read_text(encoding="utf-8")
    require(re.search(r'entry\s*\(\s*\{\s*["\']admin["\']\s*,\s*["\']store["\']', store),
            "The iStore menu route is missing")
    menu = {}
    for path in (root / "usr/share/luci/menu.d").glob("*.json"):
        data = json.loads(path.read_text(encoding="utf-8"))
        require(not any("quickstart" in key.lower() for key in data), "QuickStart menu route remains")
        menu.update(data)
    require("admin/network/network" in menu, "Core network interface menu was removed")
    require("admin/system/packages" not in menu, "A second software package frontend remains")
    controllers = root / "usr/lib/lua/luci/controller"
    parent_route = re.compile(r'entry\s*\(\s*\{\s*["\']admin["\']\s*,\s*["\']status["\']\s*,\s*["\']nbcpe["\']\s*\}')
    owners = [path.name for path in controllers.glob("*.lua")
              if parent_route.search(path.read_text(encoding="utf-8"))]
    require(owners == ["zz_mwrt_c8.lua"] and "admin/status/nbcpe" not in menu,
            "Conflicting C8 homepage registrations: " + repr(owners))
    home = (controllers / "zz_mwrt_c8.lua").read_text(encoding="utf-8")
    require('/luci-static/argon' in home and 'view("c2000max/home-c8")' in home
            and 'template("zmode/modem_info")' in home
            and home.count('entry({"admin", "home"}') == 2
            and home.count('_("C8-688 首页"), 0)') == 2
            and 'build_url("admin", "home")' in home
            and "action_root_home" in home and "action_c8_home" in home,
            "MYOS/MWRT homepage selection is incomplete")
    for name in ("zz_mwrt_modem.lua", "zz_mwrt_qmodem.lua", "zz_mwrt_fancontrol.lua"):
        require(not (controllers / name).exists(), "Old controller competes for C8 routes: " + name)
    for relative in ("www/luci-static/resources/view/c2000max/home-c8.js",
                     "usr/lib/lua/luci/view/zmode/modem_info.htm",
                     "usr/lib/lua/luci/controller/modem.lua",
                     "usr/lib/lua/luci/model/cbi/myos_c8_modem.lua",
                     "usr/lib/lua/luci/model/cbi/myos_c8_sms.lua",
                     "usr/lib/lua/luci/view/myos_c8_modem/status.htm",
                     "usr/lib/lua/luci/view/myos_c8_modem/diagnostics.htm",
                     "usr/lib/lua/luci/view/myos_c8_modem/menu.htm",
                     "usr/lib/lua/luci/view/myos_c8_modem/settings_intro.htm",
                     "www/luci-static/myos/c8-cellular.js",
                     "www/luci-static/myos/c8-cellular.css"):
        require((root / relative).is_file(), "Missing native C8 interface: " + relative)
    modem = (controllers / "modem.lua").read_text(encoding="utf-8")
    require('template("myos_c8_modem/status")' in modem
            and 'template("myos_c8_modem/diagnostics")' in modem
            and 'form("myos_c8_modem")' in modem
            and 'alias("admin", "modem", "settings")' in modem
            and 'post("action_request")' in modem,
            "C8 status, diagnostics and explicit settings routes must remain separate")
    menu_js = (root / "www/luci-static/c2000max-ui/menu.js").read_text(encoding="utf-8")
    header = (root / "usr/lib/lua/luci/view/themes/argon/header.htm").read_text(encoding="utf-8")
    dashboard = (root / "www/luci-static/resources/view/c2000max/home-c8.js").read_text(encoding="utf-8")
    require("C8-688" in menu_js and "homeUrl: L.url('admin','home')" in menu_js
            and 'brand_name = "C8-688"' in header and "url('admin/status/nbcpe')" in header,
            "MYOS/MWRT theme homepage links diverge")
    require("myos_c8_status" in dashboard and "cpe_status" in dashboard and "sim_label:value.sim_label" in dashboard
            and not re.search(r"AK68|get_csq", dashboard, re.I), "Dashboard bypasses the native shared status source")
    require((controllers / "myos_c8_status.lua").is_file(), "Missing C8 identity/sensor dashboard API")
    switch = (root / "www/luci-static/resources/view/myos/system-switch.js").read_text(encoding="utf-8")
    require("C8-688" in switch and "NBCPE" not in switch, "System-switch interface has the wrong device identity")
    photographs = {"c8-688-front.png": "82dd97862c0e765a91cad787a818d38c35af33fbba944e54ce8088cfe1796118",
                   "c8-688-angle.png": "964710ae502830ce11a37ca68f307ebf918cfcdef6465e2fd21585394b933aa9"}
    for directory in ("www/luci-static/c2000max-ui/assets", "www/luci-static/router3d"):
        for name, digest in photographs.items():
            require(hashlib.sha256((root / directory / name).read_bytes()).hexdigest() == digest,
                    "C8 product photograph differs from user input: " + directory + "/" + name)
    myos_art = (root / "www/luci-static/c2000max-ui/dashboard.js").read_text(encoding="utf-8")
    mwrt_art = (root / "usr/lib/lua/luci/view/zmode/modem_info.htm").read_text(encoding="utf-8")
    require(all(name in myos_art and name in mwrt_art for name in photographs),
            "Both C8 theme artwork selectors must reference the user product photographs")
    return {"quickstart_removed": True, "store": "iStore", "network_menu_preserved": True,
            "homepage_owner": "zz_mwrt_c8.lua", "shared_home_route": "admin/home",
            "user_product_photographs_verified": True, "separate_cellular_views": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--root", type=Path, help="Compare the packed SquashFS to this assembled SquashFS")
    parser.add_argument("--version", default=VERSION)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    source, image_path = args.source.resolve(), args.image.resolve()
    assets = source / "target/linux/mediatek/image/nradio-c8-688"
    host = source / "staging_dir/host/bin"
    firmware = image_path.read_bytes()
    metadata = json.loads(run_bytes(host / "fwtool", "-i", "-", image_path).decode("utf-8"))
    require(metadata.get("metadata_version") == "1.1" and metadata.get("compat_version") == "1.0",
            "Unexpected firmware metadata compatibility")
    require(metadata.get("supported_devices") == [BOARD], "Firmware supported_devices mismatch")
    manifest = metadata.get("version", {})
    require(manifest.get("dist") == "MYOS" and manifest.get("version") == args.version
            and manifest.get("board") == BOARD and manifest.get("target") == "mediatek/filogic",
            "Firmware version or target metadata mismatch")
    prefix = "sysupgrade-" + BOARD
    with tarfile.open(image_path, "r:") as archive:
        files = [member for member in archive.getmembers() if member.isfile()]
        require({member.name for member in files} == {prefix + "/CONTROL", prefix + "/kernel", prefix + "/root"}
                and len(files) == 3, "Unexpected C8 TAR members")
        require(archive.extractfile(prefix + "/CONTROL").read() == ("BOARD=" + BOARD + "\n").encode(),
                "C8 TAR CONTROL mismatch")
        kernel_data = archive.extractfile(prefix + "/kernel").read()
        root_data = archive.extractfile(prefix + "/root").read()
    require(root_data[:4] == b"hsqs" and len(root_data) >= 96, "Invalid SquashFS root")
    squashfs_size = struct.unpack_from("<Q", root_data, 40)[0]
    require(96 <= squashfs_size <= len(root_data), "Invalid SquashFS bytes_used")
    require(struct.unpack_from("<HH", root_data, 28) == (4, 0), "Expected SquashFS 4.0")
    require(not any(root_data[squashfs_size:]), "Unexpected nonzero data after SquashFS")
    root_data = root_data[:squashfs_size]
    if args.root:
        require(args.root.read_bytes()[:squashfs_size] == root_data, "Packed root differs from assembled SquashFS")

    fit = fdt_properties(kernel_data)
    require(string(fit["/configurations", "default"]) == "config-1", "Unexpected FIT default configuration")
    require(string(fit["/configurations/config-1", "kernel"]) == "kernel-1"
            and string(fit["/configurations/config-1", "fdt"]) == "fdt-1", "Unexpected FIT image references")
    for node in ("/images/kernel-1", "/images/fdt-1"):
        require(string(fit[node, "arch"]) == "arm64", "FIT image architecture mismatch")
        check_hashes(fit, node, fit[node, "data"])
    node = "/images/kernel-1"
    require(string(fit[node, "compression"]) == "lzma" and string(fit[node, "type"]) == "kernel"
            and string(fit[node, "os"]) == "linux", "FIT kernel format mismatch")
    require(cells(fit[node, "load"]) == (LOAD_ADDRESS,) and cells(fit[node, "entry"]) == (LOAD_ADDRESS,),
            "C8 FIT load/entry must be 0x48000000")
    image = lzma.decompress(fit[node, "data"], format=lzma.FORMAT_ALONE)
    require(image[56:60] == b"ARM\x64" and struct.unpack_from("<Q", image, 8)[0] == 0,
            "ARM64 Image header does not match FIT load address")
    require(re.search(rb"Linux version " + re.escape(KERNEL.encode()) + rb"(?:\s|\0)", image),
            "Packed Linux kernel release mismatch")
    source_image = source / "build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/Image"
    require(source_image.read_bytes() == image, "FIT kernel differs from current source Image")
    require(int(string(fit["/", "rootfs_size"])) == squashfs_size, "FIT rootfs size mismatch")
    require(string(fit["/", "rootfs_crc32"]) == format(zlib.crc32(root_data) & 0xFFFFFFFF, "08x"),
            "FIT rootfs CRC mismatch")
    check_hashes(fit, "/rootfs", root_data)
    hardware = check_device_tree(fit["/images/fdt-1", "data"])

    with tempfile.TemporaryDirectory(prefix="verify-c8-", dir=assets) as temporary:
        directory = Path(temporary)
        squashfs = directory / "root.squashfs"
        squashfs.write_bytes(root_data)
        unpacked = directory / "root"
        console = run_bytes(host / "unsquashfs4", "-lln", squashfs, "dev/console").decode("utf-8")
        require(re.search(r"^crw-------\s+0/0\s+5,\s*1\s+.*\s+[^\s]*/dev/console$", console, re.M),
                "Packed /dev/console must be root-owned character device 5:1, mode 0600")
        # A non-root verifier checks the device inode without recreating it.
        run_bytes(host / "unsquashfs4", "-no-progress", "-d", unpacked,
                  "-excludes", squashfs, "dev/console")
        packed = check_root_files(unpacked, args.version)
        modules = check_modules(unpacked, source)
        official = check_official_runtime(unpacked, source)
        ui = check_ui(unpacked)
        factory_wifi = check_factory_wifi(unpacked)
    report = {"file": str(image_path), "bytes": len(firmware), "sha256": hashlib.sha256(firmware).hexdigest(),
              "version": args.version, "board": BOARD, "rootfs_bytes": squashfs_size,
              "fit_hashes_valid": True, "rootfs_hashes_valid": True, "metadata_valid": True,
              "console_device_inode_valid": True,
              "extraction": {"account": "non-root", "excluded_device": "/dev/console",
                             "device_inode": {"major": 5, "minor": 1, "mode": "0600", "uid": 0, "gid": 0},
                             "reason": "Build account cannot mknod; packed inode validated before extraction"},
              "hardware": hardware, "modules": modules, "packed": packed, "official_runtime": official, "ui": ui,
              "factory_wifi": factory_wifi,
              "live_device_validation": "pending"}
    rendered = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.write_text(rendered, encoding="utf-8")
    print(rendered, end="", flush=True)


if __name__ == "__main__":
    main()
