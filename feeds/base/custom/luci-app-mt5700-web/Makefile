include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for the MT5700 Web control panel
LUCI_DESCRIPTION:=Manage the lightweight MT5700 Go WebSocket and AT service
LUCI_DEPENDS:=+mt5700-web-go
LUCI_PKGARCH:=all
PKG_VERSION:=1.2.0
PKG_RELEASE:=3

PKG_FILE_MODES:= \
	/www/luci-static/resources/view/mt5700-web/status_v8.js:root:root:0644

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
