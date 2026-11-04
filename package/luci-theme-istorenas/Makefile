include $(TOPDIR)/rules.mk

PKG_NAME:=luci-theme-istorenas
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_SOURCE:=luci-theme-istorenas_1.0.0_all.ipk
PKG_SOURCE_URL:=https://istore.istoreos.com/repo/all/nas_luci
PKG_HASH:=692d309d05d89e1367890261ab91b8389c0ea22ee74f11288384c6ca0c7ab1cd
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

define Package/luci-theme-istorenas
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Themes
  TITLE:=iStoreNAS Login Page
  URL:=https://github.com/linkease/nas-packages-luci
  DEPENDS:=+luci-theme-argon
endef

define Package/luci-theme-istorenas/description
  Upstream iStoreNAS resources with the existing LuCI theme selection preserved.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(TAR) -xOzf $(DL_DIR)/$(PKG_SOURCE) ./data.tar.gz | $(TAR) -xzf - -C $(PKG_BUILD_DIR)
	$(Build/Patch)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/luci-theme-istorenas/install
	$(CP) $(PKG_BUILD_DIR)/etc $(PKG_BUILD_DIR)/usr $(PKG_BUILD_DIR)/www $(1)/
endef

define Package/luci-theme-istorenas/postinst
[ -n "$${IPKG_INSTROOT}" ] || {
	[ -f /etc/uci-defaults/31_luci-istorenas-login ] && (. /etc/uci-defaults/31_luci-istorenas-login) && rm -f /etc/uci-defaults/31_luci-istorenas-login
	rm -f /tmp/luci-indexcache /tmp/luci-indexcache.*
	rm -rf /tmp/luci-modulecache/
	/etc/init.d/rpcd reload 2>/dev/null
	exit 0
}
endef

$(eval $(call BuildPackage,luci-theme-istorenas))
