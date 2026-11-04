include $(TOPDIR)/rules.mk

PKG_NAME:=c2000max-5g
PKG_VERSION:=20260303
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk

define Build/Compile
endef

define Package/quectel-CM-5G-M
  SECTION:=net
  CATEGORY:=Network
  TITLE:=NRadio C2000-MAX Quectel 5G connection manager
  DEPENDS:=+libpthread +libjson-c
endef

define Package/quectel-CM-5G-M/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/rootfs/usr/bin/quectel-CM-M $(1)/usr/bin/
endef

define Package/qmodem
  SECTION:=net
  CATEGORY:=Network
  TITLE:=NRadio C2000-MAX modem manager scripts
  DEPENDS:=+quectel-CM-5G-M +tom_modem +kmod-usb-net-qmi-wwan +kmod-usb-serial-option +kmod-usb-wdm
endef

define Package/qmodem/install
	$(CP) ./files/rootfs/. $(1)/
	$(INSTALL_DIR) $(1)/etc $(1)/etc/init.d $(1)/etc/uci-defaults $(1)/etc/config \
		$(1)/etc/hotplug.d/iface $(1)/etc/hotplug.d/net $(1)/etc/hotplug.d/usb \
		$(1)/usr/bin $(1)/usr/libexec/rpcd $(1)/usr/share/rpcd/acl.d \
		$(1)/usr/share/qmodem/vendor $(1)/usr/share/qmodem/led_scripts \
		$(1)/usr/lib/lua/luci/view/admin_status/index
	$(INSTALL_BIN) ./files/rootfs/etc/init.d/* $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/rootfs/etc/uci-defaults/* $(1)/etc/uci-defaults/
	$(INSTALL_BIN) ./files/rootfs/usr/libexec/rpcd/* $(1)/usr/libexec/rpcd/
	$(INSTALL_BIN) ./files/rootfs/usr/share/qmodem/*.sh $(1)/usr/share/qmodem/
	$(INSTALL_BIN) ./files/rootfs/usr/share/qmodem/vendor/*.sh $(1)/usr/share/qmodem/vendor/
	$(INSTALL_BIN) ./files/rootfs/usr/share/qmodem/led_scripts/*.sh $(1)/usr/share/qmodem/led_scripts/
	$(INSTALL_BIN) ./files/rootfs/etc/hotplug.d/iface/* $(1)/etc/hotplug.d/iface/
	$(INSTALL_BIN) ./files/rootfs/etc/hotplug.d/net/* $(1)/etc/hotplug.d/net/
	$(INSTALL_BIN) ./files/rootfs/etc/hotplug.d/usb/* $(1)/etc/hotplug.d/usb/
	$(INSTALL_DATA) ./files/rootfs/usr/share/qmodem/*.json $(1)/usr/share/qmodem/
	$(INSTALL_DATA) ./files/rootfs/usr/share/qmodem/vendor/*.json $(1)/usr/share/qmodem/vendor/
	$(INSTALL_DATA) ./files/rootfs/usr/share/rpcd/acl.d/*.json $(1)/usr/share/rpcd/acl.d/
	$(INSTALL_DATA) ./files/rootfs/usr/lib/lua/luci/view/admin_status/index/*.htm $(1)/usr/lib/lua/luci/view/admin_status/index/
	rm -rf $(1)/lib/modules
	rm -f $(1)/usr/bin/quectel-CM-M
	rm -f $(1)/bin/is-opkg
endef

define Package/tom_modem
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=QModem communication tool from C2000 MAX
  DEPENDS:=+libubus +libubox +libjson-c +libblobmsg-json
endef

define Package/tom_modem/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/tom_modem/usr/bin/tom_modem $(1)/usr/bin/
endef

define Package/sms-forwarder-next
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=QModem SMS forwarding service from C2000 MAX
  DEPENDS:=+tom_modem +jq +curl +ucode +ucode-mod-ubus +ucode-mod-uci
endef

define Package/sms-forwarder-next/conffiles
/etc/config/sms_forwarder
endef

define Package/sms-forwarder-next/install
	$(CP) ./files/sms-forwarder-next/. $(1)/
	$(INSTALL_BIN) ./files/sms-forwarder-next/etc/init.d/sms_forwarder $(1)/etc/init.d/sms_forwarder
endef

define Package/luci-app-qmodem-next
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI Modern UI for QModem from C2000 MAX
  DEPENDS:=+qmodem +luci-base +sms-forwarder-next
endef

define Package/luci-app-qmodem-next/install
	$(CP) ./files/luci-app-qmodem-next/. $(1)/
	chmod 0644 \
		$(1)/etc/qmodem/.gitkeep \
		$(1)/lib/upgrade/keep.d/qmodme-next \
		$(1)/usr/share/luci/menu.d/luci-app-qmodem-next.json \
		$(1)/usr/share/rpcd/acl.d/luci-app-qmodem-next.json \
		$(1)/www/luci-static/resources/qmodem/* \
		$(1)/www/luci-static/resources/view/qmodem/* \
		$(1)/www/luci-static/resources/view/status/include/11_modem.js
	chmod 0755 $(1)/usr/share/rpcd/ucode/qmodem_sms
endef

define Package/luci-i18n-qmodem-next-zh-cn
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=4. Translations
  TITLE:=Chinese translation for luci-app-qmodem-next from C2000 MAX
  DEPENDS:=+luci-app-qmodem-next
endef

define Package/luci-i18n-qmodem-next-zh-cn/install
	$(CP) ./files/luci-i18n-qmodem-next-zh-cn/. $(1)/
	chmod 0755 $(1)/etc/uci-defaults/luci-i18n-qmodem-next-zh-cn
	chmod 0644 $(1)/usr/lib/lua/luci/i18n/luci-app-qmodem-next.zh-cn.lmo
endef

define Package/luci-app-embed
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI Embedded Page opener
  VERSION:=1.0.0-r2
  PKGARCH:=all
  DEPENDS:=+luci-base +tom_modem
endef

define Package/luci-app-embed/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/etc/uci-defaults \
		$(1)/usr/share/luci/menu.d $(1)/usr/share/rpcd/acl.d \
		$(1)/www/luci-static/resources/view/embed
	$(INSTALL_DATA) ./files/luci-app-embed/etc/config/embed_simcard $(1)/etc/config/
	$(INSTALL_BIN) ./files/luci-app-embed/etc/init.d/embed_simcard $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/luci-app-embed/etc/uci-defaults/80_embed_simcard $(1)/etc/uci-defaults/
	$(INSTALL_DATA) ./files/luci-app-embed/usr/share/luci/menu.d/luci-app-embed.json $(1)/usr/share/luci/menu.d/
	$(INSTALL_DATA) ./files/luci-app-embed/usr/share/rpcd/acl.d/luci-app-embed.json $(1)/usr/share/rpcd/acl.d/
	$(INSTALL_DATA) ./files/luci-app-embed/www/luci-static/resources/view/embed/htmlview.js \
		./files/luci-app-embed/www/luci-static/resources/view/embed/simcard.js \
		$(1)/www/luci-static/resources/view/embed/
endef

$(eval $(call BuildPackage,quectel-CM-5G-M))
$(eval $(call BuildPackage,tom_modem))
$(eval $(call BuildPackage,sms-forwarder-next))
$(eval $(call BuildPackage,qmodem))
$(eval $(call BuildPackage,luci-app-qmodem-next))
$(eval $(call BuildPackage,luci-i18n-qmodem-next-zh-cn))
$(eval $(call BuildPackage,luci-app-embed))
