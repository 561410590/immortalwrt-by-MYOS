include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for Easy QoS
LUCI_DEPENDS:=+wget-ssl +tc +kmod-sched-core +kmod-ifb +ebtables-legacy-utils +ebtables-legacy  @!PACKAGE_luci-app-eqos

PKG_MAINTAINER:=Jianhui Zhao <jianhuizhao329@gmail.com>
PKG_NAME:=luci-app-eqos-mtk
PKG_RELEASE:=202610151000

define Build/Prepare/luci-app-eqos-mtk
	$(INSTALL_BIN) ./root/etc/init.d/eqos ./root/etc/init.d/dhcp_mark.sh $(PKG_BUILD_DIR)/root/etc/init.d/
	$(INSTALL_BIN) ./root/usr/sbin/eqos ./root/usr/sbin/loadbalance $(PKG_BUILD_DIR)/root/usr/sbin/
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
