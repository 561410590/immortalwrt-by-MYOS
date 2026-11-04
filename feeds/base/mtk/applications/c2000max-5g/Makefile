include $(TOPDIR)/rules.mk

PKG_NAME:=c2000max-5g
PKG_VERSION:=20260303
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Build/Compile
endef

define Package/quectel-CM-5G-M
  SECTION:=net
  CATEGORY:=Network
  TITLE:=NRadio C2000-MAX Quectel 5G connection manager
  DEPENDS:=+libpthread +libjson-c
endef

define Package/quectel-CM-5G-M/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/rootfs/usr/bin/quectel-CM-M $(1)/usr/bin/
endef

define Package/qmodem
  SECTION:=net
  CATEGORY:=Network
  TITLE:=NRadio C2000-MAX modem manager scripts
  DEPENDS:=+quectel-CM-5G-M +kmod-usb-net-qmi-wwan +kmod-usb-serial-option +kmod-usb-wdm
endef

define Package/qmodem/install
	$(CP) ./files/rootfs/. $(1)/
	rm -rf $(1)/lib/modules
	rm -f $(1)/usr/bin/quectel-CM-M
	find $(1) -type d -exec chmod 0755 {} +
	chmod 0755 $(1)/etc/init.d/* $(1)/usr/share/qmodem/*.sh $(1)/usr/libexec/rpcd/*
endef

$(eval $(call BuildPackage,quectel-CM-5G-M))
$(eval $(call BuildPackage,qmodem))
