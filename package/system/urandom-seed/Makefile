include $(TOPDIR)/rules.mk

PKG_NAME:=urandom-seed
PKG_RELEASE:=4
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/urandom-seed
  SECTION:=base
  CATEGORY:=Base system
  DEPENDS:=+getrandom
  TITLE:=/etc/urandom.seed handling for OpenWrt
  URL:=https://openwrt.org/
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Quilt
endef

define Build/Compile/Default
endef
Build/Compile = $(Build/Compile/Default)

define Package/urandom-seed/install
	$(CP) ./files/* $(1)/
	$(INSTALL_BIN) ./files/etc/init.d/urandom_seed $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/sbin/urandom_seed $(1)/sbin/
endef

$(eval $(call BuildPackage,urandom-seed))
