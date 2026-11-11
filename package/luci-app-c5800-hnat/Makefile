include $(TOPDIR)/rules.mk

LUCI_TITLE:=MWRT TurboACC for NRadio MediaTek HNAT
LUCI_DEPENDS:=+luci-base +luci-compat +luci-lib-jsonc
LUCI_PKGARCH:=all
LUCI_MINIFY_JS:=0
LUCI_MINIFY_LUA:=0
PKG_VERSION:=1.0
PKG_RELEASE:=202611112200
PKG_LICENSE:=GPL-3.0-only

PKG_FILE_MODES:= \
	$(shell find $(CURDIR)/root -type d -printf '/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/root -type f -printf '/%P:root:root:0644 ') \
	/etc/init.d/turboacc:root:root:0755 \
	/usr/libexec/rpcd/luci.turboacc:root:root:0755

define Build/Prepare/luci-app-c5800-hnat
	find $(PKG_BUILD_DIR)/root -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR)/root -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/turboacc $(PKG_BUILD_DIR)/root/usr/libexec/rpcd/luci.turboacc
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot package scan signature
