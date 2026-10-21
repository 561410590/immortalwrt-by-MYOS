# Copyright (C) 2018-2020 L-WRT Team
# Copyright (C) 2021-2025 xiaorouji
# Copyright (C) 2026 Openwrt-Passwall Organization
#
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-passwall
PKG_VERSION:=26.10.4
PKG_RELEASE:=1
PKG_PO_VERSION:=$(PKG_VERSION)

PKG_CONFIG_DEPENDS:= \
	CONFIG_PACKAGE_$(PKG_NAME)_Iptables_Transparent_Proxy \
	CONFIG_PACKAGE_$(PKG_NAME)_Nftables_Transparent_Proxy \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Geoview \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Haproxy \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Hysteria \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_NaiveProxy \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Shadowsocks_Rust_Client \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Shadowsocks_Rust_Server \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_ShadowsocksR_Libev_Client \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_ShadowsocksR_Libev_Server \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Shadow_TLS \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Simple_Obfs \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_SingBox \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_V2ray_Geodata \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_V2ray_Plugin \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Xray \
	CONFIG_PACKAGE_$(PKG_NAME)_INCLUDE_Xray_Plugin

LUCI_TITLE:=LuCI support for PassWall
LUCI_PKGARCH:=all
LUCI_DEPENDS:=+coreutils +coreutils-base64 +coreutils-nohup +coreutils-timeout +curl \
	+chinadns-ng +dns2socks +dnsmasq-full +ip-full \
	+libuci-lua +lua +luci-compat +luci-lib-jsonc \
	+microsocks +resolveip +tcping +lyaml

define Package/$(PKG_NAME)/config
menu "Configuration"
	depends on PACKAGE_$(PKG_NAME)

config PACKAGE_$(PKG_NAME)_Iptables_Transparent_Proxy
	bool "Iptables Transparent Proxy"
	select PACKAGE_ipset
	select PACKAGE_ipt2socks
	select PACKAGE_iptables
	select PACKAGE_iptables-zz-legacy
	select PACKAGE_iptables-mod-conntrack-extra
	select PACKAGE_iptables-mod-extra
	select PACKAGE_iptables-mod-iprange
	select PACKAGE_iptables-mod-socket
	select PACKAGE_iptables-mod-tproxy
	select PACKAGE_kmod-ipt-nat
	default y if ! PACKAGE_firewall4

config PACKAGE_$(PKG_NAME)_Nftables_Transparent_Proxy
	bool "Nftables Transparent Proxy"
	select PACKAGE_ipt2socks
	select PACKAGE_nftables
	select PACKAGE_kmod-nft-socket
	select PACKAGE_kmod-nft-tproxy
	select PACKAGE_kmod-nft-nat
	select PACKAGE_kmod-nf-reject
	select PACKAGE_kmod-nf-reject6
	default y if PACKAGE_firewall4

config PACKAGE_$(PKG_NAME)_INCLUDE_Geoview
	bool "Include Geoview"
	select PACKAGE_geoview
	default y if aarch64||arm||i386||x86_64

config PACKAGE_$(PKG_NAME)_INCLUDE_Haproxy
	bool "Include Haproxy"
	select PACKAGE_haproxy
	default y if aarch64||arm||i386||x86_64

config PACKAGE_$(PKG_NAME)_INCLUDE_Hysteria
	bool "Include Hysteria"
	select PACKAGE_hysteria
	default n

config PACKAGE_$(PKG_NAME)_INCLUDE_NaiveProxy
	bool "Include NaiveProxy"
	depends on !(arc||armeb||loongarch64||mips||mips64||powerpc||TARGET_gemini)
	select PACKAGE_naiveproxy
	default n

config PACKAGE_$(PKG_NAME)_INCLUDE_Shadowsocks_Rust_Client
	bool "Include Shadowsocks Rust Client"
	depends on !i386
	select PACKAGE_shadowsocks-rust-sslocal
	default y if aarch64||x86_64

config PACKAGE_$(PKG_NAME)_INCLUDE_Shadowsocks_Rust_Server
	bool "Include Shadowsocks Rust Server"
	depends on !i386
	select PACKAGE_shadowsocks-rust-ssserver
	default n

config PACKAGE_$(PKG_NAME)_INCLUDE_ShadowsocksR_Libev_Client
	bool "Include ShadowsocksR Libev Client"
	select PACKAGE_shadowsocksr-libev-ssr-local
	select PACKAGE_shadowsocksr-libev-ssr-redir
	default y

config PACKAGE_$(PKG_NAME)_INCLUDE_ShadowsocksR_Libev_Server
	bool "Include ShadowsocksR Libev Server"
	select PACKAGE_shadowsocksr-libev-ssr-server
	default n

config PACKAGE_$(PKG_NAME)_INCLUDE_Shadow_TLS
	bool "Include Shadow-TLS"
	select PACKAGE_shadow-tls
	depends on aarch64||arm||x86_64
	default n

config PACKAGE_$(PKG_NAME)_INCLUDE_Simple_Obfs
	bool "Include Simple-Obfs (Shadowsocks Plugin)"
	select PACKAGE_simple-obfs-client
	default y

config PACKAGE_$(PKG_NAME)_INCLUDE_SingBox
	bool "Include Sing-Box"
	select PACKAGE_sing-box
	default y if aarch64||arm||i386||x86_64

config PACKAGE_$(PKG_NAME)_INCLUDE_V2ray_Geodata
	bool "Include V2ray_Geodata"
	select PACKAGE_v2ray-geoip
	select PACKAGE_v2ray-geosite
	default n

config PACKAGE_$(PKG_NAME)_INCLUDE_V2ray_Plugin
	bool "Include V2ray-Plugin (Shadowsocks Plugin)"
	select PACKAGE_v2ray-plugin
	default y if aarch64||arm||i386||x86_64

config PACKAGE_$(PKG_NAME)_INCLUDE_Xray
	bool "Include Xray"
	select PACKAGE_xray-core
	select PACKAGE_unzip
	default y if aarch64||arm||i386||x86_64

config PACKAGE_$(PKG_NAME)_INCLUDE_Xray_Plugin
	bool "Include Xray-Plugin (Shadowsocks Plugin)"
	select PACKAGE_xray-plugin
	default n

endmenu
endef

define Package/$(PKG_NAME)/conffiles
/etc/config/passwall
/etc/config/passwall_server
/etc/passwall/
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
rm -f $${IPKG_INSTROOT}/usr/share/passwall/rules/*.nft
rm -rf $${IPKG_INSTROOT}/tmp/etc/passwall_tmp
exit 0
endef

define Package/$(PKG_NAME)/postrm
#!/bin/sh
rm -f $${IPKG_INSTROOT}/usr/share/passwall/rules/*.nft
rm -rf $${IPKG_INSTROOT}/tmp/etc/passwall_tmp
exit 0
endef

define Build/Prepare/luci-app-passwall
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/hysteria2.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/naive.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/ray.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/sing-box.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/ss-rust.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/ss.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/ssr.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/trojan-plus.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/type/tuic.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/client/log.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/hysteria2.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/ray.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/sing-box.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/socks.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/ss-rust.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/ss.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/ssr.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/type/trojan-plus.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/passwall/server/user.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/passwall/util_trojan.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/passwall/util_tuic.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/view/passwall/acl/view_chinadns_log.htm
	rm -f $(PKG_BUILD_DIR)/luasrc/view/passwall/node_list/link_add_node.htm
	rm -f $(PKG_BUILD_DIR)/luasrc/view/passwall/node_list/link_share_man.htm
	rm -f $(PKG_BUILD_DIR)/luasrc/view/passwall/server/users_list_status.htm
	rm -f $(PKG_BUILD_DIR)/root/etc/uci-defaults/luci-passwall
	rm -f $(PKG_BUILD_DIR)/root/usr/share/passwall/helper_smartdns.sh
	find $(PKG_BUILD_DIR)/root -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR)/root -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/hotplug.d/iface/98-passwall
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/hotplug.d/ntp/30-passwall-resync
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/passwall
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/passwall_server
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/uci-defaults/luci-app-passwall
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/uci-defaults/luci-app-passwall_server
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/app.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/clash_subconverter.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/haproxy.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/haproxy_check.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/iptables.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/lease2hosts.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/luci_update.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/monitor.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/nftables.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/rule_update.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/socks_auto_switch.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/subscribe.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/tasks.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/test.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/passwall/utils.sh
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
