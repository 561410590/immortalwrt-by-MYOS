include $(TOPDIR)/rules.mk

PKG_NAME:=c2000max-5g
PKG_VERSION:=20260303
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk

QMODEM_ROOT:=./files/rootfs
QMODEM_INIT:=etc/init.d/qmodem_init etc/init.d/qmodem_network etc/init.d/qmodem_reboot
QMODEM_TOOLS:=$(addprefix usr/share/qmodem/,modem_scan.sh modem_hook.sh modem_ctrl.sh modem_dial.sh led_scripts/m60k63.sh)
QMODEM_RPC:=usr/libexec/rpcd/modem_ctrl usr/libexec/rpcd/qmodem
QMODEM_DATA:=etc/hotplug.d/iface/80-wwan-led etc/hotplug.d/net/20-modem-net etc/hotplug.d/usb/20-modem-usb \
	usr/share/qmodem/generic.sh usr/share/qmodem/modem_util.sh

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
	$(foreach f,$(QMODEM_INIT) $(QMODEM_TOOLS) $(QMODEM_RPC),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_BIN) $(QMODEM_ROOT)/$(f) $(1)/$(f);)
	$(foreach f,$(QMODEM_DATA),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_DATA) $(QMODEM_ROOT)/$(f) $(1)/$(f);)
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/uci-defaults $(1)/usr/share/qmodem/vendor $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(if $(CONFIG_TARGET_mediatek_filogic_DEVICE_nradio_n5000),./files/qmodem.n5000,$(QMODEM_ROOT)/etc/config/qmodem) $(1)/etc/config/qmodem
	$(if $(CONFIG_TARGET_mediatek_filogic_DEVICE_nradio_n5000),:,$(INSTALL_BIN) $(QMODEM_ROOT)/etc/init.d/pcie_mhi $(1)/etc/init.d/pcie_mhi)
	$(INSTALL_DATA) $(QMODEM_ROOT)/etc/uci-defaults/* $(1)/etc/uci-defaults/
	$(INSTALL_DATA) $(QMODEM_ROOT)/usr/share/qmodem/*.json $(1)/usr/share/qmodem/
	$(INSTALL_DATA) $(QMODEM_ROOT)/usr/share/qmodem/vendor/* $(1)/usr/share/qmodem/vendor/
	$(INSTALL_DATA) $(QMODEM_ROOT)/usr/share/rpcd/acl.d/*.json $(1)/usr/share/rpcd/acl.d/
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
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/config $(1)/usr/bin $(1)/usr/share/sms_forwarder
	$(INSTALL_BIN) ./files/sms-forwarder-next/etc/init.d/sms_forwarder $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/sms-forwarder-next/usr/bin/sms_forwarder_next $(1)/usr/bin/
	$(INSTALL_BIN) ./files/sms-forwarder-next/usr/share/sms_forwarder/*.sh $(1)/usr/share/sms_forwarder/
	$(INSTALL_DATA) ./files/sms-forwarder-next/etc/config/sms_forwarder $(1)/etc/config/
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
