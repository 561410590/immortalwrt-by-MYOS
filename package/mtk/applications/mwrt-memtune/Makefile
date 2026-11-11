include $(TOPDIR)/rules.mk

PKG_NAME:=mwrt-memtune
PKG_VERSION:=1
PKG_RELEASE:=5.20261107

include $(INCLUDE_DIR)/package.mk

define Package/mwrt-memtune
  SECTION:=net
  CATEGORY:=Network
  TITLE:=mwrt-memtune (MWRT 1143 ULTRA)
  DEPENDS:=+zram-swap +kmod-loop
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(PKG_BUILD_DIR)/mwrt-loopdio ./src/mwrt-loopdio.c
endef

define Package/mwrt-memtune/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mwrt-loopdio $(1)/usr/sbin/mwrt-loopdio
	$(CP) ./files/* $(1)/
	chmod 0755 $(1)/etc/uci-defaults/99-mwrt-memtune $(1)/etc/hotplug.d/iface/99-mwrt-pktsteer $(1)/etc/init.d/mwrt-swapfile $(1)/usr/bin/mwrt-pktsteer.sh $(1)/etc/hotplug.d/usb/99-mwrt-pktsteer $(1)/etc/init.d/mwrt-pktsteer
endef

$(eval $(call BuildPackage,mwrt-memtune))
