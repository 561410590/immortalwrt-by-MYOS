include $(TOPDIR)/rules.mk

LUCI_TITLE:=MYOS UI - NBCPE-688 LuCI theme
LUCI_DEPENDS:=+luci-base +luci-theme-bootstrap +rpcd-mod-luci +uclient-fetch +ca-bundle +curl +jsonfilter +ucode-mod-fs +ucode-mod-ubus +ucode-mod-uci
LUCI_MINIFY_CSS:=0
LUCI_MINIFY_JS:=0
PKG_LICENSE:=Apache-2.0
PKG_VERSION:=1.0.16
PKG_RELEASE:=13

# Set archive modes after fakeroot chown, which fnOS ACLs can otherwise alter.
PKG_FILE_MODES:= \
	$(shell find $(CURDIR)/root -type d -printf '/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/root -type f -printf '/%P:root:root:0644 ') \
	$(shell find $(CURDIR)/htdocs -type d -printf '/www/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/htdocs -type f -printf '/www/%P:root:root:0644 ') \
	$(shell find $(CURDIR)/ucode -type d -printf '/usr/share/ucode/luci/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/ucode -type f -printf '/usr/share/ucode/luci/%P:root:root:0644 ') \
	/etc/init.d/c2000max-wallpaper:root:root:0755 \
	/usr/libexec/c2000max-bing:root:root:0755 \
	/www/cgi-bin/c2000max-wallpaper:root:root:0755 \
	/etc/uci-defaults/95-c2000max-ui:root:root:0755

# Normalize a checkout copied from Windows before LuCI packages its files.
define Build/Prepare/luci-theme-c2000max-ui
	find $(PKG_BUILD_DIR) -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR) -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/c2000max-wallpaper $(PKG_BUILD_DIR)/root/usr/libexec/c2000max-bing $(PKG_BUILD_DIR)/root/www/cgi-bin/c2000max-wallpaper $(PKG_BUILD_DIR)/root/etc/uci-defaults/95-c2000max-ui
endef

define Package/luci-theme-c2000max-ui/conffiles
/etc/config/c2000max_ui
endef

define Package/luci-theme-c2000max-ui/postrm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] && exit 0
if [ "$$(uci -q get luci.main.mediaurlbase)" = '/luci-static/c2000max-ui' ]; then
	uci -q set luci.main.mediaurlbase='/luci-static/bootstrap'
fi
uci -q delete luci.themes.MYOS_UI
uci -q commit luci
exit 0
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot package scan signature
