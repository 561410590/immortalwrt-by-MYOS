#!/bin/sh

set -eu

fit="$1"
rootfs="$2"
board_name="${3:-}"

if [ ! -s "$fit" ]; then
	echo "FIT image not found: $fit" >&2
	exit 1
fi

if [ ! -s "$rootfs" ]; then
	echo "rootfs image not found: $rootfs" >&2
	exit 1
fi

size="$(wc -c < "$rootfs" | tr -d ' ')"
hashes="$(python3 - "$rootfs" <<'PY'
import hashlib
import sys
import zlib

path = sys.argv[1]
crc = 0
h = hashlib.sha1()
with open(path, "rb") as fh:
    for chunk in iter(lambda: fh.read(1024 * 1024), b""):
        crc = zlib.crc32(chunk, crc)
        h.update(chunk)
sha1 = h.hexdigest()
sha1_words = " ".join(f"0x{sha1[i:i + 8]}" for i in range(0, 40, 8))
print(f"{crc & 0xffffffff:08x} {sha1_words}")
PY
)"
set -- $hashes
crc32="$1"
shift
image_time="${SOURCE_DATE_EPOCH:-$(date +%s)}"

fdtput -t s "$fit" / rootfs_size "$size"
fdtput -t s "$fit" / rootfs_crc32 "$crc32"
fdtput -t s "$fit" / image_time "$image_time"

fdtput -cp "$fit" /rootfs
fdtput -t x "$fit" /rootfs size "0x$(printf '%x' "$size")"
fdtput -cp "$fit" /rootfs/hash-1
fdtput -t x "$fit" /rootfs/hash-1 value "0x$crc32"
fdtput -t s "$fit" /rootfs/hash-1 algo crc32
fdtput -cp "$fit" /rootfs/hash-2
fdtput -t x "$fit" /rootfs/hash-2 value "$@"
fdtput -t s "$fit" /rootfs/hash-2 algo sha1

if [ -n "$board_name" ]; then
	fdtput -t s "$fit" /images/fdt-1 description \
		"ARM64 OpenWrt ${board_name} device tree blob"
	fdtput -t s "$fit" /configurations/config-1 description "OpenWrt"
fi
