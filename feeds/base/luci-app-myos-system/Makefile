include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-myos-system
PKG_VERSION:=1.0.0
PKG_RELEASE:=2
PKG_LICENSE:=Apache-2.0
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

# Keep existing device services independently of retired theme presentation.
MYOS_LEGACY:=$(TOPDIR)/package/mtk/applications/luci-theme-c2000max-ui
MWRT_LEGACY:=$(TOPDIR)/package/mtk/applications/luci-theme-mwrt/root

define Package/luci-app-myos-system
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=Applications
  TITLE:=MYOS MAX system integration, setup and notifications
  DEPENDS:=+luci-base +luci-compat +luci-lib-jsonc +rpcd-mod-luci +rpcd-mod-ucode +curl +jsonfilter +uclient-fetch +ca-bundle +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci +qmodem +fancontrol +jq +bc +python3-ctypes
  PKGARCH:=all
endef

define Package/luci-app-myos-system/conffiles
/etc/config/mwrt
/etc/config/mwrt_fan
/etc/config/Mwrt-Assurance
/etc/config/WTMmodem
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
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/libexec/rpcd/mwrt-wizard $(1)/usr/libexec/rpcd/mwrt-wizard
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/libexec/rpcd/luci.mwrt-notice $(1)/usr/libexec/rpcd/luci.mwrt-notice
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/libexec/rpcd/luci.mwrt-ota $(1)/usr/libexec/rpcd/luci.mwrt-ota
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/bin/mwrt-bootswitch $(1)/usr/bin/mwrt-bootswitch
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/bin/mwrt-portprobe $(1)/usr/bin/mwrt-portprobe
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/bin/mwrt-cellinfo $(1)/usr/bin/mwrt-cellinfo
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/bin/mwrt-fansched $(1)/usr/bin/mwrt-fansched
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/bin/mwrt_wan_failover.sh $(1)/usr/bin/mwrt_wan_failover.sh
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MWRT_LEGACY)/etc/init.d/mwrt-fansched $(1)/etc/init.d/mwrt-fansched
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MWRT_LEGACY)/etc/init.d/mwrt-assurance $(1)/etc/init.d/mwrt-assurance
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MWRT_LEGACY)/etc/uci-defaults/99f-mwrt-fansched $(1)/etc/uci-defaults/99f-mwrt-fansched
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) $(MWRT_LEGACY)/etc/uci-defaults/99g-mwrt-cellinfo $(1)/etc/uci-defaults/99g-mwrt-cellinfo
	$(INSTALL_DIR) $(1)/usr/share/modem
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/share/modem/simstat.sh $(1)/usr/share/modem/simstat.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/share/mwrt-qmodem/modem_ctrl.sh $(1)/usr/share/mwrt-qmodem/modem_ctrl.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/share/mwrt-qmodem/modem_util.sh $(1)/usr/share/mwrt-qmodem/modem_util.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/share/mwrt-qmodem/generic.sh $(1)/usr/share/mwrt-qmodem/generic.sh
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem/vendor
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/share/mwrt-qmodem/vendor/huawei.sh $(1)/usr/share/mwrt-qmodem/vendor/huawei.sh
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_LEGACY)/etc/config/mwrt $(1)/etc/config/mwrt
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_LEGACY)/etc/config/mwrt_fan $(1)/etc/config/mwrt_fan
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_LEGACY)/etc/config/Mwrt-Assurance $(1)/etc/config/Mwrt-Assurance
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(MWRT_LEGACY)/etc/config/WTMmodem $(1)/etc/config/WTMmodem
	$(INSTALL_DIR) $(1)/etc/mwrt
	$(INSTALL_DATA) $(MWRT_LEGACY)/etc/mwrt/ota-ca.pem $(1)/etc/mwrt/ota-ca.pem
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/model/mwrt_gate.lua $(1)/usr/lib/lua/luci/model/mwrt_gate.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_fancontrol.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/controller/zz_mwrt_qmodem.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_qmodem.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/controller/zz_mwrt_ports.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_ports.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/controller/zz_mwrt_mwrt_assurance.lua $(1)/usr/lib/lua/luci/controller/zz_mwrt_mwrt_assurance.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/Airpifanctrl
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/lib/lua/luci/view/Airpifanctrl/fancontrol.htm $(1)/usr/lib/lua/luci/view/Airpifanctrl/fancontrol.htm
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/rpcd/acl.d/luci-app-mwrt-notice.json $(1)/usr/share/rpcd/acl.d/luci-app-mwrt-notice.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/rpcd/acl.d/luci-app-WTModem-wizard.json $(1)/usr/share/rpcd/acl.d/luci-app-WTModem-wizard.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/rpcd/acl.d/luci-app-mwrt-ota.json $(1)/usr/share/rpcd/acl.d/luci-app-mwrt-ota.json
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/luci/menu.d/zzz-luci-app-WTModem-wizard.json $(1)/usr/share/luci/menu.d/zzz-luci-app-WTModem-wizard.json
	$(INSTALL_DIR) $(1)/usr/share/mwrt-qmodem/vendor
	$(INSTALL_DATA) $(MWRT_LEGACY)/usr/share/mwrt-qmodem/vendor/dynamic_load.json $(1)/usr/share/mwrt-qmodem/vendor/dynamic_load.json
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/mwrt
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/view/mwrt/wizard.js $(1)/www/luci-static/resources/view/mwrt/wizard.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/mwrt
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/mwrt/wizard.js $(1)/www/luci-static/resources/mwrt/wizard.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/mwrt-wizard.css $(1)/www/luci-static/resources/mwrt-wizard.css
	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(INSTALL_DATA) $(MWRT_LEGACY)/www/luci-static/resources/mwrt-wizard-launch.js $(1)/www/luci-static/resources/mwrt-wizard-launch.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/myos
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools
	$(INSTALL_DATA) $(MYOS_LEGACY)/htdocs/luci-static/resources/tools/myos-wifi.js $(1)/www/luci-static/resources/tools/myos-wifi.js
	$(INSTALL_DIR) $(1)/www/luci-static/resources/tools
	$(INSTALL_DATA) $(MYOS_LEGACY)/htdocs/luci-static/resources/tools/myos-wifi.css $(1)/www/luci-static/resources/tools/myos-wifi.css
endef

$(eval $(call BuildPackage,luci-app-myos-system))
