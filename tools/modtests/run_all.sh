#!/bin/bash
# =============================================================================
#  run_all.sh - run every tools/modtests/*.expect through modtest (roadmap 1.8)
#
#  Usage:   tools/modtests/run_all.sh <program_roms_dir> [modtest binary]
#
#  <program_roms_dir> is what tools/build_program_roms.sh writes: <id>/program.lo, .hi.
#  Each <name>.expect belongs to <name>.lua next to it. Lines:
#      game nbajamte nbajamse nbamht    the games to run it on (each one in turn); <id>:<folder>
#                                       runs game <id> on <program_roms_dir>/<folder>/ (another revision);
#                                       modtest also gets rom=<folder>, so a line may start "rom=<folder>:"
#      frames 3000                      how many frames to run
#      env PLAY=1                       environment for modtest (may repeat)
#      opt step=2                       an option value (may repeat)
#      each code=1 2 3                  one run per value, with code=<value> added
#      run code=1 two=1                 one more run with those values (may repeat)
#      expect <frame> <symbol>[:8|:16|:32] <op> <value>   checked by modtest
#      script <path>                    test that script (relative to the repository, e.g. a
#                                       dist/NBAJamRecomp/mods/ one) instead of <name>.lua
#      extra <file>                     also load tools/modtests/<file> (a probe), may repeat
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
  sc=$(sed -n 's/\r$//; s/^script[[:space:]]\+//p' "$ex" | head -1); [ -n "$sc" ] && lua="$TOP/$sc"
  extras=$(sed -n 's/\r$//; s/^extra[[:space:]]\+//p' "$ex")
  [ -f "$lua" ] || { echo "== $name: no $name.lua"; fail=$((fail + 1)); failed+=("$name"); continue; }
  games=$(sed -n 's/\r$//; s/^game[[:space:]]\+//p' "$ex")
  frames=$(sed -n 's/\r$//; s/^frames[[:space:]]\+//p' "$ex" | tail -1)
  envs=$(sed -n 's/\r$//; s/^env[[:space:]]\+//p' "$ex")
  opts=$(sed -n 's/\r$//; s/^opt[[:space:]]\+//p' "$ex")
  # the runs: one per "each" value and one per "run" line, or a single plain run;
  # each is a list of opt=value joined by ':'
  each=$(sed -n 's/\r$//; s/^each[[:space:]]\+//p' "$ex" | head -1)
  runs=""
  if [ -n "$each" ]; then eid=${each%%=*}; for v in ${each#*=}; do runs="$runs $eid=$v"; done; fi
  runs="$runs $(sed -n 's/\r$//; s/^run[[:space:]]\+//p' "$ex" | sed 's/[[:space:]]\+/:/g')"
  [ -n "${runs// /}" ] || runs=-
  for gg in $games; do for ev in $runs; do
    g=${gg%%:*}; d=${gg#*:}
    runopts=$opts; label=$gg
    if [ "$ev" != - ]; then
      # a run's value replaces an "opt" line for the same option
      runopts=""; for o in $opts; do case ":$ev:" in *":${o%%=*}="*) ;; *) runopts="$runopts $o";; esac; done
      runopts="$runopts ${ev//:/ }"; label="$gg ${ev//:/ }"
    fi
    rm -rf "$TMP/m"; mkdir -p "$TMP/m"
    cp -r "$TOP/dist/NBAJamRecomp/mods/lib" "$TOP/dist/NBAJamRecomp/mods/sym" "$TMP/m/"; cp "$lua" "$TMP/m/"
    for x in $extras; do cp "$HERE/$x" "$TMP/m/"; done
    echo "== $name on $label"
    if [ ! -f "$ROMS/$d/program.lo" ]; then echo "FAIL: no $ROMS/$d/program.lo"; fail=$((fail + 1)); failed+=("$name/$label"); continue; fi
    # shellcheck disable=SC2086
    out=$(env $envs "$MODTEST" --expect "$ex" "$ROMS/$d/program.lo" "$ROMS/$d/program.hi" "$TMP/m" "$g" "${frames:-1000}" $runopts rom=$d 2>&1)
    rc=$?
    echo "$out" | grep -a "^PASS\|^FAIL\|expectations passed\|ERROR" | sed "s|$HERE/||"
    if [ $rc -eq 0 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); failed+=("$name/${label// /:}"); [ $rc -ne 4 ] && echo "modtest exit $rc"; fi
  done; done
done
echo "mod tests: $pass passed, $fail failed${failed:+ (${failed[*]})}"
[ $fail -eq 0 ] && exit 0 || exit 1
