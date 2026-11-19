include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-myos-system
PKG_VERSION:=1.0.0
PKG_RELEASE:=4
PKG_LICENSE:=Apache-2.0
PKG_FILE_DEPENDS:=$(CURDIR)/dual-pdp \

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
  DEPENDS+=+luci-lib-nixio +libuci-lua +ip-full +flock +conntrack +libnetfilter-conntrack +tc-full +kmod-nft-nat
  DEPENDS+=+dnsmasq-full +nftables-json +libnettle +libgmp
endef

define Package/luci-app-myos-system/conffiles
/etc/config/mwrt
/etc/config/Mwrt-Assurance
/etc/config/WTMmodem
/etc/config/dual_pdp
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
	$(INSTALL_BIN) $(MWRT_LEGACY)/usr/bin/mwrt_wan_failover.sh $(1)/usr/bin/mwrt_wan_failover.sh
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(MWRT_LEGACY)/etc/init.d/mwrt-assurance $(1)/etc/init.d/mwrt-assurance
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/99f-myos-remove-mwrt-fan-cron $(1)/etc/uci-defaults/99f-myos-remove-mwrt-fan-cron
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
	$(INSTALL_DIR) $(1)/etc/hotplug.d/iface $(1)/usr/lib/lua/dual_pdp
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/dual-pdp
	$(INSTALL_DIR) $(1)/www/luci-static/resources/dual-pdp $(1)/www/luci-static/resources/view/dual-pdp
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d $(1)/usr/libexec/inst-dual-pdp
	$(INSTALL_DATA) ./dual-pdp/etc/config/dual_pdp $(1)/etc/config/dual_pdp
	$(INSTALL_BIN) ./dual-pdp/etc/init.d/dual-pdp* $(1)/etc/init.d/
	$(INSTALL_BIN) ./dual-pdp/etc/hotplug.d/iface/* $(1)/etc/hotplug.d/iface/
	$(INSTALL_BIN) ./dual-pdp/etc/uci-defaults/98-dual-pdp-inst-features ./dual-pdp/etc/uci-defaults/99-dual-pdp-init $(1)/etc/uci-defaults/
	$(INSTALL_BIN) ./dual-pdp/usr/sbin/dual-pdp-* ./dual-pdp/usr/sbin/c2000max-traffic ./dual-pdp/usr/sbin/c2000max-feature-* $(1)/usr/sbin/
	$(INSTALL_BIN) ./dual-pdp/usr/libexec/dual-pdp-* $(1)/usr/libexec/
	$(INSTALL_BIN) ./dual-pdp/usr/libexec/rpcd/dual-pdp $(1)/usr/libexec/rpcd/
	$(INSTALL_BIN) ./dual-pdp/usr/libexec/inst-dual-pdp/dnsmasq $(1)/usr/libexec/inst-dual-pdp/
	$(INSTALL_DATA) ./dual-pdp/usr/libexec/c2000max-*.uc $(1)/usr/libexec/
	$(INSTALL_DATA) ./dual-pdp/usr/lib/lua/dual_pdp/*.lua $(1)/usr/lib/lua/dual_pdp/
	$(INSTALL_DATA) ./dual-pdp/usr/lib/libubox.so.20260213 ./dual-pdp/usr/lib/libubus.so.20251202 $(1)/usr/lib/
	$(INSTALL_DATA) ./dual-pdp/usr/share/dual-pdp/* $(1)/usr/share/dual-pdp/
	$(INSTALL_DATA) ./dual-pdp/usr/share/luci/menu.d/luci-app-dual-pdp.json $(1)/usr/share/luci/menu.d/
	$(INSTALL_DATA) ./dual-pdp/usr/share/rpcd/acl.d/luci-app-dual-pdp.json $(1)/usr/share/rpcd/acl.d/
	$(INSTALL_DATA) ./dual-pdp/lib/upgrade/keep.d/dual-pdp-inst-features $(1)/lib/upgrade/keep.d/
	$(INSTALL_DATA) ./dual-pdp/www/luci-static/resources/dual-pdp/* $(1)/www/luci-static/resources/dual-pdp/
	$(INSTALL_DATA) ./dual-pdp/www/luci-static/resources/view/dual-pdp/overview.js $(1)/www/luci-static/resources/view/dual-pdp/
endef

$(eval $(call BuildPackage,luci-app-myos-system))
