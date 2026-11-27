#!/bin/bash
# fetch_sdl.sh <linux|win> <install_dir>
# Downloads the pinned SDL2 release, checks its SHA-256 and prepares it where the
# Makefile expects it:
#   linux: static libSDL2.a built from source  -> <install_dir>/{include,lib}   (make SDL2L=<install_dir>)
#   win:   official MinGW development package  -> <install_dir>/{include,lib}   (make SDL2W=<install_dir>)
#   mac:   static libSDL2.a built from source for arm64 and x86_64 in one file, macOS 11 and later
#   android: the source tree itself, for SDL's Android project
# Skipped when <install_dir> already holds a build made by this script version.
set -euo pipefail
SDL_VERSION=2.30.9
SRC_SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4      # SDL2-2.30.9.tar.gz
MINGW_SHA256=b188d165dc4a0372042bff3d0c18e9bb90772581e92a7b8b47b521b47714f75c    # SDL2-devel-2.30.9-mingw.tar.gz
BASE=https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION

kind=$1; mkdir -p "$2"; out=$(cd "$2" && pwd)
stamp="$out/.fetched-$SDL_VERSION-$kind"
[ -f "$stamp" ] && { echo "SDL2 $SDL_VERSION ($kind) already in $out"; exit 0; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fetch() { # file sha
  curl -fsSL --retry 3 -o "$tmp/$1" "$BASE/$1"
  if command -v sha256sum >/dev/null; then echo "$2  $tmp/$1" | sha256sum -c -; else echo "$2  $tmp/$1" | shasum -a 256 -c -; fi
}
rm -rf "$out"; mkdir -p "$out"
case $kind in
  linux)
    fetch SDL2-$SDL_VERSION.tar.gz $SRC_SHA256
    tar -xzf "$tmp/SDL2-$SDL_VERSION.tar.gz" -C "$tmp"
    ( cd "$tmp/SDL2-$SDL_VERSION" && ./configure --prefix="$out" --disable-shared --enable-static >/dev/null && make -j"$(nproc)" >/dev/null && make install >/dev/null )
    ;;
  win)
    fetch SDL2-devel-$SDL_VERSION-mingw.tar.gz $MINGW_SHA256
    tar -xzf "$tmp/SDL2-devel-$SDL_VERSION-mingw.tar.gz" -C "$tmp"
    cp -r "$tmp/SDL2-$SDL_VERSION/x86_64-w64-mingw32/." "$out/"
    ;;
  mac)
    fetch SDL2-$SDL_VERSION.tar.gz $SRC_SHA256
    tar -xzf "$tmp/SDL2-$SDL_VERSION.tar.gz" -C "$tmp"
    cmake -S "$tmp/SDL2-$SDL_VERSION" -B "$tmp/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$out" \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF >/dev/null
    cmake --build "$tmp/build" -j"$(sysctl -n hw.ncpu)" >/dev/null
    cmake --install "$tmp/build" >/dev/null
    ;;
  android)
    # R7.1: the checked source tree only; SDL's Android project builds it (tools/ci/android_build.sh)
    fetch SDL2-$SDL_VERSION.tar.gz $SRC_SHA256
    tar -xzf "$tmp/SDL2-$SDL_VERSION.tar.gz" -C "$tmp"
    cp -r "$tmp/SDL2-$SDL_VERSION/." "$out/"
    ;;
  *) echo "usage: $0 <linux|win|mac|android> <dir>" >&2; exit 2 ;;
esac
touch "$stamp"
echo "SDL2 $SDL_VERSION ($kind) ready in $out"
