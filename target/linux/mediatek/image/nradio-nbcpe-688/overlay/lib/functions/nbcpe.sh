# NBCPE identity comes from this board's original bdinfo data.
nbcpe_valid_mac() {
	local mac="$1" compact first
	[ "${#mac}" -eq 17 ] || return 1
	case "$mac" in ??:??:??:??:??:??) ;; *) return 1 ;; esac
	compact="${mac//:/}"
	case "$compact" in *[!0-9a-fA-F]*|000000000000|ffffffffffff|FFFFFFFFFFFF) return 1 ;; esac
	first="${compact%??????????}"
	[ "$((0x$first & 1))" -eq 0 ]
}

nbcpe_factory_mac() {
	local mac key
	command -v bdinfo >/dev/null 2>&1 || return 1
	for key in fac_mac mac; do
		mac="$(bdinfo -g "$key" 2>/dev/null | tr A-F a-f)"
		nbcpe_valid_mac "$mac" || continue
		echo "$mac"
		return 0
	done
	return 1
}
