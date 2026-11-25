#!/bin/bash
# android_emu_test.sh <apk_dir> (roadmap R7.1): install the APK on the running emulator, start
# it, and check that it is still running after 25 s with no crash in the log, and that the
# screen is not one colour (a black screen would mean nothing was drawn). Screenshots and the
# log go to android-shots/ and android-logcat.txt.
set -euo pipefail
apk=$(ls "$1"/*.apk | head -1)
pkg=io.github.richvam.NBA_JAMs_Recomp
mkdir -p android-shots
adb install -r "$apk"
adb logcat -c
adb shell am start -n $pkg/org.libsdl.app.SDLActivity
sleep 25
adb exec-out screencap -p > android-shots/start.png
adb logcat -d > android-logcat.txt || true
grep -E "SDL|NBAJ|AndroidRuntime|DEBUG" android-logcat.txt | tail -40 || true
pid=$(adb shell pidof $pkg | tr -d '\r')
echo "pid: ${pid:-none}"
[ -n "$pid" ]
! grep -q "FATAL EXCEPTION\|Fatal signal" android-logcat.txt
python3 - android-shots/start.png <<'PY'
import sys, zlib, struct
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'\x89PNG\r\n\x1a\n'
w, h = struct.unpack('>II', d[16:24])
idat = b''; i = 8
while i < len(d):
    n, = struct.unpack('>I', d[i:i+4]); t = d[i+4:i+8]
    if t == b'IDAT': idat += d[i+8:i+8+n]
    i += 12 + n
raw = zlib.decompress(idat)
colours = len(set(raw[j:j+64] for j in range(0, min(len(raw), 4_000_000), 4099)))
print('screen %dx%d, %d different samples' % (w, h, colours))
assert colours > 20, 'the screen is one colour'
PY
# the menus by touch and the back gesture: open Settings with a tap, leave it with Android's
# back key, open it again and leave it with the on-screen Back button (screenshots of each)
size=$(adb shell wm size | sed -n 's/.*: *//p' | tail -1 | tr -d '\r'); W=${size%x*}; H=${size#*x}
[ "$W" -lt "$H" ] && { t=$W; W=$H; H=$t; }
echo "screen $W x $H"
tap() { adb shell input tap $(( W * $1 / 1000 )) $(( H * $2 / 1000 )); sleep 2; }
shot() { adb exec-out screencap -p > android-shots/$1.png; }
tap 60 625; shot settings-1
adb shell input keyevent KEYCODE_BACK; sleep 2; shot after-back-key
tap 60 625; shot settings-2
tap 942 960; shot after-back-button
pid2=$(adb shell pidof $pkg | tr -d '\r'); echo "pid after the menus: ${pid2:-none}"; [ -n "$pid2" ]
echo "android start: PASS"
