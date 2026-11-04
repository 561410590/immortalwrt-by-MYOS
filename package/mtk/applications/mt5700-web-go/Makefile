include $(TOPDIR)/rules.mk

PKG_NAME:=mt5700-web-go
PKG_VERSION:=3.0.3
PKG_RELEASE:=myos20261101
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/mt5700-web-go
  SECTION:=net
  CATEGORY:=Network
  TITLE:=MT5700 WebUI 3.0.3 with shared AT backend
  DEPENDS:=@aarch64 +libc +libgcc +qmodem +myos-ubus-at
endef

define Package/mt5700-web-go/description
 Upstream WebUI 3.0.3 with the inst R2 multi-modem service.
 Uses Ultra's existing serialized AT daemon.
endef

define Package/myos-ubus-at
  SECTION:=net
  CATEGORY:=Network
  TITLE:=MYOS shared ubus AT daemon
  DEPENDS:=+libc +libubus +libubox +libblobmsg-json +libjson-c
endef

myos-ubus-at_COMMANDS += echo 'Replaces: luci-app-5700webui' >> control;

define Package/myos-ubus-at/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/init.d
	$(INSTALL_BIN) ../5g-modem/luci-app-5700webui/root/usr/bin/ubus-at-daemon $(1)/usr/bin/ubus-at-daemon
	$(INSTALL_BIN) ../5g-modem/luci-app-5700webui/root/etc/init.d/ubus-at-daemon $(1)/etc/init.d/ubus-at-daemon
endef

define Package/luci-app-mt5700-web
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=MT5700 multi-modem control panel
  DEPENDS:=+mt5700-web-go +luci-base +rpcd-mod-file
  PKGARCH:=all
endef

define Package/mt5700-web-go/conffiles
/etc/config/mt5700-web
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/mt5700-web-go/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/usr/libexec/mt5700-web $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) ./vendor/usr/bin/mt5700-web-go $(1)/usr/bin/mt5700-web-go
	$(INSTALL_BIN) ./root/etc/init.d/mt5700-web $(1)/etc/init.d/mt5700-web
	$(INSTALL_BIN) ./root/usr/libexec/mt5700-web/* $(1)/usr/libexec/mt5700-web/
	$(INSTALL_CONF) ./root/etc/config/mt5700-web $(1)/etc/config/mt5700-web
	$(INSTALL_DIR) $(1)/www/5700
	$(INSTALL_DATA) ./root/www/5700/index.html $(1)/www/5700/index.html
	$(INSTALL_DIR) $(1)/www/mt5700-modern/assets
	$(INSTALL_DATA) ./root/www/mt5700-modern/index.html $(1)/www/mt5700-modern/index.html
	$(INSTALL_DATA) ./root/www/mt5700-modern/assets/* $(1)/www/mt5700-modern/assets/
endef

define Package/luci-app-mt5700-web/install
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/*.json $(1)/usr/share/luci/menu.d/
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/*.json $(1)/usr/share/rpcd/acl.d/
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/mt5700-web
	$(INSTALL_DATA) ./root/www/luci-static/resources/view/mt5700-web/*.js $(1)/www/luci-static/resources/view/mt5700-web/
endef

$(eval $(call BuildPackage,mt5700-web-go))
$(eval $(call BuildPackage,luci-app-mt5700-web))
$(eval $(call BuildPackage,myos-ubus-at))
