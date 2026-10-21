#!/bin/sh
# Run from the extracted recovery archive on the stock system, or on Linux PC.
base="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)" || exit 1
disk="$1"
[ -n "$disk" ] || disk=/dev/mmcblk0
state="$("$base/myos-max-gpt" status "$disk")" || { printf '%s\n' "$state"; exit 1; }
if printf '%s' "$state" | grep -q '"layout":"official"'; then
	[ "$disk" = /dev/mmcblk0 ] || exit 1
	if [ -x /usr/bin/ubenv ]; then
		envtool=/usr/bin/ubenv
	else
		envtool="$base/myos-max-ubenv"
	fi
	"$envtool" -o 0 -s boot_from_sd -v 1 || exit 1
	"$envtool" -o 1 -s boot_from_sd | grep -q 'boot_from_sd = 1' || exit 1
else
	"$base/myos-max-gpt" myos "$disk" || exit 1
fi
sync
echo 'MAX SD boot is restored. Reboot the router with the SD card inserted.'

