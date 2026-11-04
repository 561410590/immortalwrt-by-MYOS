# SPDX-Identifier-License: GPL-3.0-only
#
# Copyright (C) 2018 Lean <coolsnowwolf@gmail.com>
# Copyright (C) 2019-2022 ImmortalWrt.org

include $(TOPDIR)/rules.mk

LUCI_NAME:=luci-app-turboacc
PKG_VERSION:=1.4
PKG_RELEASE:=4
PKG_LICENSE:=GPL-3.0-only

LUCI_TITLE:=LuCI support for Turbo ACC
LUCI_DEPENDS:= \
	+luci-compat +kmod-tcp-bbr +kmod-nft-offload +luci-lua-runtime
LUCI_PKGARCH:=all

define Build/Prepare/luci-app-turboacc
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/www/luci-static/resources/view
	$(INSTALL_DATA) ./root/www/luci-static/resources/view/turboacc.js $(PKG_BUILD_DIR)/root/www/luci-static/resources/view/turboacc.js
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/usr/libexec/rpcd
	$(INSTALL_BIN) ./root/usr/libexec/rpcd/luci.turboacc $(PKG_BUILD_DIR)/root/usr/libexec/rpcd/luci.turboacc
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/usr/share/luci/menu.d
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/luci-app-turboacc.json $(PKG_BUILD_DIR)/root/usr/share/luci/menu.d/luci-app-turboacc.json
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/luci-app-turboacc.json $(PKG_BUILD_DIR)/root/usr/share/rpcd/acl.d/luci-app-turboacc.json
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/etc/init.d
	$(INSTALL_BIN) ./root/etc/init.d/turboacc $(PKG_BUILD_DIR)/root/etc/init.d/turboacc
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/etc/config
	$(INSTALL_CONF) ./root/etc/config/turboacc $(PKG_BUILD_DIR)/root/etc/config/turboacc
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/etc/uci-defaults
	$(INSTALL_BIN) ./root/etc/uci-defaults/luci-turboacc $(PKG_BUILD_DIR)/root/etc/uci-defaults/luci-turboacc
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
