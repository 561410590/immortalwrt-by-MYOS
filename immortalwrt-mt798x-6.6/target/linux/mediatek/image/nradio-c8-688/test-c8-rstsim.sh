#!/bin/sh
# Test the real factory function with simulated AT responses; no AT is sent.
. "$1"
gNet=cpe
AT_PRIVATE_PREFIX='AT^'
check_soc_vendor() { echo tdtech; }
_command_exec_raw() {
	case "$2" in
		'AT^HVSST=1,0') printf '+CME ERROR: 787\n' ;;
		'AT^HVSST=1,1') printf '%s\n' "$activation_reply" ;;
		*) return 1 ;;
	esac
}
activation_reply='+CME ERROR: 13'
if command_huawei_rstsim '' >/dev/null; then
	echo 'FAIL: activation error was accepted'
	exit 1
fi
activation_reply='OK'
if ! command_huawei_rstsim '' >/dev/null; then
	echo 'FAIL: successful activation was rejected'
	exit 1
fi
echo 'Activation error propagates; OK activation succeeds.'
