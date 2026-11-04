include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-mt5700-web
PKG_VERSION:=1.2.0
PKG_RELEASE:=4
PKGARCH:=all
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-mt5700-web
  SECTION:=luci
  CATEGORY:=LuCI
  TITLE:=LuCI support for the MT5700 Web control panel
  DEPENDS:=+luci-base +mt5700-web-go
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-mt5700-web/install
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/mt5700-web
	$(INSTALL_DATA) ./htdocs/luci-static/resources/view/mt5700-web/status_v8.js $(1)/www/luci-static/resources/view/mt5700-web/status_v8.js
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/luci-app-mt5700-web.json $(1)/usr/share/luci/menu.d/luci-app-mt5700-web.json
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/luci-app-mt5700-web.json $(1)/usr/share/rpcd/acl.d/luci-app-mt5700-web.json
endef

$(eval $(call BuildPackage,luci-app-mt5700-web))
