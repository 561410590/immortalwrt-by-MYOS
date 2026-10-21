#
# Copyright (C) 2018-2019 Ycarus (Yannick Chabanois) <ycarus@zugaina.org>
#
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI Support for OpenMPTCProuter
LUCI_DEPENDS:=+luci-lib-json +openmptcprouter-api +openmptcprouter
PKG_VERSION:=2026.09.11
PKG_RELEASE:=1
PKG_LICENSE:=GPLv3
LUCI_MINIFY_CSS:=0

#include ../luci/luci.mk
include $(TOPDIR)/feeds/luci/luci.mk

define Package/luci-app-openmptcprouter/conffiles
/etc/sysctl.d/zzz_openmptcprouter.conf
endef

# call BuildPackage - OpenWrt buildroot signature
