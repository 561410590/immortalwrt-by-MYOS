include $(TOPDIR)/rules.mk

PKG_NAME:=leigod-acc
PKG_VERSION:=2026.08.27
PKG_RELEASE:=1.202610151000

include $(INCLUDE_DIR)/package.mk

define Package/leigod-acc
  SECTION:=net
  CATEGORY:=Network
  TITLE:=leigod-acc (MWRT 1143 ULTRA)
  DEPENDS:=+libpcap +iptables-nft +kmod-ipt-nat +iptables-mod-tproxy +kmod-ipt-tproxy +kmod-netem +tc-full +kmod-ipt-ipset +ipset +kmod-tun +curl
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/leigod-acc/install
	$(CP) ./files/* $(1)/
	$(LN) /usr/sbin/leigod/acc-gw.router.arm64 $(1)/usr/sbin/leigod/acc_upgrade_monitor
	chmod 0755 $(1)/usr/sbin/leigod/leigod_uninstall.sh $(1)/usr/sbin/leigod/monitor.sh $(1)/usr/sbin/leigod/acc-gw.router.arm64 $(1)/etc/uci-defaults/99-leigod-acc $(1)/etc/init.d/acc $(1)/usr/sbin/leigod/acc.init
endef

$(eval $(call BuildPackage,leigod-acc))
