#!/bin/ash
append DRIVERS "mt7981"

prepare_mt7981() {
	#prepare_ralink_wifi mt7981
	:
}

scan_mt7981() {
	#scan_ralink_wifi mt7981 mt7981
	:
}

fixup_mt7981() {
	echo "not support"
}

disable_mt7981() {
	local _vif=

	cd /sys/class/net/ || return
	for _vif in apcli*; do
		if echo "$_vif"|grep -q -e "apcli"; then
			ifconfig "$_vif" down
		fi
	done
	for _vif in ra*; do
		if echo "$_vif"|grep -q -e "ra"; then
			ifconfig "$_vif" down
			ubus call network.interface.lan remove_device "{'name':'$_vif'}"
		fi
	done
}

enable_mt7981() {
	local _vif=
	local _tmp=

	for _vif in ra0 rax0 rai0; do
		iwinfo "$_vif" info|grep -q mt7981 && ifconfig "$_vif" up
	done

	cd /sys/class/net/ || return
	for _vif in ra*; do
		if echo "$_vif"|grep -q -e "ra"; then
			ifconfig "$_vif" up
			ubus call network.interface.lan add_device "{'name':'$_vif'}"

			_tmp=$(brctl show|grep "$_vif")
			if [ -z "$_tmp" ]; then
				echo "add $_vif to br-lan" >> /tmp/wifi.log
				brctl addif br-lan "$_vif"
			fi
		fi
	done
	for _vif in apcli*; do
		if echo "$_vif"|grep -q -e "apcli"; then
			ifconfig "$_vif" up
		fi
	done
}

generate_config() {
	local devidx="$1"
	local prefix="$2"
	local hwmode="$3"
	local radio="$4"
	local wlan="$5"
	local htmode="HE20"
	local band="$6"
	local band_str="2g"
	local idx=0

	[ "$band" = "1" ] && {
		htmode="HE80"
		band_str="5g"
	}

	cat <<EOF
config wifi-device  $radio
	option type     mac80211
	option vendor   ralink
	option phyname  mt7981
	option hwmode   $hwmode
	option band     $band_str
	option htmode   $htmode
	option channel  0
	option devidx   $devidx
	option autoch   3
	option g256qam  1
	option e2paccmode 2
	option bandidx  $band

EOF
	for suf in "" "_0" "_1" "_2"; do
		cat <<EOF
config wifi-iface   ${wlan}${suf}
	option device   $radio
	option ifname   ${prefix}${idx}
	option network  lan
	option mode     ap
	option ssid     SSID${suf}
	option encryption none
	option disabled 1

EOF
		idx=$((idx+1))
	done
}

detect_mt7981() {
	[ -s "/etc/config/wireless" ] && return

	cat /proc/bus/pci/devices|grep -q mt7981 && {
		generate_config "0" "ra" "11ax" "radio0" "wlan0" "0"
		generate_config "0" "rax" "11ax" "radio1" "wlan1" "1"
	}
}

detect_old_mt7981() {
	detect_mt7981
}
