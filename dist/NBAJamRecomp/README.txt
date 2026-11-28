NBA JAM Recomp - Midway's NBA JAM saga recompiled from its source code
=====================================================================
Created by Pocket_lucho with Claude (Anthropic).
Downloads and news: https://github.com/richvam/NBA-JAMs-Recomp/releases
Original project:   https://github.com/PocketLucho/NBA-JAM---Recompiled
If you want to support me, go to https://buymeacoffee.com/pocket_lucho

Run NBAJamRecomp.exe (64-bit Windows 10/11, no installation), NBAJamRecomp on
Linux, or NBA JAM Recomp in Applications on a Mac. Everything works
with a controller alone; the keyboard and the mouse work too. In the menus the
directions move, A (Enter) chooses, B (Esc) goes back, the shoulder buttons
(Q, E) change tab, and the bottom line says what each button does.

In a game, Esc, the Guide button, or Start held for one second (a quick press
still joins the game) open the pause menu: resume, volume, screenshot, settings,
controls, help, coins, service credit, TEST switch, reset, and "Quit to the
game list".

"News" in the menu tells you when a newer version is published (it only tells
you; nothing is downloaded). It can be switched off in Settings, Program.



IMPORTANT: YOU PROVIDE THE GAME DATA
------------------------------------
The program does NOT include any Midway graphics, sound, music or code: they
are copyrighted and cannot be distributed. Copy your own files into the roms\
folder. The program finds them by itself (by CRC32, whatever the file name,
inside .zip files too) and extracts what it needs when a game starts; there is
nothing to unpack.

  Game                           What to put in roms\
  -----------------------------  ---------------------------------------------
  NBA JAM (1993)                 MAME romset nbajam.zip
  NBA JAM Tournament Edition     MAME romset nbajamte.zip, or the ZIP of the
                                 historicalsource/nba-jam-tournament-edition repo
                                 (without nbajamte.zip it uses the SE sound ROMs)
  NBA JAM T.E. Special Edition   the ZIP of the Asure/nba-jam-tournament-edition-SE repo
  NBA Hangtime                   MAME romset nbahangt.zip (ver L1.3 or L1.1)
  NBA Maximum Hangtime           ver 1.3: ZIP of the historicalsource/nba-hangtime
                                 repo + nbahangt.zip (graphics and DCS sound);
                                 or ver 1.03: MAME romset nbamht.zip

Repository ZIPs are downloaded on GitHub with "Code -> Download ZIP":
  https://github.com/historicalsource/nba-jam-tournament-edition
  https://github.com/Asure/nba-jam-tournament-edition-SE
  https://github.com/historicalsource/nba-hangtime

A game with missing files is shown in grey; selecting it tells you what is
missing.

The versions tagged SRC (T.E. 4.0, Special Edition 5.02 and Maximum Hangtime
1.3) were compiled from the published source code and the result is
byte-identical to the original ROMs, so it does not matter where the data
comes from. To build them yourself use tools/build_roms.sh from the source
code: its output in data\ is used when the ROMs are not found.


CONTROLS
--------
                Move              Shoot  Pass  Turbo           Start  Coin
  Controller    stick or d-pad    A      B     X/shoulder/trig  Start  Back
  Keys, P1      arrows            Z      X     C                1      5
  Keys, P2      W  S  A  D        F      G     V                2      6
  Keys, P3      numpad 8/2/4/6    0      .     Enter            3      7
  Keys, P4      I  K  J  L        H      ;     U                4      8

  Each controller is one player. Main menu -> "Controls": pick a player, pick
  or create a profile, choose an action and press the button or stick
  direction for it (press a bound one again to remove it; X/Delete clears the
  action; waiting five seconds or B cancels). Set "Game" to one game to give
  it its own profile. Shortcuts: one input holds several arcade buttons at
  once - Shove (turbo+pass), Turbo shot (turbo+shoot) and two of your choice.
  "Export profile" writes it to controller_profiles\export\; to import, copy
  .ini profiles to controller_profiles\import\ and choose "Import", or drop
  the file onto the window. "About and help" -> Controls lists what every
  action is on right now. Keyboard keys are changed in nbajam_recomp.ini.

  Settings (main menu, or "Video and sound" in the pause menu): the picture,
  the TV safe area, the volume, the menus' own music and sounds (played from
  the highlighted game's ROMs), the cabinet switches, the pause menu buttons.

  Keyboard shortcuts for the same things:
  Esc  pause menu                    F1  help over the game
  F2   TEST switch (service menu)    F3  reset
  F4   picture: Sharp -> Smooth -> Scanlines -> CRT -> Curved CRT
  F5   scaling: Fit -> Smooth -> Integer
  F6   scanline strength
  F7   shape: Original 4:3 -> Widescreen -> Stretch
  F8   around the 4:3 picture on a wide screen: black -> blurred picture -> overlays
  F10  frame times: frame rate, frame time and a graph (Settings, Video too)
  F11  fullscreen (or Alt+Enter; on a Mac also Ctrl+Cmd+F or Cmd+Return)
  F12  screenshot (PNG in the screenshots\ folder; Print Screen works too)
  P    pause without the menu        - / +  volume
  9    service button (credit)

Settings are kept in nbajam_recomp.ini next to the .exe. Each game's high
scores and settings (NVRAM/CMOS) are kept in saves\.


VIDEO (shaders)
---------------
  The picture is drawn with OpenGL shaders; all of this is in Settings, Video.
  Scaling (F5):
    Fit          fills the height of the window; pixels stay crisp (default)
    Smooth       the same size, filtered
    Integer      every game line covers the same number of screen rows, so
                 scanlines are perfect, but borders remain on most screens;
                 the program tells you when they are large
  Shape (F7):
    Original 4:3 as the arcade monitor showed it
    Widescreen   all five games: during play you see more of the court, as
                 wide as your window, nothing stretched: 16:9, 21:9 and wider
                 up to 1000 game columns (2.5 times the 400 of the arcade; a
                 32:9 screen gets thin sides). Menus, team select, halftime
                 and other fixed screens stay 4:3. The players' names and
                 turbo bars spread out to the sides; popups and messages stay
                 in the middle. It needs the game's symbol file in mods\sym
                 (included); without it F7 skips Widescreen.
    Stretch      fills the whole window; the picture is distorted
  Widest view (Settings, Video): Window (default), 16:9 or 21:9. Limits how
  wide Widescreen gets, for a very wide screen that feels too zoomed out; the
  rest of the window gets sides. In the .ini: max_view_width=widest, 16:9 or
  21:9.
  Sides (F8): what is shown around a 4:3 picture on a wider screen: black, the
  picture itself blurred and dimmed, or an overlay (border) from the overlays\
  folder, as in RetroArch. Two are included; overlays\README.txt explains how to
  add your own PNGs or RetroArch border overlays (.cfg + .png).
  Look (F4):
    Sharp        clean pixels at any size
    Smooth       bilinear filtering
    Scanlines    dark gaps between lines; F6 changes the strength
    CRT          gaussian beam per line, RGB aperture mask and gamma
    Curved CRT   the same with tube curvature and vignette
  If the graphics card cannot draw the chosen look fast enough, the program
  switches to the next cheaper one by itself and says so (until the window
  size or the look is changed). frame_time_log=1 in the .ini writes how long
  each picture took to frame_times.log, for a report.
  Without OpenGL 2.1 a basic mode is used (opengl=0 in the .ini forces
  it).


ONLINE PLAY
-----------
  Highlight the game and choose "Online game" in the menu (or X on a pad, N).
  * The host chooses "Host a game": they are player 1 and wait in the lobby,
    which lists who has joined. "Start the game" starts it.
  * The host's lobby shows a code like 6BR0E-7C2YD ("Copy my code" copies
    it). The others choose "Join a game" and type the code (on the same
    network the host's address works too): on the on-screen keyboard with a
    pad, or on the keyboard (Ctrl+V pastes). They become players 2, 3 and 4
    in order of arrival. Hosts joined before are listed under "Join again".
  * No answer after a few seconds: the player sends the host their own code
    (shown on their screen); the host chooses "Let a player in" and types it.
  * The code also works from the host's own home network: the player finds
    the host there by itself.
  * Everybody plays with their player 1 controls (P1 keys or first pad).
  * Everybody needs the same ROMs; the app tells you if they differ.
  * Input delay: 3-4 on a LAN, 5-7 over the internet. If the game stalls now
    and then ("Waiting for the network..."), raise the delay.
  * Over the internet most homes need no router settings: the program asks
    the router to open UDP port 7845 (UPnP), and codes let two computers
    reach each other directly when it does not. Codes come from a public
    STUN server (stun.l.google.com unless stun_server= says otherwise),
    which sees your IP address; both can be switched off on the Online
    screen. When both routers are of the strictest kind (mobile networks,
    some workplaces), forward UDP port 7845 by hand or use a virtual LAN
    (Tailscale, ZeroTier, Radmin VPN).
  * Windows, Linux, macOS and Android play together; the lobby shows what
    each player plays on. With an IPv6 address the host also gets a longer
    code: send it if the short one does not connect (often on mobile data).
  * A phone moving from Wi-Fi to mobile data mid-game goes on at its new
    address when the other side's router lets it in. On Android, switching
    to another app leaves the online game.
  * The pause menu opens online too; the game goes on for everybody.
  * If "DESYNC" appears, leave (pause menu, "Leave the online game") and host
    the game again.


LUA MODS
--------
  Scripts in the mods folder change or add to the games: house rules, codes,
  hidden players, a fantasy draft, on-screen info, a box score saved to stats.
  Choose "Lua mods" in the main menu (not during a game) to turn them on and set
  their options. mods\README.txt lists the scripts that come with the program and
  explains how to write one.
  * Online, the host's mods run for everybody, with the host's options; every
    player needs the same script files. A few scripts (the fantasy draft, CPU vs
    CPU, random matchups, auto initials) do nothing in an online game.
  * Scripts that change a game keep it out of the players' saved records
    ("Protect records", on by default).


ANDROID
-------
  * Install the APK on the phone (allow installing from that app when asked).
  * ROMs: main menu, "Add ROMs" (files or zips) or "Add a ROM folder", in the
    phone's file picker. They are copied into the app's folder:
    Android/data/io.github.richvam.NBA_JAMs_Recomp/files (seen over USB too).
    Uninstalling deletes that folder: copy it first to keep ROMs and saves.
  * Touch: tap to choose, the back gesture or "< Back" goes back; in a game
    the back gesture opens the pause menu. Touch controls (stick, Shoot,
    Pass, Turbo, Start, Coin) show while no controller is connected;
    Settings -> Touch moves, resizes and fades them. A TV remote works the
    menus.
  * Switching to another app pauses the game; if Android closes the app
    meanwhile, the next start goes on from the same moment.


FOLDERS
-------
  roms\         your ROMs / ZIPs (see above)
  overlays\     borders shown around the picture (F8)
  saves\        each game's NVRAM
  screenshots\  screenshots (F12)
  logs\         nbajam_recomp.log (and .old.log, the session before): send
                it with a bug report
  mods\         Lua mods (see LUA MODS)
  stats\        files Lua mods save (box scores, drafts, ladders)
  data\         (optional) games you built with tools/build_roms.sh


HOW IT IS MADE
--------------
  * Emulation: MAME CPU cores (TMS34010, M6809, ADSP-2105), YM2151 through
    ymfm, and the Midway T-Unit and Wolf Unit boards on a minimal framework.
  * Build: a TI GSPA/GSPLNK compatible assembler/linker written for this
    project (tools/gspasm) and Midway's original LOAD2.EXE run under emu2.
    See docs/BUILDING.md in the source code.

NBA JAM, NBA Hangtime and Midway are trademarks of their owners. Non-profit
fan project, not affiliated with them.

WHERE YOUR FILES ARE KEPT
-------------------------
This package is portable: the file portable.txt next to the program makes it keep
settings, saves, screenshots, controller profiles, mods and roms in this folder.
Without portable.txt the program uses your user folder instead
(Windows: %APPDATA%\NBAJamRecomp, Linux: ~/.local/share/NBAJamRecomp,
Mac: ~/Library/Application Support/NBAJamRecomp; on Android always
Android/data/io.github.richvam.NBA_JAMs_Recomp/files), so it can be
installed somewhere it cannot write. If it then finds an older setup next to the
program it offers to move it. The About screen shows the folder in use.
