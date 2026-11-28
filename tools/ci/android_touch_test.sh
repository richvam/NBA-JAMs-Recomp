#!/bin/bash
# android_touch_test.sh (roadmap R7.3), run by android_emu_test.sh from the main menu: the
# on-screen controls in Settings, Touch, Arrange. Pass is dragged somewhere else with a
# finger and Back keeps the new place (checked in the app's log). CI has no ROMs, so the
# controls are not seen in a game here: tools/headless/touchtest checks what they press.
set -euo pipefail
# a failed check: what Android recorded of a crash, so the reason is in the CI log
trap 'echo "--- failed: the crash log ---"; adb logcat -d -b crash | tail -80 || true' ERR
size=$(adb shell wm size | sed -n 's/.*: *//p' | tail -1 | tr -d '\r'); W=${size%x*}; H=${size#*x}
[ "$W" -lt "$H" ] && { t=$W; W=$H; H=$t; }
key() { for k in "$@"; do adb shell input keyevent KEYCODE_$k; sleep 0.6; done; }
adb logcat -c
# Settings (2nd row of the menu), then the Touch tab (E moves a tab on), then down to
# "Arrange the controls" (the labels are skipped) and Enter
adb shell input tap $(( W * 60 / 1000 )) $(( H * 625 / 1000 )); sleep 2
key E E E E; sleep 1
adb exec-out screencap -p > android-shots/settings-touch.png
key DPAD_DOWN DPAD_DOWN DPAD_DOWN ENTER; sleep 2
adb exec-out screencap -p > android-shots/arrange-1.png
# drag Pass (78% across, 80% down by default) up and to the left
adb shell input swipe $(( W * 78 / 100 )) $(( H * 80 / 100 )) $(( W * 60 / 100 )) $(( H * 45 / 100 )) 800; sleep 2
adb exec-out screencap -p > android-shots/arrange-2.png
key BACK; sleep 2
adb logcat -d > android-logcat-touch.txt || true
line=$(grep -a "touch layout saved:" android-logcat-touch.txt | tail -1)
echo "$line"
# the third pair is Pass: it must have moved from 0.780,0.800
pass=$(echo "$line" | sed 's/.*saved: //' | cut -d';' -f3)
echo "Pass is now at $pass"
[ -n "$pass" ] && [ "$pass" != "0.780,0.800" ]
echo "android touch arrange: PASS"
# online needs the INTERNET permission: the app logs its local addresses at start (a socket
# it could not make would leave none). The Online screen itself needs a game with ROMs.
grep -a "network: local addresses" android-logcat.txt | tail -1
grep -aq "network: local addresses: [0-9]" android-logcat.txt
echo "android online: PASS"
# R7.4: a TV remote (or a controller's d-pad in keyboard mode) works the menus: back to the
# menu, where Settings still has the focus; the centre key opens it; Back leaves it; two rows
# down is Controls
adb logcat -c
key BACK; sleep 2
key DPAD_CENTER; sleep 2
key BACK; sleep 2
key DPAD_DOWN DPAD_DOWN DPAD_CENTER; sleep 2
adb exec-out screencap -p > android-shots/remote-controls.png
key BACK; sleep 2
adb logcat -d > android-logcat-remote.txt || true
grep -a "screen: " android-logcat-remote.txt | sed 's/.*screen: /screen: /'
grep -a "screen: " android-logcat-remote.txt | sed 's/.*screen: //' | tr -d '\r' | tr '\n' ' ' | grep -q "menu settings menu controls menu"
echo "android remote: PASS"
# R7.5: put away with Home and brought back: the app logs both and is still the same process
# (a game kept and resumed is checked with ROMs by tools/audit/h_lifecycle.pad)
pkg=io.github.richvam.NBA_JAMs_Recomp
pid1=$(adb shell pidof $pkg | tr -d '\r')
adb logcat -c
key HOME; sleep 3
adb shell monkey -p $pkg -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 4
adb exec-out screencap -p > android-shots/after-home.png
adb logcat -d > android-logcat-life.txt || true
grep -a "lifecycle:" android-logcat-life.txt | sed 's/.*lifecycle: /lifecycle: /'
grep -aq "lifecycle: put away, on menu" android-logcat-life.txt
grep -aq "lifecycle: back, on menu" android-logcat-life.txt
[ "$(adb shell pidof $pkg | tr -d '\r')" = "$pid1" ]
echo "android lifecycle: PASS"
# R7.8: three phone shapes (wm size is portrait: width x height of the phone held upright)
# - 720x1280 (an older 16:9 phone), 1080x2340 (a tall phone such as an S24+), 1080x1920
# (16:9 at 1080p, a handheld such as the Thor); the app keeps running, draws the menu, logs the
# safe area it keeps clear
adb logcat -c
for s in 720x1280:320 1080x2340:420 1080x1920:400; do
  sz=${s%:*}; dpi=${s#*:}
  adb shell wm size $sz; adb shell wm density $dpi; sleep 4
  adb exec-out screencap -p > android-shots/phone-$sz.png
  [ "$(adb shell pidof $pkg | tr -d '\r')" = "$pid1" ]
  bytes=$(wc -c < android-shots/phone-$sz.png); echo "phone $sz: $bytes bytes"; [ "$bytes" -gt 20000 ]
done
adb shell wm size reset; adb shell wm density reset; sleep 3
adb logcat -d > android-logcat-sizes.txt || true
grep -a "safe area:" android-logcat-sizes.txt | sed 's/.*safe area/safe area/' | sort -u
echo "android phone sizes: PASS"
