include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI app for OAF
LUCI_PKGARCH:=all
LUCI_DEPENDS:=+appfilter +kmod-oaf +luci-compat +luci-lib-jsonc +wrtbwmon
LUCI_MINIFY_CSS:=0

PKG_NAME:=luci-app-oaf
PKG_VERSION:=7.0
PKG_RELEASE:=2
PKG_PO_VERSION:=$(PKG_VERSION)-r$(PKG_RELEASE)

C2000MAX_OAF_DIR:=$(TOPDIR)/target/linux/mediatek/image/nradio-c2000-max
PKG_FILE_DEPENDS += $(C2000MAX_OAF_DIR)/c2000max-oaf.lua \
	$(C2000MAX_OAF_DIR)/c2000max-oaf-entry.lua \
	$(C2000MAX_OAF_DIR)/c2000max-oaf-stats.init

# Both the UI and translation payloads must have traversable parent directories.
# This package contains only Lua, JavaScript, assets, and shell scripts.
RSTRIP:=sh $(CURDIR)/normalize-permissions.sh

define Build/Prepare/luci-app-oaf
	install -m 0644 -D $(C2000MAX_OAF_DIR)/c2000max-oaf.lua \
		$(PKG_BUILD_DIR)/luasrc/model/c2000max_oaf.lua
	install -m 0644 -D $(C2000MAX_OAF_DIR)/c2000max-oaf-entry.lua \
		$(PKG_BUILD_DIR)/luasrc/controller/appfilter.lua
	install -m 0755 -D $(C2000MAX_OAF_DIR)/c2000max-oaf-stats.init \
		$(PKG_BUILD_DIR)/root/etc/init.d/c2000max-oaf-stats
	sed -i 's/require "luci.util"/require "luci.model.c2000max_oaf"/g' $(PKG_BUILD_DIR)/luasrc/controller/oaf*.lua
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
