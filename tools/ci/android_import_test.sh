#!/bin/bash
# android_import_test.sh (roadmap R7.2), run by android_emu_test.sh with the app on screen:
# a zip in Download is chosen with "Add ROMs" in the system file picker (driven with
# uiautomator) and must arrive in the app's roms folder. CI has no ROMs, so the zip is a
# small made-up one: this checks the import, not that a game becomes playable.
set -euo pipefail
pkg=io.github.richvam.NBA_JAMs_Recomp
d=/sdcard/Android/data/$pkg/files
name=nbaj-import-test.zip
python3 -c "import zipfile; z=zipfile.ZipFile('$name','w'); z.writestr('readme.txt','import test'); z.close()"
adb push $name /sdcard/Download/$name
adb shell am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE -d file:///sdcard/Download/$name >/dev/null

size=$(adb shell wm size | sed -n 's/.*: *//p' | tail -1 | tr -d '\r'); W=${size%x*}; H=${size#*x}
[ "$W" -lt "$H" ] && { t=$W; W=$H; H=$t; }
# "Add ROMs" is the 7th row of the menu panel (Settings, the 2nd, is at 62.5% of the height)
adb shell input tap $(( W * 60 / 1000 )) $(( H * 825 / 1000 ))
sleep 4
adb exec-out screencap -p > android-shots/picker.png

# tap the element whose text (or description) matches, from a uiautomator dump
tap_text() {
  adb shell uiautomator dump /data/local/tmp/ui.xml >/dev/null 2>&1 || return 1
  adb pull /data/local/tmp/ui.xml ui.xml >/dev/null 2>&1 || return 1
  local xy
  xy=$(python3 - "$1" <<'PY'
import re, sys, xml.etree.ElementTree as ET
want = sys.argv[1]
for n in ET.parse('ui.xml').iter('node'):
    if want in (n.get('text') or '') or want in (n.get('content-desc') or ''):
        x1, y1, x2, y2 = map(int, re.findall(r'\d+', n.get('bounds')))
        print((x1 + x2) // 2, (y1 + y2) // 2)
        break
PY
)
  [ -n "$xy" ] || { echo "not on screen: $1"; return 1; }
  echo "tap '$1' at $xy"
  adb shell input tap $xy
}
picker_open() { adb shell dumpsys window | grep -i mCurrentFocus | grep -q documentsui; }

grep -q documentsui <(adb shell dumpsys window | grep -i mCurrentFocus) || { echo "the file picker did not open"; adb shell dumpsys window | grep -i mCurrentFocus; exit 1; }
# the file may be under Recent; if not, open Downloads from the side list
if ! tap_text $name; then
  tap_text "Show roots" || true; sleep 2
  tap_text "Downloads" || tap_text "Download"; sleep 3
  tap_text $name
fi
sleep 3
# with several files allowed, a tap may only select the file: then confirm with Select
if picker_open; then
  adb exec-out screencap -p > android-shots/picker-selected.png
  tap_text "Select" || tap_text "SELECT" || tap_text "Open" || tap_text "OPEN" || true
fi
sleep 2
# a tap on the name may be ignored: try the file's icon, then the keyboard (arrows, Enter)
if picker_open; then
  echo "still in the picker: tapping the icon"
  adb shell input tap 36 232; sleep 3
fi
if picker_open; then
  echo "still in the picker: keyboard"
  for k in DPAD_DOWN DPAD_DOWN DPAD_DOWN ENTER; do adb shell input keyevent KEYCODE_$k; sleep 1; done
  sleep 2
fi
picker_open && echo "the picker is still open" || echo "back in the app"
sleep 5
adb exec-out screencap -p > android-shots/after-import.png
# the app reports what it copied (adb's shell cannot list Android/data here)
adb logcat -d > android-logcat-import.txt || true
grep -a "Added 1 file" android-logcat-import.txt
echo "android import: PASS"
"$(dirname "$0")/android_touch_test.sh"
