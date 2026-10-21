include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-leigod-acc
PKG_VERSION:=2
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-leigod-acc
  SECTION:=net
  CATEGORY:=Network
  TITLE:=luci-app-leigod-acc (MWRT 1143 ULTRA)
  DEPENDS:=+leigod-acc +jq +curl +luci-lua-runtime
  PKGARCH:=all
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-leigod-acc/install
	$(CP) ./files/* $(1)/
	chmod 0755 $(1)/usr/libexec/leigod-pause $(1)/etc/uci-defaults/99-luci-leigod
endef

$(eval $(call BuildPackage,luci-app-leigod-acc))
