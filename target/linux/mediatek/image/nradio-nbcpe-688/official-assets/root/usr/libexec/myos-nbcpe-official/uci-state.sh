#!/bin/sh
# State helpers expected by the official WWAN protocol. No persistent commit.
type uci_revert_state >/dev/null 2>&1 || uci_revert_state() {
	/sbin/uci -P /var/state revert "$1${2:+.$2}${3:+.$3}"
}
type uci_set_state >/dev/null 2>&1 || uci_set_state() {
	[ "$#" = 4 ] || return 0
	/sbin/uci -P /var/state set "$1.$2${3:+.$3}=$4"
}
type uci_get_state >/dev/null 2>&1 || uci_get_state() {
	local result
	result=$(/sbin/uci -P /var/state -q get "$1${2:+.$2}${3:+.$3}")
	[ -n "$result" ] && printf '%s\n' "$result" || printf '%s\n' "$4"
}
