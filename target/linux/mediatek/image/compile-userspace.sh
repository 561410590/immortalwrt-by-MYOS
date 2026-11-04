#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$root"
make defconfig
kernel_targets=$(python3 - <<'PY'
from pathlib import Path
import re
selected = set(re.findall(r'^CONFIG_PACKAGE_(kmod-[^=]+)=y$', Path('.config').read_text(), re.M))
targets = {'package/kernel/linux/compile'}
makefile = ''
for line in Path('tmp/.packageinfo').read_text().splitlines():
    if line.startswith('Source-Makefile: '):
        makefile = line.split(': ', 1)[1]
    elif line.startswith('Package: ') and line.split(': ', 1)[1] in selected:
        targets.add(str(Path(makefile).parent) + '/compile')
print(' '.join(sorted(targets)))
PY
)
make $kernel_targets -j6 V=s
# Build installer userspace dependencies before assembling the native rootfs.
make \
    package/feeds/packages/coreutils/compile package/feeds/packages/lyaml/compile \
    package/feeds/packages/yaml/compile package/feeds/packages/udns/compile \
    package/network/utils/resolveip/compile package/feeds/packages/libev/compile \
    package/feeds/packages/libsodium/compile package/network/utils/iproute2/compile \
    package/system/uci/compile package/libs/libbpf/compile \
    package/libs/elfutils/compile package/network/utils/iptables/compile \
    package/libs/libpcap/compile package/feeds/packages/miniupnpd/compile \
    package/network/utils/ipset/compile package/feeds/packages/conntrack-tools/compile \
    package/feeds/packages/libnetfilter-cthelper/compile package/feeds/packages/libnetfilter-cttimeout/compile \
    package/feeds/packages/libnetfilter-queue/compile package/libs/libtool/compile \
    package/utils/lua5.3/compile \
    -j6 V=s
make package/feeds/packages/jq/compile package/feeds/packages/bc/compile -j6 V=s
make package/feeds/packages/openvpn/compile -j6 V=s
make package/feeds/istore/luci-app-store/compile package/mtk/applications/mtwifi-cfg/compile package/mtk/applications/luci-app-mtwifi-cfg/compile -j6 V=s
make package/luci-theme-c2000max-ui/compile package/luci-theme-mwrt/compile -j6 V=s
