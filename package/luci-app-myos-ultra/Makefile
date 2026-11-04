include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-myos-ultra
PKG_VERSION:=1.0
PKG_RELEASE:=202611031200
PKG_LICENSE:=Apache-2.0
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

# Preserve device services without installing either retired theme.
MYOS_SOURCE:=$(TOPDIR)/package/mtk/applications/luci-theme-c2000max-ui
MWRT_SOURCE:=$(TOPDIR)/package/mtk/applications/luci-theme-mwrt

define Package/luci-app-myos-ultra
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=Applications
  TITLE:=MYOS Ultra device services
  DEPENDS:=+luci-base +luci-compat +luci-lib-jsonc +rpcd-mod-luci +rpcd-mod-ucode +jq +bc +qmodem +fancontrol +curl +jsonfilter +uclient-fetch +ca-bundle +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci
endef

define Package/luci-app-myos-ultra/conffiles
/etc/config/mwrt
/etc/config/mwrt_fan
/etc/config/WTMmodem
/etc/config/Mwrt-Assurance
endef

define Build/Compile
endef

define Package/luci-app-myos-ultra/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MYOS_SOURCE)/root/etc/init.d/myos-factory-wifi $(1)/etc/init.d/myos-factory-wifi
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MYOS_SOURCE)/root/etc/init.d/myos-nros-app $(1)/etc/init.d/myos-nros-app
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MYOS_SOURCE)/root/etc/uci-defaults/94-myos-factory-wifi $(1)/etc/uci-defaults/94-myos-factory-wifi
	$(INSTALL_DIR) $(1)/lib/functions
	$(INSTALL_BIN) $(MYOS_SOURCE)/root/lib/functions/myos-factory-wifi.sh $(1)/lib/functions/myos-factory-wifi.sh
	$(INSTALL_DIR) $(1)/lib/preinit
	$(INSTALL_BIN) $(MYOS_SOURCE)/root/lib/preinit/82_myos_factory_wifi $(1)/lib/preinit/82_myos_factory_wifi
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MYOS_SOURCE)/root/usr/share/rpcd/acl.d/luci-theme-c2000max-ui.json $(1)/usr/share/rpcd/acl.d/luci-theme-c2000max-ui.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(INSTALL_DATA) $(MYOS_SOURCE)/root/usr/share/rpcd/ucode/c2000max-ui $(1)/usr/share/rpcd/ucode/c2000max-ui
	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/etc/config/Mwrt-Assurance $(1)/etc/config/Mwrt-Assurance
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/etc/config/WTMmodem $(1)/etc/config/WTMmodem
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/etc/config/mwrt $(1)/etc/config/mwrt
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/etc/config/mwrt_fan $(1)/etc/config/mwrt_fan
	$(INSTALL_DIR) $(1)/etc/hotplug.d/usb
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/hotplug.d/usb/40-mwrt-qmodem-dedup $(1)/etc/hotplug.d/usb/40-mwrt-qmodem-dedup
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/init.d/mwrt-assurance $(1)/etc/init.d/mwrt-assurance
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/init.d/mwrt-fansched $(1)/etc/init.d/mwrt-fansched
	$(INSTALL_DIR) $(1)/etc/mwrt
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/etc/mwrt/ota-ca.pem $(1)/etc/mwrt/ota-ca.pem
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/uci-defaults/99f-mwrt-fansched $(1)/etc/uci-defaults/99f-mwrt-fansched
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/uci-defaults/99g-mwrt-cellinfo $(1)/etc/uci-defaults/99g-mwrt-cellinfo
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/uci-defaults/99h-mwrt-qmodem-dedup $(1)/etc/uci-defaults/99h-mwrt-qmodem-dedup
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/etc/uci-defaults/99j-mwrt-wizard $(1)/etc/uci-defaults/99j-mwrt-wizard
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/install-wizard.sh $(1)/usr/bin/install-wizard.sh
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/mwrt-bootswitch $(1)/usr/bin/mwrt-bootswitch
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/mwrt-cellinfo $(1)/usr/bin/mwrt-cellinfo
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/mwrt-fansched $(1)/usr/bin/mwrt-fansched
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/mwrt-qmodem-dedup $(1)/usr/bin/mwrt-qmodem-dedup
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/mwrt-qmodem-usb3 $(1)/usr/bin/mwrt-qmodem-usb3
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/bin/mwrt_wan_failover.sh $(1)/usr/bin/mwrt_wan_failover.sh
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/controller/mwrt_hwinfo.lua $(1)/usr/lib/lua/luci/controller/mwrt_hwinfo.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/controller/zz_mwrt_modem.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_modem.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/controller/zz_mwrt_mwrt_assurance.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_mwrt_assurance.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/controller/zz_mwrt_qmodem.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_qmodem.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/controller/zz_mwrt_ultra_features.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_ultra_features.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/zmode
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/model/cbi/zmode/mwrt_wan_failover.lua $(1)/usr/lib/lua/luci/model/cbi/zmode/mwrt_wan_failover.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/Airpifanctrl
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/view/Airpifanctrl/fancontrol.htm $(1)/usr/lib/lua/luci/view/Airpifanctrl/fancontrol.htm
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/zmode
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/lib/lua/luci/view/zmode/mwrt_wan_failover_status.htm $(1)/usr/lib/lua/luci/view/zmode/mwrt_wan_failover_status.htm
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/libexec/rpcd/luci.mwrt-notice $(1)/usr/libexec/rpcd/luci.mwrt-notice
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/libexec/rpcd/luci.mwrt-ota $(1)/usr/libexec/rpcd/luci.mwrt-ota
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/libexec/rpcd/mwrt-wizard $(1)/usr/libexec/rpcd/mwrt-wizard
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/share/luci/menu.d/zzz-luci-app-WTModem-wizard.json $(1)/usr/share/luci/menu.d/zzz-luci-app-WTModem-wizard.json
	$(INSTALL_DIR) $(1)/usr/share/modem
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/share/modem/simstat.sh $(1)/usr/share/modem/simstat.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/share/mwrt-qmodem/generic.sh $(1)/usr/share/mwrt-qmodem/generic.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/share/mwrt-qmodem/modem_ctrl.sh $(1)/usr/share/mwrt-qmodem/modem_ctrl.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/share/mwrt-qmodem/modem_util.sh $(1)/usr/share/mwrt-qmodem/modem_util.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem/vendor
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/share/mwrt-qmodem/vendor/dynamic_load.json $(1)/usr/share/mwrt-qmodem/vendor/dynamic_load.json
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem/vendor
	$(INSTALL_BIN) $(MWRT_SOURCE)/root/usr/share/mwrt-qmodem/vendor/huawei.sh $(1)/usr/share/mwrt-qmodem/vendor/huawei.sh
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/share/rpcd/acl.d/luci-app-WTModem-wizard.json $(1)/usr/share/rpcd/acl.d/luci-app-WTModem-wizard.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/share/rpcd/acl.d/luci-app-mwrt-notice.json $(1)/usr/share/rpcd/acl.d/luci-app-mwrt-notice.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/usr/share/rpcd/acl.d/luci-app-mwrt-ota.json $(1)/usr/share/rpcd/acl.d/luci-app-mwrt-ota.json
	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/www/luci-static/resources/mwrt-wizard-launch.js $(1)/www/luci-static/resources/mwrt-wizard-launch.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/www/luci-static/resources/mwrt-wizard.css $(1)/www/luci-static/resources/mwrt-wizard.css
	$(INSTALL_DIR) $(1)/www/luci-static/resources/mwrt
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/www/luci-static/resources/mwrt/wizard.js $(1)/www/luci-static/resources/mwrt/wizard.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/mwrt
	$(INSTALL_DATA) $(MWRT_SOURCE)/root/www/luci-static/resources/view/mwrt/wizard.js $(1)/www/luci-static/resources/view/mwrt/wizard.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/myos
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools
	$(INSTALL_DATA) $(MYOS_SOURCE)/htdocs/luci-static/resources/tools/myos-wifi.js $(1)/www/luci-static/resources/tools/myos-wifi.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools
	$(INSTALL_DATA) $(MYOS_SOURCE)/htdocs/luci-static/resources/tools/myos-wifi.css $(1)/www/luci-static/resources/tools/myos-wifi.css
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/model/mwrt_gate.lua $(1)/usr/lib/lua/luci/model/mwrt_gate.lua
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./root/etc/uci-defaults/zz-myos-ultra-theme $(1)/etc/uci-defaults/zz-myos-ultra-theme
endef

$(eval $(call BuildPackage,luci-app-myos-ultra))
