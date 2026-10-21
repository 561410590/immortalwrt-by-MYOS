#!/bin/sh
# Shared first-boot defaults. Source this file; callers decide boot timing.

myos_factory_wifi_mark_if_new() {
	local root="${1:-}"
	[ -e "$root/overlay/upper/etc/config/wireless" ] && return 1
	[ -e "$root/overlay/etc/config/wireless" ] && return 1
	[ -d "$root/tmp" ] || return 1
	: > "$root/tmp/myos-factory-wifi-init"
}

myos_factory_wifi_board() {
	if command -v board_name >/dev/null 2>&1; then
		board_name
	else
		cat /tmp/sysinfo/board_name 2>/dev/null
	fi
}

myos_factory_wifi_prefix() {
	case "$1" in
		nradio,nbcpe-688|HCMT7981-emmc-NB68|nradio_wt9111_No2-manper_NB68)
			printf '%s\n' NBCPE ;;
		nradio,c5800-688|HCMT7981-emmc|nradio,c2000-max|HCMT7987-EMMC|nradio,c2000-ultra|HCMT7987-SNSD)
			printf '%s\n' @NRadio ;;
		*) return 1 ;;
	esac
}

myos_factory_wifi_validate_mac() {
	local mac first
	mac="$(printf '%s' "$1" | tr 'a-f' 'A-F')"
	case "$mac" in
		[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]) ;;
		*) return 1 ;;
	esac
	case "$mac" in
		00:00:00:00:00:00|FF:FF:FF:FF:FF:FF|00:66:88:00:00:00) return 1 ;;
	esac
	first="${mac%%:*}"
	[ $((0x$first & 1)) -eq 0 ] || return 1
	printf '%s\n' "$mac"
}

myos_factory_wifi_read_blob() {
	local path="$1" version offset name value
	[ -r "$path" ] || return 1
	# Official libbdinfo: header byte 3 selects v1's 0x180-byte header;
	# current v2 data starts at byte 15. Never scan the binary header for keys.
	version="$(dd if="$path" bs=1 skip=3 count=1 2>/dev/null)"
	case "$version" in
		"$(printf '\001')") offset=384 ;;
		"$(printf '\002')") offset=15 ;;
		*) return 1 ;;
	esac
	dd if="$path" bs=65536 count=1 2>/dev/null |
		dd bs=1 skip="$offset" 2>/dev/null |
		tr '\000\377' '\n\n' |
		while IFS='=' read -r name value; do
			name="$(printf '%s' "$name" | tr -d ' \t\r')"
			[ "$name" = fac_mac ] || continue
			value="$(printf '%s' "$value" | tr -d ' \t\r')"
			myos_factory_wifi_validate_mac "$value"
			return
		done
}

myos_factory_wifi_bdinfo_paths() {
	local mtd size erase name path
	# MAX/Ultra retain factory bdinfo in SPI-NOR, even while booting from SD.
	if [ -r /proc/mtd ]; then
		while IFS=': ' read -r mtd size erase name; do
			case "$name" in
				'"bdinfo"'|'"oeminfo"') printf '/dev/%s\n' "$mtd" ;;
			esac
		done < /proc/mtd
	fi
	if command -v blkid >/dev/null 2>&1; then
		blkid -t PARTLABEL=bdinfo -o device 2>/dev/null
	fi
	[ ! -r /dev/disk/by-partlabel/bdinfo ] || printf '%s\n' /dev/disk/by-partlabel/bdinfo
	[ ! -r /mnt/app_data/bdinfo ] || printf '%s\n' /mnt/app_data/bdinfo
	return 0
}

myos_factory_wifi_mac() {
	local mac path paths
	if command -v bdinfo >/dev/null 2>&1; then
		mac="$(bdinfo -g fac_mac 2>/dev/null)"
		if myos_factory_wifi_validate_mac "$mac"; then return 0; fi
	fi
	paths="$(myos_factory_wifi_bdinfo_paths)"
	for path in $paths; do
		mac="$(myos_factory_wifi_read_blob "$path")" || continue
		if myos_factory_wifi_validate_mac "$mac"; then return 0; fi
	done
	return 1
}

myos_factory_wifi_collect() {
	local section="$1" mode networks network device band hwmode encryption guest is_guest lan=0
	config_get mode "$section" mode
	[ "$mode" = ap ] || return 0
	case "$section" in *guest*) return 0 ;; esac
	config_get guest "$section" guest 0
	config_get is_guest "$section" is_guest 0
	[ "$guest" = 1 ] && return 0
	[ "$is_guest" = 1 ] && return 0
	config_get networks "$section" network
	for network in $networks; do
		[ "$network" = lan ] && lan=1
	done
	[ "$lan" = 1 ] || return 0
	config_get device "$section" device
	[ -n "$device" ] || return 0
	case " $myos_fw_seen " in *" $device "*) return 0 ;; esac
	config_get band "$device" band
	case "$band" in
		2g|5g) ;;
		*)
			config_get hwmode "$device" hwmode
			case "$hwmode" in
				11a) band=5g ;;
				11b|11g) band=2g ;;
				*) return 0 ;;
			esac ;;
	esac
	config_get encryption "$section" encryption
	case "$encryption" in ''|none|open|wep*) encryption=psk-mixed ;; esac
	myos_fw_seen="$myos_fw_seen $device"
	myos_fw_plan="$myos_fw_plan $section:$band:$encryption"
	case "$band" in 2g) myos_fw_has2g=1 ;; 5g) myos_fw_has5g=1 ;; esac
}

myos_factory_wifi_apply() {
	local root="${1:-}" board="${2:-}" mac prefix suffix item section band encryption rest
	local myos_fw_seen='' myos_fw_plan='' myos_fw_has2g=0 myos_fw_has5g=0
	[ -f "$root/tmp/myos-factory-wifi-init" ] || return 0
	[ -n "$board" ] || board="$(myos_factory_wifi_board)"
	prefix="$(myos_factory_wifi_prefix "$board")" || return 1
	mac="$(myos_factory_wifi_mac)" || return 1
	suffix="$(printf '%s' "$mac" | tr -d ':' | cut -c 9-)"
	command -v config_load >/dev/null 2>&1 || . /lib/functions.sh
	config_load wireless || return 1
	config_foreach myos_factory_wifi_collect wifi-iface
	# All supported models have two bands. Retry before network startup if either
	# primary AP has not been generated yet; do not consume the first-boot marker.
	[ "$myos_fw_has2g" = 1 ] && [ "$myos_fw_has5g" = 1 ] || return 1
	for item in $myos_fw_plan; do
		section="${item%%:*}"
		rest="${item#*:}"
		band="${rest%%:*}"
		encryption="${rest#*:}"
		case "$band" in 2g) band=2.4G ;; 5g) band=5G ;; esac
		uci -q set "wireless.$section.ssid=$prefix-$suffix-$band" || return 1
		uci -q set "wireless.$section.key=12345678" || return 1
		uci -q set "wireless.$section.encryption=$encryption" || return 1
	done
	uci -q commit wireless || return 1
	rm -f "$root/tmp/myos-factory-wifi-init"
}
