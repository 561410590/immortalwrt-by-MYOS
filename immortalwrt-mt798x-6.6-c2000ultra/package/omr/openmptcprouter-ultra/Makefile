include $(TOPDIR)/rules.mk

PKG_NAME:=openmptcprouter-ultra
PKG_VERSION:=2026.09.11
PKG_RELEASE:=1
PKG_LICENSE:=GPL-3.0-only

include $(INCLUDE_DIR)/package.mk

define Package/openmptcprouter-ultra
  SECTION:=net
  CATEGORY:=Network
  TITLE:=OpenMPTCProuter integration for C2000 Ultra
  DEPENDS:=@TARGET_mediatek_filogic \
	+openmptcprouter +luci-app-openmptcprouter \
	+mptcp +luci-app-mptcp +omr-tracker +luci-app-omr-tracker \
	+glorytun +luci-app-glorytun-tcp \
	+shadowsocks-libev-ss-local +shadowsocks-libev-ss-tunnel \
	+shadowsocks-libev-ss-rules-nft +luci-app-shadowsocks-libev \
	+ip-full +ss +nstat +tc-full +kmod-sched-core +kmod-inet-mptcp-diag \
	+firewall4 +nftables-json +iptables-nft +iputils-ping +conntrack \
	+ca-bundle +curl +resolveip +jsonfilter
endef

define Build/Compile
endef

define Package/openmptcprouter-ultra/install
	$(INSTALL_DIR) $(1)
endef

$(eval $(call BuildPackage,openmptcprouter-ultra))
