#!/bin/bash
# netsoak.sh - the netplay soak of roadmap 7.3: every Lua script that comes with the program on,
# two and four peers on this computer, a long run, the game state hashes equal on every peer.
#
#   tools/netsoak.sh <base> <nettest> [frames] [games...]
#     base     the folder whose roms/ holds the user's ROM sets (read only)
#     nettest  the built tools/headless/nettest
#     frames   default 30000
#     games    default: nbajamte nbajamse nbajam nbahangt nbamht
#
# The host turns on every script in dist/NBAJamRecomp/mods (MODS_ON empty = all, with their
# default options) and sends that set with the game start; each client runs exactly that set
# (roadmap 1.7). nettest plays scripted inputs and prints "<frame> <hash>" every 300 frames.
# A run passes when every peer ends normally (no desync, exit 0), all print the same hashes,
# and no script stopped with an error. Exit status 0 when every run passes.
set -u
base=$1; nt=$(realpath "$2"); frames=${3:-30000}; shift 3 2>/dev/null || shift $#
games=${*:-nbajamte nbajamse nbajam nbahangt nbamht}
top=$(cd "$(dirname "$0")/.." && pwd)
mods=$top/dist/NBAJamRecomp/mods
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
port=7900 pass=0 fail=0 failed=()
for g in $games; do
  for n in 2 4; do
    port=$((port + 1))
    d=$tmp/$g-$n; mkdir -p "$d"
    # each peer gets its own copy of the scripts (and its own stats/ next to them)
    for k in $(seq 1 $n); do mkdir -p "$d/p$k"; cp -r "$mods" "$d/p$k/mods"; done
    MODS=$d/p1/mods PLAYERS=$n "$nt" "$base" "$g" host "$port" "$frames" 4 > "$d/1.out" 2> "$d/1.err" &
    pids=($!)
    sleep 1
    for k in $(seq 2 $n); do
      MODS=$d/p$k/mods "$nt" "$base" "$g" client 127.0.0.1 "$port" "$frames" > "$d/$k.out" 2> "$d/$k.err" &
      pids+=($!)
    done
    ok=1 why=""
    for k in $(seq 1 $n); do
      wait "${pids[$((k - 1))]}"; rc=$?
      [ $rc -eq 0 ] || { ok=0; why="$why peer $k exit $rc;"; }
    done
    lines=$(wc -l < "$d/1.out")
    [ "$lines" -ge $((frames / 300 - 1)) ] || { ok=0; why="$why only $lines hashes;"; }
    for k in $(seq 2 $n); do cmp -s "$d/1.out" "$d/$k.out" || { ok=0; why="$why peer $k hashes differ;"; }; done
    if grep -q "error" "$d"/*.err; then ok=0; why="$why $(grep -h -m1 "error" "$d"/*.err | head -1);"; fi
    mods_line=$(grep -h -m1 "running the host's Lua mods" "$d/2.err" | sed 's/.*mods: //' | tr ',' '\n' | wc -l)
    if [ $ok -eq 1 ]; then
      pass=$((pass + 1)); echo "PASS $g, $n peers: $lines hashes equal, $mods_line scripts on, $(tail -1 "$d/1.err" | sed 's/.*done/done/')"
    else
      fail=$((fail + 1)); failed+=("$g/$n"); echo "FAIL $g, $n peers:$why"
      tail -3 "$d"/*.err | sed 's/^/    /'
    fi
  done
done
echo "net soak: $pass passed, $fail failed${failed:+ (${failed[*]})}"
[ $fail -eq 0 ]
