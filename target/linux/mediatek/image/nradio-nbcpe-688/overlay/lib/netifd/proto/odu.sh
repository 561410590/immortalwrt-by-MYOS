#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
	. /lib/functions.sh
	. ../netifd-proto.sh
	init_proto "$@"
}

proto_odu_init_config() {
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

mt5700_odu_session_ready() {
	local response families
	response=$(cpetools.sh -t 0 -i "$interface" -c 'AT^NDISSTATQRY=8' 5)
	families=$(printf '%s\n' "$response" | tr -d '\r' | awk -F, '
		/^[[:space:]]*\^NDISSTATQRY:/ {
			sub(/^[^:]*:[[:space:]]*/, "")
			for (i = 1; i + 3 <= NF; i += 4) {
				state = $i; family = $(i + 3)
				gsub(/[[:space:]"]/, "", state)
				gsub(/[[:space:]"]/, "", family)
				if (state == "1") print family
			}
		}')
	case "$pdptype" in
		ip) printf '%s\n' "$families" | grep -qx IPV4 ;;
		ipv6) printf '%s\n' "$families" | grep -qx IPV6 ;;
		*) [ -n "$families" ] ;;
	esac
}

mt5700_odu_connect() {
	local changed="/tmp/myos-nbcpe-pdp-changed-$interface"
	local response
	if [ -f "$changed" ]; then
		response=$(cpetools.sh -t 0 -i "$interface" -c 'AT^NDISDUP=8,0' 5)
		if ! printf '%s\n' "$response" | tr -d '\r' | grep -qx OK; then
			echo 'MT5700 CID 8 disconnect response unavailable; continuing connect request'
		fi
	elif mt5700_odu_session_ready; then
		echo 'MT5700 CID 8 already connected; reusing data session'
		return 0
	fi
	response=$(cpetools.sh -t 0 -i "$interface" -c 'AT^NDISDUP=8,1' 5)
	# Acknowledgement is not connectivity. DHCP determines the actual address;
	# transient AT/readback failures must not prevent the data interface starting.
	if printf '%s\n' "$response" | tr -d '\r' | grep -qE '^(OK|.*DUPLICATED.*)$'; then
		rm -f "$changed"
		echo 'MT5700 CID 8 connect request submitted'
		return 0
	fi
	echo 'MT5700 CID 8 connect response unavailable; data interface will still start'
	[ "$(uci -q get network.globals.nbcpe)" = 1 ] || proto_notify_error "$interface" CONNECT_FAILED
	return 1
}

dail_odu_core(){
	local profile="$1"
	local apn="$2"
	local auth="$3"
	local username="$4"
	local password="$5"
	local pdptype="$6"
	local autoconnect="$7"


	echo "Starting network $interface"

	[ "$pdptype" = "ip" -o "$pdptype" = "ipv6" -o "$pdptype" = "ipv4v6" ] || pdptype="ipv4v6"
	[ -z "$profile" ] && profile="1"
	[ -z "$auth" ] && auth="none"

	if [ "$vendor" = tdtech_mt5700_odu ] && [ "$(uci -q get network.globals.nbcpe)" = 1 ]; then
		setup_apn "$apn" "$profile" "$auth" "$username" "$password" "$pdptype" || {
			echo 'NBCPE APN command failed; retaining the modem session and continuing data startup'
		}
		mt5700_odu_connect
		return $?
	fi
	setup_apn "$apn" "$profile" "$auth" "$username" "$password" "$pdptype" || return 1


	echo "Checking register status"
	local reg_max=3
	[ "$vendor" = "tdtech_mt5700_odu" ] && reg_max=24
	local reg_error=0
	while true;do
		local _regstat=$(ubus call infocd cpeinfo "{'name':'$interface'}")
		echo "_regstat:$_regstat"
		STAT=$(echo "$_regstat"|jsonfilter -e '$["STAT"]')
		SIM=$(echo "$_regstat"|jsonfilter -e '$["SIM"]')
		MODE=$(echo "$_regstat"|jsonfilter -e '$["MODE"]')
	
		if [ "$STAT" = "register" ]; then
			echo "register ok"
			break
		else
			
			reg_error=$((reg_error+1))
			echo "Failed to register($reg_error)"
			if [ $reg_error -ge $reg_max ];then
				proto_notify_error "$interface" CONNECT_FAILED
				return 1
			fi				
			[ "$vendor" = "tdtech_mt5700_odu" ] || proto_notify_error "$interface" CONNECT_FAILED
			sleep 5
		fi			
	done
	
	if [ "$vendor" = "tdtech_mt5700_odu" ]; then
		mt5700_odu_connect
		return $?
	fi

	local _automatic=$(uci -q get cpecfg.config.automatic)
	[ -z "$_automatic" ] && _automatic="1"
	local automatic_support=$(uci -q get "network.$interface.automatic")

	if [ "$_automatic" != "1" -o "$automatic_support" != "1" ];then
		echo "Connecting modem"
		eval cpetools.sh -t 1 -i "$interface" -c "$autoconnect" || {
			echo "Failed to connect"
			$(proto_notify_error "$interface" CONNECT_FAILED)
			return 1
		}
	fi
	return 0
}
proto_odu_setup() {
	local interface="$1"
	local pdptype_alia
	local initialize setmode connect finalize devname devpath
	local nbcpe_odu=0
	[ "$vendor" = tdtech_mt5700_odu ] && [ "$(uci -q get network.globals.nbcpe)" = 1 ] && nbcpe_odu=1

	local _mode=$(uci -q get network.${interface}.mode)
	local apn auth username password pincode delay mode pdptype profile profile_permanent autoconnect  brmode $PROTO_DEFAULT_OPTIONS
	json_get_vars apn auth username password pincode delay mode pdptype profile profile_permanent autoconnect $PROTO_DEFAULT_OPTIONS

	brmode=$(uci -q get cpebr.config.enable)
	[ -n "$brmode" ] || brmode=0

	[ "$metric" = "" ] && metric="0"

	[ -n "$profile" ] || profile=1

	[ "$pdptype" = "ip" -o "$pdptype" = "ipv6" -o "$pdptype" = "ipv4v6" ] || pdptype="ipv4v6"

	[ -n "$ifname" ] || {
		echo "The interface could not be found."
		proto_notify_error "$interface" NO_IFACE
		return 1
	}

	json_load "$(cat /etc/gcom/ncm.json)"
	json_select "$vendor"
	[ $? -ne 0 ] && {
		echo "Unsupported modem"
		proto_notify_error "$interface" UNSUPPORTED_MODEM
		return 1
	}

	if [ "$nbcpe_odu" != 1 ]; then
	json_get_values initialize initialize
	for i in $initialize; do
		if echo "$i" |grep -qs "ethernet" ;then
			cf_ifname=$(uci -q get network.$interface.ifname)
			if [ -z "$cf_ifname" ];then
				i=$(echo "$i"|sed "s/,1/,0/g")
			fi
		fi

		eval cpetools.sh -t 1 -i "$interface" -c "$i" || {
			echo "Failed to initialize modem"
			proto_notify_error "$interface" INITIALIZE_FAILED
			[ "$nbcpe_odu" = 1 ] || return 1
		}
	done

	json_get_values configure configure
	json_get_vars connect finalize
	echo "Configuring modem"
	for i in $configure; do
		eval cpetools.sh -t 1 -i "$interface"  -c "$i" || {
			echo "Failed to configure modem"
			proto_notify_error "$interface" CONFIGURE_FAILED
			[ "$nbcpe_odu" = 1 ] || return 1
		}
	done

	adapt_apn_common "$profile" "$apn" "$auth" "$username" "$password" "$pdptype" "$connect" || return 1
	fi


	echo "Setting up $ifname"
	proto_init_update "$ifname" 1
	proto_add_data
	json_add_string "manufacturer" "$vendor"
	json_add_string "pdptype" "$pdptype"
	proto_close_data
	proto_send_update "$interface"

	local zone="$(fw4 -q network "$interface" 2>/dev/null)"

	[ "$pdptype" = "ip" -o "$pdptype" = "ipv4v6" ] && {
		json_init
		json_add_string name "${interface}_4"
		json_add_string device "@$interface"

		if [ "$brmode" = "0" ]; then
			json_add_string proto "dhcp"
		else
			json_add_string proto "none"
		fi

		proto_add_dynamic_defaults
		[ -n "$zone" ] && {
			json_add_string zone "$zone"
		}
		json_close_object
		ubus call network add_dynamic "$(json_dump)"
	}

	if [ "$interface" = cpe ] && [ "$vendor" = tdtech_mt5700_odu ] &&
	   [ "$(uci -q get network.globals.nbcpe)" = 1 ] && [ "$brmode" = 0 ]; then
		# IPv6 startup does not depend on an IPv4 lease or modem readback.
		/usr/libexec/myos-nbcpe-ipv6 || echo 'NBCPE IPv6 child startup failed'
	else
	[ "$pdptype" = "ipv6" -o "$pdptype" = "ipv4v6" ] && {
		json_init
		json_add_string name "${interface}_6"
		json_add_string device "@$interface"

		if [ "$brmode" = "0" ]; then
			json_add_string proto "dhcpv6"
		else
			json_add_string proto "none"
		fi

		json_add_string extendprefix 1
		proto_add_dynamic_defaults
		[ -n "$zone" ] && {
			json_add_string zone "$zone"
		}
		json_close_object
		ubus call network add_dynamic "$(json_dump)"
	}
	fi

	if [ "$nbcpe_odu" = 1 ]; then
		# E5 uses CID 8 (MT5700 application guide 3.2.2). Start DHCP first;
		# AT availability and dial acknowledgements do not gate the data link.
		dail_odu_core "$profile" "$apn" "$auth" "$username" "$password" "$pdptype" "$connect" ||
			echo 'NBCPE dial response incomplete; DHCP remains running'
	fi

	[ -n "$finalize" ] && {		
		eval cpetools.sh -t 1 -i "$interface" -c "$finalize" || {
			echo "Failed to configure modem"
			proto_notify_error "$interface" FINALIZE_FAILED
			return 1
		}
	}
	return 0
}

proto_odu_teardown() {
	local interface="$1"
	local _automatic=$(uci -q get cpecfg.config.automatic)
	local automatic_support=$(uci -q get "network.$interface.automatic")
	local work_mode=$(uci -q get "network.$interface.mode")
	
	local disconnect
	local profileid=""
	local profile
	[ -f "/tmp/profileid_${interface}" ] && profileid=$(cat /tmp/profileid_${interface})
	json_get_vars profile
	[ -n "$profileid" ] && profile="$profileid"
	[ -n "$profile" ] || profile=1
	[ -z "$_automatic" ] && _automatic="1"
	echo "Stopping network $interface"
	if [ "$work_mode" = "odu" ] && [ "$(uci -q get network.globals.nbcpe)" = "1" ] &&
	   [ "$vendor" = "tdtech_mt5700_odu" ]; then
		# netifd removes its DHCP children. Keep the modem's reusable CID 8
		# session; the legacy disconnect command is only AT, not a hangup.
		proto_init_update "*" 0
		proto_send_update "$interface"
		return 0
	fi

	json_load "$(cat /etc/gcom/ncm.json)"
	json_select "$vendor" || {
		echo "Unsupported modem"
		proto_notify_error "$interface" UNSUPPORTED_MODEM
		return 1
	}

	if [ "$_automatic" != "1" -o "$automatic_support" != "1" ];then
		json_get_vars disconnect	
		[ -n "$disconnect" ] && {
			eval cpetools.sh -t 1 -i "$interface" -c "$disconnect" || {
				echo "Failed to disconnect"
				proto_notify_error "$interface" DISCONNECT_FAILED
				return 1
			}
		}
	fi

	proto_init_update "*" 0
	proto_send_update "$interface"
}
[ -n "$INCLUDE_ONLY" ] || {
	add_protocol odu
}
