include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-n5000-system
PKG_VERSION:=1.0
PKG_RELEASE:=7
PKGARCH:=all
PKG_LICENSE:=Apache-2.0

include $(INCLUDE_DIR)/package.mk

MYOS_SOURCE:=$(TOPDIR)/package/mtk/applications/luci-theme-c2000max-ui
MWRT_SOURCE:=$(TOPDIR)/package/mtk/applications/luci-theme-mwrt/root
MYOS_EXEC:=etc/uci-defaults/94-myos-factory-wifi lib/functions/myos-factory-wifi.sh lib/preinit/82_myos_factory_wifi etc/init.d/myos-factory-wifi
MYOS_DATA:=
MWRT_EXEC:=usr/libexec/mwrt-usbinfo usr/libexec/rpcd/luci.mwrt-notice usr/libexec/rpcd/luci.mwrt-ota
MWRT_DATA:=etc/config/mwrt etc/mwrt/ota-ca.pem usr/lib/lua/luci/model/mwrt_gate.lua usr/lib/lua/luci/controller/mwrt_hwinfo.lua usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua usr/share/rpcd/acl.d/luci-app-mwrt-notice.json usr/share/rpcd/acl.d/luci-app-mwrt-ota.json
USB_NETWORK_DATA:=usr/share/luci/menu.d/myos-usb-network.json usr/share/rpcd/acl.d/myos-usb-network.json www/luci-static/resources/view/myos/usb-network.js

define Package/luci-app-n5000-system
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=Applications
  TITLE:=N5000 wireless defaults, update, fan, notifications and USB networking
  DEPENDS:=+luci-base +luci-mod-network +luci-compat +luci-lib-jsonc +rpcd-mod-luci +rpcd-mod-ucode +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci +uclient-fetch +ca-bundle +curl +jsonfilter +jq +jshn +bc +fancontrol +libuci-lua +luci-lib-nixio +libubus-lua +uhttpd +libustream-openssl +openssl-util
endef

define Package/luci-app-n5000-system/conffiles
/etc/config/mwrt
endef

define Build/Compile
endef

define Package/luci-app-n5000-system/install
	$(foreach f,$(MYOS_EXEC),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_BIN) $(MYOS_SOURCE)/root/$(f) $(1)/$(f);)
	$(foreach f,$(MYOS_DATA),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_DATA) $(MYOS_SOURCE)/root/$(f) $(1)/$(f);)
	$(foreach f,$(MWRT_EXEC),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_BIN) $(MWRT_SOURCE)/$(f) $(1)/$(f);)
	$(foreach f,$(MWRT_DATA),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_DATA) $(MWRT_SOURCE)/$(f) $(1)/$(f);)
	$(foreach f,$(USB_NETWORK_DATA),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_DATA) ./root/$(f) $(1)/$(f);)
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/zzz-n5000-vantage.json $(1)/usr/share/luci/menu.d/
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./root/usr/libexec/rpcd/myos.usb-network $(1)/usr/libexec/rpcd/
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools $(1)/www/luci-static/resources/view/myos
	$(INSTALL_DATA) $(MYOS_SOURCE)/htdocs/luci-static/resources/tools/myos-wifi.js $(MYOS_SOURCE)/htdocs/luci-static/resources/tools/myos-wifi.css $(1)/www/luci-static/resources/tools/
	$(INSTALL_BIN) ./95-myos-n5000 $(1)/etc/uci-defaults/95-myos-n5000
endef

$(eval $(call BuildPackage,luci-app-n5000-system))
