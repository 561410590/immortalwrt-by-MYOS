include $(TOPDIR)/rules.mk

LUCI_TITLE:=MWRT original custom theme and device homepage
LUCI_DEPENDS:=+luci-base +luci-compat +luci-lib-jsonc +jq +bc +qmodem +curl +jsonfilter +rpcd-mod-ucode +fancontrol
LUCI_MINIFY_LUA:=0
LUCI_MINIFY_CSS:=0
LUCI_MINIFY_JS:=0
PKG_LICENSE:=Apache-2.0
PKG_VERSION:=2.3.1
PKG_RELEASE:=1148.202610211000

PKG_FILE_MODES:= \
	$(shell find $(CURDIR)/root -type d -printf '/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/root -type f -printf '/%P:root:root:0644 ') \
	/etc/uci-defaults/96-mwrt-theme:root:root:0755 \
	/usr/bin/mwrt-cellinfo:root:root:0755 \
	/usr/libexec/argon/online_wallpaper:root:root:0755 \
	/usr/share/modem/simstat.sh:root:root:0755 \
	/usr/share/mwrt-qmodem/modem_ctrl.sh:root:root:0755 \
	/www/cgi-bin/router3d-data:root:root:0755 \
	/usr/libexec/rpcd/mwrt-wizard:root:root:0755 \
	/usr/bin/mwrt-gen-translations:root:root:0755 \
	/usr/bin/mwrt-cachebust:root:root:0755 \
	/etc/init.d/mwrt-fansched:root:root:0755 \
	/etc/uci-defaults/99f-mwrt-fansched:root:root:0755 \
	/etc/uci-defaults/99g-mwrt-cellinfo:root:root:0755 \
	/etc/uci-defaults/99i-mwrt-luci-perf:root:root:0755 \
	/etc/uci-defaults/99j-mwrt-wizard:root:root:0755 \
	/usr/bin/install-wizard.sh:root:root:0755 \
	/usr/bin/install-mwrt-global.sh:root:root:0755 \
	/usr/bin/install-luci-perf.sh:root:root:0755 \
	/usr/libexec/rpcd/luci.mwrt-notice:root:root:0755 \
	/usr/bin/mwrt-fansched:root:root:0755 \
	/usr/libexec/rpcd/luci.mwrt-ota:root:root:0755 \
	/etc/uci-defaults/99k-mwrt-global:root:root:0755 \
	/usr/bin/mwrt_wan_failover.sh:root:root:0755 \
	/etc/init.d/mwrt-assurance:root:root:0755 \
	/usr/bin/mwrt-bootswitch:root:root:0755

define Build/Prepare/luci-theme-mwrt
	find $(PKG_BUILD_DIR)/root -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR)/root -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/uci-defaults/96-mwrt-theme \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt-cellinfo \
		$(PKG_BUILD_DIR)/root/usr/libexec/argon/online_wallpaper \
		$(PKG_BUILD_DIR)/root/usr/share/modem/simstat.sh \
		$(PKG_BUILD_DIR)/root/usr/share/mwrt-qmodem/modem_ctrl.sh \
		$(PKG_BUILD_DIR)/root/www/cgi-bin/router3d-data \
		$(PKG_BUILD_DIR)/root/usr/libexec/rpcd/mwrt-wizard \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt-gen-translations \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt-cachebust \
		$(PKG_BUILD_DIR)/root/etc/init.d/mwrt-fansched \
		$(PKG_BUILD_DIR)/root/etc/uci-defaults/99f-mwrt-fansched \
		$(PKG_BUILD_DIR)/root/etc/uci-defaults/99g-mwrt-cellinfo \
		$(PKG_BUILD_DIR)/root/etc/uci-defaults/99i-mwrt-luci-perf \
		$(PKG_BUILD_DIR)/root/etc/uci-defaults/99j-mwrt-wizard \
		$(PKG_BUILD_DIR)/root/usr/bin/install-wizard.sh \
		$(PKG_BUILD_DIR)/root/usr/bin/install-mwrt-global.sh \
		$(PKG_BUILD_DIR)/root/usr/bin/install-luci-perf.sh \
		$(PKG_BUILD_DIR)/root/usr/libexec/rpcd/luci.mwrt-notice \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt-fansched \
		$(PKG_BUILD_DIR)/root/usr/libexec/rpcd/luci.mwrt-ota \
		$(PKG_BUILD_DIR)/root/etc/uci-defaults/99k-mwrt-global \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt_wan_failover.sh \
		$(PKG_BUILD_DIR)/root/etc/init.d/mwrt-assurance \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt-bootswitch
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot package scan signature
