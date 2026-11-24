# Changelog

What changed in each version of NBA JAM Recomp, written for players.

**Rule:** every pull request adds at least one line under "Unreleased", in the section it
belongs to. CI checks this (`tools/ci/check_changelog.py`). A pull request that changes
nothing a player or modder would notice (CI, refactoring, the roadmap) is marked with the
`no-changelog` label instead. At release time "Unreleased" becomes the release page.

Sections, in this order: **Added**, **Changed**, **Fixed**, **Online**, **Known issues**.

## Unreleased

### Added
- Linux: an AppImage (`...-linux-x86_64.AppImage`), one file that runs on current distributions and the Steam Deck; data in `~/.local/share/NBAJamRecomp`. Tested in CI on Ubuntu 24.04, Debian 12, Fedora 41 and Arch.
- README: how to check a download against the release's `SHA256SUMS.txt`, while the program is not signed.
- ROMs can be dragged onto the window (files, zips or a folder): they are copied into the ROM folder and the games are looked for again. "Open my ROM folder" in the main menu opens it.
- Windows: a crash writes a report (`crashes/*.txt` and a minidump `*.dmp`) in the data folder, to attach to an issue.
- A Windows installer (`...-windows-x64-setup.exe`, Inno Setup): per user, no administrator prompt, Start menu and optional desktop shortcut, data in `%APPDATA%\NBAJamRecomp`; uninstalling keeps your ROMs and saves unless you say yes. Built and tested (silent install and uninstall) in CI.
- Menus fill a wide window: on a 16:9 or 21:9 screen the menus are laid out that wide (up to 21:9; wider windows get black sides), instead of a 4:3 box.
- Controls: "Plays as" puts a controller on any cabinet player (1 to 4), kept for that controller, so two controllers can play on the same team.
- If the graphics card cannot draw the chosen look (F4) fast enough, the program switches to the next
  cheaper one on its own and says so. `frame_time_log=1` in `nbajam_recomp.ini` writes how long
  the picture takes to `frame_times.log`.
- Settings, Video: "Widest view" limits Widescreen to 16:9 or 21:9 if the full width of a very wide
  screen feels too zoomed out; the rest of the window gets sides. Shape there now also offers
  Widescreen.
- True widescreen on NBA Hangtime, Maximum Hangtime and NBA JAM (1993) too (F7, "Widescreen").
- True widescreen on NBA JAM T.E. 4.0 and T.E. Special Edition: press F7 until it says
  "Widescreen" and, during play, the court, stands and crowd fill a wide window instead of
  sides being added. Nothing is stretched and the game itself is not changed. Menus, team
  select, halftime and other fixed screens stay 4:3.
- Widescreen: the players' names, turbo bars and the shot clock spread out to the sides of
  the wide picture, each player's at their own side; centred signs stay in the middle.
- Save states (off by default): turn on Settings, Program, "Save states" and an offline game's pause menu gets Save and Load with three slots per game. They are never available online.
- Lua mods: a "Lua mods" menu, per-game settings in `mods.ini`, and two scripts: quarter
  length and no team-select timer.
- Controller remapping: profiles per player and per game, button shortcuts, import and export.
- The program shows its version, build and data folder in About, and its version on the
  main menu. `NBAJamRecomp --version` prints it.
- Your files can live in your user folder (`%APPDATA%\NBAJamRecomp` on Windows,
  `~/.local/share/NBAJamRecomp` on Linux), so the program can be installed where it cannot
  write. The zip and tar packages stay portable: they ship with `portable.txt` and keep
  everything in their own folder, as before.
- A copy without `portable.txt` that finds an older setup next to the program offers to
  move it.
- This changelog.
- Release pages are written from this changelog: what changed, which download is for which
  system, first-run steps, and whether the version plays online with the previous one.
- News: the main menu tells you when a newer version has been published and the News screen
  shows its release notes. It only informs you; nothing is downloaded or installed. Turn it
  off with Left/Right on "News and updates" or `news=0` in `nbajam_recomp.ini`.
- About shows the latest release and where to get it.
- Side fill (F8): around a 4:3 picture on a wider screen you can have black, the picture
  itself blurred and dimmed, or an overlay.
- Overlays and borders, as in RetroArch: PNGs in the `overlays` folder are shown around
  the game, drawn over it where they have a transparent opening. RetroArch border overlays
  (`.cfg` with its `.png`) can be copied in as they are. Two original borders are included.
- A Settings screen (from the main menu): picture shape, scaling, look, scanline strength, what fills the sides of a 4:3 picture, fullscreen, volume, the cabinet switches (free play, four players, country, video clips) and the check for new versions. The keys F4 to F8, F11, - and + still work.
- An on-screen keyboard, so a controller is enough to type the host's address, the UDP port and a new profile's name. A physical keyboard still types, and Ctrl+V pastes.
- The Online screen remembers the last five hosts you joined, under "Join again".
- A pause menu in the game: Esc, the Guide button, or holding Start and Back on a controller. Resume, volume, screenshot, settings, controls, Lua mods, help, coins, service credit, TEST switch, reset, and quit to the game list. Online the game goes on while it is open, and what would change the game is greyed. The held buttons can be changed to L3 and R3 in Settings.
- Menu music and sounds: the menus play the highlighted game's own tune and its cursor, confirm and back sounds, from its ROMs. Switches and a volume for them in Settings, Sound tab; "Music on" shows in the status line. Without the game's ROMs the menus are silent.
- Settings, Video: "Menus inside a TV's safe area" keeps the menus away from the edges a TV cuts off.
- Lua mods can share code: `nba.include("lib/name.lua")` runs a file from `mods/lib/`. And `nba.sym("name")` gives the address of a name from the game's source, on T.E. 4.0, T.E. Special Edition 5.02 and Maximum Hangtime 1.3.
- Lua mod options can be switches, number ranges or a tick per player, can be put under headings, and can grey each other out.
- Online games over the internet usually need no router settings any more: the host's router is asked to open the port, and players join with a short code; if that is not enough, the host lets a player in by the player's own code.
- Lua mods can be set from the pause menu during an offline game. Options a mod marks as live change the game at once; the others apply next game.
- A player on the host's own home network can join with the host's code too: the game finds the host on that network by itself.
- Lua mods can react to the game: `lib/jam.lua` calls `on_matchup`, `on_game_start`, `on_quarter(n)` and `on_game_end` on NBA Jam T.E. 4.0, T.E. Special Edition 5.02 and Maximum Hangtime 1.3.
- Lua mods can press buttons (`nba.press`) and draw text, boxes and lines over the game picture (`nba.text`, `nba.rect`, `nba.line`).
- Online games run the host's Lua mods on every computer, with the host's options; any mod can be used online now. A player whose mod files differ is told which one.
- Lua mods can use game addresses and game events on NBA Hangtime (L1.3 and L1.1), Maximum Hangtime 1.03 and NBA JAM 1993 too.
- The README lists the cabinet codes of every game, and docs/CODES.md the secret players of T.E. and SE.
- New Lua mod codes_te.lua: tournament mode, big head and maximum power for T.E. and T.E. Special Edition as switches, without entering the cabinet codes.
- codes_te.lua now has every T.E. and Special Edition cabinet code: huge head, quick hands,
  goaltending, shot percentage, computer assistance off, baby-size players, expanded roster,
  switch teams at halftime, and the secret players (chosen by name for each player).
- New Lua mod codes_mht.lua: the number and stick codes of NBA Hangtime (L1.3, L1.1) and
  Maximum Hangtime (1.3, 1.03) as switches.
- New Lua mod codes_jam.lua: the NBA JAM (1993) cabinet codes as switches.
- The cabinet code mods show "CODES ACTIVE:" and the forced codes at tip-off.
- New Lua mod unlock_players.lua: pick any hidden player for each player slot, on every game,
  without entering initials or a PIN.
- unlock_players.lua: expanded rosters on T.E. (turbo on team select steps through all of a
  team's players).
- Lua mods can no longer write NVRAM (saved player records, audits, operator settings):
  nba.write8/16/32 stop the script with an error there.
- New Lua mod handicap.lua (every game): a head start of up to 50 points for either team, with
  "Only against the CPU" on by default.
- "Protect records" (on by default) in the scripts that change a game: such a game is not
  counted in the players' saved records.
- New Lua mod operator_settings.lua (T.E., SE): the service menu's game settings as options,
  without changing the saved settings; on SE also expanded rosters.
- New Lua mod cpu_difficulty.lua (T.E., SE): the computer players' skill, and the catch-up help
  off.
- New Lua mod fire_rules.lua (T.E., SE): baskets needed to catch fire, always or never on fire
  for each team.
- New Lua mod turbo_rules.lua (T.E., SE): infinite turbo per player, turbo drain and refill.
- New Lua mod rules.lua (T.E., SE): shot clock off or another length, injuries off, goaltending
  not called.
- New Lua mod full_game.lua (every game): a started game is paid through all four quarters.
- New Lua mod target_score.lua (every game): first to N points wins, and a mercy rule.
- New Lua mod fast_boot.lua (every game): no wait on the power-on screen, and optionally no
  "any button to continue" prompt after a blank NVRAM.
- modtest: NVPREP=<frames> boots the game once first, so a test starts from a valid NVRAM.
- New Lua mod practice.lua (every game): the game clock stands still and there is no shot clock.
- New Lua mod show_info.lua (T.E., SE): shooting, turbo and on-fire counters for each player on the overlay during play.
- New Lua mod stats_export.lua (T.E., SE): saves the box score of each finished game to stats/ as CSV and JSON Lines.
- fantasy_draft.lua ladder slots: four ladders, each with its own pair and teams beaten, chosen in the mods menu and resumed where they were left, also after closing the program; "Start over" drafts a new pair into a slot.
- fantasy_draft.lua ladder mode: a drafted pair takes on the computer's teams one game after another, in the game's own order, with its teams beaten kept in stats/ (not in the players' records). Option: the computer drafts too, from the players left.
- fantasy_draft.lua options: draft order (fixed, random, loser of the last game first), the pool (team rosters, all players, everyone with the hidden players), the same player on both sides, and replaying the last saved draft. Every draft is saved to stats/.
- Lua scripts can read back the files they saved in stats/ (`nba.load`); not in an online game.
- New Lua mod fantasy_draft.lua (T.E., SE, offline): the players who joined draft players from every team on team select, in snake order with a pick timer, and play with them on the team they then choose.
- Lua scripts can read a player's buttons (`nba.input`) and keep them from the game for a while (`nba.hold`).
- New Lua mod sound_options.lua (T.E., SE): turns the music during play, or the announcer, off. The referee's whistle and the team select, halftime and end-of-game tunes stay.
- New Lua mod random_matchup.lua (T.E., SE): random teams and player pairs on team select, for the players, the computer or both, with any teams left out.
- New Lua mod auto_initials.lua (T.E., SE): enters each chosen player's initials and birthday on the record screen with that player's own controls, so the saved record loads without typing.
- New Lua mod hitboxes.lua (T.E., SE): body boxes, ball reach, push reach, the ball and the rims on the overlay during play.
- Lua mods can write files into stats/ with nba.save (plain names, size and rate limits, no reading back) and read the local date with nba.date.
- New Lua mod cpu_vs_cpu.lua (every game): whole games between two computer teams, one after another, for watching.

### Changed
- The pause menu opens by holding Start for one second on a controller (a quick press still joins
  the game). Start + Back opened a Windows menu; it and L3 + R3 can still be chosen in Settings,
  Program.
- The pause menu no longer has Lua mods: changing them in the middle of a game could leave it
  in a state the scripts do not expect. They are set in the main menu.
- "Quarter length" says in its description that over 9:00 the game clock does not show the time
  correctly (the quarter still lasts the time chosen).
- The emulator can now save and load the whole state of a running game (every CPU, sound board, video and timer), checked on all five games, together with the state of the Lua scripts that are running and the changes they made to the program ROM. There is no save menu yet; this is groundwork for later online features. A state is about 1.6 MB (110 to 140 KB compressed) and takes about 0.1 ms to save and 0.13 ms to load.
- The read-me has a Lua mods section, and mods/README.txt lists every script that comes with the program, the games it runs on and which ones stay off online.
- The CRT mask is as coarse at 4K as at 1080p (it was twice as fine at 4K), and it lines up
  with the picture wherever the picture sits on the screen.
- Windows: the program draws at the monitor's real resolution when display scaling is set to
  150% or 200%, instead of being enlarged by the system. A new window is sized from the
  screen (about three quarters of its height) instead of always 1200x900; `scale=` in the
  settings file still sets a fixed size.
- Scaling (F5) is now Fit, Smooth or Integer, and **Fit is the default**: the picture fills
  the height of the screen with crisp pixels, so there is no border above and below at
  1080p or 4K. Integer is still there and says so when it leaves a large part of the screen
  unused. An `integer_scale=` line in an older settings file is not carried over.
- Shape (F7) is now Original 4:3 or Stretch. Stretch is the old 16:9, 16:10 and fill modes
  under an honest name, and works on any monitor shape, ultrawide included. There is no true
  widescreen view yet (see Known issues).
- The interface and the documentation are English only for now. The language option is gone
  from the main menu.
- Downloads are on the Releases page; builds are no longer kept in the repository.
- The README is rewritten for players: download, where the ROMs go, first run, controls,
  online, mods and troubleshooting. Build instructions moved to `docs/BUILDING.md`.
- The version is taken from the release tag instead of the build date.
- Menus, messages and the in-game notices are drawn in the project's own chunky lettering instead of the plain 8x8 font. The screens are laid out as before; file names and web addresses keep the plain font so their lower case stays readable. Rebuilt screens follow.
- The main menu is rebuilt: the games in a list with the highlighted game's name in its own lettering beside it, and the reason when a game cannot be played. Colours, background and lettering follow the highlighted game when its ROMs are there. Everything on it works with a controller, keyboard or mouse. Quitting asks first. "Look for ROMs again" rescans the roms folder without a restart. The news on/off switch moved to Settings.
- The Lua mods and Controls screens are rebuilt in the new style. Lua mods shows the list and the highlighted script's page side by side, with the whole description. Controls groups who (game, player, profile), the buttons and the profile; a button is still bound by pressing it.
- The Online, lobby, About and News screens are rebuilt like the rest of the menus and work with a controller, keyboard or mouse. The lobby lists the players with their ping, and the host can change the input delay there. About is now "About and help": it lists the controls of each player as they are set now, and the program's keys. The UDP port is typed instead of stepped.
- In a game, Esc opens the pause menu instead of leaving the game at once; "Quit to the game list" is in the menu.
- The README files describe the menus and the pause menu first; the F-keys are shortcuts.

### Fixed
- The build after the pause and online picker changes: a comment had swallowed the rest of a line in the game list.
- With a controller, "Online game" in the main menu could only be used for the last game in the
  list. Left and right on that row now choose the game.
- Widescreen: a strip of the stands at the right edge (16:9) could stay empty, where the original
  screen ended.
- Widescreen: popup boxes (the controls help at the tip-off) moved to the side while their text
  stayed in the middle. Only names and turbo bars along the top spread out now.
- Widescreen: players, the ball, hoops, poles and padding near the sides are drawn whole instead
  of missing or cut off where the original 4:3 screen ended.
- Saving the Settings screen no longer switched the shape from Widescreen back to Original 4:3.
- Widescreen: the end-of-quarter banner and other messages low on the screen stay whole in the
  middle; only the players' names and turbo bars spread to the sides.
- `--pad-script` files saved with Windows line endings work (every `expect` line failed before).
- A Lua script that read a ROM address out of range, or wrote to NVRAM, got a confusing error ("invalid option '%0'") instead of the message with the address.
- With a controller in use, moving or resizing the window under a still mouse pointer no longer takes the focus away from the controller.

### Online
- Widescreen and 4:3 players can play each other online: each player's choice only changes
  their own picture, never the game.
- Hosting opens the router's port by PCP or NAT-PMP too, when the router has no UPnP.
- IPv6: a computer with a public IPv6 address adds it to its code (the code gets longer), and a
  player who has IPv6 too connects over it, with no router in the way. An IPv6 address can be
  typed as the host's address too.
- A player who cannot reach the host at all (both behind strict routers) can join through a
  player already in: the host's lobby shows a longer code for that, and the host's player
  list says who comes through whom.
- The lobby shows your connection type (open, moderate or strict) and, for every other player,
  the ping, the jitter and whether the path is direct, relayed or through the host.
- Games with three or four players react faster when the players are far from the host but
  near each other: every player sends their inputs straight to the others too, and the host
  is only the way round when two players cannot reach each other.
- For developers: how the games draw, and the plan for true widescreen, are written down in
  `docs/WIDESCREEN.md`; the program does not show more of the court yet.
- For developers: the emulator can draw the game's blitter commands into a wider picture and
  watch the game's display routine, without changing the game (`widetest` checks it). Nothing
  uses it yet.
- For developers: a wide picture of Tournament Edition 4.0 gameplay can be drawn off-screen at
  any width from 400 to 1000 columns: the court floor, the stands and the courtside crowd
  continue to the sides, and players, the ball and the hoops keep their place. The game is not
  changed. The program does not show it yet; `widetest` checks it and can save pictures.
- An online game does not start while a Lua mod that writes memory or runs every frame is
  switched on, because the players could not be kept in sync. Mods that only patch the game
  at load still work if every player has the same ones on. "Quarter length" is affected and
  has to be off online.

### Known issues
- "No team select timer" removes the countdown on one of the two screens it looks for on
  T.E. and T.E. Special Edition.
- Long mod descriptions are cut off in the Lua mods list.
