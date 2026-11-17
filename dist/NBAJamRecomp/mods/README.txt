NBA JAM Recomp - Lua mods
=========================

Every .lua file in this folder appears in the main menu under "Lua mods".
X (Delete) turns the highlighted mod on or off. A (Enter) opens its page:
  * Apply to: All games / Selected games (then tick the games you want)
  * the mod's own options
Settings are saved in mods.ini next to the program.

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
                               live memory (RAM, I/O) as the main CPU sees it

Shared code
-----------
  nba.include("lib/name.lua")  runs mods/lib/name.lua (sub-folders allowed) and returns
                               what it returns, like require. A file runs once per script,
                               however often it is included. Only paths starting with lib/
                               and ending in .lua are accepted: nothing outside mods/lib/.
Files in lib/ are not listed as mods. A library that writes RAM counts as a RAM write of
every script that includes it (see Online play).

Symbols
-------
  nba.sym("name")              the bit address of a name from the game's source, for the
                               revision that is running, or nil.
They come from mods/sym/<game>_<crc>.lua, where <crc> is the CRC-32 of the program ROM, so
a symbol file is only used on the exact revision it was made from. For a game built from
its source the file is made by tools/map2sym.py from the assembler's .map; it holds names
and addresses only. An entry can also be { addr = a, pattern = { words }, at = b }: it is
used only when the ROM holds those words at b (the code that proves the address).
Shipped now: NBA JAM T.E. Special Edition 5.02 (4176 names). Example:
  local scores = nba.sym("scores")      -- 0x010F2F30 on SE 5.02
  if scores then nba.log(nba.read16(scores)) end

Each instruction of the original source assembles to fixed words (e.g. "movi X,a0" is
09E0 followed by X, low word first), so a mod can find the code it wants by pattern
and works on every revision that shares that code. See quarter_length.lua and
no_select_timer.lua.

Online games: the program ROM is part of the netplay fingerprint, so all players need
the same mods turned on with the same options.

Online play
-----------
For now, an online game does not start while a mod that has an on_frame hook or that
writes RAM (nba.write8 / write16 / write32) is on for that game: the players could not be
kept in sync. Mods that only patch the ROM in on_load work online; every player must
have the same ones on with the same options. ("Quarter length" has an on_frame hook for
quarters longer than 9 minutes, so it has to be off online.)
