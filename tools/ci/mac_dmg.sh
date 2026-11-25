#!/bin/bash
# mac_dmg.sh <NBA JAM Recomp.app> <out.dmg> - roadmap R6.3: the disk image a Mac user downloads.
# It opens to the app, a link to /Applications to drag it onto, and a short read-me on the first
# start (the app is not signed by Apple, so macOS asks once). Compressed (UDZO). Run on macOS.
set -euo pipefail
app=$1; dmg=$2
stage=$(mktemp -d); trap 'rm -rf "$stage"' EXIT
cp -R "$app" "$stage/"
ln -s /Applications "$stage/Applications"
cat > "$stage/First start - read me.txt" <<'TXT'
NBA JAM Recomp for macOS (Apple Silicon and Intel, macOS 11 or later)

1. Drag "NBA JAM Recomp" onto "Applications".
2. The first time, macOS says it cannot check the app for malicious software, because it
   is not signed with an Apple developer certificate. To open it anyway:
   - macOS 15 and later: open it once, click Done, then System Settings > Privacy & Security,
     scroll down to "NBA JAM Recomp was blocked" and click Open Anyway.
   - macOS 11 to 14: Control-click the app in Applications, choose Open, then Open.
   After that it opens normally.
3. No game data is included. Put your own MAME ROM sets (nbajam.zip and the others) in
   ~/Library/Application Support/NBAJamRecomp/roms, or drag them onto the window.

Keys on a Mac: Ctrl+Cmd+F or Cmd+Return fullscreen, Cmd+Q quit; the F keys may need fn.
TXT
rm -f "$dmg"
hdiutil create -volname "NBA JAM Recomp" -srcfolder "$stage" -ov -format UDZO "$dmg" >/dev/null
hdiutil verify "$dmg" >/dev/null
echo "$dmg ($(du -h "$dmg" | cut -f1))"
