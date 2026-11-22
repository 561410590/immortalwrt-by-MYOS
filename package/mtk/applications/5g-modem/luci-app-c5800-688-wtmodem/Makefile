# Copyright (C) 2026
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-c5800-688-wtmodem
PKG_VERSION:=1.0.0
PKG_RELEASE:=2
PKG_LICENSE:=GPL-3.0-or-later
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI support for C5800-688 WTModem
  DEFAULT:=y
  DEPENDS:=+luci-compat +python3-light +libgcc
endef

define Package/$(PKG_NAME)/conffiles
/etc/config/apninfo
/etc/config/modem
endef

define Package/$(PKG_NAME)/description
 C5800-688 5G modem control plugin.
endef

define Package/$(PKG_NAME)/extra_provides
	echo libc.so.6
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/etc/uci-defaults
	$(INSTALL_DATA) ./root/etc/config/* $(1)/etc/config/
	$(INSTALL_BIN) ./root/etc/uci-defaults/* $(1)/etc/uci-defaults/
	$(INSTALL_DIR) $(1)/usr/bin $(1)/usr/sbin $(1)/usr/lib $(1)/usr/share/modem
	$(INSTALL_BIN) $(filter-out %/pdul.lua %/smstrun-title.conf,$(wildcard ./root/usr/bin/*)) $(1)/usr/bin/
	$(INSTALL_DATA) ./root/usr/bin/pdul.lua ./root/usr/bin/smstrun-title.conf $(1)/usr/bin/
	$(INSTALL_BIN) ./root/usr/sbin/c5800-route-mode $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_BIN) ./root/usr/libexec/myos-c5800-cell-cache $(1)/usr/libexec/
	$(INSTALL_BIN) ./root/etc/init.d/myos-c5800-cell-cache $(1)/etc/init.d/
	$(INSTALL_DATA) ./root/usr/lib/libbdinfo.so* $(1)/usr/lib/
	$(INSTALL_BIN) $(filter-out %/Quectel %/RM500X,$(wildcard ./root/usr/share/modem/*)) $(1)/usr/share/modem/
	$(INSTALL_DATA) ./root/usr/share/modem/Quectel ./root/usr/share/modem/RM500X $(1)/usr/share/modem/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller $(1)/usr/lib/lua/luci/model/cbi
	$(INSTALL_DATA) ./root/usr/lib/lua/*.lua $(1)/usr/lib/lua/
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/controller/*.lua $(1)/usr/lib/lua/luci/controller/
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/model/cbi/*.lua $(1)/usr/lib/lua/luci/model/cbi/
	$(foreach view,Smstrun cellscan zmode, \
		$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/$(view); \
		$(INSTALL_DATA) ./root/usr/lib/lua/luci/view/$(view)/*.htm $(1)/usr/lib/lua/luci/view/$(view)/;)
	$(INSTALL_DIR) $(1)/www/luci-static/resources/protocol
	$(INSTALL_DATA) ./root/www/luci-static/resources/*.css ./root/www/luci-static/resources/*.js $(1)/www/luci-static/resources/
	$(INSTALL_DATA) ./root/www/luci-static/resources/protocol/*.js $(1)/www/luci-static/resources/protocol/
	$(FIND) $(1) -type d -exec chmod 0755 {} +
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
