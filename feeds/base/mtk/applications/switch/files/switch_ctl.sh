#!/bin/sh

[ ! -x /usr/sbin/switch ] && return

sw_read_reg0() {
	local port="$1"
	local value hex

	value=$(switch phy cl22 r "$port" 0 2>/dev/null | awk -F'=' 'NF {gsub(/^[ \t]+|[ \t]+$/, "", $NF); print $NF; exit}')
	case "$value" in
		0[xX]*)
			hex=${value#0x}
			hex=${hex#0X}
			case "$hex" in
				''|*[!0-9a-fA-F]*)
					logger -t switch_ctl "skip port $port: invalid reg0 '$value'"
					return 1
					;;
			esac
			;;
		''|*[!0-9]*)
			logger -t switch_ctl "skip port $port: invalid reg0 '$value'"
			return 1
			;;
	esac

	printf '%s\n' "$value"
}

# power off ports
# $1 ports list, ex: "1 2 3"
sw_poweroff_ports() {
	local ori_value
	local set_value
	[ -z "$1" ] && return 1
	for p in $1; do
		# read original value of register 0
		ori_value=$(sw_read_reg0 "$p") || continue

		# register 0, bit 11 is power down control bit, set to 1
		set_value=$((ori_value | 0x800))
		switch phy cl22 w "$p" 0 "$set_value" >/dev/null 2>&1
	done
}

# power on ports
# $1 ports list, ex: "1 2 3"
sw_poweron_ports() {
	local ori_value
	local set_value
	[ -z "$1" ] && return 1
	for p in $1; do
		# read original value of register 0
		ori_value=$(sw_read_reg0 "$p") || continue

		# register 0, bit 11 is power down control bit, set to 0
		set_value=$((ori_value & ~0x800))
		switch phy cl22 w "$p" 0 "$set_value" >/dev/null 2>&1
	done
}
# restart Auto-neg on ports
# $1 ports list, ex: "1 2 3"
sw_restart_port() {
	local ori_value
	local set_value
	[ -z "$1" ] && return 1
	for p in $1; do
		# read original value of register 0
		ori_value=$(sw_read_reg0 "$p") || continue

		# register 0, bit 9 is Restart Auto-Negotiation bit, set to 1
		set_value=$((ori_value | 0x200))
		switch phy cl22 w "$p" 0 "$set_value" >/dev/null 2>&1
	done
}
