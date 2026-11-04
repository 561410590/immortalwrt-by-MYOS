#!/bin/sh
set -eu

root="${1:?Missing package payload directory}"
case "$root" in
    */luci-app-oaf|*/luci-i18n-oaf-*) ;;
    *) exit 1 ;;
esac
[ -d "$root" ]

find "$root" -type d -exec chmod 0755 {} +
find "$root" -type f -exec chmod 0644 {} +
for path in etc/init.d etc/uci-defaults; do
    [ ! -d "$root/$path" ] || find "$root/$path" -type f -exec chmod 0755 {} +
done
