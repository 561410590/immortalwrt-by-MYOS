#!/bin/bash
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
tree=$(cd -- "$(dirname -- "$0")" && pwd)
# Cached build tools contain the original absolute path. Mount only this copy
# at that path inside a private namespace; the Ultra checkout is not written.
exec unshare --mount --propagation private /bin/bash -c '
 tree=$1
 shift
 mount --bind "$tree" /vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6-c2000ultra
 cd /vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6-c2000ultra
 export FORCE_UNSAFE_CONFIGURE=1
 set -o pipefail
 make "$@" FORCE=1 VERSION_NUMBER="V${MYOS_BUILD_VERSION:-3.1.41-202611061200}" 2>&1 | tee "$tree/build-n5000.log"
' n5000-build "$tree" "$@"
