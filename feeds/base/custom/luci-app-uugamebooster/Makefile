# Copyright (C) 2021 Openwrt.org
#
# This is free software, licensed under the Apache License, Version 2.0 .
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for UUgamebooster
LUCI_DEPENDS:=+luci-compat +curl +ca-bundle +firewall4 +flock +kmod-tun +ip-full \
	+kmod-nf-conntrack-netlink +nftables-json +kmod-nft-core \
	+kmod-nft-nat +kmod-nft-compat +iptables-nft +ip6tables-nft \
	+ipset +kmod-ipt-core +kmod-ipt-ipset +kmod-ipt-nat \
	+kmod-ipt-nat-extra +kmod-nft-tproxy \
	+kmod-nft-socket +iptables-mod-tproxy +iptables-mod-socket
LUCI_PKGARCH:=all

PKG_NAME:=luci-app-uugamebooster
PKG_VERSION:=1.0
PKG_RELEASE:=19.myos1700

define Package/luci-app-uugamebooster/postinst
#!/bin/sh
[ -z "$$IPKG_INSTROOT" ] || exit 0
/etc/init.d/uuplugin enable
if [ "$$(uci -q get uuplugin.uuplugin.enabled)" = 1 ]; then
 /etc/init.d/uuplugin restart
fi
exit 0
endef

define Package/luci-app-uugamebooster/prerm
#!/bin/sh
[ -z "$$IPKG_INSTROOT" ] || exit 0
/etc/init.d/uuplugin stop
/usr/libexec/uuplugin-launcher --cleanup-only || logger -t uuplugin '清理出现错误，请查看系统日志'
exit 0
endef

include $(TOPDIR)/feeds/luci/luci.mk
# call BuildPackage - OpenWrt buildroot signature
