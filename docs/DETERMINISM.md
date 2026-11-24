# Identical emulation on every build (roadmap R5.1 audit)

Online play, save states sent between computers and, later, ports to other processors all need
the same game to give the same result, frame for frame, on every processor and compiler. This
is the audit of everything that feeds the game state, done on 2026-11-24, with what was found,
what was fixed and what was shown to be harmless.

## How it was checked

`tools/headless/goldentest <roms> <frames> [games]` plays each game with scripted two-player
inputs and prints, every 600 frames, a hash of the main RAM (the game state), of every picture
and of every sound sample, and at the end a hash of a save state. Two builds that agree print
the same lines. With the owner's ROMs, 3 000 frames of all five games:

| Build | Result |
|---|---|
| GCC 15, x86-64 Linux, `-O2` | the reference |
| the same with every uninitialised local set to zero (`-ftrivial-auto-var-init=zero`) | identical |
| the same with every uninitialised local set to a pattern (`=pattern`) | identical |
| the reference with every heap allocation filled with 0xA5, then 0x5A (`MALLOC_PERTURB_`) | identical |
| MinGW-w64 GCC, Windows 11 (another OS and C library), run on the owner's PC | identical |

Before the fixes below, the game (RAM, picture, sound) was already identical in all of them,
but the save states were not: they held bytes that were never set.

## Findings

| What | Where | Result |
|---|---|---|
| **Padding in save states.** A state wrote some structures as raw bytes, padding included, and padding is never set: `attotime` (4 bytes between seconds and attoseconds, in every timer), the video chip's DMA state, the OKI sound chip's voices and ADPCM state. | `src/emu/emu.h`, `src/board/midway_video.h`, `src/board/williams_adpcm.h`, `src/mame/sound/okiadpcm.h` | **Fixed.** Each writes its fields one by one. `state_io` now refuses, at compile time, any type with padding (`std::has_unique_object_representations`), so a new one cannot slip in. States from earlier versions do not load (they never loaded into another version anyway). |
| **`long double` in timer arithmetic.** `attotime / n` divided in `long double`, which is 80 bits on x86-64 Linux, 128 on ARM64 Linux and 64 with MSVC and on Apple. It sets the DCS sound board's timer, which the main CPU sees through the sound board. | `src/emu/emu.h`, used by `src/board/dcs.cpp` | **Fixed**, exact in integers. Harmless before in practice: the difference only arose for periods of a second or more, and the DCS periods are far shorter (the hashes did not change). |
| **`pow()` in the OKI step table.** `floor(16 * pow(1.1, step))` relies on the C library's `pow`. | `src/mame/sound/okiadpcm.cpp` | **Fixed**: the 49 values are written out (checked against exact rational arithmetic; same values). |
| **`char` signedness.** `char` is unsigned on ARM Linux and Android, signed on x86 and MSVC. | whole emulation | **Fixed**: ARM builds use `-fsigned-char` (R4.2), and `emu.h` now refuses to build with an unsigned `char`. Android builds (R7) must pass the flag. |
| **Signed integer overflow.** Undefined in C++; GCC and Clang may optimise on that assumption, differently by version. | whole emulation | **Fixed**: `-fwrapv` (overflow wraps, as MSVC does in practice). An MSVC build (R6/R7) needs no flag. |
| **Fused multiply-add.** Clang contracts `a * b + c` into one instruction by default, and ARM has it, which rounds differently. | floating point (see below) | **Fixed**: `-ffp-contract=off` on every build. |
| **Shifts of negative numbers.** Implementation-defined before C++20. | whole emulation | **Harmless**: the build is always C++20, where both shifts are defined (the Makefile now adds `-std=c++20` even when the environment sets its own flags). |
| **Floating point.** Found in the sound mixing (OKI volume, YM2151 and DCS output, resampling), the frame rate for pacing, and logs. | `src/board/*` | **Harmless to the game**: none of it is read back by any emulated CPU; it only shapes the sound sent to the speakers. With `-ffp-contract=off` and IEEE arithmetic (SSE2 / NEON, no x87) it is the same on every target anyway; the sound hashes agree. |
| **Byte order.** Memory regions, states and hashes are read as little-endian bytes. | whole emulation | **Harmless**: every target in the roadmap is little-endian; `emu.h` refuses to build on a big-endian processor. |
| **Uninitialised memory.** | whole emulation | **None that matters**: identical results with locals set to zero or a pattern and with the heap filled with two bytes (table above). |
| **Struct layout in states.** | `state_io` | Covered by the padding fix: no type with padding can be written whole. |

## Not covered yet

- MSVC: there is no MSVC build yet (R6/R7).
- A real ARM64 machine. `tools/golden/golden_cross.sh` (R5.2) already compares GCC x86-64,
  Clang x86-64 and Clang ARM64 under qemu-user: identical, 3 000 frames of all five games,
  save states included. The CI machines have no ROMs (none may be uploaded), so this is a
  local check.
- The Lua engine (R5.4).
