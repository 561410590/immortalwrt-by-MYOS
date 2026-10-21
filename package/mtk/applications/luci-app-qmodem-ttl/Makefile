include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-qmodem-ttl
PKG_VERSION:=3.0.0
PKG_RELEASE:=1.202610151000

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-qmodem-ttl
  SECTION:=net
  CATEGORY:=Network
  TITLE:=luci-app-qmodem-ttl (MWRT 1143 ULTRA)
  DEPENDS:=+qmodem +luci-lua-runtime +firewall4 +nftables-json
  PKGARCH:=all
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-qmodem-ttl/install
	$(CP) ./files/* $(1)/
	chmod 0755 $(1)/etc/init.d/qmodem_ttl
endef

$(eval $(call BuildPackage,luci-app-qmodem-ttl))
