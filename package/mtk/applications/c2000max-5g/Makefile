include $(TOPDIR)/rules.mk

PKG_NAME:=c2000max-5g-prebuilt
PKG_VERSION:=20260303
PKG_RELEASE:=1

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/modules/*.ko $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define KernelPackage/qmi_wwan_s
  SUBMENU:=WWAN Support
  TITLE:=NRadio C2000-MAX Sierra QMI WWAN module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-usb-net +kmod-usb-wdm
  FILES:=$(PKG_BUILD_DIR)/qmi_wwan_s.ko
  AUTOLOAD:=$(call AutoLoad,82,qmi_wwan_s)
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
  DEPENDS:=+quectel-CM-5G-M +kmod-pcie_mhi +kmod-qmi_wwan_q +kmod-qmi_wwan_f +kmod-qmi_wwan_s
endef

define Package/qmodem/install
	$(CP) ./files/rootfs/. $(1)/
	rm -rf $(1)/lib/modules
	rm -f $(1)/usr/bin/quectel-CM-M
endef

$(eval $(call KernelPackage,qmi_wwan_s))
$(eval $(call BuildPackage,quectel-CM-5G-M))
$(eval $(call BuildPackage,qmodem))
