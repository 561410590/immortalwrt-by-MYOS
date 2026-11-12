include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for Qiyou Game Booster
LUCI_DEPENDS:=+qiyougamebooster
PKG_VERSION:=1.2.0
PKG_RELEASE:=3

define Build/Prepare/luci-app-qiyougamebooster
	chmod 0644 \
		$(PKG_BUILD_DIR)/luasrc/controller/qiyougamebooster.lua \
		$(PKG_BUILD_DIR)/luasrc/model/cbi/qiyougamebooster.lua \
		$(PKG_BUILD_DIR)/htdocs/qiyougamebooster.png \
		$(PKG_BUILD_DIR)/root/usr/share/rpcd/acl.d/luci-app-qiyougamebooster.json
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage: luci.mk registers the app and its translations.
