#!/bin/bash
# =============================================================================
#  run_all.sh - run every tools/modtests/*.expect through modtest (roadmap 1.8)
#
#  Usage:   tools/modtests/run_all.sh <program_roms_dir> [modtest binary]
#
#  <program_roms_dir> is what tools/build_program_roms.sh writes: <id>/program.lo, .hi.
#  Each <name>.expect belongs to <name>.lua next to it. Lines:
#      game nbajamte nbajamse nbamht    the games to run it on (each one in turn)
#      frames 3000                      how many frames to run
#      env PLAY=1                       environment for modtest (may repeat)
#      opt step=2                       an option value (may repeat)
#      expect <frame> <symbol>[:8|:16|:32] <op> <value>   checked by modtest
#  The script runs from a fresh folder holding it, mods/lib and mods/sym.
#  Exit code 1 when any expectation fails.
# =============================================================================
ROMS=$(realpath "${1:?usage: run_all.sh <program_roms_dir> [modtest]}")
HERE=$(dirname "$(realpath "$0")")
TOP=$(realpath "$HERE/../..")
MODTEST=$(realpath "${2:-$TOP/build/linux/modtest}")
[ -x "$MODTEST" ] || { echo "modtest not found: $MODTEST (make BUILD=build/linux build/linux/modtest)"; exit 1; }
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0; failed=()
for ex in "$HERE"/*.expect; do
  name=$(basename "$ex" .expect); lua="$HERE/$name.lua"
  [ -f "$lua" ] || { echo "== $name: no $name.lua"; fail=$((fail + 1)); failed+=("$name"); continue; }
  games=$(sed -n 's/\r$//; s/^game[[:space:]]\+//p' "$ex")
  frames=$(sed -n 's/\r$//; s/^frames[[:space:]]\+//p' "$ex" | tail -1)
  envs=$(sed -n 's/\r$//; s/^env[[:space:]]\+//p' "$ex")
  opts=$(sed -n 's/\r$//; s/^opt[[:space:]]\+//p' "$ex")
  for g in $games; do
    rm -rf "$TMP/m"; mkdir -p "$TMP/m"
    cp -r "$TOP/dist/NBAJamRecomp/mods/lib" "$TOP/dist/NBAJamRecomp/mods/sym" "$TMP/m/"; cp "$lua" "$TMP/m/"
    echo "== $name on $g"
    if [ ! -f "$ROMS/$g/program.lo" ]; then echo "FAIL: no $ROMS/$g/program.lo"; fail=$((fail + 1)); failed+=("$name/$g"); continue; fi
    # shellcheck disable=SC2086
    out=$(env $envs "$MODTEST" --expect "$ex" "$ROMS/$g/program.lo" "$ROMS/$g/program.hi" "$TMP/m" "$g" "${frames:-1000}" $opts 2>&1)
    rc=$?
    echo "$out" | grep -a "^PASS\|^FAIL\|expectations passed\|ERROR" | sed "s|$HERE/||"
    if [ $rc -eq 0 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); failed+=("$name/$g"); [ $rc -ne 4 ] && echo "modtest exit $rc"; fi
  done
done
echo "mod tests: $pass passed, $fail failed${failed:+ (${failed[*]})}"
[ $fail -eq 0 ] && exit 0 || exit 1
