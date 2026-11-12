#!/bin/sh
set -eu

root="${1:?rootfs path required}"
release="${2:-V2.1.21}"
revision="${3:?revision required}"
signature="MYOS by ImmortalWrt ${release}"
tuna_base="https://mirrors.tuna.tsinghua.edu.cn/openwrt/releases/24.10.5"

write_distfeeds() {
	cat > "$1" <<EOF
src/gz MYOS_core ${tuna_base}/targets/mediatek/filogic/packages
src/gz MYOS_base ${tuna_base}/packages/aarch64_cortex-a53/base
src/gz MYOS_luci ${tuna_base}/packages/aarch64_cortex-a53/luci
src/gz MYOS_packages ${tuna_base}/packages/aarch64_cortex-a53/packages
src/gz MYOS_routing ${tuna_base}/packages/aarch64_cortex-a53/routing
src/gz MYOS_telephony ${tuna_base}/packages/aarch64_cortex-a53/telephony
EOF
}

mkdir -p "$root/etc/config" "$root/etc/modules.d" "$root/etc/opkg" \
	"$root/etc/uci-defaults" "$root/usr/lib"

if [ -f "$root/bin/config_generate" ]; then
	sed -i 's/192\.168\.[67]\.1/192.168.66.1/g' "$root/bin/config_generate"
	sed -i "s/set system\.@system\[-1\]\.hostname='ImmortalWrt'/set system.@system[-1].hostname='MYOS'/" "$root/bin/config_generate"
fi

write_distfeeds "$root/etc/opkg/distfeeds.conf"

cat > "$root/etc/openwrt_release" <<EOF
DISTRIB_ID='MYOS'
DISTRIB_RELEASE='${release}'
DISTRIB_REVISION='${revision}'
DISTRIB_TARGET='mediatek/filogic'
DISTRIB_ARCH='aarch64_cortex-a53'
DISTRIB_DESCRIPTION='${signature}'
DISTRIB_TAINTS='no-all busybox'
EOF

printf '%s\n' "$release" > "$root/etc/openwrt_version"

cat > "$root/usr/lib/os-release" <<EOF
NAME="MYOS"
VERSION="${release}"
ID="myos"
ID_LIKE="lede openwrt"
PRETTY_NAME="${signature}"
VERSION_ID="${release}"
BUILD_ID="${revision}"
OPENWRT_BOARD="mediatek/filogic"
OPENWRT_ARCH="aarch64_cortex-a53"
OPENWRT_TAINTS="no-all busybox"
OPENWRT_DEVICE_MANUFACTURER="NRadio"
OPENWRT_DEVICE_PRODUCT="C2000-MAX"
OPENWRT_RELEASE="${signature}"
EOF

[ -e "$root/etc/os-release" ] || ln -s ../usr/lib/os-release "$root/etc/os-release"
[ ! -f "$root/etc/config/uhttpd" ] || sed -i "s/^[[:space:]]*option[[:space:]][[:space:]]*commonname.*/\toption commonname '${signature}'/" "$root/etc/config/uhttpd"
printf '\n%s\n\n' "$signature" > "$root/etc/banner"

# The vendor bootstrap writes this file to overlay and reboots. Packaging it in
# ROM avoids an endless reboot loop when rootfs_data falls back to ramoverlay.
printf '%s\n' 'mt7993' > "$root/etc/modules.d/66-mt7993"
chmod 644 "$root/etc/modules.d/66-mt7993"
cat > "$root/etc/rc.local" <<'EOF'
# Put custom commands here. MT7993 autoload is packaged in ROM.

exit 0
EOF
chmod 644 "$root/etc/rc.local"

if [ -f "$root/usr/lib/lua/luci/version.lua" ]; then
	sed -i 's/^luciname[[:space:]]*=.*/luciname    = "LuCI MYOS branch"/' "$root/usr/lib/lua/luci/version.lua"
	sed -i "s/^luciversion[[:space:]]*=.*/luciversion = \"26.056.11316~a9f2a15 ${signature}\"/" "$root/usr/lib/lua/luci/version.lua"
fi

cat > "$root/etc/config/wireless" <<'EOF'
config wifi-device 'MT7993_1_1'
	option type 'mtwifi'
	option phy 'ra0'
	option band '2g'
	option dbdc_main '1'
	option channel 'auto'
	option txpower '100'
	option htmode 'EHT40'
	option country 'CN'
	option mu_beamformer '1'
	option noscan '1'
	option serialize '1'

config wifi-iface 'default_MT7993_1_1'
	option device 'MT7993_1_1'
	option network 'lan'
	option mode 'ap'
	option ssid 'ImmortalWrt-2.4G'
	option encryption 'none'
	option mlo '0'

config wifi-device 'MT7993_1_2'
	option type 'mtwifi'
	option phy 'rai0'
	option band '5g'
	option dbdc_main '0'
	option channel 'auto'
	option txpower '100'
	option htmode 'EHT160'
	option country 'CN'
	option mu_beamformer '1'
	option noscan '0'
	option serialize '1'

config wifi-iface 'default_MT7993_1_2'
	option device 'MT7993_1_2'
	option network 'lan'
	option mode 'ap'
	option ssid 'ImmortalWrt-5G'
	option encryption 'none'
	option mlo '0'
EOF

# MT7993 MLO station setup currently crashes in mtk_hwifi. Keep WiFi 7 enabled
# in single-link mode until the vendor MLO implementation is safe.
for profile in \
	"$root/etc/wireless/mediatek/mt7993.b0.dat" \
	"$root/etc/wireless/mediatek/mt7993.b1.dat"; do
	[ ! -f "$profile" ] || sed -i \
		-e 's/^MldGroup=.*/MldGroup=0/' \
		-e 's/^EHT_ApEmlsr_mr=.*/EHT_ApEmlsr_mr=0/' \
		"$profile"
done

cat > "$root/etc/uci-defaults/98-c2000max-disable-mlo" <<'EOF'
#!/bin/sh

[ "$(board_name)" = 'nradio,c2000-max' ] || exit 0

uci -q set wireless.MT7993_1_1.htmode='EHT40'
uci -q set wireless.MT7993_1_2.htmode='EHT160'
uci -q set wireless.default_MT7993_1_1.mlo='0'
uci -q set wireless.default_MT7993_1_2.mlo='0'
uci -q commit wireless

for profile in \
	/etc/wireless/mediatek/mt7993.b0.dat \
	/etc/wireless/mediatek/mt7993.b1.dat; do
	[ ! -f "$profile" ] || sed -i \
		-e 's/^MldGroup=.*/MldGroup=0/' \
		-e 's/^EHT_ApEmlsr_mr=.*/EHT_ApEmlsr_mr=0/' \
		"$profile"
done

exit 0
EOF
chmod 755 "$root/etc/uci-defaults/98-c2000max-disable-mlo"

fancontrol_config="$root/etc/config/fancontrol"
if [ -f "$fancontrol_config" ]; then
	sed -i \
		-e "s|^[[:space:]]*option[[:space:]]*enabled.*|\toption enabled '1'|" \
		-e "s|^[[:space:]]*option[[:space:]]*thermal_file.*|\toption thermal_file '/sys/devices/virtual/thermal/thermal_zone0/temp'|" \
		-e "s|^[[:space:]]*option[[:space:]]*fan_file.*|\toption fan_file '/sys/bus/platform/devices/pwm-fan/hwmon/hwmon1/pwm1'|" \
		-e "s|^[[:space:]]*option[[:space:]]*start_temp.*|\toption start_temp '55'|" \
		-e "s|^[[:space:]]*option[[:space:]]*start_speed.*|\toption start_speed '128'|" \
		"$fancontrol_config"
fi

cat > "$root/etc/uci-defaults/99-default-settings-chinese" <<EOF
#!/bin/sh

uci -q get system.@imm_init[0] > /dev/null || uci -q add system imm_init > /dev/null
uci -q batch <<-UCI
	set system.@system[0].timezone='CST-8'
	set system.@system[0].zonename='Asia/Shanghai'
	set system.@imm_init[0].system_chn='1'
	set system.@imm_init[0].opkg_mirror='${tuna_base}'
	set network.lan.ipaddr='192.168.66.1'
	set wireless.MT7993_1_1.channel='auto'
	set wireless.MT7993_1_1.country='CN'
	set wireless.MT7993_1_2.channel='auto'
	set wireless.MT7993_1_2.country='CN'
	commit system
	commit network
	commit wireless
UCI
cat > /etc/opkg/distfeeds.conf <<'FEEDS'
src/gz MYOS_core ${tuna_base}/targets/mediatek/filogic/packages
src/gz MYOS_base ${tuna_base}/packages/aarch64_cortex-a53/base
src/gz MYOS_luci ${tuna_base}/packages/aarch64_cortex-a53/luci
src/gz MYOS_packages ${tuna_base}/packages/aarch64_cortex-a53/packages
src/gz MYOS_routing ${tuna_base}/packages/aarch64_cortex-a53/routing
src/gz MYOS_telephony ${tuna_base}/packages/aarch64_cortex-a53/telephony
FEEDS
exit 0
EOF
chmod 755 "$root/etc/uci-defaults/99-default-settings-chinese"

turboacc="$root/etc/init.d/turboacc"
if [ -f "$turboacc" ] && ! grep -q '^set_hnat_state()' "$turboacc"; then
	awk '
		BEGIN {
			inserted = 0
			skipping_hnat_disable = 0
			skipping_service = 0
			patched_hnat_state = 0
			patched_reload = 0
			patched_stop = 0
			inside_ap_mode = 0
			close_ap_mode = 0
			patched_ap_guard = 0
			patched_wgetrc = 0
		}
		skipping_hnat_disable {
			if (/^[[:space:]]*fi[[:space:]]*$/)
				skipping_hnat_disable = 0
			next
		}
		skipping_service {
			if (/^}/)
				skipping_service = 0
			next
		}
		!inserted && /^start_service\(\)/ {
			print "set_hnat_state() {"
			print "\tlocal wanted=\"$1\""
			print "\tlocal path=\"/sys/kernel/debug/hnat/hook_toggle\""
			print "\tlocal current"
			print ""
			print "\t[ -e \"$path\" ] || return 0"
			print "\tcurrent=\"$(cat \"$path\" 2>/dev/null)\""
			print "\tcase \"$current\" in"
			print "\tenabled) current=1 ;;"
			print "\tdisabled) current=0 ;;"
			print "\t*) return 1 ;;"
			print "\tesac"
			print "\t[ \"$current\" = \"$wanted\" ] || printf \"%s\\n\" \"$wanted\" > \"$path\""
			print "}"
			print ""
			inserted = 1
		}
		/^[[:space:]]*echo 0 > \/sys\/kernel\/debug\/hnat\/hook_toggle[[:space:]]*$/ { next }
		/^[[:space:]]*echo 1 > \/sys\/kernel\/debug\/hnat\/hook_toggle[[:space:]]*$/ { next }
		/^[[:space:]]*if \[ "\$fastpath" != "mediatek_hnat" \] && lsmod \| grep -q "mtkhnat"; then[[:space:]]*$/ {
			print "\tif [ \"$fastpath\" != \"mediatek_hnat\" ]; then"
			print "\t\tset_hnat_state 0"
			print "\tfi"
			skipping_hnat_disable = 1
			next
		}
		/^[[:space:]]*echo "\$fastpath_mh_eth_hnat" > "\$hnat_path\/hook_toggle"[[:space:]]*$/ {
			print "\t\tset_hnat_state \"$fastpath_mh_eth_hnat\""
			patched_hnat_state = 1
			next
		}
		/^[[:space:]]*uci -q commit "firewall"[[:space:]]*$/ { next }
		/^[[:space:]]*\/etc\/init.d\/firewall restart >"\/dev\/null" 2>&1[[:space:]]*$/ { next }
		/^[[:space:]]*available_interfaces=""[[:space:]]*$/ {
			print "\tif [ -n \"$fastpath_mh_eth_hnat_ap\" ]; then"
			print
			inside_ap_mode = 1
			next
		}
		inside_ap_mode && /^[[:space:]]*echo "Invalid IPv4 address: \$fastpath_mh_eth_hnat_ap"[[:space:]]*$/ {
			print
			close_ap_mode = 1
			next
		}
		close_ap_mode && /^[[:space:]]*fi[[:space:]]*$/ {
			print
			print "\tfi"
			inside_ap_mode = 0
			close_ap_mode = 0
			patched_ap_guard = 1
			next
		}
		/^[[:space:]]*sed -i .*\/etc\/wgetrc[[:space:]]*$/ {
			print "\t[ -f /etc/wgetrc ] || : > /etc/wgetrc"
			print
			patched_wgetrc = 1
			next
		}
		/^reload_service\(\)/ {
			print "reload_service() {"
			print "\tstart_service"
			print "}"
			patched_reload = 1
			skipping_service = 1
			next
		}
		/^stop_service\(\)/ {
			print "stop_service() {"
			print "\tset_hnat_state 0"
			print "}"
			patched_stop = 1
			skipping_service = 1
			next
		}
		{ print }
		END {
			if (!inserted || !patched_hnat_state || !patched_reload || !patched_stop || !patched_ap_guard || !patched_wgetrc)
				exit 1
		}
	' "$turboacc" > "$turboacc.tmp"
	mv "$turboacc.tmp" "$turboacc"
	chmod 755 "$turboacc"
fi

if [ -f "$turboacc" ]; then
	sed -i 's/echo "7 \$fastpath_mh_eth_hnat_bind_rate"/echo "11 \$fastpath_mh_eth_hnat_bind_rate"/' "$turboacc"
	sh -n "$turboacc"
	grep -q '^set_hnat_state()' "$turboacc"
	grep -q 'echo "11 \$fastpath_mh_eth_hnat_bind_rate" > "\$hnat_path/hnat_setting"' "$turboacc"
	! grep -q 'echo "7 \$fastpath_mh_eth_hnat_bind_rate"' "$turboacc"
	! grep -q '^[[:space:]]*ifconfig rai\?0 down' "$turboacc"
	! grep -q '^[[:space:]]*echo [01] > /sys/kernel/debug/hnat/hook_toggle' "$turboacc"
	! grep -q '^[[:space:]]*/etc/init.d/firewall restart' "$turboacc"
fi

cat > "$root/etc/init.d/c2000max_lanip" <<'EOF'
#!/bin/sh /etc/rc.common
START=98
STOP=10

fix_lanip() {
	local board ip_changed
	board="$(cat /tmp/sysinfo/board_name 2>/dev/null)"
	[ "$board" = "nradio,c2000-max" ] || return 0
	ip_changed=0
	if [ "$(uci -q get network.lan.ipaddr)" != "192.168.66.1" ]; then
		uci -q set network.lan.ipaddr='192.168.66.1'
		ip_changed=1
	fi
	if [ "$(uci -q get network.lan.def_ipaddr)" != "192.168.66.1" ]; then
		uci -q set network.lan.def_ipaddr='192.168.66.1'
	elif [ "$ip_changed" = 0 ]; then
		return 0
	fi
	uci -q commit network
	[ "$ip_changed" = 1 ] || return 0
	ifdown lan >/dev/null 2>&1 || true
	sleep 1
	ifup lan >/dev/null 2>&1 || true
	/etc/init.d/dnsmasq restart >/dev/null 2>&1 || true
	logger -t c2000max-lanip 'forced lan ipaddr to 192.168.66.1'
}

boot() { fix_lanip; }
start() { fix_lanip; }
EOF
chmod 755 "$root/etc/init.d/c2000max_lanip"
mkdir -p "$root/etc/rc.d"
ln -sf ../init.d/c2000max_lanip "$root/etc/rc.d/S98c2000max_lanip"

exit 0
