#!/bin/bash
# mac_bundle.sh <universal executable> <version> <out_dir> - roadmap R6.2: NBA JAM Recomp.app
#   Contents/MacOS/NBAJamRecomp      the program (arm64 + x86_64)
#   Contents/Info.plist              dist/macos/Info.plist with the version
#   Contents/Resources/NBAJamRecomp.icns   from dist/linux/nbajamrecomp.svg (rsvg-convert, iconutil)
#   Contents/Resources/{mods,overlays,roms,README.txt}   what the other packages ship next to
#     the program; SDL gives Contents/Resources as the program's folder, and the first start
#     copies the mods from there to ~/Library/Application Support/NBAJamRecomp
# No portable.txt: a Mac app keeps its data in Application Support. Signed ad hoc (codesign -s -)
# so it runs on Apple Silicon. Run on macOS. Never touches game data.
set -euo pipefail
exe=$1; version=$2; out=$3
root=$(cd "$(dirname "$0")/../.." && pwd)
app="$out/NBA JAM Recomp.app"
rm -rf "$app"; mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp "$exe" "$app/Contents/MacOS/NBAJamRecomp"
strip -S "$app/Contents/MacOS/NBAJamRecomp" 2>/dev/null || true
sed "s/@VERSION@/$version/g" "$root/dist/macos/Info.plist" > "$app/Contents/Info.plist"
plutil -lint "$app/Contents/Info.plist"
res="$app/Contents/Resources"
cp "$root/dist/NBAJamRecomp/README.txt" "$res/"
cp -r "$root/dist/NBAJamRecomp/mods" "$res/mods"
cp -r "$root/dist/NBAJamRecomp/overlays" "$res/overlays"
mkdir -p "$res/roms"; cp "$root/dist/NBAJamRecomp/roms/README.txt" "$res/roms/"
# the icon: every size macOS asks for, from the SVG
iconset=$(mktemp -d)/NBAJamRecomp.iconset; mkdir -p "$iconset"
for s in 16 32 128 256 512; do
  rsvg-convert -w $s -h $s "$root/dist/linux/nbajamrecomp.svg" -o "$iconset/icon_${s}x${s}.png"
  rsvg-convert -w $((s * 2)) -h $((s * 2)) "$root/dist/linux/nbajamrecomp.svg" -o "$iconset/icon_${s}x${s}@2x.png"
done
iconutil -c icns "$iconset" -o "$res/NBAJamRecomp.icns"
# the same checks as package.sh: every shipped script, nothing that looks like game data
want=$(cd "$root/dist/NBAJamRecomp/mods" && find . -type f | sort); have=$(cd "$res/mods" && find . -type f | sort)
[ "$want" = "$have" ] || { echo "bundle mods/ differs from dist/NBAJamRecomp/mods, refusing" >&2; exit 1; }
if find "$app" -type f \( -iname '*.bin' -o -iname '*.rom' -o -iname '*.irw' -o -iname '*.u[0-9]*' -o -iname '*.zip' \) | grep -q .; then
  echo "bundle contains ROM-like files, refusing" >&2; exit 1
fi
codesign --force --sign - --timestamp=none "$app"
codesign --verify --strict "$app"
echo "$app"
