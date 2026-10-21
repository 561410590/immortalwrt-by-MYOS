#
# Copyright (C) 2018-2019 Ycarus (Yannick Chabanois) <ycarus@zugaina.org>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=openmptcprouter
PKG_VERSION:=0.95
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk

MY_DEPENDS := \
    omr-tracker

define Package/$(PKG_NAME)
SECTION:=OMR
CATEGORY:=OpenMPTCProuter
DEPENDS:=$(foreach p,$(MY_DEPENDS),+$(p))
TITLE:=OpenMPTCProuter config
endef

define Package/$(PKG_NAME)/description
OpenMPTCProuter package
endef

define Package/$(PKG_NAME)/conffiles
/etc/config/$(PKG_NAME)
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/hotplug.d/iface \
		$(1)/etc/uci-defaults $(1)/usr/lib/openmptcprouter $(1)/bin
	$(INSTALL_BIN) ./files/etc/init.d/openmptcprouter-vps $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/hotplug.d/iface/omrvpn $(1)/etc/hotplug.d/iface/
	$(INSTALL_BIN) ./files/etc/firewall.omr-server $(1)/etc/
	$(INSTALL_BIN) ./files/bin/omr-tracebox ./files/bin/omr-tracebox-json \
		./files/bin/omr-mtu ./files/bin/omr-mtu6 ./files/bin/tcpping $(1)/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
