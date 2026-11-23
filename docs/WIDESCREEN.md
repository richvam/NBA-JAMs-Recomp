# Widescreen: how the games draw (research note)

Roadmap W2, the design note that W3 to W5 rest on. It is research only: nothing here
changes the program. It is based on the game sources (`DISP.EQU`, `NDSP1.ASM`, `PLYR.ASM`,
`PLYR2.ASM`, `COLL2.ASM`, `BAKGND.ASM`, `SCORE.ASM` and others), on the owner's ROMs read
in place, and on measurements taken in the emulator during a CPU-vs-CPU game of
Tournament Edition 4.0. Line numbers refer to the Tournament Edition source unless the
text says otherwise.

## Summary

* **The screen is 400 columns.** The games draw into video memory 512 pixels wide, with a 56-pixel pad on the left (`SCRNXP`). The screen box is `SCRNST = [0,-32]` to `SCRNEND = [254,432]`, and the screen centre is x = 200.
* **The camera is game logic.** `WORLDTLX`, the left edge of the view, decides which objects collide (`COLL2.ASM`) and where players and the ball are placed. Changing it would change the game.
* **A wider view can be "view only"** if the camera stays exactly as the game sets it and only the drawing is wider. Every place that draws for 400 columns can be widened outside the game. None of them needs a change to game memory.
* **The art runs out at the ends of the court.** The arena backdrop spans mid-court −512 to +675, so at the game's furthest camera position it covers exactly a 4:3 view. A wider view therefore needs its own camera centre: it follows the game's camera but stops short of the ends.
* **Recommendation (W2.6):** draw the wide picture in the program from the game's own object lists, read-only, with the existing blitter emulation drawing into a wider image. Do not patch the game to draw wider. Fixed screens stay 4:3.

## W2.1: every place that assumes 400 columns (Tournament Edition 4.0)

| Where | What it does |
|---|---|
| `DISP.EQU` 30-33 | `SCRNXP` 56 (left pad in the 512-wide bitmap), `SCRNST`, `SCRNEND`, `SCRNMID` [128,200] |
| `NDSP1.ASM` 925-941 | The DMA window per page: `[253,0]`, or `[509,256]` for page 1. The page offset `[PAGE1YO, SCRNXP]` adds the 56-pixel pad. |
| `NDSP1.ASM` 2002-2007, 2026-2029 | Start-up: the window is set to the full 512 columns. `SCRNTL`/`SCRNLR` are set from `SCRNST`/`SCRNEND`. |
| `NDSP1.ASM` 970-971 | `DISPLAY` loads `SCRNLR`/`SCRNTL` as the clip box for every object. |
| `NDSP1.ASM` 578+ (`dma_objlst2d`) | 2D objects: screen = world − `WORLDTL`, clipped to `SCRNTL`..`SCRNLR`. Objects outside are not drawn but still exist. |
| `NDSP1.ASM` 309+ (`dma_objlst`, `#3d`) | 3D objects (players, ball, hoops): screen x = 200 + (x − camera centre) × (z + 819 − 894) / 819.2. They are culled against the same box. |
| `NDSP1.ASM` 1140-1290 (`gnd_dodma`) | The court floor: each line of `COURT` (1277 × 138 pixels, 8 bits per pixel) is drawn 400 pixels wide (`[1,400]`), starting at `COURT + (1277/2 − 200)` plus a per-line offset of `gndx` × (1 + line/288). |
| `NDSP1.ASM` 1218, `PLYR2.ASM` 689 | `gndx` = camera centre − mid-court. It drives the floor's perspective. |
| `PLYR2.ASM` 490-690 | The camera follows the ball within mid-court ± `MAX_VIEW2` (253), or ± `MAX_VIEW1` (310) for inbounds and dunks (`GAME.EQU` 328-329). It is held 200 left of its centre (`WRLDMID-200±…`). |
| `PLYR.ASM` 8236-8248 | Taking the ball out: the camera jumps to ± `MAX_VIEW1`. |
| `PLYR.ASM` 216-219 | Players are held within mid-court ± 337 (± 370 for the second limit). |
| `COLL2.ASM` 196-219 | Collisions are checked only for objects from `WORLDTLX` to +400, in four 100-pixel strips. |
| `BAKGND.ASM` 348-370 (`BGND_UD1`) | Background blocks are added to and removed from the display list around `SCRNTL`..`SCRNLR` plus a pad. Only fixed screens use it; the list is empty during play. |
| `SCORE.ASM` 209-340 | Off-screen arrows. A player left of `WORLDTLX`+10, or right of the view, for too long gets an arrow at screen x 40 or 325. |
| `PLYR.ASM` 425, `PLYR2.ASM` 227 | Players and the ball are placed relative to `WORLDTLX` at the start of a quarter. |
| `PLYR2.ASM` 4527 | Camera flashes in the crowd: a random x across 400 columns. |
| `SCORE2.ASM` 708 | Screen shake moves `WORLDTLX` by a random amount, then back. |
| `HSTD.ASM` 902, `RACKUP.ASM` 1002/2161 | Initials entry and the rack-up tiles are placed relative to `WORLDTLX`. |
| `NDSP1.ASM` 1937-1960 (`SCRTST`) | Is an object on screen (`SCRNTL`..`SCRNLR`)? The referee is deleted once it walks off (`PLYR.ASM` 6820). |
| Score, clock and turbo bars | Screen-relative objects (`B_SCRNREL`), placed in 400-column screen coordinates. |

## W2.2: view only or affects play

**Affects play.** These read the camera or the screen box and change game state. They must stay exactly as they are:
* **The camera itself** (`WORLDTLX`, `gndx`, the `MAX_VIEW` limits, screen shake). The collision scan follows it, so a different camera would change which objects collide.
* **Collisions,** `WORLDTLX` to +400.
* **Where players, the ball and the initials boxes are placed,** relative to `WORLDTLX`.
* **Off-screen arrows.** They create objects from the shared pool of 450.
* **Background block streaming** (`BGND_UD1`). It moves objects between the free list and the display list, so a wider box would change object allocation. Fixed screens only.
* **The referee's deletion** (`SCRTST`). An object's lifetime depends on the screen box.
* **`DISPLAY` itself.** Besides drawing, it adds every object's velocity to its position (`vel_add`), so it must run as it does now.

**View only.** These only decide which pixels reach the screen:
* the DMA window and the 512-column video memory
* the clipping in `dma_objlst`, `dma_objlst2d` and `dma_objlst2dscl`
* the 400-pixel floor lines in `gnd_dodma`
* the +200 screen centre in the 3D projection
* where the screen-relative score, clock and turbo bars sit

**Conclusion.** A view-only widescreen is possible if the game's camera and every screen-box test are left alone, and the wider picture is drawn from what the game already holds. Patching `SCRNTL`/`SCRNLR` would widen the drawing, but it would also change the affects-play items above: the referee's lifetime and the background streaming. So the wide view must not come from patching those values. A 4:3 peer and a widescreen peer then run the same game bit for bit, which W3.5 checks.

Three visible quirks follow from keeping the camera, all of them in the original game too:
* An off-screen arrow can point at a player who is now visible at the side.
* Objects more than 200 pixels from the game's camera centre do not collide.
* The referee disappears when he leaves the 4:3 area.

The arrows can be left out of the wide drawing for players inside it. That is a drawing choice, not a game change.

## W2.3: how much art exists beyond 4:3

Measured on the owner's ROMs (Tournament Edition 4.0 and SE 5.02 share the art):

* **The court floor `COURT`** is 1277 × 138 pixels, and its non-transparent part is a trapezoid:
  * line y covers x = 189 − 1.37y to 1087 + 1.37y
  * that is 899 pixels on the top line and the full 1277 on the bottom line
  * outside it, colour 0 is see-through, so the stands behind show there, as they do in the 4:3 picture's top corners
  * the floor reaches both baselines
* **The arena backdrop** is the stands and banners. During play it is six 2D objects in world coordinates, three 400-pixel panels in each of two rows:
  * they span mid-court −512 to +675 (the right panel is the left one flipped)
  * they scroll one-to-one with the camera
  * during play the background block list is empty; the arena is made of objects
* **The game's camera centre** reached −300 and +299 in a CPU game. The code allows ±310.

So the art limits the widest view at a given render centre c (from mid-court) to a half-width of min(512 + c, 675 − c):

| View | Columns | Half | Render centre must stay within | The game's camera goes to |
|---|---|---|---|---|
| 4:3 | 400 | 200 | −312 … +475 | ±310 (fits; this is why the backdrop is that wide) |
| 16:9 | 533 | 266 | −246 … +409 | ±310 |
| 21:9 | 700 | 350 | −162 … +325 | ±310 |
| 32:9 | 1067 | 533 | +21 … +142 | ±310 |

The widest view the art allows is 1187 columns, with the render centre fixed at +81. What this means for each width:
* **16:9 and 21:9:** the wide view follows the game's camera, but stops 64 or 148 pixels short of each end of the court.
* **Near a basket:** the action then sits off-centre in the wide view. A player at the hoop (±320) is still well inside the view.
* **32:9:** the view barely moves. W5.1 should cap it at the widest clean width, as the roadmap already plans.

NBA JAM 1993 has the same floor geometry (1277 × 138, the same trapezoid) at 6 bits per pixel. Its backdrop is not measured yet; that is for W4.4.

## W2.4: which screens are gameplay

The display mode `dtype` tells them apart, and the program can read it each frame:

* **3D (`dtype` > 0): gameplay,** from the tip-off to the final buzzer, set in `BB.ASM` 1515. It also covers the attract-mode demo game (`demogame_start`). This is where the wide view applies.
* **2D (`dtype` = 0): fixed screens, drawn for 400 columns.** They stay 4:3 with the W1.5 side fill:
  * the title screen (`show_title2`) and the copyright notice (`show_copyright`)
  * the attract pages (`drw_love`, `drw_school`, `drw_stars`, `drw_ladder`, `drw_cutaway`, `hint_page`, `new_page`, `stats_page`), including the final game stats
  * team select and buy-in (`buyin_screen`), and the grand champions screen (`grand_champs_screen`, `drw_chicks`)
  * halftime and its clips (`halftime_showclips`)
  * the screen wipe (`WIPEOUT`) and the game-start setup (`BB.ASM` 1443)
  * initials entry and records (`HSTD.ASM`, `RECORD.ASM`) and the test menu (`AUDIT.ASM`)
* **Scaled transitions (`dtype` < 0):** `scrn_scalein`/`scrn_scaleout` and the 2D scale modes zoom a fixed screen in or out. They stay 4:3.

W3.3 switches on `dtype` with the side fill fading in or out, so no frame is stretched.

## W2.5: Hangtime, Maximum Hangtime, the Wolf Unit, NBA JAM 1993

* **Hangtime and Maximum Hangtime** (sources `SRC/`):
  * the same screen constants (`SCRNXP` 56, the same `SCRNST`/`SCRNEND`/`SCRNMID`)
  * a wider court: `GNDI_W` 1762 × 144 (`WORLD.EQU`), hoops at ±367, the camera to ±312 or ±375 (`MAX_VIEW2`/`MAX_VIEW1`)
  * the floor comes from a court or an outdoor court picture, chosen by `SAGCRT`/`SAGOTD`, with a bank number and a bits-per-pixel setting (`COURT_BC`). So the Tournament Edition search by pattern does not find it; W4.2 reads it from the source
  * the camera is read in the same kinds of places: ball and player placement, off-screen arrows, initials, screen shake
  * there is no camera-ranged collision scan like `COLL2.ASM`
  * `dtype` separates 2D and 3D screens as in Tournament Edition
* **The Wolf Unit** uses the same blitter and the same 512-column video memory in our emulation (`midway_video.cpp`). Picture addresses start at 0 instead of `0x2000000`. Nothing here changes the approach.
* **NBA JAM 1993:**
  * the Tournament Edition floor-drawing code is found in its program ROM by pattern
  * `COURT` is the same trapezoid at 6 bits per pixel
  * the camera, collision and backdrop code is matched against Tournament Edition by pattern in W4.4. Its symbol file has 918 names, including `WORLDTLX`.

## W2.6: the engine approach

**A. A wider video memory and patched games.** The emulated video memory grows past 512 columns, and each game is patched in memory to draw wider. The patches cover:
* the DMA window
* the page offset
* the clip box used by the drawing loops (separate from `SCRNTL`/`SCRNLR`, which the game logic also reads)
* the floor line width and start in `gnd_dodma`
* the background streaming box on fixed screens, if they were ever widened

That is about 6 to 8 patch sites per game and revision, so about 40 with five games and their revisions, each found by pattern and checked. The risks:
* **Game state:** any patch that touches what `DISPLAY` or `BGND_UD1` decide changes object allocation, which changes the game state hash. That breaks netplay between a 4:3 and a widescreen player.
* **Timing:** the blitter would draw more each frame, and it is timed on the board.
* **The art:** the end-of-court limit still needs a render camera. The game's own camera must not change, so this patching cannot provide one.

**B. The program draws the wide picture from the game's lists (recommended).** At the moment `DISPLAY` starts, the program reads the object list, the camera, `gndx`, `dtype` and the palettes from emulated memory, without writing anything. It then draws:
* the floor lines from `COURT`, wider
* 2D and 3D objects with the game's own projection, around its own render camera
* screen-relative objects (score, clock) moved to the edges or centre as W3.4 decides

The drawing uses the blitter code that already exists (`midway_video.cpp`) with a wider target, so scaling, flipping, transparency and palettes behave exactly as on the board. Costs and risks:
* **No game patches:** game state is untouched, so the state hash is the same with any view width, and a widescreen player can play a 4:3 player online by construction.
* **Code per game:** a C++ version of three short drawing loops (`dma_objlst`, `dma_objlst2d`, `gnd_dodma`) and the addresses of a few variables, all already in the symbol files for four of the five games.
* **Timing:** the lists must be read when `DISPLAY` reads them. Read later, `vel_add` would already have moved the objects by one step. This needs a hook in the emulator at `DISPLAY`'s entry that only reads.
* **Extra pixels:** objects the game already deleted off-screen (the referee) are not there to draw.
* **Fixed screens** keep the game's own 400-column picture from video memory, as now.

**Choice: B.**
* It is view only by construction, which W2.2 shows is the requirement.
* It needs no patch sites to keep up across five games and their revisions.
* It gives the render camera that W2.3 shows the art needs.

A costs less code at first, but it cannot be both wide and safe for netplay. The gate before W3 is the owner's approval of this choice.
