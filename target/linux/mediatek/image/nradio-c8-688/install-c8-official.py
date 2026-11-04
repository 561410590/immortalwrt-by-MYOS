#!/usr/bin/env python3
"""Install the inspected C8 NROS userspace in a private runtime directory."""

from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess


PREFIX = Path("opt/myos-c8-official")
PROGRAMS = ("atsd", "atsd_cli", "infocd", "infocd_consumer", "smsd", "cellular_init", "cellular_record")
FILES = {
    "usr/sbin/atsd": "db5d7a1f28a49456e852d5021b121ce2faa1e7e2006d28d791abcb7f004f259c",
    "usr/sbin/atsd_cli": "47b4dd69341166b0d4e48ed7c954a23764232890d65f34142e42860360ee5070",
    "usr/sbin/infocd": "a8c0fd87ec26a6e8b6d41cc48da00e6e4eda767ebd7a0da06599b503666a142d",
    "usr/sbin/infocd_consumer": "e31ec9d62087a717983eaf94b0a52f5d6d99300dad3a93470177cf9e5f1cf881",
    "usr/sbin/smsd": "6551b6b37320654a376fb431731775ff6a1e9779507aeadb6506a35fa40b778f",
    "usr/sbin/cellular_init": "9bf8fa384d5001976d8ffb849def7a147a27a56ea22eaab4042f2a162e1e4866",
    "usr/sbin/cellular_record": "331ce05e40ee6e0787c3d7a35ef88715492be68e6d24fec2d07b5e1cd80e7b94",
    "lib/libuci.so": "0be7ff2fbc450c99f672a39caf87fcbf837feb413cb8fdbd9800f4f792c79d49",
    "lib/libubox.so.20210516": "19d2b71511f1c065efe4baf3489d6214dbe2d84a89a53113f2794e9a41496600",
    "lib/libblobmsg_json.so.20210516": "0e9c20071f600b999ec704a3a4acbff0be7a155933f0e5d2c178fd3b559cb1e6",
    "usr/lib/libiwinfo.so.20210430": "0939381964b4dbefaae2c6761a9a113d4584889c008da49bd22d5a3fd41293a2",
    "usr/lib/libjson-c.so.5": "4562fc14bc92bea5a9f584d734a49e7f0a90b0416003b6e24cb3e79687ab0e3a",
    "usr/lib/libnradio-utils.so.1.0.0": "f52d45a3229966c1d50e1754368e074ca57f2033da1e2ce61453a155fc15e8f2",
    "usr/lib/libcrypto.so.1.1": "8bd3a804e58b17a44a74e05aa6d2e37bac9740beda16fb32aeba4b78ac6db35c",
    "lib/libgcc_s.so.1": "6baf0c0086b0bd2915019e942a2fa7b2b4dfb4e0935db43a71fc54add13d1a69",
    "lib/libc.so": "9281a3f32ebb381eb349638200ca556869896d322dd46338fdb5546dc20b56b1",
    "lib/ld-musl-aarch64.so.1": "9281a3f32ebb381eb349638200ca556869896d322dd46338fdb5546dc20b56b1",
    "usr/lib/libbdinfo.so.3.3": "8b02e98e42e72127cc5426db7e2b620bc3421da1952f7305f725d105f51ca46c",
    "usr/lib/libnl-tiny.so": "bad5c0328298995d962322d041b2fe709dc90177d531347bb68fc7b1dc19c68e",
}
SCRIPTS = (
    "etc/cpetools/generic.sh", "etc/cpetools/huawei.sh", "etc/cpetools/tdtech.sh",
    "etc/gcom/ncm.json", "etc/freq.json", "etc/hotplug.d/usb/00_wwan.sh",
    "etc/init.d/atsd", "etc/init.d/cellular_init", "etc/init.d/cellular_power",
    "etc/init.d/infocd", "etc/init.d/smsd", "lib/netifd/proto/adapt_apn.sh",
    "lib/netifd/proto/ncm.sh", "lib/netifd/proto/wwan.sh",
    "lib/network/wwan/3466_3301", "usr/bin/cpetools.sh",
    "usr/lib/lua/luci/luapdu.lua", "usr/lib/lua/luci/luapdu/string.lua",
    "usr/lib/lua/luci/luapdu/smsobject.lua",
)
WRAPPER = """#!/bin/sh
runtime=/opt/myos-c8-official
program=${1-}
[ "$#" -gt 0 ] && shift
case "$program" in
    atsd|atsd_cli|infocd|infocd_consumer|smsd|cellular_init|cellular_record) ;;
    *) printf '%s\\n' 'Unsupported C8 official program' >&2; exit 64 ;;
esac
unset LD_PRELOAD LD_LIBRARY_PATH LD_DEBUG LD_AUDIT
exec "$runtime/lib/ld-musl-aarch64.so.1" --library-path "$runtime/lib" \\
    "$runtime/bin/$program" "$@"
"""


def inspect_elf(data):
    if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError("Official runtime requires AArch64 little-endian ELF64")
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    if size < 56 or offset + size * count > len(data):
        raise ValueError("Truncated official ELF program headers")
    headers = [struct.unpack_from("<IIQQQQQQ", data, offset + index * size)
               for index in range(count)]
    interpreter = next((data[item[2]:item[2] + item[5]].rstrip(b"\0").decode("ascii")
                        for item in headers if item[0] == 3), None)
    dynamic = next((item for item in headers if item[0] == 2), None)
    needed = []
    if dynamic:
        tags = [struct.unpack_from("<qQ", data, pos)
                for pos in range(dynamic[2], dynamic[2] + dynamic[5], 16)]
        string_address = next((value for tag, value in tags if tag == 5), None)
        if string_address is not None:
            segment = next(item for item in headers if item[0] == 1
                           and item[3] <= string_address < item[3] + item[5])
            start = segment[2] + string_address - segment[3]
            needed = [data[start + value:data.index(b"\0", start + value)].decode("ascii")
                      for tag, value in tags if tag == 1]
    return {"interpreter": interpreter, "needed": needed}


def put_text(path, content, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    path.chmod(mode)


def validate_reference_json(source, origin, data):
    try:
        json.loads(data)
        return "python-json"
    except json.JSONDecodeError:
        # NROS uses json-c's non-strict grammar, including trailing commas.
        parser = source / "staging_dir/host/bin/jshn"
        if not parser.is_file():
            raise ValueError("Host json-c parser is required for the original NROS JSON: " + str(origin))
        result = subprocess.run([str(parser), "-R", str(origin)], stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE, check=False)
        if result.returncode:
            raise ValueError("Host json-c rejected the original NROS JSON: " + str(origin)
                             + ": " + result.stderr.decode("utf-8", errors="replace").strip())
        return "host-jshn-json-c"


def install_official(source, assets, work):
    """Copy only the inspected ABI closure; native-overlay is applied afterwards."""
    source, assets, work = Path(source), Path(assets), Path(work)
    reference = assets / "official-assets/root"
    available = {Path(name).name for name in FILES if not name.startswith("usr/sbin/")}
    records, blobs = [], []
    for relative, expected in FILES.items():
        data = (reference / relative).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected:
            raise ValueError("Official C8 source SHA256 mismatch: " + relative)
        elf = inspect_elf(data)
        missing = set(elf["needed"]) - available
        if missing:
            raise ValueError("Incomplete official ABI closure: " + relative + ": " + repr(sorted(missing)))
        if relative.startswith("usr/sbin/") and elf["interpreter"] != "/lib/ld-musl-aarch64.so.1":
            raise ValueError("Unexpected official interpreter: " + relative)
        destination = PREFIX / ("bin" if relative.startswith("usr/sbin/") else "lib") / Path(relative).name
        records.append({"source": "/" + relative, "installed": "/" + destination.as_posix(),
                        "size": len(data), "sha256": digest, **elf})
        blobs.append((destination, data))
    for destination, data in blobs:
        path = work / destination
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(0o755)

    script_records = []
    for relative in SCRIPTS:
        origin = reference / relative
        data = origin.read_bytes()
        data.decode("utf-8", errors="strict")
        validation = None
        if relative.endswith(".json"):
            validation = validate_reference_json(source, origin, data)
        installed = "lib/network/wwan/3466:3301" if relative == "lib/network/wwan/3466_3301" else relative
        target = work / installed
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(origin, target)
        target.chmod(0o644 if relative.endswith((".json", ".lua")) or relative.startswith("lib/network/") else 0o755)
        overlay = assets / "native-overlay" / installed
        final_data = overlay.read_bytes() if overlay.is_file() else data
        script_records.append({"source": "/" + relative, "path": "/" + installed,
                               "reference_size": len(data), "reference_sha256": hashlib.sha256(data).hexdigest(),
                               "installed_size": len(final_data),
                               "installed_sha256": hashlib.sha256(final_data).hexdigest(),
                               "native_overlay": overlay.is_file(), "json_validation": validation})
        if relative == "usr/bin/cpetools.sh":
            original = work / PREFIX / "share/cpetools.sh"
            original.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(origin, original)
            original.chmod(0o755)
    put_text(work / "etc/config/cellular_init", "config cellular 'cpe'\n\nconfig cellular 'cpe1'\n")
    put_text(work / "usr/libexec/myos-c8-official", WRAPPER, 0o755)
    for name in PROGRAMS:
        put_text(work / "usr/sbin" / name,
                 '#!/bin/sh\nexec /usr/libexec/myos-c8-official ' + name + ' "$@"\n', 0o755)
    manifest = {"origin": "User-designated C8 NROS 2.3.3.n0.c0 read-only ELF inspection",
                "architecture": "aarch64_cortex-a53", "prefix": "/" + PREFIX.as_posix(),
                "loader": "/opt/myos-c8-official/lib/ld-musl-aarch64.so.1",
                "global_library_replacement": False, "files": records, "reference_scripts": script_records,
                "excluded_live_config": ["/etc/config/cellular_init"],
                "kernel_modules_imported": False}
    put_text(work / PREFIX / "manifest.json", json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return manifest
