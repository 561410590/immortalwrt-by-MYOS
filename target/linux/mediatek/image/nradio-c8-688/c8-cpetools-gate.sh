#!/bin/sh
# Explicit -s uses the factory switch implementation and its GPIO names.
command=
update=0
sim=
interface=cpe
OPTIND=1
while getopts "i:rRdkc:t:m:s:l:L:a:f:uhS" option; do
	case "$option" in
		i) interface="$OPTARG" ;;
		s) sim="$OPTARG" ;;
		S|r|R|d|k|l|L) exit 77 ;;
		c) command="$OPTARG" ;;
		u) update=1 ;;
		\?) exit 64 ;;
	esac
done
if [ -n "$sim" ]; then
	case "$sim" in 1|2|3) ;; *) exit 64 ;; esac
	[ "$interface" = cpe ] && [ -z "$command" ] && [ "$update" = 0 ] || exit 64
	if [ "$MYOS_C8_FACTORY_CHILD" != 1 ]; then
		exec /usr/libexec/myos-c8-native request "{\"action\":\"select_sim\",\"origin\":\"module_settings\",\"slot\":$sim}"
	fi
	/usr/libexec/myos-c8-native prepare-sim-gpio || exit $?
	MYOS_C8_SIM_CTRL_DIR=/var/run/myos-c8-sim-gpio
	export MYOS_C8_SIM_CTRL_DIR
	uci -q set cpesel.sim.mode=1 && uci -q commit cpesel || exit 1
	exec /bin/sh /opt/myos-c8-official/share/cpetools.sh "$@"
fi
case "$command" in
	usim_set|usim_reset|rstsim|reboot|cfun_c|cfun_o|reset|preinit|pdp) exit 77 ;;
esac
upper_command=$(printf '%s' "$command" | tr '[:lower:]' '[:upper:]')
case "$upper_command" in *SCICHG=*|*SIMSWITCH=*|*HVSST=*) exit 77 ;; esac
if [ "$update" = 1 ]; then
	exec /usr/libexec/myos-c8-native apply-saved
fi
exec /bin/sh /opt/myos-c8-official/share/cpetools.sh "$@"
