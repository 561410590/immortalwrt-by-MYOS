include $(TOPDIR)/rules.mk

PKG_NAME:=mwrt-openclash-core
PKG_VERSION:=1.19.32
PKG_RELEASE:=20261013
include $(INCLUDE_DIR)/package.mk

define Package/mwrt-openclash-core
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Official Mihomo ARM64 core for OpenClash
  DEPENDS:=@aarch64 +luci-app-openclash
endef

define Build/Compile
endef

define Package/mwrt-openclash-core/install
	$(INSTALL_DIR) $(1)/etc/openclash/core
	$(INSTALL_BIN) ./files/clash_meta $(1)/etc/openclash/core/clash_meta
endef

$(eval $(call BuildPackage,mwrt-openclash-core))
