include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-uugamebooster
PKG_VERSION:=1.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-uugamebooster
  SECTION:=net
  CATEGORY:=Network
  TITLE:=luci-app-uugamebooster (MWRT 1143 ULTRA)
  DEPENDS:=+kmod-tun +luci-compat +libstdcpp +kmod-ipt-nat +curl +luci-lua-runtime
  PKGARCH:=all
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-uugamebooster/install
	$(CP) ./files/* $(1)/
	chmod 0755 $(1)/etc/init.d/uuplugin $(1)/etc/uci-defaults/45_luci-uuplugin
endef

$(eval $(call BuildPackage,luci-app-uugamebooster))
