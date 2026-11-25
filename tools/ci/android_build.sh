#!/bin/bash
# android_build.sh <sdl_source_dir> <out_dir> (roadmap R7.1)
# Puts SDL's own Android project (from the pinned SDL source, tools/ci/fetch_sdl.sh android)
# around the program and builds a debug APK for 64-bit ARM, plus x86_64 for the emulator in CI.
# Needs ANDROID_HOME and ANDROID_NDK_HOME (preinstalled on GitHub's Ubuntu runners) and JDK 17.
# The APK is signed with the Android debug key; the project's own release key is not set up yet.
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
ls -la "$out"
