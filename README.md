# NBA JAM Recomp

Midway's NBA JAM arcade games, **rebuilt from their original source code**, in one program
for Windows, Linux, macOS and Android: a game picker, CRT shaders, remappable controllers,
touch controls, Lua mods and online play for up to four players on any mix of them.

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
| Windows 10/11, 64-bit, installer | `NBAJamRecomp-<version>-windows-x64-setup.exe` (from the next release on) |
| Windows 10/11, 64-bit, portable | `NBAJamRecomp-<version>-windows-x64.zip` |
| Linux and Steam Deck, 64-bit (x86-64), AppImage | `NBAJamRecomp-<version>-linux-x86_64.AppImage` (from the next release on) |
| Linux, 64-bit (x86-64), Flatpak | `NBAJamRecomp-<version>-linux-x86_64.flatpak` (from the next release on) |
| Linux, 64-bit (x86-64), portable | `NBAJamRecomp-<version>-linux-x64.tar.gz` |
| macOS 11 or later, Apple Silicon and Intel, disk image | `NBAJamRecomp-<version>-macos-universal.dmg` (from the next release on) |
| Linux on ARM 64-bit (Raspberry Pi 4/5 with a 64-bit system, ARM laptops), AppImage or portable | `NBAJamRecomp-<version>-linux-aarch64.AppImage`, `NBAJamRecomp-<version>-linux-arm64.tar.gz` (from the next release on) |
| Android 5 or later, 64-bit ARM (phones, tablets, handhelds such as the AYN Thor), APK | `NBAJamRecomp-<version>-android.apk` (from the next release on) |

The Windows installer needs no administrator rights: it installs for your user, adds a Start
menu entry (and a desktop one if you tick it), and keeps your ROMs, settings and saves in
`%APPDATA%\NBAJamRecomp`; the Start menu has a shortcut to its `roms` folder. Uninstalling
asks whether to delete that folder too, and keeps it unless you say yes. The archives need no
installing: unpack them anywhere. The AppImage is one file: make it executable
(`chmod +x`, or Properties, Allow executing) and run it; on a Steam Deck, in desktop mode. It
keeps your ROMs, settings and saves in `~/.local/share/NBAJamRecomp` (drag ROMs onto its
window, or use Open my ROM folder). The Flatpak installs with
`flatpak install --user NBAJamRecomp-<version>-linux-x86_64.flatpak` (or by opening it in
your software centre) and runs in a sandbox that sees only its own folder:
`~/.var/app/io.github.richvam.NBA_JAMs_Recomp/data/NBAJamRecomp`, with the ROMs in its `roms`.
Drag ROMs onto its window, or use Open my ROM folder. Windows may show a SmartScreen warning
because the program is not signed: choose "More info", then "Run anyway".

On a Mac, open the disk image and drag NBA JAM Recomp onto Applications. It keeps your ROMs,
settings and saves in `~/Library/Application Support/NBAJamRecomp` (drag ROMs onto its window,
or use Open my ROM folder). The app is not signed with an Apple developer certificate, so the
first time macOS says it cannot check it. On macOS 15 and later: open it once, click Done,
then in System Settings, Privacy & Security, click **Open Anyway** next to "NBA JAM Recomp was
blocked". On macOS 11 to 14: Control-click the app in Applications, choose Open, then Open.
After that it opens normally. The disk image has these steps in a read-me too.

On Android, open the APK on the phone and allow installing from the app you opened it with
when Android asks (it is not in the Play Store). The app turns to landscape. It keeps your
ROMs, settings and saves in `Android/data/io.github.richvam.NBA_JAMs_Recomp/files`, a folder a
computer also sees over USB. Uninstalling the app deletes that folder, ROMs and saves
included: copy it first if you want to keep them.

**Checking a download.** The program and installer are not signed yet, so Windows cannot
say who made them. Each release lists the SHA-256 checksum of every file in
`SHA256SUMS.txt`. In PowerShell, `Get-FileHash .\NBAJamRecomp-<version>-windows-x64-setup.exe`
prints the checksum of your copy; it must be the same as the one in that file. On Linux,
`sha256sum -c SHA256SUMS.txt --ignore-missing` checks every file you downloaded; on a Mac,
`shasum -a 256 -c SHA256SUMS.txt --ignore-missing`.

## 2. Where the ROMs go

Copy your own files into the `roms` folder: next to the program for the archives, or
`%APPDATA%\NBAJamRecomp\roms` for the Windows installer. The program recognises
every file by its contents (CRC32), whatever its name, inside zips and sub-folders too,
so there is nothing to unpack or rename.

The easiest way: **drag your ROM files, zips or a folder of them onto the program's window**
while the game list is shown. They are copied into the `roms` folder and the games are
looked for again at once. **Open my ROM folder** in the main menu opens the folder itself;
after copying files there by hand, choose **Look for ROMs again**.

On Android, choose **Add ROMs** in the main menu and pick one or more files (zips too) in
the phone's file picker, or **Add a ROM folder** to pick a whole folder. They are copied into
the app's `roms` folder (a file that is already there is not copied twice) and the games are
looked for again; the message says how many games are ready. No storage permission is asked
for. Files copied into `Android/data/io.github.richvam.NBA_JAMs_Recomp/files/roms` from a
computer over USB work too.

| Game | What to put in `roms` |
|---|---|
| NBA JAM (1993) | MAME romset `nbajam.zip` |
| NBA JAM Tournament Edition | MAME romset `nbajamte.zip`, or the ZIP of the [historicalsource/nba-jam-tournament-edition](https://github.com/historicalsource/nba-jam-tournament-edition) repository (without `nbajamte.zip` it uses the Special Edition sound ROMs) |
| NBA JAM T.E. Special Edition | the ZIP of the [Asure/nba-jam-tournament-edition-SE](https://github.com/Asure/nba-jam-tournament-edition-SE) repository, or a pre-built SE (see below) |
| NBA Hangtime | MAME romset `nbahangt.zip` (L1.3 or L1.1) |
| NBA Maximum Hangtime | version 1.3: the ZIP of the [historicalsource/nba-hangtime](https://github.com/historicalsource/nba-hangtime) repository plus `nbahangt.zip` (graphics and sound), or a pre-built 1.3 (see below); or version 1.03: MAME romset `nbamht.zip` |

Repository ZIPs are downloaded on GitHub with "Code", then "Download ZIP".

**Pre-built ROMs.** Special Edition and Maximum Hangtime 1.3 also load from ROM files that
were already built, without the source repositories:

* the program ROM chips themselves (SE: `uj12`/`ug12`; MHT 1.3: `U54`/`U63`), under any name,
  loose or in a zip, together with the graphics and sound ROMs as above;
* or what [`tools/build_roms.sh`](tools/build_roms.sh) writes for the game: a folder (or a zip
  of it) holding `program.bin` and `gfx.bin`, plus `sound/u3.bin`, `u12.bin` and `u13.bin` for
  SE. `program.bin` is recognised by its contents; `gfx.bin` and `sound/` are read from the
  same folder. Maximum Hangtime takes its music and speech from `nbahangt.zip` when it is there
  (they are not part of the source release), and is silent without it.

## 3. First run

1. Run `NBAJamRecomp` (`NBAJamRecomp.exe` on Windows).
2. Pick a game with a controller, the keyboard or the mouse. A game shown in grey is missing
   files; the panel beside the list says which.
3. Insert a coin and press Start (on the keyboard: `5`, then `1`; on a controller: Back,
   then Start).
4. In a game, open the **pause menu** with `Esc`, the Guide button, or by holding Start for one
   second (a quick press of Start still joins the game, as on the cabinet). It has everything the program can do during a game: settings,
   controls, coins, the TEST switch, reset, help, and "Quit to the game list".

Everything works with a controller alone: menus, settings, button binding, Lua mods, online
games (the host's address is typed on an on-screen keyboard), the pause menu.

On Android everything works by touch as well: tap to choose, tap a tab to switch, and the
back gesture (or the **< Back** button in the corner) goes back; in a game the back gesture
opens the pause menu. Switching to another app pauses the game; if Android closes the app
while it is away, the next start goes on with that game, paused, from the same moment.

The downloads are **portable**: settings, saves, screenshots, controller profiles, mods and
ROMs all stay in the unpacked folder, because it contains a file called `portable.txt`.
Without that file the program keeps them in your user folder instead
(`%APPDATA%\NBAJamRecomp` on Windows, `~/.local/share/NBAJamRecomp` on Linux,
`~/Library/Application Support/NBAJamRecomp` on a Mac). The About
screen shows which folder is in use.

Updating: unpack the new version over the old folder, or into a new one and copy `roms`,
`saves` and `nbajam_recomp.ini` across. The main menu tells you when a newer version has
been published (**News and updates**; it only tells you, nothing is downloaded, and it can
be switched off there).

## 4. Controls

In the menus: the directions move, A (Enter) chooses, B (Esc) goes back, the shoulder
buttons (Q and E) change tab, X (Delete) is the extra action a screen names at the bottom.
The mouse and touch work too. The bottom line always says which button does what, in the
names of the device you used last. A TV remote works the menus: its arrows move, the centre
key chooses and Back goes back.

**Touch controls** (Android, or any touch screen): a stick and Shoot, Pass, Turbo, Start and
Coin buttons are drawn over the game for player 1. A finger can slide from one button to the
next. On Android they show by default while no controller is connected and hide when one
is; on a computer they are off until switched on. **Settings**, tab **Touch**: Automatic, On or Off, their size and opacity, **Arrange**
(drag each one where you want it; kept for next time) and back to the default places.

In a game, by default:

| | Move | Shoot | Pass | Turbo | Start | Coin |
|---|---|---|---|---|---|---|
| Controller | stick or d-pad | A | B | X, a shoulder or a trigger | Start | Back |
| Keyboard, player 1 | arrow keys | Z | X | C | 1 | 5 |
| Keyboard, player 2 | W S A D | F | G | V | 2 | 6 |
| Keyboard, player 3 | numpad 8 2 4 6 | numpad 0 | numpad . | numpad Enter | 3 | 7 |
| Keyboard, player 4 | I K J L | H | ; | U | 4 | 8 |

Each controller is one player: the first connected is player 1, and so on. To play as
another player (for example players 1 and 3, on the same team), open **Controls**, choose
the controller and set **Plays as**; this is kept for that controller (offline games; online
the host gives each computer its seat). To change its buttons, open **Controls** in the main menu:
choose the controller, choose or create a profile, choose an action and press the button for
it. Any game can have its own profile. One button can hold several arcade buttons at once
(shove, turbo shot and two shortcuts of your choice). Profiles can be exported and imported
as `.ini` files. **About and help**, tab Controls, lists what every action is on right now.
Keyboard keys are changed in `nbajam_recomp.ini`.

**Settings** (main menu, or "Video and sound" in the pause menu) has the picture (shape,
scaling, look, scanlines, what fills the sides of a 4:3 picture, fullscreen, the TV safe
area, a log of frame times), the sound (volume, the menus' music and sounds), the touch
controls, the cabinet switches and the program's own options. The menus play the highlighted game's own music and sounds, from its
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
| F7 | shape: Original 4:3, Widescreen, or Stretch (fills the window, distorts the picture). Widescreen is true widescreen on all five games: during play you see more of the court, as wide as the window, nothing stretched; fixed screens stay 4:3, and the players' names and turbo bars spread out to the sides. It needs the game's symbol file in `mods/sym` (included); without it F7 skips Widescreen. |
| F8 | around the 4:3 picture on a wider screen: black, the picture blurred and dimmed, then each overlay in the `overlays` folder (see [Overlays](#overlays-and-borders)) |
| F10 | frame times (frame rate, frame time, graph), like MangoHUD |
| F11 or Alt+Enter (Mac: also Ctrl+Cmd+F or Cmd+Return) | fullscreen |
| F12 or Print Screen | screenshot, saved in `screenshots` (on a Mac laptop the F keys may need fn) |
| P | pause without the menu |
| - and + | volume |
| 9 | service credit |

### Overlays and borders

### Widescreen

Press F7 until it says "Widescreen" (or choose it in Settings, Video, Shape). During play every game shows more of the court, as wide as your window, with nothing stretched. The game itself is not changed, so a widescreen player and a 4:3 player can play each other online.

| Window shape | Example | What you get |
|---|---|---|
| 16:9 | 1920x1080, 2560x1440, 3840x2160 | 534 game columns instead of 400 |
| 21:9 | 2560x1080, 3440x1440, 3840x1600 | about 712 to 720 columns |
| 32:9 | 5120x1440 | 1000 columns (the widest), thin sides |

Menus, team select, halftime, the final stats and other fixed screens stay 4:3. The players' names and turbo bars move out towards the sides; popups and messages stay in the middle. At the ends of the court the wide view stops short of the game's own camera, because the arena art ends there. "Widest view" in Settings, Video (Window, 16:9 or 21:9; `max_view_width` in the ini) limits how wide it gets. If your graphics card cannot draw the chosen look (F4) fast enough, the program switches to a cheaper one by itself and says so; `frame_time_log=1` in the ini writes the timings to `frame_times.log`.

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

Up to four players, each on their own computer or phone: Windows, Linux, macOS and Android
play together. The lobby shows what each player plays on.

1. Highlight the game and choose **Online game** in the menu (or press X on a controller,
   or `N`).
2. One player chooses **Host a game** and waits in the lobby, which lists who has joined,
   then chooses **Start the game**. They are player 1.
3. The host's lobby shows a **code** such as `6BR0E-7C2YD`. **Copy my code** puts it on the
   clipboard to send in a chat. When the host also has an IPv6 address, a longer code is shown
   under it: send that one if the short code does not connect (often the case on mobile
   data).
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
* If a player's network changes during a game (a phone moving from Wi-Fi to mobile data), the
  game goes on at the new address, as long as the other side's router lets it in; otherwise
  the game ends after 10 seconds. On Android, switching to another app leaves the online game,
  so the others are not left waiting.
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
| `quarter_length.lua` | Quarter and overtime length from 0:30 to 12:00 (the original is 3:00). Over 9:00 the game clock does not show the time correctly (it reads 9 minutes until nine minutes are left), but the quarter lasts the time chosen. |
| `no_select_timer.lua` | Removes the countdown on the team select screen, and optionally on initials entry. |
| `codes_te.lua` | T.E. and T.E. Special Edition: every cabinet code as a switch, without entering it: tournament mode, big head, huge head, maximum power, quick hands, goaltending, shot percentage, computer assistance off, baby-size players, expanded roster, switch teams at halftime, and a secret player for each player. Per-player codes apply to each player you tick. While tournament mode is on, the codes it blocks in the game are greyed. |
| `codes_mht.lua` | NBA Hangtime (L1.3, L1.1) and Maximum Hangtime (1.3, 1.03): every number code and stick code as a switch, without entering it (per-player codes for each player you tick). Not included: the number code lock and the special combo, whose parts each have their own switch. While tournament mode is on, the codes it clears are greyed. |
| `codes_jam.lua` | NBA JAM (1993, rev 3.01): its five cabinet codes as switches, without entering them: big head, quick hands, goaltending, shot percentage, computer assistance off (per-player codes for each player you tick). |
| `unlock_players.lua` | Every game: play as any hidden player without entering initials or a PIN. One choice per player slot: Normal, or a hidden player by name (119 names across the games; a name the running game does not have is skipped). On T.E., also expanded rosters: turbo on team select steps through all of a team's players. (SE 5.02 opens its rosters only through the operator adjustment "expanded roster".) |
| `handicap.lua` | Every game: a head start of 0 to 50 points for either team, given at tip-off. "Only against the CPU" (on by default) gives a team its head start only when nobody plays on the other team, so games between people stay even. The points are real: the scoreboard shows them, they count in the first quarter, and no player is credited with them. |
| `operator_settings.lua` | T.E. and T.E. Special Edition: the service menu's game settings as options (difficulty, game timer speed, head size, computer assistance, tournament mode, credits for a full game, 4 player win bonus, attract mode sound, free play) and, on SE, expanded rosters. The saved settings are not changed: the test menu still shows and edits them. |
| `cpu_difficulty.lua` | T.E. and T.E. Special Edition: the computer players' skill held at a level from -15 to +15 (the game otherwise changes it in play with the score), and the catch-up help off for everyone, people and computer. |
| `fire_rules.lua` | T.E. and T.E. Special Edition: the baskets in a row needed to catch fire (1 to 8; the game uses 3), and for each team "always on fire" or "never on fire". |
| `turbo_rules.lua` | T.E. and T.E. Special Edition: infinite turbo for the players you tick, how fast turbo drains while held (from very fast to very slow; the game uses one unit every 8 frames), and a faster or instant refill. |
| `rules.lua` | T.E. and T.E. Special Edition: shot clock off or 20, 30 or 35 seconds, injuries off, and goaltending not called. |
| `full_game.lua` | Every game: a started game is paid through all four quarters, so no buy-in screen comes between quarters. Credits are not touched. |
| `target_score.lua` | Every game: first to 11, 15, 21, 30, 50, 75 or 100 points wins, and a mercy rule that ends the game when a team leads by 10 to 30 points. The game ends through its own end-of-game screens. |
| `fast_boot.lua` | Every game: no wait on the power-on screen, so attract mode starts after about a tenth of a second instead of 1 second (T.E., SE, NBA JAM 1993), or about 0.9 seconds instead of 3.6 (Maximum Hangtime, Hangtime). Optionally the game also goes on by itself after a blank NVRAM instead of waiting for a button. It acts at power-on: after turning it on, reset the game. |
| `practice.lua` | Every game: a practice game. The game clock stands still, so the quarter never ends, and there is no shot clock. Each can be switched back on. Turn the script off to end the game. |
| `show_info.lua` | T.E. and SE: during play, one line per player at the bottom (or top) of the screen with field goals and percentage, threes, a turbo bar, and the player's baskets in a row or ON FIRE. |
| `stats_export.lua` | T.E. and SE: when a game ends, adds its box score to `stats/<game>_box.csv` (one row per player: teams, score, points, shots, threes, dunks, free throws, rebounds, assists, steals, blocks, human or CPU) and `stats/<game>_games.jsonl` (one JSON line per game). |
| `cpu_vs_cpu.lua` | Every game: starts whole games between two computer teams, for watching. From attract mode it starts a game, picks a random team (not on NBA JAM 1993), and lets the computer play all four players through four quarters; then the next game starts, or attract mode comes back. "Label players: CPU" shows CPU VS CPU in play, and on Hangtime and Maximum Hangtime keeps the drones' tags from showing player 1. Turn it off to play yourself. |
| `hitboxes.lua` | T.E. and SE, during play: draws what the game tests on the overlay. Each player's body box (blue team 1, red team 2) with its depth on the floor, the ball reach used for steals and blocks (filled while a steal or block is on), the push reach while a push is on, the ball and the rims. Each layer can be turned off. |
| `auto_initials.lua` | T.E. and SE: when a player joins, enters the initials and birthday set in the options with that player's controls, so the saved record loads without typing. Choose the players it enters for. |
| `random_matchup.lua` | T.E. and SE: on team select, picks a random team and pair of players for the players, the computer or both. Teams can be left out one by one. |
| `sound_options.lua` | T.E. and SE: turns the music during play, or the announcer, off, from the next game. The referee's whistle and the tunes between quarters stay. |
| `fantasy_draft.lua` | T.E. and SE, offline: on team select the players who joined draft players from every team, in snake order with a pick timer, with up, down and Shoot. Then each side picks the team it plays as and takes the court with its drafted players. Options: order (fixed, random, loser first), pool (team rosters, all players, everyone with the hidden players), the same player on both sides, and replay the last draft. Ladder mode: the drafted pair takes on the computer's teams one after another, in the game's own order, saved between games, in four slots that can be resumed from the mods menu; the computer can draft too. |

The scripts that change a game (`handicap.lua`, `unlock_players.lua`, the cabinet code mods) have "Protect records", on by default: such a game is not counted in the players' saved records (games, wins, losses, win streaks) on T.E., SE, Hangtime and Maximum Hangtime.

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
| The program closes by itself (Windows) | It writes a report in the `crashes` folder of its data folder (About shows which folder): a `.txt` and a `.dmp`. Attach both to an issue on GitHub. |
| A controller does nothing | Open **Controls**: the top line shows the pad the program sees. Select an action and press the button you want. |
| Android: the new APK does not install over the old one | A test build may be signed with another key than the one you have. Copy `Android/data/io.github.richvam.NBA_JAMs_Recomp/files` to a computer, uninstall, install the new one, and copy the folder back. Releases are all signed with one key and install over each other. |
| Android: the touch controls are missing | They hide while a controller is connected. **Settings**, **Touch**: set them to On. |
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
