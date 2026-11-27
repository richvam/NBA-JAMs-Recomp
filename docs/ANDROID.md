# Android

NBA JAM Recomp runs on Android 5 or later with a 64-bit ARM processor: phones, tablets and
handhelds such as the AYN Thor. The APK also has an x86_64 build, used by CI's emulator.

## Building

`tools/ci/android_build.sh <SDL source> <out folder>` puts SDL's own Android project (from the
pinned SDL source, `tools/ci/fetch_sdl.sh android`) around the program and builds with Gradle.
It needs JDK 17, `ANDROID_HOME` and `ANDROID_NDK_HOME` (GitHub's Ubuntu runners have them).

It always makes `NBAJamRecomp-<version>-android-debug.apk`, signed with the Android debug key,
for the emulator test. It also makes a release APK when it has a key:

* with the project's key (below): `NBAJamRecomp-<version>-android.apk`, the one in releases;
* with `ANDROID_TEST_KEY=1`: a throwaway key made for that run signs
  `NBAJamRecomp-<version>-android-testkey.apk`. CI does this on every branch so the signing
  steps are tested; that APK is never released.

## The signing key (project owner)

Android installs an update only when it is signed with the same key as the installed app, so
every release has to be signed with one key that is kept for good. The key never goes into the
repository. It is created once by the owner and stored as four CI secrets:

```
keytool -genkeypair -keystore nbajam-release.jks -alias nbajam -keyalg RSA -keysize 4096 -validity 10000
base64 -w0 nbajam-release.jks > nbajam-release.jks.b64
gh secret set ANDROID_KEYSTORE_B64 --repo richvam/NBA-JAMs-Recomp < nbajam-release.jks.b64
gh secret set ANDROID_KEYSTORE_PASSWORD --repo richvam/NBA-JAMs-Recomp
gh secret set ANDROID_KEY_ALIAS --repo richvam/NBA-JAMs-Recomp --body nbajam
gh secret set ANDROID_KEY_PASSWORD --repo richvam/NBA-JAMs-Recomp
```

Keep `nbajam-release.jks` and its passwords somewhere safe, outside the repository, and delete
the `.b64` file. A lost key means users have to uninstall (and lose their saves inside the app)
before a release signed with a new key installs.

A tag (`v*`) with no key fails the Android job on purpose, so a release never ships an APK
signed with some other key.

## Data

The app's folder is `Android/data/io.github.richvam.NBA_JAMs_Recomp/files` on shared storage,
which a computer sees over USB: `roms/`, `saves/`, `states/`, `mods/`, `nbajam_recomp.ini` and
`frame_times.log` (Settings, Video, "Log frame times") are there. ROMs are added in the app with
"Add ROMs" or "Add a ROM folder" (the system file picker; no storage permission).
