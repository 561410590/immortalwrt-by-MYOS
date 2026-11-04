include $(TOPDIR)/rules.mk

PKG_NAME:=cpetools
PKG_VERSION:=2.13
PKG_RELEASE:=20240106
include $(INCLUDE_DIR)/package.mk

define Package/cpetools
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=C2000MAX original CPE control tools
  DEPENDS:=+rpcd +comgt
  PKGARCH:=all
endef

define Build/Compile
endef

define Package/cpetools/install
	$(CP) ./files/. $(1)/
	chmod 0755 $(1)/usr/bin/cpetools.sh $(1)/etc/cpetools/*.sh
endef

$(eval $(call BuildPackage,cpetools))
