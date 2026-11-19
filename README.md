# NBA JAM Recomp

Midway's NBA JAM arcade games, **rebuilt from their original source code**, in one program
for Windows and Linux: a game picker, CRT shaders, remappable controllers, Lua mods and
online play for up to four players.

Created by **Pocket_lucho** with **Claude** (Anthropic).
If you want to support me, go to https://buymeacoffee.com/pocket_lucho

| Game | Board | Status |
|---|---|---|
| NBA JAM (1993), rev 3.01 | T-Unit | runs from the MAME romset (its source code was never published) |
| NBA JAM Tournament Edition, rev 4.0 | T-Unit | **built from source: program and graphics byte-identical to the original ROM** |
| NBA JAM T.E. Special Edition, rev 5.02 (Asure, 2025) | T-Unit | **built from source: program byte-identical** |
| NBA Hangtime, L1.3 / L1.1 | Wolf Unit | runs from the ROMs |
| NBA Maximum Hangtime, 1.3 | Wolf Unit | **built from source: program byte-identical** (1.03 from MAME also runs) |

**No game data is included.** Midway's code, graphics, sound and music are copyrighted, so
neither this repository nor the downloads contain any of them. You bring your own files
(see [Where the ROMs go](#2-where-the-roms-go)).

Contents: [Download](#1-download) · [Where the ROMs go](#2-where-the-roms-go) ·
[First run](#3-first-run) · [Controls](#4-controls) · [Online](#5-online) ·
[Lua mods](#6-lua-mods) · [Troubleshooting](#7-troubleshooting) ·
[Building it yourself](docs/BUILDING.md) · [What changed](CHANGELOG.md)

## 1. Download

Get the latest version from the **[Releases page](https://github.com/richvam/NBA-JAMs-Recomp/releases)**.

| System | File |
|---|---|
| Windows 10/11, 64-bit | `NBAJamRecomp-<version>-windows-x64.zip` |
| Linux, 64-bit (x86-64) | `NBAJamRecomp-<version>-linux-x64.tar.gz` |

Unpack it anywhere. There is nothing to install. Windows may show a SmartScreen warning
because the program is not signed: choose "More info", then "Run anyway".

## 2. Where the ROMs go

Copy your own files into the `roms` folder next to the program. The program recognises
every file by its contents (CRC32), whatever its name, inside zips and sub-folders too,
so there is nothing to unpack or rename.

| Game | What to put in `roms` |
|---|---|
| NBA JAM (1993) | MAME romset `nbajam.zip` |
| NBA JAM Tournament Edition | MAME romset `nbajamte.zip`, or the ZIP of the [historicalsource/nba-jam-tournament-edition](https://github.com/historicalsource/nba-jam-tournament-edition) repository (without `nbajamte.zip` it uses the Special Edition sound ROMs) |
| NBA JAM T.E. Special Edition | the ZIP of the [Asure/nba-jam-tournament-edition-SE](https://github.com/Asure/nba-jam-tournament-edition-SE) repository |
| NBA Hangtime | MAME romset `nbahangt.zip` (L1.3 or L1.1) |
| NBA Maximum Hangtime | version 1.3: the ZIP of the [historicalsource/nba-hangtime](https://github.com/historicalsource/nba-hangtime) repository plus `nbahangt.zip` (graphics and sound); or version 1.03: MAME romset `nbamht.zip` |

Repository ZIPs are downloaded on GitHub with "Code", then "Download ZIP".

## 3. First run

1. Run `NBAJamRecomp` (`NBAJamRecomp.exe` on Windows).
2. Pick a game with a controller, the keyboard or the mouse. A game shown in grey is missing
   files; the panel beside the list says which.
3. Insert a coin and press Start (on the keyboard: `5`, then `1`; on a controller: Back,
   then Start).
4. In a game, open the **pause menu** with `Esc`, the Guide button, or by holding Start and
   Back for half a second. It has everything the program can do during a game: settings,
   controls, coins, the TEST switch, reset, help, and "Quit to the game list".

Everything works with a controller alone: menus, settings, button binding, Lua mods, online
games (the host's address is typed on an on-screen keyboard), the pause menu.

The downloads are **portable**: settings, saves, screenshots, controller profiles, mods and
ROMs all stay in the unpacked folder, because it contains a file called `portable.txt`.
Without that file the program keeps them in your user folder instead
(`%APPDATA%\NBAJamRecomp` on Windows, `~/.local/share/NBAJamRecomp` on Linux). The About
screen shows which folder is in use.

Updating: unpack the new version over the old folder, or into a new one and copy `roms`,
`saves` and `nbajam_recomp.ini` across. The main menu tells you when a newer version has
been published (**News and updates**; it only tells you, nothing is downloaded, and it can
be switched off there).

## 4. Controls

In the menus: the directions move, A (Enter) chooses, B (Esc) goes back, the shoulder
buttons (Q and E) change tab, X (Delete) is the extra action a screen names at the bottom.
The mouse and touch work too. The bottom line always says which button does what, in the
names of the device you used last.

In a game, by default:

| | Move | Shoot | Pass | Turbo | Start | Coin |
|---|---|---|---|---|---|---|
| Controller | stick or d-pad | A | B | X, a shoulder or a trigger | Start | Back |
| Keyboard, player 1 | arrow keys | Z | X | C | 1 | 5 |
| Keyboard, player 2 | W S A D | F | G | V | 2 | 6 |
| Keyboard, player 3 | numpad 8 2 4 6 | numpad 0 | numpad . | numpad Enter | 3 | 7 |
| Keyboard, player 4 | I K J L | H | ; | U | 4 | 8 |

Each controller is one player. To change its buttons, open **Controls** in the main menu:
choose the player, choose or create a profile, choose an action and press the button for
it. Any game can have its own profile. One button can hold several arcade buttons at once
(shove, turbo shot and two shortcuts of your choice). Profiles can be exported and imported
as `.ini` files. **About and help**, tab Controls, lists what every action is on right now.
Keyboard keys are changed in `nbajam_recomp.ini`.

**Settings** (main menu, or "Video and sound" in the pause menu) has the picture (shape,
scaling, look, scanlines, what fills the sides of a 4:3 picture, fullscreen, the TV safe
area), the sound (volume, the menus' music and sounds), the cabinet switches and the
program's own options. The menus play the highlighted game's own music and sounds, from its
ROMs; without them they are silent.

The keyboard also has shortcuts for what is in those menus:

| Key | Does |
|---|---|
| Esc | pause menu |
| F1 | help over the game |
| F2 | test switch (the game's service menu) |
| F3 | reset the game |
| F4 | picture: sharp, smooth, scanlines, CRT, curved CRT |
| F5 | scaling: Fit (fills the height, crisp pixels), Smooth (the same, filtered), Integer (whole-number size, may leave borders) |
| F6 | scanline strength |
| F7 | shape: Original 4:3, or Stretch (fills the window, distorts the picture). There is no true widescreen view yet: the games draw a 4:3 picture, and showing more of the court needs changes to each game that are still to come. |
| F8 | around the 4:3 picture on a wider screen: black, the picture blurred and dimmed, then each overlay in the `overlays` folder (see [Overlays](#overlays-and-borders)) |
| F11 or Alt+Enter | fullscreen |
| F12 or Print Screen | screenshot, saved in `screenshots` |
| P | pause without the menu |
| - and + | volume |
| 9 | service credit |

### Overlays and borders

On a screen wider than 4:3, Settings (Video, "Sides of a 4:3 picture") or F8 puts a picture around the game, as RetroArch's overlays do.
Two come with the program (`cabinet_purple`, `tube_shadow`). To use your own, put a PNG in
the `overlays` folder (next to the program in the portable download; About shows the data
folder otherwise) and choose it in Settings, or press F8 until its name appears.

* The usual border is a 16:9 image, for example 1920x1080, with a transparent 4:3 opening
  in the middle (1440x1080). The game shows through the opening and the border is drawn
  over it, so it can overlap the edge of the picture: a rounded tube, a shadow, a frame.
* The image is scaled to the height of the screen and centred, so the same border fits
  16:9, 16:10 and ultrawide screens.
* **RetroArch border overlays work:** copy the `.cfg` and its `.png` into `overlays`. The
  image named by `overlay0_overlay` is used; button layouts are ignored.
* A PNG with no transparency is drawn behind the game instead.
* Use scaling Fit or Smooth, so the picture fills the opening. `overlay_opacity=` in
  `nbajam_recomp.ini` makes a border see-through (0.1 to 1).
* PNGs must be 8 bits per channel and not interlaced.

<!-- codes:start (written by tools/codes_from_source.py) -->
### Cabinet codes

The games accept their original cabinet codes; this is what each one does, read from the
games' source (and, for NBA JAM 1993, its program). The tables for Lua mods are in
`mods/lib/codes_<game>.lua`; the secret players are listed in [docs/CODES.md](docs/CODES.md).

**NBA JAM Tournament Edition and T.E. Special Edition** (the same codes): tap any button on the
matchup screen to count taps; hold the others at tip-off.

| Code | How |
| --- | --- |
| Tournament mode | Hold turbo, pass, shoot and right on the matchup screen |
| Maximum power | Hold turbo, pass, shoot and down on the matchup screen (player 1 without start; players 2-4 with start held) |
| Show shot percentage | Tap any button 15 times or more, then turn the stick a full circle, on the matchup screen |
| Computer assistance off | Tap any button exactly 10 times on the matchup screen (about 1.5 s in, when the game checks) |
| Powerup goaltending | Tap any button exactly 24 times on the matchup screen |
| Quick hands | Tap any button 5 times or more on the matchup screen, then hold shoot and down at tip-off |
| Big head | Hold up, turbo and pass at tip-off |
| Huge head | Tap any button exactly 6 times on the matchup screen, then hold up, turbo and pass at tip-off |
| Baby-size players | Hold down and right, and press turbo, shoot, pass three times (one at a time) on the matchup screen |
| Expanded roster | On team select, tap pass 4 times or more while the stick has been turned all the way round |
| Switch teams at halftime | On the halftime team screen, hold pass and right for 2 seconds (120 frames); then pass changes the team |

Tournament mode (turbo, pass, shoot and right) also turns the tap codes off. The secret players
are entered as initials and a birth date: 61 in T.E., 68 in SE.

**NBA JAM (1993)**:

| Code | How |
| --- | --- |
| Show shot percentage | Tap any button 15 times or more, then turn the stick a full circle, on the matchup screen |
| Computer assistance off | Tap any button exactly 10 times on the matchup screen |
| Powerup goaltending | Tap any button exactly 8 times on the matchup screen |
| Quick hands | Tap any button 5 times or more on the matchup screen, then hold turbo, pass, shoot and down at tip-off |
| Big head | Hold up, turbo and pass at tip-off |

**NBA Hangtime and NBA Maximum Hangtime** (the same codes): on the versus screen, the three
icons count turbo, shoot and pass presses (stick up counts down). With a human on each team,
both teams must enter a number code; tournament mode needs two humans.

| Number | Code |
| --- | --- |
| 111 | Tournament mode |
| 025 | Baby-sized players |
| 048 | No music during play |
| 937 | Goaltending allowed |
| 616 | Shot blocking |
| 709 | Maximum steals |
| 802 | Maximum power |
| 284 | Maximum speed |
| 552 | Hyper speed |
| 273 | Stealth turbo |
| 461 | Infinite turbo |
| 390 | No pushing |
| 120 | Fast turbo passes |

| Stick and buttons | Code |
| --- | --- |
| Press down on the versus screen: the player's three icons stop changing | Lock the number code |
| Hold up, turbo and pass together | Big head |
| Up, up, pass, turbo (within 1 s) | Huge head |
| Turn the stick a full circle clockwise from up (within 1 s); only the first player to do it | Show shot percentage |
| Left, left, pass, turbo (within 1 s) | No tag arrows |
| Down, down, shoot, turbo (within 1 s) | No drift on block jumps |
| Hold right and press pass twice (within 1 s) | Computer assistance off |
| Hold right and press shoot, turbo, pass (within 1 s) | ABA ball |
| Hold left and press turbo twice (within 1 s) | Outdoor court |
| Shoot, shoot, then hold shoot and turn the stick a full circle clockwise from up (within 1.3 s) | Special combo (ABA ball, outdoor court, shot percentage, big head) |

Maximum Hangtime 1.03 is older: ABA ball also gives the outdoor court, and the outdoor-court
and special combos do nothing there.
<!-- codes:end -->

## 5. Online

Up to four players, each on their own computer.

1. Highlight the game and choose **Online game** in the menu (or press X on a controller,
   or `N`).
2. One player chooses **Host a game** and waits in the lobby, which lists who has joined,
   then chooses **Start the game**. They are player 1.
3. The host's lobby shows a **code** such as `6BR0E-7C2YD`. **Copy my code** puts it on the
   clipboard to send in a chat.
4. The others choose **Join a game** and type the code (or, on the same network, the host's
   address), on the on-screen keyboard with a controller or on the keyboard (Ctrl+V
   pastes). They become players 2, 3 and 4 in the order they arrive. A host joined before
   is listed under **Join again**.
5. If a player gets no answer after a few seconds, the screen shows that player's own code.
   They send it to the host, who chooses **Let a player in** and types it.

Good to know:

* Everybody plays with their own player 1 keys or first controller.
* Everybody needs the same ROMs and the same version of the program; you are told if they differ.
* Over the internet no router settings are needed in most homes. When hosting, the program
  asks the router to open UDP port 7845 (UPnP). If the router does not allow that, the
  codes let the two computers reach each other directly ("hole punching").
  * Codes are found by asking a public STUN server (`stun.l.google.com` by default, the
    `stun_server=` setting), which sees your IP address. Both can be switched off on the
    Online screen.
  * It cannot work when both routers are of the strictest kind (common on mobile networks
    and at some workplaces). Then the host forwards UDP port 7845 by hand, or everybody
    joins a virtual network (Tailscale, ZeroTier, Radmin VPN).
* Input delay: 3 to 4 on a home network, 5 to 7 over the internet. If the game keeps
  stopping with "Waiting for the network...", raise it (on the Online screen or in the lobby).
* The pause menu opens online too, but the game goes on for everybody while it is open.
* If "DESYNC" appears, leave (pause menu, "Leave the online game") and host the game again.
* Lua mods: the host's mods run on every computer, with the host's options, whatever each
  player has switched on (their own settings come back afterwards). The lobby lists them.
  Every player needs the same mod files; if one differs or is missing, that player is told
  which.

## 6. Lua mods

Scripts in the `mods` folder appear in the main menu under **Lua mods**. X (Delete) turns
the highlighted script on or off; A (Enter) opens its page, where you choose the games it
applies to and set its options. Your choices are saved in `mods.ini`. Mods cannot touch your files or your system.

Included:

| Script | What it does |
|---|---|
| `quarter_length.lua` | Quarter and overtime length from 0:30 to 12:00 (the original is 3:00). The scoreboard has one minutes digit, so above 9:00 it shows 9 until nine minutes are left. |
| `no_select_timer.lua` | Removes the countdown on the team select screen, and optionally on initials entry. |
| `codes_te.lua` | T.E. and T.E. Special Edition: every cabinet code as a switch, without entering it: tournament mode, big head, huge head, maximum power, quick hands, goaltending, shot percentage, computer assistance off, baby-size players, expanded roster, switch teams at halftime, and a secret player for each player. Per-player codes apply to each player you tick. While tournament mode is on, the codes it blocks in the game are greyed. |
| `codes_mht.lua` | NBA Hangtime (L1.3, L1.1) and Maximum Hangtime (1.3, 1.03): every number code and stick code as a switch, without entering it (per-player codes for each player you tick). Not included: the number code lock and the special combo, whose parts each have their own switch. While tournament mode is on, the codes it clears are greyed. |
| `codes_jam.lua` | NBA JAM (1993, rev 3.01): its five cabinet codes as switches, without entering them: big head, quick hands, goaltending, shot percentage, computer assistance off (per-player codes for each player you tick). |

The cabinet code mods show "CODES ACTIVE:" and the codes that are switched on at the top of the picture for four seconds from tip-off, so everyone playing can see what is forced.

The first two were checked on T.E. rev 4.0, T.E. Special Edition rev 5.02, Hangtime and
Maximum Hangtime; `codes_te.lua` on T.E. rev 4.0 and Special Edition rev 5.02. If a mod cannot find what it needs in a game, it says so on screen and changes
nothing. To write your own, see [`mods/README.txt`](dist/NBAJamRecomp/mods/README.txt).

## 7. Troubleshooting

| Problem | What to do |
|---|---|
| A game is grey | Select it: the program says which files are missing. Check the table in [Where the ROMs go](#2-where-the-roms-go). |
| "CMOS invalid, factory settings restored" the first time | Normal on a game's first start. Press any button. |
| No picture, or a black window | Your graphics driver may lack OpenGL 2.1. Set `opengl=0` in `nbajam_recomp.ini` for the basic mode (no shaders). |
| The picture is too wide | Press F7 until it says "Original 4:3". "Stretch" distorts the picture. |
| Borders on every side | That is Integer scaling, which only grows in whole steps. Press F5 until it says "Fit". |
| A controller does nothing | Open **Controls**: the top line shows the pad the program sees. Select an action and press the button you want. |
| I cannot find my saves or settings | Open **About**: the "Data" line shows the folder. |
| Online: "your ROMs (or Lua mods) are not the same as the host's" | Use the same ROM files. |
| Online: "your Lua mod ... is not the same as the host's" | Copy the host's file (and the `mods/lib/` files it uses) into your `mods` folder. |
| Online: the others cannot join | A player on the host's own home network finds the host by the code by itself; if that fails, they type the host's local address (shown under "Same network"). From elsewhere, join with the host's code, not a `10.`, `192.168.` or `172.16`-`31` address: those only work on the same network. If there is still no answer, the player sends their own code and the host chooses **Let a player in**. If that fails too, forward UDP port 7845 on the host's router or use a virtual network. |
| The menu never mentions new versions | News needs an internet connection and "News and updates: on". With no connection it stays quiet. |

Something else? Open an issue on this repository and say which game and which version
(About shows it).

## Licence and credits

The code of this project is released under the BSD 3-Clause licence (see `LICENSE`).
Third-party code: MAME CPU cores and ymfm (BSD 3-Clause), miniz (MIT / Unlicense),
font8x8 (public domain), SDL2 (zlib), Lua 5.4 (MIT).

* Midway Games: the original games, and the source code released by
  [historicalsource](https://github.com/historicalsource)
* [Asure](https://github.com/Asure): NBA JAM T.E. Special Edition and its sources
* The MAME team, Aaron Giles (ymfm), dmsc (emu2)
* The original project: [PocketLucho/NBA-JAM---Recompiled](https://github.com/PocketLucho/NBA-JAM---Recompiled)

NBA JAM, NBA Hangtime and Midway are trademarks of their owners. This is a non-profit fan
project, not affiliated with them.
