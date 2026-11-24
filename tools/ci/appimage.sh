#!/bin/bash
# appimage.sh <x86_64|aarch64> <unpacked package folder> <out file>
# Builds the AppImage (roadmap R4.1) from the Linux package (the unpacked .tar.gz). The
# package's portable.txt is left out: an AppImage cannot write inside itself, so the program
# keeps settings, saves, mods and ROMs in ~/.local/share/NBAJamRecomp (src/app/paths.h) and
# copies the shipped mods there on first start.
# appimagetool and the AppImage runtime are pinned releases, checked by SHA-256.
set -euo pipefail
arch=$1; pkg=$(realpath "$2"); out=$(realpath -m "$3")
root=$(cd "$(dirname "$0")/../.." && pwd)
TOOL_VERSION=1.9.1
RUNTIME_VERSION=20251108
case $arch in
  x86_64)  RUNTIME_SHA=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d ;;
  aarch64) RUNTIME_SHA=00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444 ;;
  *) echo "unknown arch $arch" >&2; exit 2 ;;
esac
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# the tool runs on the build machine, so it is the build machine's architecture
host=$(uname -m)
case $host in x86_64) HOST_SHA=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 ;; aarch64) HOST_SHA=f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158 ;; *) echo "no appimagetool for $host" >&2; exit 2 ;; esac
curl -fsSL --retry 3 -o "$tmp/appimagetool" "https://github.com/AppImage/appimagetool/releases/download/$TOOL_VERSION/appimagetool-$host.AppImage"
echo "$HOST_SHA  $tmp/appimagetool" | sha256sum -c -
curl -fsSL --retry 3 -o "$tmp/runtime" "https://github.com/AppImage/type2-runtime/releases/download/$RUNTIME_VERSION/runtime-$arch"
echo "$RUNTIME_SHA  $tmp/runtime" | sha256sum -c -
chmod +x "$tmp/appimagetool"

app=$tmp/NBAJamRecomp.AppDir
lib=$app/usr/lib/nbajamrecomp
mkdir -p "$lib" "$app/usr/share/applications" "$app/usr/share/icons/hicolor/scalable/apps"
cp -r "$pkg"/. "$lib/"
rm -f "$lib/portable.txt"
[ -x "$lib/NBAJamRecomp" ] || { echo "no NBAJamRecomp in $pkg" >&2; exit 1; }
cp "$root/dist/linux/nbajamrecomp.desktop" "$app/nbajamrecomp.desktop"
cp "$root/dist/linux/nbajamrecomp.desktop" "$app/usr/share/applications/"
cp "$root/dist/linux/nbajamrecomp.svg" "$app/nbajamrecomp.svg"
cp "$root/dist/linux/nbajamrecomp.svg" "$app/usr/share/icons/hicolor/scalable/apps/"
cat > "$app/AppRun" <<'EOF'
#!/bin/sh
HERE=$(dirname "$(readlink -f "$0")")
exec "$HERE/usr/lib/nbajamrecomp/NBAJamRecomp" "$@"
EOF
chmod +x "$app/AppRun"

mkdir -p "$(dirname "$out")"
ARCH=$arch "$tmp/appimagetool" --appimage-extract-and-run --no-appstream --runtime-file "$tmp/runtime" "$app" "$out"
ls -la "$out"
