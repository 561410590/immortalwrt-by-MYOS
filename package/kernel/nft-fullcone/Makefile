include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=nft-fullcone
PKG_VERSION:=07d93b626ce5ea885cd16f9ab07fac3213c355d9
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/nft-fullcone
  SUBMENU:=Netfilter Extensions
  TITLE:=nftables Full Cone NAT
  DEPENDS:=+kmod-nf-nat +kmod-nft-core
  KCONFIG:=CONFIG_NF_CONNTRACK_EVENTS=y
  FILES:=$(PKG_BUILD_DIR)/nft_fullcone.ko
  AUTOLOAD:=$(call AutoLoad,35,nft_fullcone)
endef

define KernelPackage/nft-fullcone/description
  Full cone UDP NAT expression for firewall4/nftables, derived from
  xt_FULLCONENAT. TCP and other protocols use masquerading semantics.
  The pinned source is included in src for reproducible offline builds.
endef

define Build/Compile
	+$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,nft-fullcone))
