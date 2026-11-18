#!/bin/bash
# =============================================================================
#  hangtime_symbols.sh - the symbol files of NBA Hangtime L1.3 and L1.1 and NBA Maximum
#  Hangtime 1.03 (roadmap 2.3)
#
#  Usage:   tools/hangtime_symbols.sh <repos_dir> <emu2> <work_dir> <mht103.lo> <mht103.hi>
#
#  1. tools/build_program_roms.sh with WORK=<work_dir> (the Maximum Hangtime 1.3 tree)
#  2. Hangtime L1.3, L1.1 and Maximum Hangtime 1.1 from the repository's BACKUP/CODE113L,
#     CODE111L and CODE111M over that tree; each must be identical to ROMS/7M_13L, 7M_11L,
#     7M_11M. map2sym.py writes the L1.3 and L1.1 symbol files from their .map files.
#  3. Maximum Hangtime 1.03 has no source: sym_carry.py carries the 1.3 names to it, and again
#     from 1.1, and keeps the names both give the same address.
#  <mht103.lo/.hi>: the 1.03 program ROMs (MAME nbamht.zip, l1.03 u54/u63).
#  Writes into dist/NBAJamRecomp/mods/sym/. Keep <work_dir> outside the repository.
# =============================================================================
set -e
[ $# -eq 5 ] || { sed -n '3,16p' "$0"; exit 1; }
REPOS=$(realpath "$1"); EMU2=$(realpath "$2"); W=$(realpath -m "$3")
T=$(dirname "$(realpath "$0")"); SYM=$T/../dist/NBAJamRecomp/mods/sym
HT=$REPOS/nba-hangtime; [ -d "$HT" ] || HT=$REPOS/nba-hangtime-main
G="python3 $T/gspasm/gspasm.py"
[ -f "$W/ht/ht.map" ] || WORK="$W" bash "$T/build_program_roms.sh" "$REPOS" "$EMU2" "$W/out"
for v in 113L:13L 111L:11L 111M:11M; do
  c=${v%%:*}; r=${v#*:}; B=$W/b$c; rm -rf "$B"; cp -r "$W/ht" "$B"
  for f in "$HT"/BACKUP/CODE$c/*; do cp "$f" "$B/$(basename "$f" | tr a-z A-Z)"; done
  ( cd "$B" && $G . BB.CMD "$B/out" --irw "$W/htimg/bbvda.irw" > "$B/asm.log" 2>&1 )
  cmp "$B/out.lo" "$HT/ROMS/7M_$r.U54" && cmp "$B/out.hi" "$HT/ROMS/7M_$r.U63" && echo "   $c identical to 7M_$r"
done
python3 "$T/map2sym.py" "$W/b113L/out.map" nbahangt "$W/b113L/out.lo" "$W/b113L/out.hi" "$SYM"
python3 "$T/map2sym.py" "$W/b111L/out.map" nbahangt "$W/b111L/out.lo" "$W/b111L/out.hi" "$SYM"
mkdir -p "$W/sym111M"; python3 "$T/map2sym.py" "$W/b111M/out.map" nbamht "$W/b111M/out.lo" "$W/b111M/out.hi" "$W/sym111M"
python3 "$T/sym_carry.py" "$SYM/nbamht_6ae2feac.lua" "$HT/ROMS/7M_13M.U54" "$HT/ROMS/7M_13M.U63" "$4" "$5" nbamht "$SYM" \
  --agree "$W"/sym111M/nbamht_*.lua "$HT/ROMS/7M_11M.U54" "$HT/ROMS/7M_11M.U63"
