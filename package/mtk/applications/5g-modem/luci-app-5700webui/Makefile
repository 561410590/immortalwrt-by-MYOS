# Copyright (C) 2026
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5700webui
PKG_VERSION:=1.0-r32
PKG_RELEASE:=1
PKG_LICENSE:=GPL-3.0-or-later
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI support for MT5700 web UI
  DEPENDS:=+luci +python3 +python3-asyncio +python3-websockets +python3-pyserial +python3-aiohttp +libc
endef

define Package/$(PKG_NAME)/description
  MT5700 web UI files extracted from the known-good C5800-688 firmware.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(CP) ./root/. $(1)/
	rm -rf $(1)/www/5700
	find $(1) -type d -exec chmod 0755 {} +
	find $(1)/www/luci-static $(1)/usr/share -type f -exec chmod 0644 {} +
	chmod 0755 $(1)/etc/init.d/* $(1)/etc/uci-defaults/* $(1)/www/cgi-bin/*
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
