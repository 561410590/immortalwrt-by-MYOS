include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-uugamebooster
PKG_VERSION:=1.1
PKG_RELEASE:=202610271600

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-uugamebooster
  SECTION:=net
  CATEGORY:=Network
  TITLE:=UU GameAcc for MYOS Ultra
  DEPENDS:=+kmod-tun +luci-compat +libstdcpp +kmod-ipt-nat +curl +luci-lua-runtime +ip-full +kmod-nft-compat +kmod-nf-conntrack-netlink +kmod-nft-tproxy +kmod-nft-socket
  PKGARCH:=all
endef

PKG_FILE_MODES:= \
	$(shell find $(CURDIR)/files -type d -printf '/%P:root:root:0755 ') \
	$(shell find $(CURDIR)/files -type f -printf '/%P:root:root:0644 ') \
	/etc/init.d/uuplugin:root:root:0755 \
	/etc/uci-defaults/45_luci-uuplugin:root:root:0755 \
	/usr/libexec/uuplugin-launcher:root:root:0755 \
	/usr/libexec/uuplugin-xtables:root:root:0755

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

define Package/luci-app-uugamebooster/install
	$(CP) ./files/* $(1)/
	chmod 0755 $(1)/etc/init.d/uuplugin $(1)/etc/uci-defaults/45_luci-uuplugin
	chmod 0755 $(1)/usr/libexec/uuplugin-launcher $(1)/usr/libexec/uuplugin-xtables
endef

$(eval $(call BuildPackage,luci-app-uugamebooster))
