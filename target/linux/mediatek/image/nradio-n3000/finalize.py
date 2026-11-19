from pathlib import Path
import importlib
import json
import subprocess
import sys
from types import SimpleNamespace


tree, work, root = map(Path, sys.argv[1:])
board = "NRadio-N3000-emmc"
release = dict(line.split("=", 1) for line in (root / "etc/openwrt_release").read_text().splitlines() if "=" in line)
info = dict(dist="MYOS", version=release["DISTRIB_RELEASE"].strip("'"),
            revision=release["DISTRIB_REVISION"].strip("'"), target="mediatek/filogic", board=board)
metadata = {"metadata_version": "1.1", "compat_version": "1.0", "supported_devices": [board], "version": info}
firmware = work / "sysupgrade.bin"
fwtool = tree / "staging_dir/host/bin/fwtool"
subprocess.run([fwtool, "-I", "-", firmware], input=json.dumps(metadata).encode(), check=True)
sys.dont_write_bytecode = True
sys.path.insert(0, str(tree.parent))
audit = importlib.import_module("build-myos")
model = SimpleNamespace(tree=tree, name="N3000", donors=())
reports = work / "permissions"
reports.mkdir(exist_ok=True)
package_source = SimpleNamespace(tree=work, name="N3000", donors=(work / "repo",))
audit.package_permissions(package_source, root, reports)
audit.rootfs_permissions(root, reports / "assembled-rootfs.tsv")
audit.final_permissions(model, firmware, root, reports)
print(f"{firmware} ({firmware.stat().st_size} bytes)", flush=True)
