#!/bin/bash
# test_appimage.sh <AppImage> - the R4.1 check, run inside a distribution container as root.
# Installs Xvfb and Mesa with the distribution's own package manager, then:
#   1. the AppImage answers --version
#   2. audit A (tools/audit/a_noroms.pad: first start with no ROMs, every setting changed)
#      passes under Xvfb with software OpenGL
#   3. the shipped Lua mods were copied into the per-user data folder
# There is no FUSE in a container, so the AppImage runs with APPIMAGE_EXTRACT_AND_RUN=1.
set -uo pipefail
img=$(realpath "$1")
root=$(cd "$(dirname "$0")/../.." && pwd)
. /etc/os-release
echo "== $PRETTY_NAME"
if command -v apt-get >/dev/null; then
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq && apt-get install -y -qq xvfb xauth libgl1-mesa-dri libgl1 >/dev/null
elif command -v dnf >/dev/null; then
  dnf install -y -q xorg-x11-server-Xvfb xorg-x11-xauth mesa-dri-drivers mesa-libGL which >/dev/null
elif command -v pacman >/dev/null; then
  pacman -Sy --noconfirm --needed xorg-server-xvfb xorg-xauth mesa which >/dev/null
fi
export APPIMAGE_EXTRACT_AND_RUN=1
cd "$(mktemp -d)"     # where the AppImage unpacks itself
fail=0
check() { if [ "$1" = 0 ]; then echo "PASS  $2"; else echo "FAIL  $2"; fail=1; fi; }

"$img" --version >version.txt 2>&1; r=$?
check $r "--version: $(head -1 version.txt)"

data=$(mktemp -d)
bash "$root/tools/audit/run_audit.sh" "$img" "$root/tools/audit/a_noroms.pad" "$data" | sed 's/^/    /'; r=${PIPESTATUS[0]}
check $r "audit A (no ROMs, every setting) under Xvfb"

n=$(find "$data/mods" -name '*.lua' 2>/dev/null | wc -l)
[ "$n" -gt 10 ]; check $? "the shipped mods are in the data folder ($n scripts)"
exit $fail
