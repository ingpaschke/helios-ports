#!/bin/sh
# Build a private i386 musl for the original ILP32 data generators.
set -eu
nh_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
nh_musl=${1:?Usage: tools/bootstrap-host.sh /path/to/musl-source}
nh_musl=$(CDPATH= cd -- "$nh_musl" && pwd)
mkdir -p "$nh_root/build/host-libc"
cd "$nh_root/build/host-libc"
"$nh_musl/configure" --target=i386 --prefix="$nh_root/build/host-sdk" \
    --disable-shared CC='gcc -m32' AR=ar RANLIB=ranlib >configure.log 2>&1
make -j"${JOBS:-4}" AR=ar RANLIB=ranlib >build.log 2>&1
make install AR=ar RANLIB=ranlib >install.log 2>&1
