# Copyright (C) 2026
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-5700webui
PKG_VERSION:=1.0-r30
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
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
