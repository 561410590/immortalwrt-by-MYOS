include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=c5800-mt5700-ncm
PKG_RELEASE:=3
PKG_LICENSE:=GPL-2.0-only
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/c5800-mt5700-ncm
  SUBMENU:=USB Support
  TITLE:=C5800 internal MT5700 NCM receive driver
  DEPENDS:=@TARGET_mediatek +kmod-usb-core +kmod-mii
  FILES:=$(PKG_BUILD_DIR)/wt_ncm_lab.ko
  AUTOLOAD:=$(call AutoLoad,49,wt_ncm_lab)
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

define KernelPackage/c5800-mt5700-ncm/install
	$(INSTALL_DIR) $(1)/etc/hotplug.d/usb
	$(INSTALL_BIN) ./files/09-c5800-mt5700-ncm $(1)/etc/hotplug.d/usb/
endef

$(eval $(call KernelPackage,c5800-mt5700-ncm))
