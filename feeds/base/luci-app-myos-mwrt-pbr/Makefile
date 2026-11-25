include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-myos-mwrt-pbr
PKG_VERSION:=1166
PKG_RELEASE:=1
PKG_FILE_DEPENDS:=$(CURDIR)/root

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-myos-mwrt-pbr
  SECTION:=luci
  CATEGORY:=LuCI
  TITLE:=MWRT policy routing for MYOS
  DEPENDS:=+luci-compat +luci-lib-jsonc +luci-lib-nixio +libuci-lua +ucode-mod-fs +ucode-mod-uci +ucode-mod-ubus +ucode-mod-uloop +ip-full +nftables-json +flock
endef

define Package/luci-app-myos-mwrt-pbr/conffiles
/etc/config/mwrt_pbr
endef

define Build/Compile
endef

define Package/luci-app-myos-mwrt-pbr/install
	$(INSTALL_DIR) $(1)
	$(CP) ./root/* $(1)/
endef

$(eval $(call BuildPackage,luci-app-myos-mwrt-pbr))

