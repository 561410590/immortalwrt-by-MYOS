include $(TOPDIR)/rules.mk

PKG_NAME:=c2000max-wifi7-prebuilt
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

define KernelPackage/mt_wifi_cmn
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek WiFi common module
  DEPENDS:=@TARGET_mediatek_filogic
  FILES:=$(PKG_BUILD_DIR)/mt_wifi_cmn.ko
  AUTOLOAD:=$(call AutoLoad,60,mt_wifi_cmn)
endef

define KernelPackage/mt_wifi7
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek WiFi 7 AP module
  DEPENDS:=@TARGET_mediatek_filogic +blkid +kmod-mt_wifi_cmn +kmod-mediatek_hnat
  FILES:=$(PKG_BUILD_DIR)/mt_wifi.ko
  AUTOLOAD:=$(call AutoLoad,61,mt_wifi)
endef

define KernelPackage/mt_wifi7/install
	$(CP) ./files/root/. $(1)/
endef

define KernelPackage/mt_hwifi
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek hardware WiFi module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-mt_wifi7 +kmod-mt_wifi_cmn
  FILES:=$(PKG_BUILD_DIR)/mtk_hwifi.ko
  AUTOLOAD:=$(call AutoLoad,62,mtk_hwifi)
endef

define KernelPackage/mt_hwifi/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(CP) ./files/firmware/. $(1)/lib/firmware/
	rm -rf $(1)/lib/firmware/inside-secure
endef

define KernelPackage/connac_if
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek CONNAC interface module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-mt_hwifi +kmod-mt_wifi7
  FILES:=$(PKG_BUILD_DIR)/connac_if.ko
  AUTOLOAD:=$(call AutoLoad,63,connac_if)
endef

define KernelPackage/mtk_pci
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek WiFi PCI module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-mt_hwifi +kmod-mt_wifi_cmn +kmod-connac_if
  FILES:=$(PKG_BUILD_DIR)/mtk_pci.ko
  AUTOLOAD:=$(call AutoLoad,63,mtk_pci)
endef

define KernelPackage/mtk_wed
  SUBMENU:=Network Devices
  TITLE:=NRadio C2000-MAX MediaTek WED module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-warp +kmod-mt_hwifi
  FILES:=$(PKG_BUILD_DIR)/mtk_wed.ko
  AUTOLOAD:=$(call AutoLoad,64,mtk_wed)
endef

define KernelPackage/mt7993
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek MT7993 module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-mt_hwifi +kmod-mt_wifi_cmn
  FILES:=$(PKG_BUILD_DIR)/mt7993.ko
  AUTOLOAD:=$(call AutoLoad,65,mt7993)
endef

define KernelPackage/mt7993_dbg
  SUBMENU:=Wireless Drivers
  TITLE:=NRadio C2000-MAX MediaTek MT7993 debug module
  DEPENDS:=@TARGET_mediatek_filogic +kmod-mt7993 +kmod-mtk_pci
  FILES:=$(PKG_BUILD_DIR)/mt7993_dbg.ko
endef

$(eval $(call KernelPackage,mt_wifi_cmn))
$(eval $(call KernelPackage,mt_wifi7))
$(eval $(call KernelPackage,mt_hwifi))
$(eval $(call KernelPackage,connac_if))
$(eval $(call KernelPackage,mtk_pci))
$(eval $(call KernelPackage,mtk_wed))
$(eval $(call KernelPackage,mt7993))
$(eval $(call KernelPackage,mt7993_dbg))
