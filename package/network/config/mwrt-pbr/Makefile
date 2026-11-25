include $(TOPDIR)/rules.mk

PKG_NAME:=mwrt-policy-routing
PKG_VERSION:=1166.24
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/mwrt-pbr
  SECTION:=net
  CATEGORY:=Network
  TITLE:=MWRT policy routing
  PKGARCH:=all
  DEPENDS:=+ucode +ucode-mod-fs +ucode-mod-uci +ucode-mod-ubus +ucode-mod-uloop +ip-full +nftables-json +kmod-nft-core +appfilter
endef

define Package/mwrt-pbr/conffiles
/etc/config/mwrt_pbr
endef

define Package/luci-app-mwpbr
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=MWRT policy routing interface
  PKGARCH:=all
  DEPENDS:=+mwrt-pbr +luci-compat +luci-lib-nixio +luci-lib-jsonc +libuci-lua +luci-app-appfilter
endef

define Build/Compile
endef

define Package/mwrt-pbr/install
	$(INSTALL_DIR) $(1)/usr/libexec $(1)/usr/share/ucode/mwrt
	$(INSTALL_BIN) ./files/usr/libexec/mwrt-pbr ./files/usr/libexec/mwrt-wans \
		./files/usr/libexec/mwrt-wanmon $(1)/usr/libexec/
	$(INSTALL_DATA) ./files/usr/share/ucode/mwrt/*.uc $(1)/usr/share/ucode/mwrt/
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/etc/hotplug.d/iface $(1)/lib/upgrade/keep.d
	$(INSTALL_CONF) ./files/etc/config/mwrt_pbr $(1)/etc/config/
	$(INSTALL_BIN) ./files/etc/init.d/mwrt-pbr $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/hotplug.d/iface/91-mwrt-pbr $(1)/etc/hotplug.d/iface/
	$(INSTALL_DATA) ./files/lib/upgrade/keep.d/mwrt-pbr $(1)/lib/upgrade/keep.d/
endef

define Package/luci-app-mwpbr/install
	$(INSTALL_DIR) $(1)/usr/libexec $(1)/usr/lib/lua/luci/controller \
		$(1)/usr/lib/lua/luci/view/mwpbr $(1)/www/luci-static/resources/mwpbr
	$(INSTALL_BIN) ./files/usr/libexec/mwrt-clients $(1)/usr/libexec/
	$(INSTALL_DATA) ./files/usr/lib/lua/luci/controller/mwpbr.lua $(1)/usr/lib/lua/luci/controller/
	$(INSTALL_DATA) ./files/usr/lib/lua/luci/view/mwpbr/page.htm $(1)/usr/lib/lua/luci/view/mwpbr/
	$(INSTALL_DATA) ./files/www/luci-static/resources/mwpbr/* $(1)/www/luci-static/resources/mwpbr/
endef

$(eval $(call BuildPackage,mwrt-pbr))
$(eval $(call BuildPackage,luci-app-mwpbr))
