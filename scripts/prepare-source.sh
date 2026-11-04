#!/bin/sh
set -eu

source_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$source_root"

for feed in packages luci routing telephony istore nas nas_luci; do
    [ -d "feeds/$feed" ] || {
        echo "Missing bundled feed: feeds/$feed" >&2
        exit 1
    }
done
[ -d feeds/base ] || {
    echo 'feeds/base is unavailable; check the sibling MAX checkout described in README-OPEN-SOURCE.md' >&2
    exit 1
}

# -i rebuilds indexes only. A normal update would overwrite the bundled changes.
./scripts/feeds update -i -a
