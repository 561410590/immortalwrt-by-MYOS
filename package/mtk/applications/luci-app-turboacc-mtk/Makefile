# SPDX-Identifier-License: GPL-3.0-only
#
# Copyright (C) 2018 Lean <coolsnowwolf@gmail.com>
# Copyright (C) 2019-2022 ImmortalWrt.org

include $(TOPDIR)/rules.mk

LUCI_NAME:=luci-app-turboacc
PKG_VERSION:=1.4
PKG_RELEASE:=6.20261030.n5000
PKG_LICENSE:=GPL-3.0-only

LUCI_TITLE:=LuCI support for original MTK TurboACC
LUCI_DEPENDS:= \
	+luci-compat +kmod-tcp-bbr +kmod-nft-offload +luci-lua-runtime
LUCI_PKGARCH:=all

define Build/Prepare/luci-app-turboacc
	find $(PKG_BUILD_DIR)/root $(PKG_BUILD_DIR)/htdocs -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR)/root $(PKG_BUILD_DIR)/htdocs -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/turboacc \
		$(PKG_BUILD_DIR)/root/etc/uci-defaults/turboacc \
		$(PKG_BUILD_DIR)/root/usr/libexec/rpcd/luci.turboacc
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
