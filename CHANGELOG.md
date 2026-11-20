# Changelog

What changed in each version of NBA JAM Recomp, written for players.

**Rule:** every pull request adds at least one line under "Unreleased", in the section it
belongs to. CI checks this (`tools/ci/check_changelog.py`). A pull request that changes
nothing a player or modder would notice (CI, refactoring, the roadmap) is marked with the
`no-changelog` label instead. At release time "Unreleased" becomes the release page.

Sections, in this order: **Added**, **Changed**, **Fixed**, **Online**, **Known issues**.

## Unreleased

### Added
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

### Changed
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
- With a controller in use, moving or resizing the window under a still mouse pointer no longer takes the focus away from the controller.

### Online
- An online game does not start while a Lua mod that writes memory or runs every frame is
  switched on, because the players could not be kept in sync. Mods that only patch the game
  at load still work if every player has the same ones on. "Quarter length" is affected and
  has to be off online.

### Known issues
- No true widescreen. The games draw a 4:3 picture; on a wide screen you get that picture
  with sides (black, blurred or your own image, F8) or stretched (F7). Showing more of the
  court needs changes to each game and is planned, starting with Tournament Edition.
- "No team select timer" removes the countdown on one of the two screens it looks for on
  T.E. and T.E. Special Edition.
- Long mod descriptions are cut off in the Lua mods list.
