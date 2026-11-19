"""Add N3000 applications and their userspace package sources to the offline feed."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile

tree, work, reference = map(Path, sys.argv[1:])
here = Path(__file__).resolve().parent
repo = work / "repo"
refinfo = reference / "usr/lib/opkg/info"

# MT798x userspace is shared. Existing MYOS packages take precedence. The USB
# edition of QModem uses upstream QMI, unlike the earlier PCIe-specific build.
base_names = {p.name.split("_", 1)[0] for p in repo.glob("*.ipk")}
supplement = tree.with_name(tree.name + "-n5000") / "bin/packages/aarch64_cortex-a53"
for path in sorted(supplement.glob("*/*.ipk")):
    name = path.name.split("_", 1)[0]
    if name.startswith("kmod-") or name == "kernel":
        continue
    if name not in base_names or name in {"qmodem", "tom_modem", "quectel-CM-5G-M"}:
        link = repo / path.name
        if not link.exists():
            link.symlink_to(path)


def copy_entry(staging, name):
    rel = Path(name.lstrip("/"))
    if ".." in rel.parts:
        raise ValueError(name)
    source = reference / rel
    if not source.parent.resolve().is_relative_to(reference.resolve()):
        raise ValueError(name)
    target = staging / rel
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.is_symlink():
        target.symlink_to(os.readlink(source))
    elif source.is_file():
        shutil.copyfile(source, target)
        # Retain execute bits. Remove inherited group/other write from vendor files.
        target.chmod(source.stat().st_mode & 0o755)
    else:
        raise FileNotFoundError(source)
    return target


def original_func(source, name):
    start = re.search(r"^function " + name + r"\(", source, re.M)
    if start is None:
        raise ValueError(name)
    stop = re.search(r"^end\s*$", source[start.end():], re.M)
    return source[start.start():start.end()+stop.end()] + "\n"


def cellular(staging):
    files = ["usr/bin/" + n for n in ("sendat2", "smssend", "pdul.lua", "pdu_decoder", "mwrt-smscfg", "mwrt-cellinfo")]
    files += ["usr/lib/lua/luci/view/zmode/smsc.htm", "usr/libexec/mwrt-notice-ack.uc"]
    for name in files:
        copy_entry(staging, name)
    controller = staging / "usr/lib/lua/luci/controller/wtmodem_n3000.lua"
    controller.parent.mkdir(parents=True, exist_ok=True)
    status = staging / "usr/lib/lua/luci/view/wtmodem/status.htm"
    status.parent.mkdir(parents=True)
    shutil.copyfile(here / "wtmodem-status.htm", status)
    at = staging / "usr/bin/sendat"
    shutil.copyfile(here / "wtmodem-sendat", at)
    at.chmod(0o755)
    # Use the port selected for the MT5700 by the QModem scanner.
    cellinfo = staging / "usr/bin/mwrt-cellinfo"
    contents = cellinfo.read_text().replace('ATPORT=1', 'ATPORT="$1"')
    contents = contents.replace('grep -q 5700 /tmp/modconf.conf 2>/dev/null || exit 0',
                                'case "$ATPORT" in /dev/*) ;; *) exit 1 ;; esac')
    cellinfo.write_text(contents.replace('sendat "$ATPORT" "$1"', 'tom_modem -d "$ATPORT" -o a -c "$1" -t 3'))
    source = (reference / "usr/lib/lua/luci/controller/modem.lua").read_text()
    actions = ("action_smslist", "action_smssend", "action_smscfg")
    controller.write_text((here / "wtmodem.lua").read_text() + "\n" +
                          "\n".join(original_func(source, name) for name in actions).replace(
                              'smsforward.settings.enable', 'sms_forwarder.sms_forward.enable'))
    # MYOS supplies its own SMS forwarder under QModem.
    view = staging / "usr/lib/lua/luci/view/zmode/smsc.htm"
    view.write_text(view.read_text().replace('build_url("admin", "modem", "smsforward")',
                                             'build_url("admin", "modem", "qmodem", "sms_forward")'))
    # The SMS sender uses the standard sms-tool send interface on generic USB modems.
    sender = staging / "usr/bin/smssend"
    contents = sender.read_text().replace('"/usr/bin/sms_tool_q"', '"/usr/bin/sms_tool"')
    start = contents.index('    local f = io.open(PORT_CACHE, "r")')
    end = contents.index('    if port == ""', start)
    sender.write_text(contents[:start] +
                      '''    local port = readall("uci -q get 'qmodem.@modem-device[0].at_port'"):gsub("%s+", "")
    if port == "" then port = "/dev/ttyUSB" .. AT_INDEX end
''' + contents[end:])


def package(name):
    with tempfile.TemporaryDirectory(dir=work) as tmp:
        staging = Path(tmp)
        control_dir = staging / "CONTROL"
        control_dir.mkdir()
        control = (refinfo / (name + ".control")).read_text()
        if name == "luci-app-WTModem":
            cellular(staging)
            control = re.sub(r"^Depends:.*$", "Depends: libc, luci-compat, luci-lua-runtime, luci-lib-jsonc, luci-app-qmodem-next, qmodem, tom_modem, sms-forwarder-next, sms-tool, flock", control, flags=re.M)
            control = re.sub(r"^Version:.*$", "Version: 1.5-r165.myos2", control, flags=re.M)
            control = re.sub(r"^Architecture:.*$", "Architecture: aarch64_cortex-a53", control, flags=re.M)
        else:
            listing = (refinfo / (name + ".list")).read_text().splitlines()
            for item in listing:
                if item:
                    target = copy_entry(staging, item)
                    if item in ("/etc/openclash/custom/openclash_custom_firewall_rules.sh",
                                "/etc/openclash/custom/openclash_custom_overwrite.sh",
                                "/usr/share/passwall/luci_update.sh", "/usr/share/passwall2/luci_update.sh"):
                        target.chmod(0o755)
            if name == "luci-app-openclash" and not (staging / "etc/openclash/GeoIP.dat").exists():
                shutil.copyfile(reference / "usr/share/v2ray/geoip.dat", staging / "etc/openclash/GeoIP.dat")
            conffiles = refinfo / (name + ".conffiles")
            if conffiles.exists():
                shutil.copyfile(conffiles, control_dir / "conffiles")
            for hook in ("prerm", "postrm"):
                path = refinfo / (name + "." + hook)
                if path.exists():
                    shutil.copyfile(path, control_dir / hook)
                    (control_dir / hook).chmod(0o755)
        # The reference firmware's opkg metadata no longer contains build-time postinst.
        # Stock LuCI initializers are shipped in /etc/uci-defaults and run on first boot.
        (control_dir / "control").write_text(re.sub(r"^Installed-Size:.*\n", "", control, flags=re.M))
        subprocess.run(["sh", tree / "scripts/ipkg-build", staging, repo], check=True)


for app in ("luci-app-openclash", "luci-app-passwall", "luci-app-passwall2",
            "luci-i18n-passwall-zh-cn", "luci-i18n-passwall2-zh-cn",
            "sing-box", "xray-core", "naiveproxy", "hysteria", "tuic-client",
            "shadowsocks-rust-sslocal", "shadowsocks-rust-ssserver", "shadow-tls",
            "v2ray-plugin", "xray-plugin", "v2ray-geoip", "v2ray-geosite",
            "luci-app-wireguard", "luci-i18n-wireguard-zh-cn", "luci-app-WTModem"):
    # Prefer the user-supplied N3000 release for these applications.
    for existing in repo.glob(app + "_*.ipk"):
        if existing.is_symlink():
            existing.unlink()
    package(app)
