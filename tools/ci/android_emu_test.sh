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
echo "android start: PASS"
