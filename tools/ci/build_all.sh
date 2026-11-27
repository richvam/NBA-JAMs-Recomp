#!/bin/bash
# build_all.sh <linux|win|mac> <sdl_dir> <exe_out>
# What CI does for one platform, runnable on any Ubuntu/WSL box:
#   linux: libcore, liblua, the app and the ROM-free tests (luatest)
#   win:   the same libraries and the app, cross-compiled with mingw-w64
#   mac:   (on macOS) the app for arm64 and x86_64, joined into one universal file with lipo,
#          and the ROM-free tests for this Mac's own processor in build/mac
set -euo pipefail
cd "$(dirname "$0")/../.."
target=$1; sdl=$(realpath "$2"); exe_out=$3
j=$(nproc 2>/dev/null || sysctl -n hw.ncpu)
case $target in
  linux)
    make -j"$j" BUILD=build/linux build/linux/libcore.a build/linux/liblua.a build/linux/modtest build/linux/luatest build/linux/luadet build/linux/pathtest build/linux/newstest build/linux/nattest build/linux/uitest build/linux/touchtest build/linux/replaytest build/linux/arttest build/linux/sndlog
    make BUILD=build/linux SDL2L="$sdl" linux-app
    make BUILD=build/linux SDL2L="$sdl" build/linux/viewtest
    cp build/linux/NBAJamRecomp "$exe_out"
    ;;
  mac)
    host=$(uname -m)
    for a in arm64 x86_64; do
      cc="clang -arch $a -mmacosx-version-min=11.0"; cxx="clang++ -arch $a -mmacosx-version-min=11.0"
      make -j"$j" BUILD=build/mac-$a CC="$cc" CXX="$cxx" build/mac-$a/libcore.a build/mac-$a/liblua.a build/mac-$a/miniz.o
      make BUILD=build/mac-$a CC="$cc" CXX="$cxx" SDL2L="$sdl" linux-app
    done
    lipo -create build/mac-arm64/NBAJamRecomp build/mac-x86_64/NBAJamRecomp -output "$exe_out"
    lipo -info "$exe_out"
    # the tests, for this Mac's processor
    rm -rf build/mac; ln -s mac-$host build/mac
    make -j"$j" BUILD=build/mac-$host CXX="clang++ -mmacosx-version-min=11.0" CC="clang -mmacosx-version-min=11.0" build/mac-$host/luatest build/mac-$host/luadet build/mac-$host/pathtest build/mac-$host/uitest build/mac-$host/touchtest build/mac-$host/replaytest build/mac-$host/arttest build/mac-$host/nattest
    make BUILD=build/mac-$host CXX="clang++ -mmacosx-version-min=11.0" CC="clang -mmacosx-version-min=11.0" SDL2L="$sdl" build/mac-$host/viewtest
    ;;
  win)
    export PATH="$PWD/build/mingw-bin:$PATH"   # posix-threads variants first (winpthread, as the Makefile links)
    mkdir -p build/mingw-bin
    for t in g++ gcc; do ln -sf "$(command -v x86_64-w64-mingw32-$t-posix)" build/mingw-bin/x86_64-w64-mingw32-$t; done
    make -j"$j" BUILD=build/win CXX=x86_64-w64-mingw32-g++ CC=x86_64-w64-mingw32-gcc build/win/libcore.a
    make WIN_OUT=build/win-out SDL2W="$sdl" win
    cp build/win-out/NBAJamRecomp.exe "$exe_out"
    ;;
  *) echo "usage: $0 <linux|win|mac> <sdl_dir> <exe_out>" >&2; exit 2 ;;
esac
