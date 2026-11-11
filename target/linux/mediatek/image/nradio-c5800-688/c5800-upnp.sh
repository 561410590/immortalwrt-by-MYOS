#!/bin/sh
# Follow the active LAN policy without changing routing or modem state.
c5800_upnp_device() {
	local policy iface device
	policy="$(uci -q get mwan3.lan_default.use_policy)"
	if [ -n "$policy" ]; then
		iface="$(ubus call mwan3 status '{"section":"policies"}' |
			jsonfilter -l 1 -e "@.policies.ipv4['$policy'][@.percent=100].interface")"
		[ -n "$iface" ] && network_get_device device "$iface"
	fi
	if [ -z "$device" ]; then
		device="$(ip -4 route get 1.1.1.1 2>/dev/null |
			sed -n 's/.* dev \([^ ]*\).*/\1/p' | head -n 1)"
	fi
	if [ -z "$device" ]; then
		network_find_wan iface && network_get_device device "$iface"
	fi
	[ -n "$device" ] && printf '%s\n' "$device"
}
