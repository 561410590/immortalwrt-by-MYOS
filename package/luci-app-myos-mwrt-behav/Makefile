include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-myos-mwrt-behav
PKG_VERSION:=1166
PKG_RELEASE:=1
PKG_FILE_DEPENDS:=$(CURDIR)/root

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-myos-mwrt-behav
  SECTION:=luci
  CATEGORY:=LuCI
  TITLE:=MWRT behavior management policies for MYOS
  DEPENDS:=+luci-compat +luci-lib-jsonc +luci-lib-nixio +libuci-lua +nftables-json +appfilter
endef

define Package/luci-app-myos-mwrt-behav/conffiles
/etc/config/mwrt_adv
endef

define Build/Compile
endef

define Package/luci-app-myos-mwrt-behav/install
	$(INSTALL_DIR) $(1)
	$(CP) ./root/* $(1)/
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/lib/upgrade/keep.d
	$(INSTALL_BIN) ./files/myos-mwrt-ac.init $(1)/etc/init.d/myos-mwrt-ac
	$(INSTALL_DATA) ./files/myos-mwrt-ac.keep $(1)/lib/upgrade/keep.d/myos-mwrt-ac
endef

$(eval $(call BuildPackage,luci-app-myos-mwrt-behav))
