include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-openclash
PKG_VERSION:=0.48.002
PKG_RELEASE:=1
PKG_MAINTAINER:=vernesong <https://github.com/vernesong/OpenClash>

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/config
	config PACKAGE_kmod-inet-diag
	default y if PACKAGE_$(PKG_NAME)

	config PACKAGE_luci-compat
	default y if PACKAGE_$(PKG_NAME)

	config PACKAGE_kmod-nft-tproxy
	default y if PACKAGE_firewall4

	config PACKAGE_kmod-ipt-nat
	default y if ! PACKAGE_firewall4

	config PACKAGE_iptables-mod-tproxy
	default y if ! PACKAGE_firewall4

	config PACKAGE_iptables-mod-extra
	default y if ! PACKAGE_firewall4

	config PACKAGE_dnsmasq_full_ipset
	default y if ! PACKAGE_firewall4

	config PACKAGE_dnsmasq_full_nftset
	default y if PACKAGE_firewall4

	config PACKAGE_ipset
	default y if ! PACKAGE_firewall4
endef

define Package/$(PKG_NAME)
	CATEGORY:=LuCI
	SUBMENU:=3. Applications
	TITLE:=LuCI support for clash
	PKGARCH:=all
	DEPENDS:=+dnsmasq-full +bash +curl +ca-bundle +ip-full \
	+ruby +ruby-yaml +kmod-tun +unzip
	MAINTAINER:=vernesong
endef

define Package/$(PKG_NAME)/description
	A LuCI support for clash
endef

define Build/Prepare
	$(CP) $(CURDIR)/root $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/luasrc $(PKG_BUILD_DIR)
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/openclash/groups-config.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/openclash/proxy-provider-config.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/openclash/proxy-provider-file-manage.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/model/cbi/openclash/servers-config.lua
	rm -f $(PKG_BUILD_DIR)/luasrc/view/openclash/dvalue.htm
	rm -f $(PKG_BUILD_DIR)/luasrc/view/openclash/select_git_cdn.htm
	rm -f $(PKG_BUILD_DIR)/luasrc/view/openclash/toolbar_show.htm
	rm -f $(PKG_BUILD_DIR)/root/etc/openclash/custom/openclash_custom_domain_dns_policy.list
	rm -f $(PKG_BUILD_DIR)/root/etc/openclash/custom/openclash_custom_localnetwork_ipv4.list
	rm -f $(PKG_BUILD_DIR)/root/etc/openclash/custom/openclash_custom_localnetwork_ipv6.list
	rm -f $(PKG_BUILD_DIR)/root/etc/openclash/custom/openclash_custom_proxy_server_dns_policy.list
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/Connections-CgFIezGO.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/Connections-N_Z6THP_.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/DocumentTitle-DG90V81t.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/FiraSans-Regular-BN6hITtY.ttf
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/IconChevronRight-Dm97ewab.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/IconReload--QlVe2wP.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/IconReload-BSlLPZuq.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/Overview-B99qGorf.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/Overview-BP3yBMkt.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/Proxies-BRo6NEi-.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/Proxies-Lk5uJ4LX.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/TwemojiMozilla-flags-B12sb_Bp.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/apexcharts.esm-D-nlnKto.js
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-cyrillic-400-normal-CKu2TFQr.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-cyrillic-400-normal-D9eUeF-O.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-cyrillic-ext-400-normal-Cycyekte.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-cyrillic-ext-400-normal-Dd5dApAy.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-greek-400-normal-BFCFncbp.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-greek-400-normal-DQ8sF22y.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-greek-ext-400-normal-ByG7UNQ2.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-greek-ext-400-normal-ClCfNE6D.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-latin-400-normal-66xCkunx.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-latin-400-normal-a0AnQzuD.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-latin-ext-400-normal-BCUwhuGB.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-latin-ext-400-normal-BLG_mAEf.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-vietnamese-400-normal-DUVG35gE.woff
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/assets/fira-sans-vietnamese-400-normal-ljDTQFsx.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/metacubexd/apple-touch-icon-180x180.png
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/zashboard/assets/NotoColorEmoji-flagsonly-CWWDk9km.ttf
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/zashboard/assets/TwemojiMozilla-flags-B12sb_Bp.woff2
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/ui/zashboard/assets/metacubex-BlQkOUXT.jpg
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/clash_version.sh
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_geoasn.sh
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_geoip.sh
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_geosite.sh
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_ipdb.sh
	rm -f $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_version.sh
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/dialog/dialog.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/dialog/dialog.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/display/autorefresh.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/display/fullscreen.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/display/fullscreen.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/edit/matchbrackets.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/fold/foldcode.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/fold/foldgutter.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/fold/foldgutter.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/fold/indent-fold.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/lint/js-yaml.min.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/lint/lint.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/lint/lint.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/lint/yaml-lint.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/merge/diff_match_patch.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/merge/merge.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/merge/merge.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/scroll/annotatescrollbar.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/search/jump-to-line.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/search/match-highlighter.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/search/matchesonscrollbar.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/search/matchesonscrollbar.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/search/search.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/search/searchcursor.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/addon/selection/active-line.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/fonts/TwemojiMozilla-flags-B12sb_Bp.woff2
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/lib/codemirror.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/lib/codemirror.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/mode/log/log.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/mode/properties/properties.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/mode/shell/shell.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/mode/yaml/yaml.js
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/theme/log.css
	rm -f $(PKG_BUILD_DIR)/root/www/luci-static/resources/openclash/theme/material.css
	# Replace the @OPENCLASH_VERSION@ token with the package version (browser asset cache keys).
	sed -i 's|@OPENCLASH_VERSION@|$(PKG_VERSION)|g' $(PKG_BUILD_DIR)/luasrc/openclash.lua $(PKG_BUILD_DIR)/luasrc/view/openclash/*.htm
	$(foreach po,$(wildcard ${CURDIR}/po/zh-cn/*.po), \
		po2lmo $(po) $(PKG_BUILD_DIR)/$(patsubst %.po,%.lmo,$(notdir $(po)));)
	find $(PKG_BUILD_DIR)/root -type d -exec chmod 0755 {} +
	find $(PKG_BUILD_DIR)/root -type f -exec chmod 0644 {} +
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/openclash
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/openclash/custom/openclash_custom_firewall_rules.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/openclash/custom/openclash_custom_overwrite.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/uci-defaults/luci-openclash
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/log.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_chnroute.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_core.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_curl.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_custom_domain_dns.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_debug.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_debug_dns.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_debug_getcon.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_download_dashboard.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_etag.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_geo.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_get_network.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_history_get.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_lgbm.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_nginx.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_oix_checkin.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_ps.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_streaming_unlock.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_sub_parser.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_update.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_urlencode.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_version.lua
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/openclash_watchdog.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/ruby.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/uci.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_change.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_groups_get.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_groups_set.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_proxys_get.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_proxys_set.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_rules_change.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/yml_stats_get.sh
	chmod 0755 $(PKG_BUILD_DIR)/root/etc/init.d/openclash
	chmod -R 0755 $(PKG_BUILD_DIR)/root/usr/share/openclash/
	exit 0
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/conffiles
endef

define Package/$(PKG_NAME)/preinst
#!/bin/sh
	if [ -f "/etc/config/openclash" ] && [ ! -f "/tmp/openclash.bak" ]; then
		cp -f "/etc/config/openclash" "/tmp/openclash.bak" >/dev/null 2>&1
		cp -rf "/etc/openclash" "/tmp/openclash" >/dev/null 2>&1
		cp -rf "/usr/share/openclash/ui" "/tmp/openclash_ui" >/dev/null 2>&1
		cp -rf "/www/luci-static/resources/openclash/pac" "/tmp/pac" >/dev/null 2>&1
	fi
	exit 0
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
	exit 0
endef

define Package/$(PKG_NAME)/prerm
#!/bin/sh
	[ -n "$(pidof clash)" ] && /etc/init.d/openclash stop 2>/dev/null
	if [ -f "/etc/config/openclash" ] && [ ! -f "/tmp/openclash.bak" ]; then
		cp -f "/etc/config/openclash" "/tmp/openclash.bak" >/dev/null 2>&1
		cp -rf "/etc/openclash" "/tmp/openclash" >/dev/null 2>&1
		cp -rf "/usr/share/openclash/ui" "/tmp/openclash_ui" >/dev/null 2>&1
		cp -rf "/www/luci-static/resources/openclash/pac" "/tmp/pac" >/dev/null 2>&1
	fi
	exit 0
endef

define Package/$(PKG_NAME)/postrm
#!/bin/sh
	DEFAULT_DNSMASQ_CFGID="$$(uci -q show "dhcp.@dnsmasq[0]" | awk 'NR==1 {split($0, conf, /[.=]/); print conf[2]}' 2>/dev/null)"
	if [ -f "/tmp/etc/dnsmasq.conf.$DEFAULT_DNSMASQ_CFGID" ]; then
	   DNSMASQ_CONF_DIR="$$(awk -F '=' '/^conf-dir=/ {print $2}' "/tmp/etc/dnsmasq.conf.$DEFAULT_DNSMASQ_CFGID" 2>/dev/null)"
	else
	   DNSMASQ_CONF_DIR="/tmp/dnsmasq.d"
	fi
	DNSMASQ_CONF_DIR=$${DNSMASQ_CONF_DIR%*/}
	rm -rf /etc/openclash >/dev/null 2>&1
	rm -rf /etc/config/openclash >/dev/null 2>&1
	rm -rf /tmp/openclash.log >/dev/null 2>&1
	rm -rf /tmp/openclash_start.log >/dev/null 2>&1
	rm -rf /tmp/openclash.change >/dev/null 2>&1
	rm -rf /usr/share/openclash >/dev/null 2>&1
	rm -rf /tmp/openclash_version_history.json >/dev/null 2>&1
	rm -rf /tmp/openclash_cdn_info.json >/dev/null 2>&1
	rm -rf /tmp/openclash_tpl_cache >/dev/null 2>&1
	rm -rf ${DNSMASQ_CONF_DIR}/dnsmasq_openclash_custom_domain.conf >/dev/null 2>&1
	rm -rf ${DNSMASQ_CONF_DIR}/dnsmasq_openclash_chnroute_pass.conf >/dev/null 2>&1
	rm -rf ${DNSMASQ_CONF_DIR}/dnsmasq_openclash_chnroute6_pass.conf >/dev/null 2>&1
	rm -rf /tmp/etc/openclash >/dev/null 2>&1
	rm -rf /tmp/openclash_announcement >/dev/null 2>&1
	rm -rf /www/luci-static/resources/openclash >/dev/null 2>&1
	rm -rf /tmp/oix* >/dev/null 2>&1
	rm -f /etc/nginx/conf.d/openclash.conf >/dev/null 2>&1
	rm -f /etc/nginx/conf.d/openclash.locations >/dev/null 2>&1
	[ -x /etc/init.d/nginx ] && /etc/init.d/nginx reload >/dev/null 2>&1
	uci -q delete firewall.openclash
	uci -q commit firewall
	[ -f "/etc/config/ucitrack" ] && {
	uci -q delete ucitrack.@openclash[-1]
	uci -q commit ucitrack
	}
	rm -rf /tmp/luci-indexcache /tmp/luci-indexcache.* /var/luci-indexcache /var/luci-indexcache.* >/dev/null 2>&1
	rm -rf /tmp/luci-modulecache /var/luci-modulecache >/dev/null 2>&1
	exit 0
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/i18n
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/*.*.lmo $(1)/usr/lib/lua/luci/i18n/
	$(CP) $(PKG_BUILD_DIR)/root/* $(1)/
	$(CP) $(PKG_BUILD_DIR)/luasrc/* $(1)/usr/lib/lua/luci/
	find $(1)/www -type d -exec chmod 0755 {} +
	find $(1)/www -type f -exec chmod 0644 {} +
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
