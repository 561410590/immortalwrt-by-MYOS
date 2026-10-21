#
# Copyright (C) 2014-2014 OpenWrt.org
# Copyright (C) 2018-2019 Ycarus (Yannick Chabanois) <ycarus@zugaina.org>
#
# This is free software, licensed under the GNU General Public License v3 or later.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mptcp
PKG_VERSION:=6.1
PKG_RELEASE:=2

PKG_MAINTAINER:=Ycarus (Yannick Chabanois) <ycarus@zugaina.org>
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
PKG_FILE_DEPENDS:=$(TOPDIR)/feeds/packages/net/unbound/files/iptools.sh

include $(INCLUDE_DIR)/package.mk

define Package/mptcp
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Multipath TCP configuration scripts
  DEPENDS:=+ip-full +iptables-nft +ethtool +ipcalc +jsonfilter +kmod-inet-mptcp-diag
  KCONFIG:=\
      CONFIG_MPTCP=y \
      CONFIG_MPTCP_IPV6=y \
      CONFIG_INET_MPTCP_DIAG=m

  PKGARCH:=all
endef

define Package/mptcp/description
  MultiPath TCP (MPTCP) is an effort towards enabling the simultaneous use of
  several IP-addresses/interfaces by a modification of TCP that presents a regular
  TCP interface to applications, while in fact spreading data across several
  subflows. Benefits of this include better resource utilization, better throughput
  and smoother reaction to failures.
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/mptcp/install
	$(INSTALL_DIR) $(1)
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/lib/unbound
	$(INSTALL_DATA) $(TOPDIR)/feeds/packages/net/unbound/files/iptools.sh $(1)/usr/lib/unbound/iptools.sh
	chmod 0755 $(1)/usr/bin/multipath $(1)/etc/init.d/mptcp $(1)/etc/hotplug.d/iface/30-mptcp $(1)/etc/uci-defaults/mptcp-defaults
endef

$(eval $(call BuildPackage,mptcp))
