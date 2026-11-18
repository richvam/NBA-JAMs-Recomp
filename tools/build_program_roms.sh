#!/bin/bash
# =============================================================================
#  build_program_roms.sh - build only the program ROMs of the three source games
#  (T.E. 4.0, SE 5.02, Maximum Hangtime 1.3), for tools/modtests/run_all.sh.
#
#  Usage:   tools/build_program_roms.sh <repos_dir> <emu2> <out_dir> [--run]
#
#  --run: then run every tools/modtests/*.expect on them (tools/modtests/run_all.sh;
#  needs build/linux/modtest).
#
#  Writes <out_dir>/<id>/program.lo and program.hi for nbajamte, nbajamse and nbamht.
#  The same build as tools/build_roms.sh (see there for the repositories), without the
#  graphics and sound. emu2 is still needed: T.E. and Maximum Hangtime include image
#  tables that Midway's LOAD2 writes from the art. Each result is compared with the
#  original ROM in its repository ("identical to the original").
#  Nothing is written into this repository: keep <out_dir> outside it.
# =============================================================================
[ $# -eq 3 ] || { [ $# -eq 4 ] && [ "$4" = --run ]; } || { sed -n '3,16p' "$0"; exit 1; }
T=$(dirname "$(realpath "$0")")
PROGRAM_ONLY=1 bash "$T/build_roms.sh" "$1" "$2" "$3" || exit 1
[ "${4:-}" = --run ] && exec bash "$T/modtests/run_all.sh" "$3"
exit 0
