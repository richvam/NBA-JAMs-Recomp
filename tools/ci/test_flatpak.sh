#!/bin/bash
# test_flatpak.sh - the R4.3 check, after the Flatpak is installed for the user:
#   1. it answers --version inside the sandbox
#   2. audit A (tools/audit/a_noroms.pad: first start with no ROMs, every setting changed)
#      passes under Xvfb inside the sandbox
#   3. the shipped Lua mods were copied into its data folder
# The audit runs through a small wrapper that gives the sandbox the test's data folder and
# the audit scripts (a real install sees neither).
set -uo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
id=io.github.richvam.NBAJamRecomp
fail=0
check() { if [ "$1" = 0 ]; then echo "PASS  $2"; else echo "FAIL  $2"; fail=1; fi; }

v=$(flatpak run --user "$id" --version 2>/dev/null); r=$?
check $r "--version in the sandbox: $v"

data=$(mktemp -d)
wrap=$(mktemp); chmod +x "$wrap"
cat > "$wrap" <<EOF
#!/bin/sh
exec flatpak run --user --filesystem="$data" --filesystem="$root/tools/audit:ro" \
  --env=NBAJAM_DATA_DIR="$data" --env=SDL_AUDIODRIVER=dummy $id "\$@"
EOF
bash "$root/tools/audit/run_audit.sh" "$wrap" "$root/tools/audit/a_noroms.pad" "$data" | sed 's/^/    /'; r=${PIPESTATUS[0]}
check $r "audit A (no ROMs, every setting) under Xvfb in the sandbox"

n=$(find "$data/mods" -name '*.lua' 2>/dev/null | wc -l)
[ "$n" -gt 10 ]; check $? "the shipped mods are in the data folder ($n scripts)"
exit $fail
