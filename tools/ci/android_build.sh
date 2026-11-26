#!/bin/bash
# android_build.sh <sdl_source_dir> <out_dir> (roadmap R7.1)
# Puts SDL's own Android project (from the pinned SDL source, tools/ci/fetch_sdl.sh android)
# around the program and builds a debug APK for 64-bit ARM, plus x86_64 for the emulator in CI.
# Needs ANDROID_HOME and ANDROID_NDK_HOME (preinstalled on GitHub's Ubuntu runners) and JDK 17.
# The debug APK is signed with the Android debug key (for CI's emulator). R8.2: a release APK is
# built too and signed with the project's own key when ANDROID_KEYSTORE_B64 (the keystore,
# base64), ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD are set (CI
# secrets, never in the repository); with ANDROID_TEST_KEY=1 instead, a throwaway key made here
# signs it, to test the steps (that APK is named -testkey and is never released).
set -euo pipefail
cd "$(dirname "$0")/../.."
root=$PWD; sdl=$(realpath "$1"); out=$2
ver=$(git describe --tags --match 'v*' --always --dirty 2>/dev/null | sed -e 's/^v//' -e '/\./!s/^/dev-/' | grep . || echo dev)
commit=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
date=$(date -u +%Y-%m-%d)
ndk_ver=$(sed -n 's/^Pkg.Revision *= *//p' "$ANDROID_NDK_HOME/source.properties")
p=build/android
mkdir -p build; rm -rf "$p"; cp -r "$sdl/android-project" "$p"
ln -s "$sdl" "$p/app/jni/SDL"
cp dist/android/main.cmake "$p/app/jni/src/CMakeLists.txt"
# the app's activity: SDL's, plus the system file picker for ROMs (R7.2)
mkdir -p "$p/app/src/main/java/io/github/richvam/nbajam"
cp dist/android/NBAJamActivity.java "$p/app/src/main/java/io/github/richvam/nbajam/"
rm -f "$p/app/jni/src/Android.mk"
python3 - "$p" "$root" "$ver" "$commit" "$date" "$ndk_ver" <<'PY'
import sys, re
p, root, ver, commit, date, ndk = sys.argv[1:]
def ed(f, a, b):
    s = open(f).read(); assert a in s, (f, a); open(f, 'w').write(s.replace(a, b, 1))
g = p + '/app/build.gradle'
ed(g, 'minSdkVersion 19', 'applicationId "io.github.richvam.NBA_JAMs_Recomp"\n        minSdkVersion 21')
ed(g, 'versionName "1.0"', 'versionName "%s"' % ver)
ed(g, '''ndkBuild {
                arguments "APP_PLATFORM=android-19"
                abiFilters 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'
            }''', '''cmake {
                arguments "-DANDROID_STL=c++_static", "-DNBAJ_ROOT=%s", "-DNBAJ_VERSION=%s", "-DNBAJ_COMMIT=%s", "-DNBAJ_DATE=%s"
                abiFilters 'arm64-v8a', 'x86_64'
            }''' % (root, ver, commit, date))
ed(g, '''ndkBuild {
                path 'jni/Android.mk'
            }''', '''cmake {
                path 'jni/CMakeLists.txt'
            }''')
ed(g, 'compileSdkVersion 34', 'compileSdkVersion 34\n    ndkVersion "%s"' % ndk)
ed(p + '/app/src/main/AndroidManifest.xml', 'android:preferMinimalPostProcessing="true"', 'android:preferMinimalPostProcessing="true"\n            android:screenOrientation="sensorLandscape"')
# online play: sockets (INTERNET), which network is up and its router (NETWORK_STATE), and
# broadcasts on Wi-Fi for the home-network search (the multicast lock)
ed(p + '/app/src/main/AndroidManifest.xml', '<uses-permission android:name="android.permission.VIBRATE" />', '<uses-permission android:name="android.permission.VIBRATE" />\n    <uses-permission android:name="android.permission.INTERNET" />\n    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />\n    <uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />\n    <uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />')
ed(p + '/app/src/main/AndroidManifest.xml', '<activity android:name="SDLActivity"', '<activity android:name="io.github.richvam.nbajam.NBAJamActivity"')
ed(p + '/app/src/main/res/values/strings.xml', '<string name="app_name">Game</string>', '<string name="app_name">NBA JAM Recomp</string>')
PY
# the files the other packages ship next to the program, as APK assets with a list of them;
# the app unpacks them on start (src/app/main.cpp, unpack_android_assets)
a="$p/app/src/main/assets"; mkdir -p "$a/shipped/roms"
cp -r dist/NBAJamRecomp/mods dist/NBAJamRecomp/overlays dist/NBAJamRecomp/README.txt "$a/shipped/"
cp dist/NBAJamRecomp/roms/README.txt "$a/shipped/roms/"
( cd "$a/shipped" && find . -type f | sed 's#^\./##' | sort ) > "$a/shipped.txt"
echo "$(wc -l < "$a/shipped.txt") shipped files in the APK"
( cd "$p" && ./gradlew --no-daemon -q assembleDebug )
mkdir -p "$out"
cp "$p/app/build/outputs/apk/debug/app-debug.apk" "$out/NBAJamRecomp-$ver-android-debug.apk"

# ---- the release APK (R8.2) ----
name=$ver; [ "${GITHUB_REF_TYPE:-}" = tag ] && name=v$ver
ks=""; suffix=""
if [ -n "${ANDROID_KEYSTORE_B64:-}" ]; then
  ks=$(mktemp -d)/release.jks
  printf '%s' "$ANDROID_KEYSTORE_B64" | base64 -d > "$ks"
elif [ "${ANDROID_TEST_KEY:-}" = 1 ]; then
  ks=$(mktemp -d)/test.jks; suffix=-testkey
  export ANDROID_KEYSTORE_PASSWORD=testpass ANDROID_KEY_ALIAS=test ANDROID_KEY_PASSWORD=testpass
  keytool -genkeypair -keystore "$ks" -storepass testpass -keypass testpass -alias test -keyalg RSA -keysize 2048 -validity 1     -dname "CN=NBA JAM Recomp test key" >/dev/null 2>&1
fi
if [ -n "$ks" ]; then
  ( cd "$p" && ./gradlew --no-daemon -q assembleRelease )
  bt=$(ls -d "$ANDROID_HOME"/build-tools/* | sort -V | tail -1)
  ls -la "$p/app/build/outputs/apk/release/"
  un=$(ls "$p"/app/build/outputs/apk/release/*.apk | head -1)
  echo "release build: $un, build tools $bt"
  "$bt/zipalign" -f -p 4 "$un" "$p/aligned.apk"
  apk="$out/NBAJamRecomp-$name-android$suffix.apk"
  "$bt/apksigner" sign --ks "$ks" --ks-pass env:ANDROID_KEYSTORE_PASSWORD --ks-key-alias "$ANDROID_KEY_ALIAS"     --key-pass env:ANDROID_KEY_PASSWORD --out "$apk" "$p/aligned.apk"
  rm -f "$ks"
  "$bt/apksigner" verify --print-certs "$apk" | grep -E "Signer #1 certificate (DN|SHA-256)"
fi
ls -la "$out"
