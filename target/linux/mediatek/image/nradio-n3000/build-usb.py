#!/usr/bin/env python3
"""Build the additional USB Ethernet drivers against the MYOS kernel used in the FIT."""
import subprocess
import sys
from pathlib import Path
from shutil import copy2

tree, linux, work, repo = map(Path, sys.argv[1:])
objects = work / "usb-build"
objects.mkdir(exist_ok=True)
source = linux / "drivers/net/usb"
for name in ("r8152.c", "aqc111.c", "aqc111.h", "asix_devices.c", "asix_common.c", "asix.h",
             "ax88172a.c", "ax88179_178a.c", "ipheth.c", "huawei_cdc_ncm.c"):
    copy2(source / name, objects / name)
copy2(linux / "drivers/net/phy/ax88796b.c", objects)
copy2(Path(__file__).with_name("Kbuild.usb"), objects / "Kbuild")
compiler = next(tree.glob("staging_dir/toolchain-aarch64*/bin/aarch64-openwrt-linux-musl-gcc"))
cross = str(compiler)[:-3]
subprocess.run(["make", "-C", linux, f"M={objects}", "ARCH=arm64", f"CROSS_COMPILE={cross}",
                "-j8", "modules"], check=True)
version = (linux / "include/config/kernel.release").read_text().strip()
abi = (linux / ".vermagic").read_text().strip()
package = work / "usb-package"
modules = package / "lib/modules" / version
modules.mkdir(parents=True, exist_ok=True)
for ko in sorted(objects.glob("*.ko")):
    subprocess.run([cross + "strip", "--strip-debug", "-o", modules / ko.name, ko], check=True)
order = "ax88796b asix ax88179_178a aqc111 r8152 ipheth huawei_cdc_ncm".split()
autoload = package / "etc/modules.d"
autoload.mkdir(parents=True, exist_ok=True)
(autoload / "65-n3000-usb-ethernet").write_text("\n".join(order) + "\n")
control = package / "CONTROL"
control.mkdir(exist_ok=True)
(control / "control").write_text(f"""Package: kmod-n3000-usb-ethernet
Version: {version}-r1
Architecture: aarch64_cortex-a53
Depends: kernel (={version}~{abi}-r1), kmod-usb-net, kmod-libphy, kmod-usb-net-cdc-ncm, kmod-usb-wdm, kmod-crypto-sha256, r8152-firmware
Description: N3000 external USB Ethernet, Huawei NCM and iPhone tethering drivers
""")
subprocess.run(["sh", tree / "scripts/ipkg-build", package, repo], check=True)
