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

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
