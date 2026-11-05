include $(TOPDIR)/rules.mk

PKG_NAME:=luci-theme-myos-new
PKG_VERSION:=1.2.2
PKG_RELEASE:=6
PKG_LICENSE:=Apache-2.0
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

define Package/luci-theme-myos-new
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=Themes
  TITLE:=MYOS-NEW universal device console
  DEPENDS:=+luci-base +luci-compat +rpcd-mod-luci +curl
  PKGARCH:=all
endef

define Package/luci-theme-myos-new/description
 Independent MYOS-NEW theme. Detects the running board and selects its product
 image at runtime. No model installation restrictions or network control hooks.
endef

define Build/Compile
endef

define Package/luci-theme-myos-new/install
	$(INSTALL_DIR) $(1) $(1)/etc $(1)/usr $(1)/usr/lib $(1)/usr/lib/lua $(1)/usr/lib/lua/luci
	$(INSTALL_DIR) $(1)/usr/share $(1)/usr/share/ucode $(1)/usr/share/ucode/luci $(1)/usr/share/ucode/luci/template $(1)/usr/share/ucode/luci/template/themes
	$(INSTALL_DIR) $(1)/www $(1)/www/luci-static $(1)/www/luci-static/myos-new $(1)/www/luci-static/resources $(1)/www/luci-static/resources/view
	$(INSTALL_DIR) $(1)/www/luci-static/myos-new/products
	$(INSTALL_DATA) ./root/www/luci-static/myos-new/*.css $(1)/www/luci-static/myos-new/
	$(INSTALL_DATA) ./root/www/luci-static/myos-new/*.js $(1)/www/luci-static/myos-new/
	$(INSTALL_DATA) ./root/www/luci-static/myos-new/*.png $(1)/www/luci-static/myos-new/
	$(INSTALL_DATA) ./root/www/luci-static/myos-new/products/* $(1)/www/luci-static/myos-new/products/
	$(INSTALL_DIR) $(1)/www/luci-static/myos-new/media
	$(INSTALL_DATA) ./root/www/luci-static/myos-new/media/*.mp4 $(1)/www/luci-static/myos-new/media/
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/myos-new
	$(INSTALL_DATA) ./root/www/luci-static/resources/view/myos-new/home.js $(1)/www/luci-static/resources/view/myos-new/
	$(INSTALL_DIR) $(1)/www/luci-static/resources/icons
	$(INSTALL_DATA) ./root/www/luci-static/resources/icons/*.svg $(1)/www/luci-static/resources/icons/
	$(INSTALL_DIR) $(1)/usr/share/ucode/luci/template/themes/myos-new
	$(INSTALL_DATA) ./root/usr/share/ucode/luci/template/themes/myos-new/*.ut $(1)/usr/share/ucode/luci/template/themes/myos-new/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/controller/myos_new.lua $(1)/usr/lib/lua/luci/controller/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/model/myos_liquid.lua $(1)/usr/lib/lua/luci/model/
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./root/etc/uci-defaults/99-myos-new $(1)/etc/uci-defaults/
endef

$(eval $(call BuildPackage,luci-theme-myos-new))
