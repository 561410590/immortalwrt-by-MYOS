#
# Copyright (C) 2018 Jo-Philipp Wich <jo@mein.io>
#
# This is free software, licensed under the Apache License, Version 2.0 .
#

include $(TOPDIR)/rules.mk

PKG_LICENSE:=Apache-2.0

LUCI_TITLE:=Package management application
LUCI_DEPENDS:=+luci-base

PKG_MAINTAINER:=Jo-Philipp Wich <jo@mein.io>

PKG_PROVIDES:=luci-app-opkg

MWRT_STORE_PROFILES := CONFIG_TARGET_mediatek_filogic_DEVICE_nradio_c5800-688 \
                       CONFIG_TARGET_mediatek_filogic_DEVICE_nradio_c8-688
PKG_CONFIG_DEPENDS += $(MWRT_STORE_PROFILES)
PKG_PREPARED_DEPENDS += $(MWRT_STORE_PROFILES)

ifneq ($(filter y,$(CONFIG_TARGET_mediatek_filogic_DEVICE_nradio_c5800-688) $(CONFIG_TARGET_mediatek_filogic_DEVICE_nradio_c8-688)),)
  PKG_VERSION:=26.315.10333~cc142cd
  PKG_RELEASE:=2
  LUCI_DEPENDS+=+curl +flock +jsonfilter

  define Build/Prepare/luci-app-package-manager
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/root/etc/mwrt \
		$(PKG_BUILD_DIR)/root/www/luci-static/resources/view
	$(INSTALL_DATA) ./mwrt1166/etc/mwrt/ota-ca.pem $(PKG_BUILD_DIR)/root/etc/mwrt/
	$(INSTALL_DATA) ./mwrt1166/www/luci-static/resources/view/package-manager.js \
		$(PKG_BUILD_DIR)/root/www/luci-static/resources/view/
	$(INSTALL_DATA) ./mwrt1166/www/luci-static/resources/pkg-zh.json \
		$(PKG_BUILD_DIR)/root/www/luci-static/resources/
	$(INSTALL_DATA) ./mwrt1166/usr/share/luci/menu.d/luci-app-package-manager.json \
		$(PKG_BUILD_DIR)/root/usr/share/luci/menu.d/
	$(INSTALL_DATA) ./mwrt1166/usr/share/rpcd/acl.d/luci-app-package-manager.json \
		$(PKG_BUILD_DIR)/root/usr/share/rpcd/acl.d/
	$(INSTALL_BIN) ./mwrt1166/usr/libexec/package-manager-call \
		./mwrt1166/usr/libexec/mwrt-store-call $(PKG_BUILD_DIR)/root/usr/libexec/
  endef
endif

include ../../luci.mk

# call BuildPackage - OpenWrt buildroot signature
