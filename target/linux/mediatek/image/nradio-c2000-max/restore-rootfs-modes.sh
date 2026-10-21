#!/bin/sh
set -eu

root="${1:?rootfs path required}"
pseudo="${2:-}"

find "$root" -type d -exec chmod 755 {} +

[ ! -e "$root/bin/busybox" ] || chmod 755 "$root/bin/busybox"
[ ! -e "$root/bin/opkg" ] || chmod 755 "$root/bin/opkg"
[ ! -e "$root/sbin/init" ] || chmod 755 "$root/sbin/init"
[ ! -e "$root/etc/preinit" ] || chmod 755 "$root/etc/preinit"
[ ! -e "$root/etc/passwd" ] || chmod 644 "$root/etc/passwd"
[ ! -e "$root/usr/lib/os-release" ] || chmod 644 "$root/usr/lib/os-release"
[ ! -e "$root/etc/shadow" ] || chmod 600 "$root/etc/shadow"

for path in etc/init.d usr/bin usr/sbin sbin bin; do
	[ -e "$root/$path" ] || continue
	find "$root/$path" -type f -exec chmod 755 {} +
done

for path in etc/config usr/lib/lua/luci www/luci-static usr/share/luci/menu.d; do
	[ -e "$root/$path" ] || continue
	find "$root/$path" -type d -exec chmod 755 {} +
	find "$root/$path" -type f -exec chmod 644 {} +
done

if [ -n "$pseudo" ] && [ -f "$pseudo" ]; then
	awk '
	function valid_mode(value) {
		return value ~ /^[0-7][0-7][0-7][0-7]?$/
	}
	function valid_number(value) {
		return value ~ /^[0-9]+$/
	}
	{
		for (i = 2; i <= NF - 4; i++) {
			if (($i == "D" || $i == "R") && valid_number($(i + 1)) && valid_mode($(i + 2)) &&
			    valid_number($(i + 3)) && valid_number($(i + 4))) {
				path = $1
				for (j = 2; j < i; j++) {
					path = path " " $j
				}
				print path "\t" $(i + 2)
				next
			}
		}
	}' "$pseudo" | while IFS='	' read -r path mode; do
		if [ "$path" = "/" ]; then
			target="$root"
		else
			target="$root/$path"
		fi
		[ -e "$target" ] || continue
		[ -n "$mode" ] || continue
		chmod "$mode" "$target"
	done
	printf '%s\n' 'dev d 755 0 0' 'dev/console c 0600 0 0 5 1' > "$pseudo"
elif [ -n "$pseudo" ]; then
	printf '%s\n' 'dev d 755 0 0' 'dev/console c 0600 0 0 5 1' > "$pseudo"
fi

for path in lib/modules etc/modules.d etc/modules-boot.d lib/firmware; do
	[ -d "$root/$path" ] || continue
	find "$root/$path" -type d -exec chmod 755 {} +
	find "$root/$path" -type f -exec chmod 644 {} +
	bad_mode="$(find "$root/$path" -type f ! -perm 0644 -print -quit)"
	[ -z "$bad_mode" ] || {
		echo "C2000MAX invalid mode for $bad_mode" >&2
		exit 1
	}
done
