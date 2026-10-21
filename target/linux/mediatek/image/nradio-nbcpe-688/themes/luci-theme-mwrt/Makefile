include $(TOPDIR)/rules.mk

LUCI_TITLE:=MWRT NBCPE-688 theme and device homepage
LUCI_DEPENDS:=+luci-base +luci-compat +luci-lib-jsonc +jq +bc
LUCI_MINIFY_LUA:=0
LUCI_MINIFY_CSS:=0
LUCI_MINIFY_JS:=0
PKG_LICENSE:=Apache-2.0
PKG_VERSION:=2.3.1
PKG_RELEASE:=2

PKG_FILE_MODES:= \
	$(shell find $(CURDIR)/root -type d -printf '/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/root -type f -printf '/%P:root:root:0644 ') \
	/etc/uci-defaults/96-mwrt-theme:root:root:0755 \
	/usr/bin/mwrt-cellinfo:root:root:0755 \
	/usr/libexec/argon/online_wallpaper:root:root:0755 \
	/usr/share/modem/simstat.sh:root:root:0755 \
	/usr/share/mwrt-qmodem/modem_ctrl.sh:root:root:0755 \
	/www/cgi-bin/router3d-data:root:root:0755

define Build/Prepare/luci-theme-mwrt
	find $(PKG_BUILD_DIR)/root -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR)/root -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/uci-defaults/96-mwrt-theme \
		$(PKG_BUILD_DIR)/root/usr/bin/mwrt-cellinfo \
		$(PKG_BUILD_DIR)/root/usr/libexec/argon/online_wallpaper \
		$(PKG_BUILD_DIR)/root/usr/share/modem/simstat.sh \
		$(PKG_BUILD_DIR)/root/usr/share/mwrt-qmodem/modem_ctrl.sh \
		$(PKG_BUILD_DIR)/root/www/cgi-bin/router3d-data
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot package scan signature
