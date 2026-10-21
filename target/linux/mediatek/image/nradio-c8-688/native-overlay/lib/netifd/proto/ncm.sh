#!/bin/sh
# C8 reuses the DHCP protocol helper for MT5700 Ethernet E5 (CID8).
# USB is the AT control interface; the data path is eth1 through GPY211B.

[ -n "$INCLUDE_ONLY" ] || {
	. /lib/functions.sh
	. ../netifd-proto.sh
	init_proto "$@"
}

proto_ncm_init_config() {
	no_device=1
	available=1
	proto_config_add_string "device:device"
	proto_config_add_string apn
	proto_config_add_string auth
	proto_config_add_string username
	proto_config_add_string password
	proto_config_add_string pincode
	proto_config_add_string delay
	proto_config_add_string mode
	proto_config_add_string pdptype
	proto_config_add_int profile
	proto_config_add_defaults
}

dail_ethernet_core() {
	local profile="$1"
	local apn="$2"
	local auth="$3"
	local username="$4"
	local password="$5"
	local pdptype="$6"
	[ "$interface" = cpe ] && [ "$vendor" = tdtech_mt5700 ] && [ "$ifname" = eth1 ] || {
		proto_notify_error "$interface" UNSUPPORTED_MODEM
		return 1
	}
	/usr/libexec/myos-c8-native netifd-connect -- "$interface" "$apn" "$auth" "$username" "$password" "$pdptype" || {
		proto_notify_error "$interface" CONNECT_FAILED
		return 1
	}
}

proto_ncm_setup() {
	local interface="$1"
	local initialize
	local apn auth username password pincode delay mode pdptype profile profile_permanent autoconnect brmode $PROTO_DEFAULT_OPTIONS
	json_get_vars apn auth username password pincode delay mode pdptype profile profile_permanent autoconnect $PROTO_DEFAULT_OPTIONS
	[ "$interface" = cpe ] && [ "$vendor" = tdtech_mt5700 ] && [ "$ifname" = eth1 ] || {
		proto_notify_error "$interface" UNSUPPORTED_MODEM
		return 1
	}
	brmode=$(uci -q get cpebr.config.enable)
	[ -n "$brmode" ] || brmode=0
	metric=$(uci -q get "network.$interface.metric")
	[ -n "$metric" ] || metric=0
	profile=8
	[ "$pdptype" = ip -o "$pdptype" = ipv6 -o "$pdptype" = ipv4v6 ] || pdptype=ipv4v6
	/usr/libexec/myos-c8-native netifd-prepare "$interface" || {
		proto_notify_error "$interface" UNSUPPORTED_MODEM
		return 1
	}

	# The official MT5700 stanza only sets AT response/echo behaviour.
	json_load "$(cat /etc/gcom/ncm.json)"
	json_select "$vendor" || {
		proto_notify_error "$interface" UNSUPPORTED_MODEM
		return 1
	}
	json_get_values initialize initialize
	for i in $initialize; do
		case "$i" in ATQ0|ATV1|ATE1|ATS0=0) ;; *) return 1 ;; esac
		cpetools.sh -t 1 -i "$interface" -c "$i" || {
			proto_notify_error "$interface" INITIALIZE_FAILED
			return 1
		}
	done
	# Do not enter native preinit/PDP reset or automatic-policy replay paths.
	dail_ethernet_core "$profile" "$apn" "$auth" "$username" "$password" "$pdptype" || return 1

	proto_init_update "$ifname" 1
	proto_add_data
	json_add_string manufacturer "$vendor"
	proto_close_data
	proto_send_update "$interface"
	# The C8 firewall zone explicitly contains cpe, cpe_4 and cpe_6.

	[ "$pdptype" = ip -o "$pdptype" = ipv4v6 ] && {
		json_init
		json_add_string name "${interface}_4"
		json_add_string ifname "@$interface"
		if [ "$brmode" = 0 ]; then
			json_add_string proto dhcp
		else
			json_add_string proto none
		fi
		proto_add_dynamic_defaults
		json_close_object
		ubus call network add_dynamic "$(json_dump)"
	}
	[ "$pdptype" = ipv6 -o "$pdptype" = ipv4v6 ] && {
		json_init
		json_add_string name "${interface}_6"
		json_add_string ifname "@$interface"
		if [ "$brmode" = 0 ]; then
			json_add_string proto dhcpv6
		else
			json_add_string proto none
		fi
		json_add_string extendprefix 1
		proto_add_dynamic_defaults
		json_close_object
		ubus call network add_dynamic "$(json_dump)"
	}
	if [ "$brmode" != 0 ]; then
		brctl addif br-lan "$ifname"
	else
		brctl delif br-lan "$ifname"
	fi
	return 0
}

proto_ncm_teardown() {
	local interface="$1"
	/usr/libexec/myos-c8-native netifd-disconnect "$interface" || proto_notify_error "$interface" DISCONNECT_FAILED
	proto_init_update "*" 0
	proto_send_update "$interface"
}

[ -n "$INCLUDE_ONLY" ] || {
	add_protocol ncm
}
