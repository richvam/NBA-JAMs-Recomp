#!/bin/bash
# state_cross.sh <base> [save_at] [frames] - roadmap R5.3: a save state made by one build
# loads in every other and plays on to the same game. Uses the builds golden_cross.sh made
# (build/g-gcc, build/g-clang, build/g-arm); with WIN_EXE=<goldentest.exe> (run through WSL's
# Windows interop) a MinGW build is included too. Each build saves at frame <save_at>, then
# every build loads every state and plays to <frames>; all outputs must equal the one from
# the build that saved without loading.
set -u
BASE=${1:?usage: state_cross.sh <base> [save_at] [frames]}; N=${2:-1800}; F=${3:-3000}
QEMU=${QEMU:-qemu-aarch64}; cd "$(dirname "$0")/../.."; O=$PWD/${OUT:-build/state-cross}
rm -rf "$O"; mkdir -p "$O"
declare -A RUN=([gcc]="build/g-gcc/goldentest" [clang]="build/g-clang/goldentest" [arm64]="$QEMU build/g-arm/goldentest")
T="gcc clang arm64"
if [ -n "${WIN_EXE:-}" ]; then RUN[win]="$WIN_EXE"; T="$T win"; fi
p() { if [ "$1" = win ]; then wslpath -w "$2"; else echo "$2"; fi; }
for t in $T; do
  mkdir -p "$O/$t"
  GOLDEN_SAVE_AT=$N GOLDEN_STATE_DIR=$(p $t "$O/$t") WSLENV=GOLDEN_SAVE_AT:GOLDEN_STATE_DIR ${RUN[$t]} "$(p $t "$BASE")" $F | tr -d '\r' > "$O/$t.saved.txt" &
done; wait
[ -s "$O/gcc.saved.txt" ] || { echo "no output: are the ROMs in $BASE/roms?"; exit 1; }
rc=0
for t in $T; do diff -q "$O/gcc.saved.txt" "$O/$t.saved.txt" >/dev/null || { echo "saved on $t: DIFFERS from gcc"; rc=1; }; done
for from in $T; do
  for to in $T; do
    GOLDEN_SAVE_AT=$N GOLDEN_LOAD_DIR=$(p $to "$O/$from") WSLENV=GOLDEN_SAVE_AT:GOLDEN_LOAD_DIR ${RUN[$to]} "$(p $to "$BASE")" $F | tr -d '\r' > "$O/$from-$to.txt" &
  done; wait
  for to in $T; do
    if diff -q "$O/gcc.saved.txt" "$O/$from-$to.txt" >/dev/null; then echo "saved on $from, loaded on $to: identical"
    else echo "saved on $from, loaded on $to: DIFFER"; diff "$O/gcc.saved.txt" "$O/$from-$to.txt" | head -6; rc=1; fi
  done
done
echo "$(wc -l < "$O/gcc.saved.txt") lines each (frames $((N+600)) to $F and the final state)"
exit $rc
