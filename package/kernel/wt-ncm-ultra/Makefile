include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=wt-ncm-ultra
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only
PKG_LICENSE_FILES:=COPYING LICENSES/preferred/GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/wt-ncm-ultra
  SUBMENU:=USB Support
  TITLE:=WT9500 CDC-NCM receive optimization for C2000 Ultra
  DEPENDS:=@TARGET_mediatek_filogic +kmod-usb-core +kmod-mii
  FILES:=$(PKG_BUILD_DIR)/wt_ncm_lab.ko
  AUTOLOAD:=$(call AutoLoad,53,wt_ncm_lab,1)
  MODPARAMS:=rx_pool_mode=1 rx_pool_copybreak=0 rx_pool_ab_mode=1 rx_pool_gro=1
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./source/. $(PKG_BUILD_DIR)/
	$(CP) ./COPYING $(PKG_BUILD_DIR)/
	$(CP) ./LICENSES $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
	chmod 0644 "$(PKG_BUILD_DIR)/wt_ncm_lab.ko"
endef

$(eval $(call KernelPackage,wt-ncm-ultra))
