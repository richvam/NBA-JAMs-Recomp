#!/bin/bash
# golden_cross.sh <base> [frames] - roadmap R5.2: goldentest built by GCC (x86-64), Clang
# (x86-64) and Clang for ARM64 (run under qemu-user), compared line by line.
# <base> is the folder that holds roms/. Needs zig (for Clang and the ARM64 cross build) and
# qemu-aarch64 on PATH, or ZIG=/path/zig QEMU=/path/qemu-aarch64. Nothing is copied anywhere.
set -u
BASE=${1:?usage: golden_cross.sh <base> [frames]}; F=${2:-3000}
ZIG=${ZIG:-zig}; QEMU=${QEMU:-qemu-aarch64}; O=${OUT:-build/golden}
cd "$(dirname "$0")/../.." && mkdir -p $O
make -j8 BUILD=build/g-gcc build/g-gcc/goldentest >/dev/null || exit 1
make -j8 BUILD=build/g-clang CXX="$ZIG c++ -target x86_64-linux-gnu" CC="$ZIG cc -target x86_64-linux-gnu" build/g-clang/goldentest >/dev/null || exit 1
make -j8 BUILD=build/g-arm CXX="$ZIG c++ -target aarch64-linux-musl" CC="$ZIG cc -target aarch64-linux-musl" \
  CXXFLAGS="-O2 -fsigned-char -static" CFLAGS="-O2 -fsigned-char" build/g-arm/goldentest >/dev/null || exit 1
build/g-gcc/goldentest "$BASE" $F > $O/gcc.txt & build/g-clang/goldentest "$BASE" $F > $O/clang.txt &
$QEMU build/g-arm/goldentest "$BASE" $F > $O/arm64.txt & wait
[ -s $O/gcc.txt ] || { echo "no output: are the ROMs in $BASE/roms?"; exit 1; }
rc=0
for b in clang arm64; do
  if diff -q $O/gcc.txt $O/$b.txt >/dev/null; then echo "gcc vs $b: identical ($(wc -l < $O/gcc.txt) lines)"
  else echo "gcc vs $b: DIFFER"; diff $O/gcc.txt $O/$b.txt | head; rc=1; fi
done
exit $rc
