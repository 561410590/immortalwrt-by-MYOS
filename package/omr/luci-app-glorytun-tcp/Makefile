#
# Copyright (C) 2018-2020 Ycarus (Yannick Chabanois) <ycarus@zugaina.org>
#
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI Interface to Glorytun TCP
LUCI_DEPENDS:=+glorytun

PKG_VERSION:=0
PKG_RELEASE:=2
PKG_LICENSE:=GPLv3

#include ../luci/luci.mk
include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
