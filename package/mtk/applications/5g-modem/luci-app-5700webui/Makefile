# Copyright (C) 2026
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5700webui
PKG_VERSION:=1.0-r31
PKG_RELEASE:=202610271600
PKG_LICENSE:=GPL-3.0-or-later
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=Ultra shared AT backend for MT5700
  DEPENDS:=+python3 +python3-asyncio +python3-websockets +python3-pyserial +python3-aiohttp +libc +libubus +libubox +libblobmsg-json +libjson-c
endef

define Package/$(PKG_NAME)/description
  Ultra shared AT backend. The modern interface is provided by mt5700-web-go.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(CP) ./root/. $(1)/
	# The replacement package owns the WebUI. Keep the existing ubus AT daemon.
	rm -rf $(1)/www/5700
	rm -f $(1)/etc/init.d/at-webserver $(1)/etc/uci-defaults/99-at-webserver-enable
	# luci-app-embed owns this view; image assembly uses the same patched bytes.
	rm -f $(1)/www/luci-static/resources/view/embed/htmlview.js
	chmod 0755 $(1)/usr/bin/atserver \
		$(1)/www/cgi-bin/at-log-clear \
		$(1)/www/cgi-bin/at-ws-info
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
