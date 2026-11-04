#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
INIT="$ROOT/files/etc/init.d/mt5700-web"
TEST_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM

# shellcheck source=/dev/null
. "$INIT"

UCI_MODE=existing
uci() {
	case "$1 $2 $3" in
	'-q show mt5700-web')
		if [ "$UCI_MODE" = existing ]; then
			printf "mt5700-web.main='service'\nmt5700-web.external='modem'\n"
		else
			printf "mt5700-web.main='service'\n"
		fi
		;;
	'-q set '*) printf '%s\n' "$*" >> "$TEST_ROOT/writes" ;;
	'-q get '*) return 0 ;;
	'-q commit mt5700-web') printf '%s\n' "$*" >> "$TEST_ROOT/writes" ;;
	esac
}

: > "$TEST_ROOT/writes"
migrate_legacy_instance
[ ! -s "$TEST_ROOT/writes" ] || {
	echo 'FAIL: quoted existing modem section was rebuilt' >&2
	exit 1
}

UCI_MODE=legacy
migrate_legacy_instance
grep -Fq "set mt5700-web.internal=modem" "$TEST_ROOT/writes" || {
	echo 'FAIL: legacy single-instance config was not migrated' >&2
	exit 1
}

echo 'MT5700 init migration tests passed'
