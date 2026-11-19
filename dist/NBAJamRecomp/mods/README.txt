NBA JAM Recomp - Lua mods
=========================

Every .lua file in this folder appears in the main menu under "Lua mods".
X (Delete) turns the highlighted mod on or off. A (Enter) opens its page:
  * Apply to: All games / Selected games (then tick the games you want)
  * the mod's own options
Settings are saved in mods.ini next to the program.
During an offline game the same page opens from the pause menu (Esc, Guide, or Start + Back
held). There, options marked live change the game at once; the others, and turning a mod on
or off, apply next game, and the page says so. Online the page is not offered.

Writing a mod
-------------
Lua 5.4 with the base, string, table, math and utf8 libraries (no io/os/require/load).
Declare the mod in a global table, then use the hooks:

  mod = {
    name = "My mod",
    description = "Shown in the mod menu.",
    games = { "nbajamte", "nbajamse" },   -- optional; omit = any game
    options = {
      { id = "speed", label = "Speed", values = { 1, 2, 3 },
        labels = { "Slow", "Normal", "Fast" }, default = 2 },  -- default is a value
    },
  }

  function on_load()      -- game loaded, before reset: patch the program ROM here
  end
  function on_frame(n)    -- after every emulated frame (n = frame number)
  end

Option types
------------
Each option may have a type (the default is a list of values, as above):
  { id = "ot", type = "toggle", label = "Overtime", default = true }
      on or off; nba.option gives true or false
  { id = "speed", type = "range", label = "Speed", min = 0.5, max = 2, step = 0.1, default = 1 }
      a number from min to max by step (at most 1001 values)
  { id = "fire", type = "players", label = "Always on fire", default = 5 }
      a tick per player 1 to 4; nba.option gives a bit mask: player p is ticked
      when (mask >> (p - 1)) & 1 == 1. default = 5 ticks players 1 and 3.
Every type can also have:
  group = "Rules"           a heading above the option; options of one group go together
  conflicts = { "fire" }    this option is greyed out while one of those options is set away
                            from its default, and nba.option then gives this option's default
                            (the choice is kept for when the other is set back)

Live options
------------
  { id = "fast", type = "toggle", label = "Fast mode", live = true }
  function on_option(id, value)   -- called when a live option is changed during a game
    if id == "fast" then ... end
  end
value is what nba.option(id) now gives. If a change greys out a live option (conflicts), it is
called for that option too, with its default. Options without live = true are read by the
script at the next game's start, as before.

Game ids: nbajam, nbajamte, nbajamse (T-Unit), nbahangt, nbamht (Wolf Unit).

The nba table
-------------
Addresses are TMS34010 *bit* addresses (byte address * 8), as in the game source.
The program ROM is at nba.ROM_BASE (0xFF800000); a word is 16 bits = 0x10 apart.

  nba.game                     id of the running game
  nba.option(id)               current value of one of your options
  nba.log(...)                 message on screen and on stderr
  nba.frame()                  frames since the game started
  nba.rom_size()               program ROM size in 16-bit words
  nba.rom_read16(a) / nba.rom_write16(a, v)
  nba.rom_read32(a) / nba.rom_write32(a, v)     low word first, like the CPU
  nba.rom_find({w1, w2, -1, ...} [, start [, max]])
                               list of addresses where the word pattern occurs;
                               -1 matches any word
  nba.read8/16/32(a), nba.write8/16/32(a, v)
                               (a write to NVRAM, 0x01400000-0x014FFFFF, stops the script with an
                               error: saved records, audits and adjustments are not for scripts)
                               live memory (RAM, I/O) as the main CPU sees it

Buttons and drawing
-------------------
  nba.press(player, buttons [, frames])
                               holds buttons for player 1-4 for `frames` frames (default
                               1), from the next frame on, together with the real ones.
                               buttons: a name or a table of names: up, down, left,
                               right, shoot, pass, turbo, start, coin.
  nba.text(x, y, str [, color [, scale]])
  nba.rect(x, y, w, h [, color [, filled]])
  nba.line(x1, y1, x2, y2 [, color])
                               draw over the game picture, in the game's own pixels
                               (0,0 is the top left of the game picture, 400 x 254 on
                               T.E., SE and Maximum Hangtime). Colors are 0xRRGGBB (opaque, white
                               by default) or 0xAARRGGBB (AA = 80 is half transparent). The
                               drawing is on the program's overlay, never in the game's
                               memory, and screenshots (F12) leave it out. Draw again in every
                               on_frame: the drawings are cleared before each one.
Example:
  function on_frame(f)
    if f == 2000 then nba.press(1, "start", 4) end
    nba.rect(8, 8, 120, 14, 0x80000000, true)
    nba.text(12, 11, "FRAME " .. f, 0xffff00)
  end
Online, a press counts only for this computer's own player, and is sent to the others like
the pad.

Shared code
-----------
  nba.include("lib/name.lua")  runs mods/lib/name.lua (sub-folders allowed) and returns
                               what it returns, like require. A file runs once per script,
                               however often it is included. Only paths starting with lib/
                               and ending in .lua are accepted: nothing outside mods/lib/.
Files in lib/ are not listed as mods. Online, a library counts as part of every script that
includes it (see Online play).

Symbols
-------
  nba.sym("name")              the bit address of a name from the game's source, for the
                               revision that is running, or nil.
They come from mods/sym/<game>_<crc>.lua, where <crc> is the CRC-32 of the program ROM, so
a symbol file is only used on the exact revision it was made from. For a game built from
its source the file is made by tools/map2sym.py from the assembler's .map; it holds names
and addresses only. An entry can also be { addr = a, pattern = { words }, at = b }: it is
used only when the ROM holds those words at b (the code that proves the address).
Shipped now: T.E. 4.0 (4144 names), T.E. Special Edition 5.02 (4176) and Maximum
Hangtime 1.3 (8089), the three games built from source. Example:
  local scores = nba.sym("scores")      -- 0x010EAFD0 on T.E. 4.0, nil on a revision without a file
  if scores then nba.log(nba.read16(scores)) end

Game events
-----------
lib/jam.lua watches the game's own state and calls functions of your script:
  local jam = nba.include("lib/jam.lua")
  function on_frame() jam.update() end          -- needed: the events are checked here
  function on_matchup() end                     -- the matchup screen before a game
  function on_game_start() end                  -- the tip-off (the clock starts)
  function on_quarter(n) end                    -- the clock starts in quarter n (5+: overtime)
  function on_game_end() end                    -- the final buzzer, or the game is over
Each is called once per game, in that order; define only the ones you need. Demo games in
attract mode give none. jam.quarter() is the current quarter (0 before tip-off) and
jam.scores() the two teams' points. jam.available is false, and nothing is called, on a game
without a symbol file (see Symbols): today T.E. 4.0, T.E. Special Edition 5.02, Maximum
Hangtime 1.3 and 1.03, NBA Hangtime L1.3 and L1.1, and NBA JAM 1993 rev 3.01 have events.
The library only reads RAM.

Cabinet codes
-------------
lib/codes_<game>.lua lists each game's cabinet codes (what each sets, how it is entered, when
the game clears it); lib/codes.lua turns them into switches:
  local codes = nba.include("lib/codes.lua")
  local T = codes.table("nbajamte")                   -- lib/codes_nbajamte.lua
  mod = { name = "...", options = codes.options(T, { "tournament", "big_head" }) }
  local drive
  function on_load() drive = codes.driver(codes.table(nba.game)) end
  function on_frame() drive.update() end
codes.options makes a toggle of a code the game keeps as one flag (or for both teams) and a
players option (a tick per player) of one it keeps per player or counts in a player's taps.
"secret_players" in the list gives four list options, secret_p1 to secret_p4: none or a secret
player, by name. codes.options(T, ids, extra) also takes more conflicts, { id = { ids } }: a
switch is greyed while one of those is on. The driver writes each switched-on code's value on
every frame of its window, so the game finds it whenever it looks, and nothing outside it: from
the start of the matchup screen until a second after tip-off for most codes, team select for
the expanded roster, the halftime team select for the team switch. A switch that is off is
never written: with all of them off the game runs exactly as without the mod. For four
seconds from tip-off the driver shows "CODES ACTIVE:" and the codes that are on, at the top
of the picture (codes.SHOW frames; 0 hides it). codes_te.lua is the example.

Each instruction of the original source assembles to fixed words (e.g. "movi X,a0" is
09E0 followed by X, low word first), so a mod can find the code it wants by pattern
and works on every revision that shares that code. See quarter_length.lua and
no_select_timer.lua.

Online games: the program ROM is part of the netplay fingerprint, so all players need
the same mods turned on with the same options.

Online play
-----------
In an online game the host's Lua mods run on every computer, with the host's options: the
host sends the list with the game start, and each player runs exactly that set for the game
(their own settings come back afterwards; nothing is saved). The lobby shows the host's mods.
Every player needs the same files: each script, and every library it names, is compared by
a hash (line ends do not matter). When a file is missing or differs, that player does not
start, and the message names the file. Mods may write RAM and use on_frame online.
  nba.netplay                  true in an online game: a script that should not run online
                               can check it in on_load and do nothing.
The pause menu's mods page stays offline only.
