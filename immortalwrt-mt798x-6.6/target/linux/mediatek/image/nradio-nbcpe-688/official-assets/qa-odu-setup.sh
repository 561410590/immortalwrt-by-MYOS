#!/bin/sh
# Host BusyBox ash + actual SDK jshn/netifd helpers, with fake cellular/ubus I/O.
set -e
SOURCE=$1
ASSETS=$SOURCE/target/linux/mediatek/image/nradio-nbcpe-688/official-assets
ROOT=$SOURCE/build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos
QA_TMP=$ASSETS/qa-build/odu
mkdir -p "$QA_TMP"
export QA_TMP
export PATH="$ASSETS/qa/bin:$SOURCE/staging_dir/host/bin:$PATH"
export QA_TRACE
. "$ROOT/usr/share/libubox/jshn.sh"
. "$ROOT/lib/netifd/utils.sh"
# Imports above are the two absolute-path imports in this exact helper.
eval "$(sed '/^\. /d' "$ROOT/lib/netifd/netifd-proto.sh")"
INCLUDE_ONLY=1
. "$ROOT/lib/netifd/proto/odu.sh"

# Source original APN/profile functions; relocate their temporary files only.
for name in setup_apn dail_adapt_driver adapt_apn_common; do
	eval "$(sed -n "/^$name(){/,/^}/p" "$ROOT/lib/netifd/proto/wwan.sh" | sed 's|/tmp/profileid_|${QA_TMP}/profileid_|g;s|/tmp/show_profile|${QA_TMP}/show_profile|g')"
done

cat() {
	if [ "$1" = /etc/gcom/ncm.json ]; then
		command cat "$ROOT/etc/gcom/ncm.json"
	else
		command cat "$@"
	fi
}
uci() {
	case "$*" in
		*network.cpe.mode) echo odu ;;
		*cpebr.config.enable) echo "$QA_BRIDGE" ;;
		*cpecfg.config.automatic|*network.cpe.automatic) echo 1 ;;
		*) return 1 ;;
	esac
}
fw4() { [ "$*" = '-q network cpe' ] && echo wan; }
ubus() {
	case "$*" in
		'call infocd cpeinfo '*) echo '{"STAT":"register","SIM":"ready","MODE":"NR"}' ;;
		'call network add_dynamic '*) printf 'dynamic\t%s\n' "$4" >> "$QA_TRACE" ;;
		*'call network.interface notify_proto '*)
			for last in "$@"; do :; done
			printf 'notify\t%s\n' "$last" >> "$QA_TRACE"
			;;
		*) printf 'unexpected ubus: %s\n' "$*" >&2; return 1 ;;
	esac
}
jsonfilter() {
	# This query belongs to fake infocd only; JSON generation uses real jshn.
	case "$2" in *STAT*) echo register ;; *SIM*) echo ready ;; *MODE*) echo NR ;; *) return 1 ;; esac
}
proto_wwan_init_pin() { return 0; }

case_index=0
for pdp in ip ipv6 ipv4v6 invalid; do
	for QA_BRIDGE in 0 1; do
		case_index=$((case_index + 1))
		QA_TRACE=$QA_TMP/case-$case_index.trace
		: > "$QA_TRACE"
		printf 'case\t%s\t%s\n' "$pdp" "$QA_BRIDGE" >> "$QA_TRACE"
		interface=cpe
		ifname=port5
		vendor=tdtech_mt5700_odu
		driver=odu
		profile_permanent=1
		printf 3 > "$QA_TMP/profileid_cpe"
		json_load "{\"apn\":\"fixture.apn\",\"auth\":\"pap\",\"username\":\"fixture\",\"password\":\"fixture\",\"pdptype\":\"$pdp\",\"profile\":3,\"profile_permanent\":1,\"metric\":7,\"peerdns\":true,\"defaultroute\":true}"
		rc=0
		proto_odu_setup cpe || rc=$?
		printf 'setup_return\t%s\n' "$rc" >> "$QA_TRACE"
	done
done
printf 'ODU_FIXTURES_EXECUTED=%s real_AT=0 network_requests=0\n' "$case_index"
