include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-myos-system
PKG_VERSION:=1.0.0
PKG_RELEASE:=2
PKG_LICENSE:=Apache-2.0
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

# Shared services are installed independently of the retired theme packages.
MYOS_LEGACY:=$(TOPDIR)/package/luci-theme-c2000max-ui
MWRT_LEGACY:=$(TOPDIR)/package/luci-theme-mwrt/root

define Package/luci-app-myos-system
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=Applications
  TITLE:=MYOS system integration, setup and notifications
  DEPENDS:=+luci-base +luci-compat +rpcd-mod-luci +curl +jsonfilter +uclient-fetch +ca-bundle +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci
  PKGARCH:=all
endef

define Package/luci-app-myos-system/conffiles
endef

define Build/Compile
endef

define Package/luci-app-myos-system/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MYOS_LEGACY)/root/etc/init.d/myos-nros-app $(1)/etc/init.d/myos-nros-app
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MYOS_LEGACY)/root/etc/init.d/myos-factory-wifi $(1)/etc/init.d/myos-factory-wifi
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MYOS_LEGACY)/root/etc/uci-defaults/94-myos-factory-wifi $(1)/etc/uci-defaults/94-myos-factory-wifi
	$(INSTALL_DIR) $(1)/lib/functions
	$(INSTALL_BIN) $(MYOS_LEGACY)/root/lib/functions/myos-factory-wifi.sh $(1)/lib/functions/myos-factory-wifi.sh
	$(INSTALL_DIR) $(1)/lib/preinit
	$(INSTALL_BIN) $(MYOS_LEGACY)/root/lib/preinit/82_myos_factory_wifi $(1)/lib/preinit/82_myos_factory_wifi
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/myos
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools
	$(INSTALL_DATA) $(MYOS_LEGACY)/htdocs/luci-static/resources/tools/myos-wifi.js $(1)/www/luci-static/resources/tools/myos-wifi.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools
	$(INSTALL_DATA) $(MYOS_LEGACY)/htdocs/luci-static/resources/tools/myos-wifi.css $(1)/www/luci-static/resources/tools/myos-wifi.css
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/libexec/rpcd/mwrt-wizard $(1)/usr/libexec/rpcd/mwrt-wizard
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/libexec/rpcd/luci.mwrt-notice $(1)/usr/libexec/rpcd/luci.mwrt-notice
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/libexec/mwrt-usbinfo $(1)/usr/libexec/mwrt-usbinfo
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/controller/zz_mwrt_features.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_features.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/model/cbi/mwrt.lua $(1)/usr/lib/lua/luci/model/cbi/mwrt.lua
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/luci/menu.d/mwrt-wizard.json $(1)/usr/share/luci/menu.d/mwrt-wizard.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/rpcd/acl.d/mwrt-features.json $(1)/usr/share/rpcd/acl.d/mwrt-features.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/rpcd/ucode/mwrt-ota $(1)/usr/share/rpcd/ucode/mwrt-ota
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/mwrt
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/view/mwrt/wizard.js $(1)/www/luci-static/resources/view/mwrt/wizard.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/mwrt
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/mwrt/wizard.js $(1)/www/luci-static/resources/mwrt/wizard.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/mwrt-wizard.css $(1)/www/luci-static/resources/mwrt-wizard.css
	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/mwrt-wizard-launch.js $(1)/www/luci-static/resources/mwrt-wizard-launch.js
	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(INSTALL_DATA) ./root/usr/lib/lua/myos_plugins.lua $(1)/usr/lib/lua/myos_plugins.lua
endef

$(eval $(call BuildPackage,luci-app-myos-system))

