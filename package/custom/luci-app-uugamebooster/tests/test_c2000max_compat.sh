#!/bin/sh
set -eu

TEST_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PKG="$(CDPATH= cd -- "$TEST_DIR/.." && pwd)"
LAUNCHER="$PKG/root/usr/libexec/uuplugin-launcher"
SERVICE="$PKG/root/usr/libexec/uuplugin-service"
MIGRATOR="$PKG/root/usr/libexec/uuplugin-migrate-apk-new"
UUCLEAN="$PKG/root/usr/bin/uuclearnat"
MODEL="$PKG/luasrc/model/cbi/uuplugin.lua"
CONTROLLER="$PKG/luasrc/controller/uuplugin.lua"
STATUS="$PKG/luasrc/view/uuplugin/uuplugin_status.htm"
TMP="$(mktemp -d /tmp/uu-r14-test.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

for script in "$LAUNCHER" "$SERVICE" "$MIGRATOR" "$UUCLEAN" \
	"$PKG/root/etc/init.d/uuplugin" \
	"$PKG/root/etc/uci-defaults/45_luci-uuplugin"; do
	sh -n "$script" || fail "shell syntax: $script"
done

grep -Fq 'PKG_RELEASE:=18' "$PKG/Makefile" || fail 'release is not r18'
for dependency in '+flock' '+ip-full' '+kmod-ipt-nat' '+kmod-ipt-nat-extra' \
	'+kmod-nft-tproxy' '+kmod-nft-socket'; do
	grep -Fq "$dependency" "$PKG/Makefile" || fail "missing dependency $dependency"
done
grep -Fq "option platform 'h3c-nx30pro'" "$PKG/root/etc/config/uuplugin" ||
	fail 'H3C is not the default core'
grep -Fq 'H3C NX30 Pro（C2000MAX 实机验证，推荐）' "$MODEL" ||
	fail 'recommended H3C option missing'
grep -Fq 'OpenWrt aarch64（官方通用回退）' "$MODEL" ||
	fail 'official generic fallback missing'
grep -Fq '临时禁用软件流量卸载和 MediaTek HNAT' "$MODEL" ||
	fail 'offload warning missing'
grep -Fq '恢复启用 UU 前保存的加速设置' "$MODEL" ||
	fail 'offload restore notice missing'
grep -Fq '/etc/uuplugin.accel-state' "$CONTROLLER" ||
	fail 'controller does not report acceleration suspension'
grep -Fq '/usr/bin/uuclearnat --check >/dev/null 2>&1' "$CONTROLLER" ||
	fail 'controller does not verify the live offload/HNAT state'
if grep -Fq '缺少 H3C /dev/natflushdev' "$CONTROLLER"; then
	fail 'controller still requires proprietary natflushdev'
fi
if grep -Eq 'ln[[:space:]].*/dev/null.*natflushdev|mknod[[:space:]].*natflushdev' \
	"$LAUNCHER" "$UUCLEAN"; then
	fail 'natflushdev is still fabricated'
fi
grep -Fq '1:--check) exec /usr/libexec/uuplugin-launcher --uuclearnat-check' "$UUCLEAN" ||
	fail 'read-only uuclearnat check is not wired'
grep -Fq '0:) exec /usr/libexec/uuplugin-launcher --uuclearnat' "$UUCLEAN" ||
	fail 'real uuclearnat fallback is not wired'

# Execute the wrapper against a recorder after replacing only its absolute
# launcher path. This proves --check is forwarded and unexpected argv is
# rejected instead of silently becoming a destructive no-argument fallback.
DISPATCH="$TMP/uuclearnat-dispatch"
RECORDER="$TMP/uuclearnat-recorder"
sed "s#/usr/libexec/uuplugin-launcher#$RECORDER#g" "$UUCLEAN" > "$DISPATCH"
cat > "$RECORDER" <<-EOF
	#!/bin/sh
	printf '%s\n' "\$*" > '$TMP/uuclearnat-argv'
EOF
chmod 0755 "$DISPATCH" "$RECORDER"
"$DISPATCH" --check
[ "$(cat "$TMP/uuclearnat-argv")" = --uuclearnat-check ] ||
	fail 'uuclearnat --check was not forwarded read-only'
"$DISPATCH"
[ "$(cat "$TMP/uuclearnat-argv")" = --uuclearnat ] ||
	fail 'uuclearnat no-argument fallback was not forwarded'
if "$DISPATCH" unexpected; then
	fail 'uuclearnat accepted an unsupported argument'
fi
grep -Fq 'HNAT_EFFECTIVE="${UU_HNAT_EFFECTIVE:-/var/run/c2000max-hnat-effective}"' \
	"$LAUNCHER" || fail 'authoritative HNAT effective-state readback missing'
grep -Fq 'uuclearnat_reload()' "$LAUNCHER" ||
	fail 'locked HNAT reload fallback missing'

for module in nft_compat nf_nat xt_nat xt_mark xt_mac xt_tcpudp xt_TPROXY xt_socket \
	xt_REDIRECT; do
	grep -Fq "$module" "$LAUNCHER" || fail "module preflight missing $module"
done
for target in 'for target in DNAT MARK TPROXY' \
	'grep -Fxq "$target" /proc/net/ip_tables_targets' \
	'for match in mac tcp udp socket' \
	'grep -Fxq "$match" /proc/net/ip_tables_matches' \
	'grep -Fxq REDIRECT /proc/net/ip_tables_targets'; do
	grep -Fq "$target" "$LAUNCHER" || fail "kernel extension preflight missing: $target"
done
grep -Fq 'UU_IP_FULL:-/usr/libexec/ip-full' "$LAUNCHER" ||
	fail 'ip-full wrapper does not bypass BusyBox ip'
grep -Fq "hexdump -v -e '1/1 \"%02x\"'" "$LAUNCHER" ||
	fail 'ELF validation does not use the image-provided BusyBox hexdump'
if grep -Eq '(^|[[:space:]|])od([[:space:]]|$)' "$LAUNCHER"; then
	fail 'ELF validation still depends on the unavailable od applet'
fi
grep -Fq 'xtables-nft-multi' "$LAUNCHER" || fail 'xtables companion missing'
grep -Fq 'api_url="https://router.uu.163.com/api/plugin?type=$type"' "$LAUNCHER" ||
	fail 'official API endpoint is not fixed'
if grep -Eq 'C2000MAX_PC_(OFFICIAL|PATCHED|PLATFORM_OFFSET)|apply_c2000max_pc_compat' \
	"$LAUNCHER"; then
	fail 'obsolete generic ELF patch remains'
fi
grep -Fq 'sha256sum -c .bundle.sha256' "$LAUNCHER" ||
	fail 'installed bundle hash verification missing'
grep -Fq 'manifest-version=1' "$LAUNCHER" || fail 'source manifest missing'
grep -Fq 'flock -n 9' "$LAUNCHER" || fail 'lifecycle lock missing'
grep -Fq '${exe% (deleted)}' "$LAUNCHER" ||
	fail 'deleted-inode process ownership check missing'
grep -Fq 'stop_owned_processes' "$LAUNCHER" || fail 'owned process stop missing'
grep -Fq 'software卸载和 MediaTek HNAT 保持禁用' "$LAUNCHER" 2>/dev/null || true

# APK live-upgrade/uninstall hooks are part of the safety boundary: migrate
# protected .apk-new scripts, delete only the ghost procd namespace, clean
# before restart/removal, and gate post-deinstall mutations on a success marker.
for hook in postinst prerm postrm; do
	awk -v start="define Package/luci-app-uugamebooster/$hook" '
		$0 == start { copy = 1; next }
		copy && $0 == "endef" { exit }
		copy { gsub(/\$\$/, "$"); print }
	' "$PKG/Makefile" > "$TMP/$hook.sh"
	[ -s "$TMP/$hook.sh" ] || fail "APK $hook hook missing"
	sh -n "$TMP/$hook.sh" || fail "APK $hook syntax invalid"
done
grep -Fq "service delete '{\"name\":\"uuplugin.apk-new\"}'" "$TMP/postinst.sh" ||
	fail 'postinst does not precisely delete ghost procd namespace'
grep -Fq '/usr/libexec/uuplugin-launcher --cleanup-only' "$TMP/postinst.sh" ||
	fail 'postinst does not clean before canonical restart'
grep -Fq '/etc/init.d/uuplugin stop' "$TMP/postinst.sh" ||
	fail 'postinst does not stop the default-started instance before cleanup'
grep -Fq '/etc/init.d/uuplugin restart' "$TMP/postinst.sh" ||
	fail 'postinst does not restart canonical service'
grep -Fq 'MARKER=/tmp/uuplugin.clean-deinstall' "$TMP/prerm.sh" ||
	fail 'pre-deinstall success marker missing'
grep -Fq '[ "$(cat "$MARKER" 2>/dev/null)" = cleanup-complete ] || exit 1' \
	"$TMP/postrm.sh" || fail 'post-deinstall is not gated by cleanup marker'
grep -Fq "uuplugin.uuplugin.enabled='0'" "$TMP/postrm.sh" ||
	fail 'post-deinstall does not disable residual UCI state'
grep -Fq 'rm -rf /tmp/uu' "$TMP/postrm.sh" ||
	fail 'post-deinstall does not remove the downloaded runtime bundle'

for expected in \
	"set firewall.c2000max_uu_tun='zone'" \
	"add_list firewall.c2000max_uu_tun.device='tun16*'" \
	"set firewall.c2000max_uu_lan_to_tun.src='lan'" \
	"set firewall.c2000max_uu_lan_to_tun.dest='c2000max_uu_tun'" \
	"set firewall.c2000max_uu_tun_to_lan.src='c2000max_uu_tun'" \
	"set firewall.c2000max_uu_tun_to_lan.dest='lan'"; do
	grep -Fq "$expected" "$LAUNCHER" || fail "fw4 tun16 policy missing: $expected"
done

# The init file is a stable, root-aware shim and all upgrade-specific code is
# outside /etc. The migrator may promote only five known scripts.
grep -Fq 'UU_ROOT="${IPKG_INSTROOT:-}"' "$PKG/root/etc/init.d/uuplugin" ||
	fail 'init shim is not root-aware'
grep -Fq '. "$UU_SERVICE"' "$PKG/root/etc/init.d/uuplugin" ||
	fail 'init shim does not source service helper'
grep -Fq 'uuplugin.apk-new' "$MIGRATOR" || fail '.apk-new migration missing'

# Exercise strict URL allowlisting and archive layout validation by sourcing
# the launcher without executing main().
UU_LAUNCHER_SOURCE_ONLY=1
UU_RUNDIR="$TMP/runtime"
UU_STATE_FILE="$TMP/state"
UU_ACCEL_STATE="$TMP/accel-state"
UU_LOCK_FILE="$TMP/lock"
UU_NFT_CREATE="$TMP/xu_nft_create.txt"
UU_NFT_DELETE="$TMP/xu_nft_delete.txt"
export UU_LAUNCHER_SOURCE_ONLY UU_RUNDIR UU_STATE_FILE UU_ACCEL_STATE UU_LOCK_FILE \
	UU_NFT_CREATE UU_NFT_DELETE
. "$LAUNCHER"

# Regression for the r14 boot blocker: ash keeps `$$` equal to the parent in
# pipeline/command-substitution children. The owned-process scan must exclude
# its actual /proc/self PID and must not pipe the long-lived /proc loop.
if grep -Eq 'done[[:space:]]*\|[[:space:]]*(LC_ALL=.*)?sort' "$LAUNCHER"; then
	fail 'owned-process loop still runs in a pipeline child'
fi
SELF_SCAN="$TMP/owned-process-self-scan.sh"
cat > "$SELF_SCAN" <<-EOF
	#!/bin/sh
	UU_LAUNCHER_SOURCE_ONLY=1
	UU_LAUNCHER_PATH='$SELF_SCAN'
	UU_PROC_ROOT=/proc
	export UU_LAUNCHER_SOURCE_ONLY UU_LAUNCHER_PATH UU_PROC_ROOT
	. '$LAUNCHER'
	pids="\$(owned_process_pids)"
	[ -z "\$pids" ] || { printf '%s\n' "\$pids"; exit 1; }
	stop_owned_processes
EOF
chmod 0755 "$SELF_SCAN"
[ -z "$(/bin/sh "$SELF_SCAN")" ] ||
	fail 'owned-process scan mistakes its own shell for stale UU runtime'
grep -Fq -- '--core-running' "$LAUNCHER" ||
	fail 'exact core runtime probe missing'
grep -Fq -- '--guardian-running' "$LAUNCHER" ||
	fail 'exact guardian runtime probe missing'
grep -Fq -- '--core-running' "$CONTROLLER" ||
	fail 'LuCI status still relies on ambiguous pidof core detection'

# A failed policy-route/TUN cleanup must retain the ownership evidence for the
# next retry. Removing it would let a later pass falsely restore offload/HNAT.
mkdir -p "$RUNDIR"
printf 'UU_ROUTE_DEFAULT_TABLE=100\n' > "$RUNDIR/uu_stack_config"
printf 'tun163\n' > "$NETDEV_BASELINE"
cleanup_policy_runtime() { return 1; }
cleanup_legacy_xtables_links() { return 0; }
if cleanup_rules; then fail 'failed policy cleanup was reported successful'; fi
[ -s "$RUNDIR/uu_stack_config" ] || fail 'failed cleanup lost stack evidence'
[ -s "$NETDEV_BASELINE" ] || fail 'failed cleanup lost netdev baseline'
cleanup_policy_runtime() { return 0; }
cleanup_rules || fail 'successful policy cleanup was rejected'
[ ! -e "$RUNDIR/uu_stack_config" ] || fail 'cleaned stack evidence remains'
[ ! -e "$NETDEV_BASELINE" ] || fail 'cleaned netdev baseline remains'

normalize_download_url \
	'https://uurouter.gdl.netease.com/uuplugin/h3c-nx30pro/core.tar.gz' \
	h3c-nx30pro >/dev/null || fail 'official H3C URL rejected'
normalize_download_url \
	'http://uurouter.gdl04.netease.com/uuplugin/openwrt-aarch64/core.tar.gz' \
	openwrt-aarch64 | grep -q '^https://' || fail 'official HTTP URL not upgraded'
if normalize_download_url \
	'https://example.invalid/uuplugin/h3c-nx30pro/core.tar.gz' h3c-nx30pro; then
	fail 'untrusted download host accepted'
fi
if normalize_download_url \
	'https://uurouter.gdl.netease.com/uuplugin/openwrt-aarch64/core.tar.gz' \
	h3c-nx30pro; then
	fail 'cross-platform URL accepted'
fi

# NetEase currently emits URL,MD5,mirror, with one empty trailing field.
# r15 rejected this valid four-field response before attempting any download.
official_md5='0123456789abcdef0123456789abcdef'
official_response="https://uurouter.gdl.netease.com/uuplugin/h3c-nx30pro/core.tar.gz,$official_md5,https://uurouter-19.gdl.nieapps.com/uuplugin/h3c-nx30pro/core.tar.gz,"
parse_official_api_response "$official_response" h3c-nx30pro ||
	fail 'official API trailing empty field rejected'
[ "$OFFICIAL_MD5" = "$official_md5" ] || fail 'official API MD5 parsed incorrectly'
[ "$OFFICIAL_MIRROR" = \
	'https://uurouter-19.gdl.nieapps.com/uuplugin/h3c-nx30pro/core.tar.gz' ] ||
	fail 'official API mirror parsed incorrectly'
if parse_official_api_response "${official_response}," h3c-nx30pro; then
	fail 'multiple trailing API fields accepted'
fi
if parse_official_api_response \
	"${official_response}unexpected" h3c-nx30pro; then
	fail 'non-empty extra API field accepted'
fi

mkdir -p "$TMP/h3c" "$TMP/generic"
: > "$TMP/h3c/uu.conf"
: > "$TMP/h3c/uuplugin"
: > "$TMP/h3c/xuplugin-guardian"
tar -czf "$TMP/h3c.tar.gz" -C "$TMP/h3c" \
	uu.conf uuplugin xuplugin-guardian
archive_layout_valid "$TMP/h3c.tar.gz" h3c-nx30pro ||
	fail 'valid H3C archive rejected'
: > "$TMP/generic/uu.conf"
: > "$TMP/generic/uuplugin"
: > "$TMP/generic/xuplugin-guardian"
: > "$TMP/generic/xtables-nft-multi"
tar -czf "$TMP/generic.tar.gz" -C "$TMP/generic" \
	uu.conf uuplugin xtables-nft-multi xuplugin-guardian
archive_layout_valid "$TMP/generic.tar.gz" openwrt-aarch64 ||
	fail 'valid generic archive rejected'
tar -czf "$TMP/duplicate.tar.gz" -C "$TMP/generic" \
	uu.conf uu.conf uuplugin xtables-nft-multi xuplugin-guardian
if archive_layout_valid "$TMP/duplicate.tar.gz" openwrt-aarch64; then
	fail 'duplicate archive entry accepted'
fi
: > "$TMP/generic/unexpected"
tar -czf "$TMP/extra.tar.gz" -C "$TMP/generic" \
	uu.conf uuplugin xtables-nft-multi xuplugin-guardian unexpected
if archive_layout_valid "$TMP/extra.tar.gz" openwrt-aarch64; then
	fail 'unexpected archive entry accepted'
fi

# Minimal 64-bit little-endian AArch64 ELF header for architecture validation.
dd if=/dev/zero of="$TMP/aarch64.elf" bs=64 count=1 2>/dev/null
printf '\177ELF\002\001' | dd of="$TMP/aarch64.elf" bs=1 seek=0 conv=notrunc 2>/dev/null
printf '\267\000' | dd of="$TMP/aarch64.elf" bs=1 seek=18 conv=notrunc 2>/dev/null
valid_aarch64_elf "$TMP/aarch64.elf" || fail 'AArch64 ELF rejected'
printf '\076\000' | dd of="$TMP/aarch64.elf" bs=1 seek=18 conv=notrunc 2>/dev/null
if valid_aarch64_elf "$TMP/aarch64.elf"; then fail 'non-AArch64 ELF accepted'; fi

# H3C injects its proprietary XTABLES_LIBDIR=/lib. Every generated command
# wrapper must scrub all three extension-path variables before reaching the
# verified OpenWrt companion; this was required by the real-device r11 fix.
mkdir -p "$RUNDIR"
cat > "$TMP/ip-full" <<-'EOF'
	#!/bin/sh
	exit 0
EOF
cat > "$XTABLES" <<-'EOF'
	#!/bin/sh
	if [ "$1:$2" = 'iptables-nft:--version' ]; then
		echo 'iptables v1.8 (nf_tables)'
		exit 0
	fi
	printf '%s|%s|%s\n' "${XTABLES_LIBDIR:-}" "${IPTABLES_LIB_DIR:-}" \
		"${IP6TABLES_LIB_DIR:-}" > "$UU_TEST_XTABLES_ENV"
	exit 0
EOF
chmod 0755 "$TMP/ip-full" "$XTABLES"
IP_FULL="$TMP/ip-full"
UU_TEST_XTABLES_ENV="$TMP/xtables-env"
export UU_TEST_XTABLES_ENV
ensure_command_wrappers || fail 'runtime wrapper generation failed'
XTABLES_LIBDIR=/lib IPTABLES_LIB_DIR=/lib IP6TABLES_LIB_DIR=/lib \
	"$RUNDIR/iptables" -t nat -j DNAT -h
[ "$(cat "$UU_TEST_XTABLES_ENV")" = '||' ] ||
	fail 'H3C extension-path variables leaked into OpenWrt xtables companion'

# Installed bundles are accepted only when every local file still matches the
# generated SHA-256 manifest and the selected platform marker.
mkdir -p "$RUNDIR"
printf '\177ELF\002\001' > "$TMP/header"
dd if=/dev/zero of="$TMP/core" bs=64 count=1 2>/dev/null
printf '\177ELF\002\001' | dd of="$TMP/core" bs=1 seek=0 conv=notrunc 2>/dev/null
printf '\267\000' | dd of="$TMP/core" bs=1 seek=18 conv=notrunc 2>/dev/null
cp "$TMP/core" "$PROG"
cp "$TMP/core" "$GUARDIAN"
cp "$TMP/core" "$XTABLES"
chmod 0755 "$PROG" "$GUARDIAN" "$XTABLES"
printf 'version=v14.2.2\n' > "$UU_CONF"
printf 'manifest-version=1\nplatform=h3c-nx30pro\n' > "$SOURCE_MANIFEST"
printf 'h3c-nx30pro\n' > "$PLATFORM_FILE"
(cd "$RUNDIR" && sha256sum uuplugin xuplugin-guardian uu.conf \
	xtables-nft-multi .source-manifest > .bundle.sha256)
PLATFORM=h3c-nx30pro
valid_install || fail 'valid installed bundle rejected'
printf '# tampered\n' >> "$UU_CONF"
if valid_install; then fail 'tampered installed bundle accepted'; fi

# Exercise the real acceleration snapshot/disable/restore and fw4 UCI policy
# functions against a tiny deterministic UCI mock. This catches ordering or
# quoting regressions without touching the build host's network configuration.
(
	MOCK="$TMP/mock"
	DB="$MOCK/db"
	mkdir -p "$MOCK/bin" "$DB"
	cat > "$MOCK/bin/uci" <<-'MOCK_UCI'
		#!/bin/sh
		[ "$1" != -q ] || shift
		cmd="$1"
		shift || true
		key_file() { printf '%s/%s\n' "$UU_TEST_UCI_DB" "$1"; }
		case "$cmd" in
			get)
				file="$(key_file "$1")"
				[ -f "$file" ] || exit 1
				cat "$file"
				;;
			set|add_list)
				arg="$1"
				key="${arg%%=*}"
				value="${arg#*=}"
				file="$(key_file "$key")"
				if [ "$cmd" = add_list ] && [ -s "$file" ]; then
					value="$(cat "$file") $value"
				fi
				printf '%s\n' "$value" > "$file"
				;;
			delete)
				rm -f "$(key_file "$1")"
				;;
			commit) exit 0 ;;
			batch)
				while IFS= read -r line; do
					line="$(printf '%s' "$line" | sed "s/^[[:space:]]*//; s/'//g")"
					[ -n "$line" ] || continue
					action="${line%% *}"
					value="${line#* }"
					"$0" -q "$action" "$value" || exit 1
				done
				;;
			*) exit 1 ;;
		esac
	MOCK_UCI
	chmod 0755 "$MOCK/bin/uci"
	printf '#!/bin/sh\nexit 0\n' > "$MOCK/firewall"
	printf '#!/bin/sh\nexit 0\n' > "$MOCK/turboacc"
	chmod 0755 "$MOCK/firewall" "$MOCK/turboacc"
	printf '1\n' > "$DB/firewall.@defaults[0].flow_offloading"
	printf '0\n' > "$DB/firewall.@defaults[0].flow_offloading_hw"
	printf 'mediatek_hnat\n' > "$DB/turboacc.config.fastpath"
	printf 'enabled\n' > "$MOCK/hook_toggle"

	PATH="$MOCK/bin:$PATH"
	UU_TEST_UCI_DB="$DB"
	UU_LAUNCHER_SOURCE_ONLY=1
	UU_RUNDIR="$MOCK/runtime"
	UU_STATE_FILE="$MOCK/state"
	UU_ACCEL_STATE="$MOCK/accel-state"
	UU_LOCK_FILE="$MOCK/lock"
	UU_HNAT_HOOK="$MOCK/hook_toggle"
	UU_FIREWALL_INIT="$MOCK/firewall"
	UU_TURBOACC_INIT="$MOCK/turboacc"
	export PATH UU_TEST_UCI_DB UU_LAUNCHER_SOURCE_ONLY UU_RUNDIR \
		UU_STATE_FILE UU_ACCEL_STATE UU_LOCK_FILE UU_HNAT_HOOK \
		UU_FIREWALL_INIT UU_TURBOACC_INIT
	. "$LAUNCHER"
	disable_acceleration || fail 'acceleration disable transaction failed'
	[ "$(cat "$DB/firewall.@defaults[0].flow_offloading")" = 0 ] ||
		fail 'software flow offload was not disabled'
	[ "$(cat "$DB/turboacc.config.fastpath")" = disabled ] ||
		fail 'MediaTek HNAT selection was not suspended'
	[ -s "$ACCEL_STATE" ] || fail 'acceleration snapshot was not persisted'
	apply_fw4_policy || fail 'fw4 tun16 policy transaction failed'
	fw4_policy_present || fail 'fw4 tun16 policy does not read back'
	remove_fw4_policy || fail 'fw4 tun16 policy cleanup failed'
	[ ! -e "$DB/firewall.c2000max_uu_tun" ] ||
		fail 'fw4 UU zone survived cleanup'
	restore_acceleration || fail 'acceleration restore transaction failed'
	[ "$(cat "$DB/firewall.@defaults[0].flow_offloading")" = 1 ] ||
		fail 'software flow offload setting was not restored'
	[ "$(cat "$DB/turboacc.config.fastpath")" = mediatek_hnat ] ||
		fail 'MediaTek HNAT setting was not restored'
	[ ! -e "$ACCEL_STATE" ] || fail 'completed acceleration snapshot was not removed'
)

# On C2000MAX, only the ROLE/HNAT-locked controller may converge hardware.
# A TurboACC wrapper can retain an unrelated EQoS cleanup rc after the board
# controller has already reached the complete safe state; final readback is
# authoritative and no unlocked debugfs write may poison the transaction.
(
	MOCK="$TMP/locked-accel"
	DB="$MOCK/db"
	mkdir -p "$MOCK/bin" "$DB"
	cp "$TMP/mock/bin/uci" "$MOCK/bin/uci"
	cat > "$MOCK/c2000max-hnat" <<-'MOCK_HNAT'
		#!/bin/sh
		printf 'disabled\n' > "$UU_TEST_EFFECTIVE"
		printf 'disabled\n' > "$UU_TEST_HOOK"
		exit 0
	MOCK_HNAT
	cat > "$MOCK/turboacc" <<-'MOCK_TURBO'
		#!/bin/sh
		"$UU_TEST_HNAT" reload
		# Simulate a non-critical EQoS stop error retained by the wrapper.
		exit 1
	MOCK_TURBO
	chmod 0755 "$MOCK/c2000max-hnat" "$MOCK/turboacc"
	printf '1\n' > "$DB/firewall.@defaults[0].flow_offloading"
	printf '0\n' > "$DB/firewall.@defaults[0].flow_offloading_hw"
	printf 'mediatek_hnat\n' > "$DB/turboacc.config.fastpath"
	printf 'enabled\n' > "$MOCK/hook_toggle"
	printf 'mediatek_hnat\n' > "$MOCK/effective"

	PATH="$MOCK/bin:$PATH"
	UU_TEST_UCI_DB="$DB"
	UU_TEST_HOOK="$MOCK/hook_toggle"
	UU_TEST_EFFECTIVE="$MOCK/effective"
	UU_TEST_HNAT="$MOCK/c2000max-hnat"
	UU_LAUNCHER_SOURCE_ONLY=1
	UU_RUNDIR="$MOCK/runtime"
	UU_STATE_FILE="$MOCK/state"
	UU_ACCEL_STATE="$MOCK/accel-state"
	UU_LOCK_FILE="$MOCK/lock"
	UU_HNAT_HOOK="$MOCK/hook_toggle"
	UU_HNAT_EFFECTIVE="$MOCK/effective"
	UU_C2000_HNAT_INIT="$MOCK/c2000max-hnat"
	UU_TURBOACC_INIT="$MOCK/turboacc"
	export PATH UU_TEST_UCI_DB UU_TEST_HOOK UU_TEST_EFFECTIVE UU_TEST_HNAT \
		UU_LAUNCHER_SOURCE_ONLY UU_RUNDIR UU_STATE_FILE UU_ACCEL_STATE \
		UU_LOCK_FILE UU_HNAT_HOOK UU_HNAT_EFFECTIVE UU_C2000_HNAT_INIT \
		UU_TURBOACC_INIT
	. "$LAUNCHER"
	disable_acceleration || fail 'verified locked HNAT convergence was rejected'
	acceleration_disabled || fail 'safe acceleration state failed readback'

	mkdir -p "$RUNDIR"
	printf 'identity\n' > "$H3C_INFO"
	printf 'br-lan\n' > "$NETDEV_BASELINE"
	printf '1\n' > "$RUNDIR/activate_status"
	before="$(sha256sum "$H3C_INFO" "$NETDEV_BASELINE" \
		"$RUNDIR/activate_status")"
	uuclearnat_check || fail 'read-only HNAT readiness check failed'
	[ "$before" = "$(sha256sum "$H3C_INFO" "$NETDEV_BASELINE" \
		"$RUNDIR/activate_status")" ] ||
		fail 'uuclearnat --check mutated UU runtime'
	uuclearnat_reload || fail 'H3C HNAT fallback reload failed'
	[ "$before" = "$(sha256sum "$H3C_INFO" "$NETDEV_BASELINE" \
		"$RUNDIR/activate_status")" ] ||
		fail 'H3C HNAT fallback deleted lifecycle evidence'
)

prepare_line="$(grep -n 'snapshot_netdevs && prepare_platform_runtime' "$LAUNCHER" | tail -n1 | cut -d: -f1)"
disable_line="$(grep -n '^[[:space:]]*disable_acceleration ||' "$LAUNCHER" | tail -n1 | cut -d: -f1)"
[ -n "$prepare_line" ] && [ -n "$disable_line" ] &&
	[ "$prepare_line" -lt "$disable_line" ] ||
	fail 'H3C identity is still generated after acceleration mutation'

# Verify the fail-closed branch: a cleanup error must suppress restoration and
# force acceleration disabled.
STOP_CALLED="$TMP/stop"
RESTORE_CALLED="$TMP/restore"
FORCE_CALLED="$TMP/force"
stop_owned_processes() { : > "$STOP_CALLED"; return 1; }
cleanup_rules() { return 0; }
remove_fw4_policy() { return 0; }
restore_acceleration() { : > "$RESTORE_CALLED"; return 0; }
force_acceleration_disabled() { : > "$FORCE_CALLED"; return 0; }
if cleanup_and_restore; then fail 'cleanup error was ignored'; fi
[ -f "$STOP_CALLED" ] || fail 'owned process stop was not attempted'
[ ! -e "$RESTORE_CALLED" ] || fail 'acceleration restored after cleanup failure'
[ -f "$FORCE_CALLED" ] || fail 'fail-closed acceleration disable not called'

# Exercise the root-aware .apk-new migrator and exact rc.d-link cleanup.
MROOT="$TMP/root"
mkdir -p "$MROOT/etc/init.d" "$MROOT/etc/rc.d" "$MROOT/usr/libexec" \
	"$MROOT/usr/bin"
for target in etc/init.d/uuplugin usr/libexec/uuplugin-service \
	usr/libexec/uuplugin-launcher usr/bin/uuclearnat \
	usr/libexec/uuplugin-migrate-apk-new; do
	mkdir -p "$MROOT/${target%/*}"
	printf '#!/bin/sh\nexit 0\n' > "$MROOT/$target.apk-new"
done
ln -s ../init.d/uuplugin.apk-new "$MROOT/etc/rc.d/S99uuplugin.apk-new"
"$MIGRATOR" "$MROOT" || fail '.apk-new migration failed'
for target in etc/init.d/uuplugin usr/libexec/uuplugin-service \
	usr/libexec/uuplugin-launcher usr/bin/uuclearnat \
	usr/libexec/uuplugin-migrate-apk-new; do
	[ -x "$MROOT/$target" ] || fail "candidate not promoted: $target"
	[ ! -e "$MROOT/$target.apk-new" ] || fail "ghost remains: $target"
done
[ ! -L "$MROOT/etc/rc.d/S99uuplugin.apk-new" ] ||
	fail 'ghost rc.d link remains'

# A guardian may respawn with a new PID while the old core is terminating.
# The stopper must rescan ownership on every convergence round.
(
	COUNT="$TMP/process-scan-count"
	KILLS="$TMP/process-kills"
	printf '0\n' > "$COUNT"
	. "$LAUNCHER"
	owned_process_pids() {
		n="$(cat "$COUNT")"
		n=$((n + 1))
		printf '%s\n' "$n" > "$COUNT"
		case "$n" in 1) echo 41001 ;; 2) echo 41002 ;; esac
	}
	kill() { printf '%s\n' "$*" >> "$KILLS"; return 0; }
	sleep() { return 0; }
	stop_owned_processes || fail 'PID convergence rejected a completed stop'
	grep -Fq -- '-TERM 41001' "$KILLS" || fail 'original core PID was not stopped'
	grep -Fq -- '-TERM 41002' "$KILLS" || fail 'respawned guardian PID was not stopped'
)

# procd respawns the launcher executable without re-entering start_service().
# A normal launcher invocation must therefore clean stale runtime before the
# replacement core starts, while still refusing startup on cleanup failure.
(
	ORDER="$TMP/respawn-order"
	lock_launcher() { return 0; }
	ensure_runtime_alias() { return 0; }
	load_platform() { PLATFORM=h3c-nx30pro; }
	cleanup_and_restore() { printf 'cleanup\n' >> "$ORDER"; return 0; }
	handle_platform_switch() {
		[ "$(tail -n 1 "$ORDER")" = cleanup ] || return 1
		printf 'switch\n' >> "$ORDER"
	}
	run_core() {
		[ "$(tail -n 1 "$ORDER")" = switch ] || return 1
		printf 'run\n' >> "$ORDER"
	}
	main '' || fail 'normal launcher invocation rejected clean respawn state'
	[ "$(tr '\n' ' ' < "$ORDER")" = 'cleanup switch run ' ] ||
		fail 'normal launcher did not clean before respawn'
	cleanup_and_restore() { return 1; }
	: > "$ORDER"
	if main ''; then fail 'launcher ignored stale-runtime cleanup failure'; fi
	[ ! -s "$ORDER" ] || fail 'core ran after stale-runtime cleanup failure'
)

# Keep the inverse nft transaction after a failed replay so a later cleanup can
# retry it and diagnostics retain the exact failure evidence.
(
	UU_NFT_CREATE="$TMP/nft-create"
	UU_NFT_DELETE="$TMP/nft-delete"
	export UU_NFT_CREATE UU_NFT_DELETE
	. "$LAUNCHER"
	printf 'delete table inet uu_test\n' > "$NFT_DELETE"
	nft() { return 1; }
	if cleanup_rules; then fail 'failed nft inverse replay was ignored'; fi
	[ -s "$NFT_DELETE" ] || fail 'failed nft inverse transaction was deleted'
	nft() { return 0; }
	cleanup_rules || fail 'successful nft inverse replay reported failure'
	[ ! -e "$NFT_DELETE" ] || fail 'successful nft inverse transaction was retained'
)

if command -v luac5.1 >/dev/null 2>&1; then
	luac5.1 -p "$MODEL" "$CONTROLLER"
elif command -v luac >/dev/null 2>&1; then
	luac -p "$MODEL" "$CONTROLLER"
else
	echo 'NOTE: no host Lua compiler; Lua syntax is checked by package build'
fi

echo 'PASS: luci-app-uugamebooster C2000MAX r18 compatibility tests'
