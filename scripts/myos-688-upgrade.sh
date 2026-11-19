# MYOS 688 multi-board sysupgrade archive.
myos_688_kernel_member() {
	local board
	board="$(board_name)"
	case "$board" in
		HCMT7981-688-emmc|HCMT7981-emmc|HCMT7981-emmc-NB68)
			printf 'myos-688/%s/kernel\n' "$board"
			;;
		*) return 1 ;;
	esac
}
