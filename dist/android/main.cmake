# R7.1: the program as libmain.so for SDL's Android project (tools/ci/android_build.sh puts
# this file in app/jni/src/CMakeLists.txt and passes NBAJ_ROOT, the repository).
cmake_minimum_required(VERSION 3.6)
project(NBAJamRecomp C CXX)

set(R ${NBAJ_ROOT})
# the same rules as the Makefile: wrapping integers, no fused multiply-add, signed char (R5)
set(DET -fwrapv -ffp-contract=off -fsigned-char)

file(GLOB LUA_SRC ${R}/src/third_party/lua/*.c)
list(REMOVE_ITEM LUA_SRC ${R}/src/third_party/lua/lua.c ${R}/src/third_party/lua/luac.c)
file(GLOB DETMATH_SRC ${R}/src/third_party/detmath/*.c)
set(CORE_SRC
  src/emu/emu.cpp
  src/mame/cpu/tms34010/tms34010.cpp src/mame/cpu/tms34010/34010dsm.cpp
  src/mame/cpu/m6809/m6809.cpp src/mame/cpu/m6809/6x09dasm.cpp
  src/mame/cpu/adsp2100/adsp2100.cpp src/mame/cpu/adsp2100/2100dasm.cpp
  src/mame/sound/okiadpcm.cpp src/mame/ymfm/ymfm_opm.cpp src/mame/ymfm/ymfm_misc.cpp src/mame/ymfm/ymfm_adpcm.cpp src/mame/ymfm/ymfm_ssg.cpp
  src/board/midway_video.cpp src/board/williams_adpcm.cpp src/board/tunit.cpp src/board/dcs.cpp src/board/wunit.cpp src/board/wide_view.cpp)
set(APP_SRC
  src/app/main.cpp src/app/games.cpp src/app/romload.cpp src/app/netplay.cpp src/app/nat.cpp src/app/gl_present.cpp
  src/app/controls.cpp src/app/mods.cpp src/app/menu_sound.cpp src/app/pad_script.cpp src/app/version.cpp src/app/paths.cpp
  src/app/news.cpp src/app/view.cpp src/app/png_read.cpp src/app/overlay.cpp src/app/ui/ui.cpp src/app/ui/gallery.cpp
  src/app/ui/lettering.cpp src/app/ui/themes.cpp src/app/ui/screens.cpp src/app/ui/keyboard.cpp src/app/ui/art_view.cpp
  src/app/rom_art.cpp src/app/touch_pad.cpp src/app/replay.cpp)
list(TRANSFORM CORE_SRC PREPEND ${R}/)
list(TRANSFORM APP_SRC PREPEND ${R}/)

add_library(nbaj_lua STATIC ${LUA_SRC})
target_compile_options(nbaj_lua PRIVATE -O2 ${DET} -std=gnu99)
add_library(nbaj_detmath STATIC ${DETMATH_SRC})
target_compile_options(nbaj_detmath PRIVATE -O2 ${DET} -fno-builtin -std=gnu99 -w)
add_library(nbaj_miniz STATIC ${R}/src/third_party/miniz/miniz.c)
target_compile_options(nbaj_miniz PRIVATE -O2 ${DET})
add_library(nbaj_core STATIC ${CORE_SRC})
target_include_directories(nbaj_core PRIVATE ${R}/src/emu ${R}/src/mame ${R}/src/board)
target_compile_options(nbaj_core PRIVATE -O2 ${DET} -std=c++20 -Wno-deprecated-declarations)

add_library(main SHARED ${APP_SRC})
target_include_directories(main PRIVATE ${R}/src/emu ${R}/src/mame ${R}/src/board ${R}/src/app ${R}/src/third_party/miniz ${R}/src/third_party/lua)
target_compile_options(main PRIVATE -O2 ${DET} -std=c++20 -Wno-deprecated-declarations)
target_compile_definitions(main PRIVATE APP_VERSION="${NBAJ_VERSION}" APP_COMMIT="${NBAJ_COMMIT}" APP_DATE="${NBAJ_DATE}")
target_link_libraries(main SDL2 nbaj_core nbaj_lua nbaj_detmath nbaj_miniz GLESv2 EGL log android)
