#!/bin/sh
set -eu
ROOT=$1
SRC=$2
VERSION=$3
REVISION=$4
ID_REVISION=$(printf '%s' "$REVISION" | tr '+' '_')
SD=$SRC/target/linux/mediatek/image/nradio-c2000-ultra/sd-tools
GPT_HELPER=${5:-"$SD/myos-ultra-gpt"}
WEB=$SRC/package/mtk/applications/5g-modem/luci-app-5700webui/root
QMODEM=$SRC/package/mtk/applications/c2000max-5g/files/rootfs
SIM=$SRC/target/linux/mediatek/image/nradio-c2000-ultra
# Package caches retain base-files between timestamp-only builds. Stamp the
# final ROM from the same version argument used for firmware metadata.
case "$VERSION" in V[0-9]*-[0-9]*) ;; *) echo "invalid MYOS version: $VERSION" >&2; exit 1 ;; esac
sed -i "s/^DISTRIB_RELEASE=.*/DISTRIB_RELEASE='$VERSION'/; s/^DISTRIB_REVISION=.*/DISTRIB_REVISION='$REVISION'/; s/^DISTRIB_DESCRIPTION=.*/DISTRIB_DESCRIPTION='MYOS by ImmortalWrt $VERSION'/" "$ROOT/etc/openwrt_release"
printf '%s\n' "$VERSION" > "$ROOT/etc/openwrt_version"
sed -i "s/^VERSION=.*/VERSION=\"$VERSION\"/; s/^VERSION_ID=.*/VERSION_ID=\"$VERSION\"/; s/^PRETTY_NAME=.*/PRETTY_NAME=\"MYOS by ImmortalWrt $VERSION\"/; s/^OPENWRT_RELEASE=.*/OPENWRT_RELEASE=\"MYOS by ImmortalWrt $VERSION\"/; s/^BUILD_ID=.*/BUILD_ID=\"$REVISION\"/" "$ROOT/usr/lib/os-release"
printf '\nMYOS by ImmortalWrt %s\n\n' "$VERSION" > "$ROOT/etc/banner"
install -D -m 0755 "$GPT_HELPER" "$ROOT/usr/libexec/myos-ultra-gpt"
install -D -m 0755 "$SD/ubenv-snsd" "$ROOT/usr/libexec/myos-ultra-ubenv"
install -D -m 0644 "$SIM/c2000ultra-upgrade.platform" "$ROOT/lib/upgrade/platform.sh"
install -D -m 0644 "$SRC/package/base-files/files/lib/upgrade/emmc.sh" "$ROOT/lib/upgrade/emmc.sh"
install -D -m 0644 "$SIM/web/luci.locations" "$ROOT/etc/nginx/conf.d/luci.locations"
install -D -m 0755 "$SIM/c2000ultra-webui-defaults.uci-default" "$ROOT/etc/uci-defaults/37-c2000ultra-webui"
install -D -m 0644 "$SIM/web/luci-cgi_io.ini" "$ROOT/etc/uwsgi/vassals/luci-cgi_io.ini"
install -D -m 0755 "$SIM/c2000ultra-board-defaults.uci-default" "$ROOT/etc/uci-defaults/36-c2000ultra-board-defaults"
install -D -m 0755 "$SD/myos-ultra-boot" "$ROOT/usr/libexec/myos-ultra-boot"
install -D -m 0755 "$SD/c2000ultra-sd-extroot.preinit" "$ROOT/lib/preinit/79_c2000ultra_sd_extroot"
install -D -m 0755 "$SD/ultra-firstboot" "$ROOT/sbin/firstboot"
install -D -m 0644 "$SD/ultra_sd.js" "$ROOT/www/luci-static/resources/view/myos/ultra_sd.js"
install -D -m 0644 "$SD/myos-ultra-sd.uc" "$ROOT/usr/share/rpcd/ucode/myos-ultra-sd"
install -D -m 0644 "$SD/myos-ultra-sd-menu.json" "$ROOT/usr/share/luci/menu.d/myos-ultra-sd.json"
install -D -m 0644 "$SD/myos-ultra-sd-acl.json" "$ROOT/usr/share/rpcd/acl.d/myos-ultra-sd.json"
install -D -m 0755 "$SD/return-myos.sh" "$ROOT/usr/share/myos-ultra-sd/return-myos.sh"
install -D -m 0644 "$SD/recovery-README.txt" "$ROOT/usr/share/myos-ultra-sd/recovery-README.txt"
printf '%s-%s\n' "$VERSION" "$ID_REVISION" > "$ROOT/etc/c2000ultra-rootfs-id"
for script in modem_dial.sh modem_ctrl.sh modem_util.sh; do
  install -D -m 0755 "$QMODEM/usr/share/qmodem/$script" "$ROOT/usr/share/qmodem/$script"
done
install -D -m 0755 "$QMODEM/usr/libexec/rpcd/qmodem" "$ROOT/usr/libexec/rpcd/qmodem"
install -D -m 0644 "$QMODEM/etc/config/qmodem" "$ROOT/etc/config/qmodem"
install -D -m 0755 "$QMODEM/etc/init.d/qmodem_network" "$ROOT/etc/init.d/qmodem_network"
install -D -m 0755 "$QMODEM/usr/share/qmodem/vendor/huawei.sh" "$ROOT/usr/share/qmodem/vendor/huawei.sh"
install -D -m 0755 "$SIM/c2000ultra-sim-select.init" "$ROOT/etc/init.d/c2000ultra-sim-select"
install -D -m 0755 "$SIM/c2000ultra-sim-defaults.uci-default" "$ROOT/etc/uci-defaults/33-c2000ultra-sim-selection"
if [ ! -f "$ROOT/etc/config/cpesel" ]; then
  printf "config cpesel 'sim'\n\toption max '4'\n\toption gval '0-2,1-1,1-2'\n\toption stype '0,0,4'\n\toption mode '1'\n\toption cur '1'\n\toption default '1'\n" > "$ROOT/etc/config/cpesel"
fi
install -D -m 0644 "$SIM/c2000ultra-simcard.js" "$ROOT/www/luci-static/resources/view/embed/simcard.js"
install -D -m 0644 "$SRC/package/mtk/applications/c2000max-5g/files/luci-app-embed/usr/share/rpcd/acl.d/luci-app-embed.json" "$ROOT/usr/share/rpcd/acl.d/luci-app-embed.json"
install -D -m 0755 "$SRC/package/mtk/applications/c2000max-5g/files/luci-app-embed/etc/uci-defaults/80_embed_simcard" "$ROOT/etc/uci-defaults/80_embed_simcard"
install -D -m 0755 "$WEB/etc/init.d/ubus-at-daemon" "$ROOT/etc/init.d/ubus-at-daemon"
install -D -m 0755 "$WEB/usr/bin/ubus-at-daemon" "$ROOT/usr/bin/ubus-at-daemon"
install -D -m 0755 "$SRC/package/mtk/applications/c2000max-5g/files/luci-app-embed/etc/init.d/embed_simcard" "$ROOT/etc/init.d/embed_simcard"
install -D -m 0644 "$WEB/www/luci-static/resources/view/embed/htmlview.js" "$ROOT/www/luci-static/resources/view/embed/htmlview.js"
ln -sf ../init.d/mt5700-web "$ROOT/etc/rc.d/S97mt5700-web"
rm -f "$ROOT/etc/rc.d/S99ubus-at-daemon"
ln -sf ../init.d/ubus-at-daemon "$ROOT/etc/rc.d/S18ubus-at-daemon"
rm -f "$ROOT"/etc/rc.d/S??embed_simcard "$ROOT"/etc/rc.d/S??c2000ultra-sim-select "$ROOT"/etc/rc.d/S??qmodem_network
ln -sf ../init.d/c2000ultra-sim-select "$ROOT/etc/rc.d/S97c2000ultra-sim-select"
ln -sf ../init.d/qmodem_network "$ROOT/etc/rc.d/S98qmodem_network"
if [ ! -f "$ROOT/etc/config/fstab" ]; then
  printf "config global\n\toption anon_mount '0'\n\toption auto_mount '1'\n\toption delay_root '5'\n\toption check_fs '0'\n" > "$ROOT/etc/config/fstab"
fi
tar -cf "$ROOT/www/myos-ultra-recovery.tar" -C "$ROOT/usr/libexec" myos-ultra-gpt myos-ultra-ubenv -C "$SD" return-myos.sh recovery-README.txt
chmod 0644 "$ROOT/www/myos-ultra-recovery.tar"
test -x "$ROOT/usr/bin/ubus-at-daemon"
test -x "$ROOT/usr/bin/tom_modem"
test -x "$ROOT/usr/sbin/fdisk"
test -x "$ROOT/usr/sbin/blkid"
test -x "$ROOT/sbin/partprobe"
test -x "$ROOT/usr/sbin/mkfs.ext4"
test -x "$ROOT/usr/sbin/resize2fs"
test -x "$ROOT/sbin/block"
test -f "$ROOT/etc/config/fstab"
test -x "$ROOT/etc/init.d/c2000ultra-sim-select"
test -f "$ROOT/etc/config/cpesel"
grep -q "option gval '0-2,1-1,1-2'" "$ROOT/etc/config/cpesel"
test -x "$ROOT/etc/init.d/qmodem_network"
grep -q 'c2000ultra-sim-select select_sim' "$ROOT/www/luci-static/resources/view/embed/simcard.js"
grep -q 'blkid -p -D' "$ROOT/lib/preinit/79_c2000ultra_sd_extroot"
grep -q 'desired=.*disk_sectors - p7_start - 33' "$ROOT/lib/preinit/79_c2000ultra_sd_extroot"
grep -q '213119' "$ROOT/lib/preinit/79_c2000ultra_sd_extroot"
grep -q 'parse_pdp_addresses' "$ROOT/usr/share/qmodem/modem_dial.sh"
grep -q 'window.location.origin' "$ROOT/www/luci-static/resources/view/embed/htmlview.js"
grep -q "option use_ubus '1'" "$ROOT/etc/config/qmodem"
