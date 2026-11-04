#!/bin/sh
set -eu
ROOT=$1
SRC=$2
VERSION=$3
REVISION=$4
ID_REVISION=$(printf '%s' "$REVISION" | tr '+' '_')
SD=$SRC/target/linux/mediatek/image/nradio-n5000/sd-tools
GPT_HELPER=${5:-"$SD/myos-n5000-gpt"}
SIM=$SRC/target/linux/mediatek/image/nradio-n5000
# Package caches retain base-files between timestamp-only builds. Stamp the
# final ROM from the same version argument used for firmware metadata.
sed -i "s/^DISTRIB_RELEASE=.*/DISTRIB_RELEASE='$VERSION'/; s/^DISTRIB_REVISION=.*/DISTRIB_REVISION='$REVISION'/; s/^DISTRIB_DESCRIPTION=.*/DISTRIB_DESCRIPTION='MYOS by ImmortalWrt $VERSION'/" "$ROOT/etc/openwrt_release"
printf '%s\n' "$VERSION" > "$ROOT/etc/openwrt_version"
sed -i "s/^VERSION=.*/VERSION=\"$VERSION\"/; s/^VERSION_ID=.*/VERSION_ID=\"$VERSION\"/; s/^PRETTY_NAME=.*/PRETTY_NAME=\"MYOS by ImmortalWrt $VERSION\"/; s/^OPENWRT_RELEASE=.*/OPENWRT_RELEASE=\"MYOS by ImmortalWrt $VERSION\"/; s/^BUILD_ID=.*/BUILD_ID=\"$REVISION\"/" "$ROOT/usr/lib/os-release"
sed -i "s#^[[:space:]]*option mediaurlbase .*#    option mediaurlbase '/luci-static/argon'#" "$ROOT/etc/config/luci"
printf '\nMYOS by ImmortalWrt %s\n\n' "$VERSION" > "$ROOT/etc/banner"
install -D -m 0755 "$GPT_HELPER" "$ROOT/usr/libexec/myos-n5000-gpt"
install -D -m 0755 "$SD/ubenv-snsd" "$ROOT/usr/libexec/myos-n5000-ubenv"
install -D -m 0644 "$SIM/n5000-upgrade.platform" "$ROOT/lib/upgrade/platform.sh"
install -D -m 0644 "$SRC/package/base-files/files/lib/upgrade/emmc.sh" "$ROOT/lib/upgrade/emmc.sh"
install -D -m 0644 "$SIM/web/luci.locations" "$ROOT/etc/nginx/conf.d/luci.locations"
install -D -m 0644 "$SIM/web/luci-cgi_io.ini" "$ROOT/etc/uwsgi/vassals/luci-cgi_io.ini"
install -D -m 0755 "$SIM/n5000-board-defaults.uci-default" "$ROOT/etc/uci-defaults/36-n5000-board-defaults"
# Copy the reviewed mobile navigation even when the theme package cache is reused.
install -D -m 0644 "$SRC/package/mtk/applications/luci-theme-mwrt/root/usr/lib/lua/luci/view/themes/argon/footer.htm" "$ROOT/usr/lib/lua/luci/view/themes/argon/footer.htm"
install -D -m 0755 "$SD/myos-n5000-boot" "$ROOT/usr/libexec/myos-n5000-boot"
install -D -m 0755 "$SD/n5000-sd-extroot.preinit" "$ROOT/lib/preinit/79_n5000_sd_extroot"
install -D -m 0755 "$SD/n5000-firstboot" "$ROOT/sbin/firstboot"
install -D -m 0644 "$SD/n5000_sd.js" "$ROOT/www/luci-static/resources/view/myos/n5000_sd.js"
install -D -m 0644 "$SD/myos-n5000-sd.uc" "$ROOT/usr/share/rpcd/ucode/myos-n5000-sd"
install -D -m 0644 "$SD/myos-n5000-sd-menu.json" "$ROOT/usr/share/luci/menu.d/myos-n5000-sd.json"
install -D -m 0644 "$SD/myos-n5000-sd-acl.json" "$ROOT/usr/share/rpcd/acl.d/myos-n5000-sd.json"
install -D -m 0755 "$SD/return-myos.sh" "$ROOT/usr/share/myos-n5000-sd/return-myos.sh"
install -D -m 0644 "$SD/recovery-README.txt" "$ROOT/usr/share/myos-n5000-sd/recovery-README.txt"
printf '%s-%s\n' "$VERSION" "$ID_REVISION" > "$ROOT/etc/n5000-rootfs-id"
if [ ! -f "$ROOT/etc/config/fstab" ]; then
  printf "config global\n\toption anon_mount '0'\n\toption auto_mount '1'\n\toption delay_root '5'\n\toption check_fs '0'\n" > "$ROOT/etc/config/fstab"
fi
tar -cf "$ROOT/www/myos-n5000-recovery.tar" -C "$ROOT/usr/libexec" myos-n5000-gpt myos-n5000-ubenv -C "$SD" return-myos.sh recovery-README.txt
chmod 0644 "$ROOT/www/myos-n5000-recovery.tar"
