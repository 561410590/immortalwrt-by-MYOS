include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-n5000-system
PKG_VERSION:=1.0
PKG_RELEASE:=3
PKGARCH:=all
PKG_LICENSE:=Apache-2.0

include $(INCLUDE_DIR)/package.mk

MYOS_SOURCE:=$(TOPDIR)/package/mtk/applications/luci-theme-c2000max-ui
MWRT_SOURCE:=$(TOPDIR)/package/mtk/applications/luci-theme-mwrt/root
MYOS_EXEC:=etc/uci-defaults/94-myos-factory-wifi lib/functions/myos-factory-wifi.sh lib/preinit/82_myos_factory_wifi etc/init.d/myos-factory-wifi
MYOS_DATA:=
MWRT_EXEC:=usr/bin/mwrt-fansched etc/init.d/mwrt-fansched etc/uci-defaults/99f-mwrt-fansched usr/libexec/mwrt-usbinfo usr/libexec/rpcd/luci.mwrt-notice usr/libexec/rpcd/luci.mwrt-ota
MWRT_DATA:=etc/config/mwrt etc/config/mwrt_fan etc/mwrt/ota-ca.pem usr/lib/lua/luci/model/mwrt_gate.lua usr/lib/lua/luci/controller/mwrt_hwinfo.lua usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua usr/lib/lua/luci/view/Airpifanctrl/fancontrol.htm usr/share/rpcd/acl.d/luci-app-mwrt-notice.json usr/share/rpcd/acl.d/luci-app-mwrt-ota.json

define Package/luci-app-n5000-system
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=Applications
  TITLE:=N5000 wireless defaults, update, fan and notification services
  DEPENDS:=+luci-base +luci-compat +luci-lib-jsonc +rpcd-mod-luci +rpcd-mod-ucode +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci +uclient-fetch +ca-bundle +curl +jsonfilter +jq +bc +fancontrol
endef

define Package/luci-app-n5000-system/conffiles
/etc/config/mwrt
/etc/config/mwrt_fan
endef

define Build/Compile
endef

define Package/luci-app-n5000-system/install
	$(foreach f,$(MYOS_EXEC),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_BIN) $(MYOS_SOURCE)/root/$(f) $(1)/$(f);)
	$(foreach f,$(MYOS_DATA),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_DATA) $(MYOS_SOURCE)/root/$(f) $(1)/$(f);)
	$(foreach f,$(MWRT_EXEC),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_BIN) $(MWRT_SOURCE)/$(f) $(1)/$(f);)
	$(foreach f,$(MWRT_DATA),$(INSTALL_DIR) $(1)/$(dir $(f)); $(INSTALL_DATA) $(MWRT_SOURCE)/$(f) $(1)/$(f);)
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools $(1)/www/luci-static/resources/view/myos
	$(INSTALL_DATA) $(MYOS_SOURCE)/htdocs/luci-static/resources/tools/myos-wifi.js $(MYOS_SOURCE)/htdocs/luci-static/resources/tools/myos-wifi.css $(1)/www/luci-static/resources/tools/
	$(INSTALL_BIN) ./95-myos-n5000 $(1)/etc/uci-defaults/95-myos-n5000
endef

$(eval $(call BuildPackage,luci-app-n5000-system))
