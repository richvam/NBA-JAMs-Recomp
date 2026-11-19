-- operator_settings.lua - the service menu's game adjustments as menu options, without the
-- test menu and without changing NVRAM (roadmap Phase 6).
--
-- The game reads every adjustment through GET_ADJ (AUDIT.ASM), which reads it from NVRAM with
-- CALLA RC_LONG. When at least one option here is set, the script points that CALLA at a
-- small routine of its own in unused program ROM space (patched in memory at load, never in a
-- file): the routine looks the adjustment up in a table next to it and returns the chosen
-- value, or reads NVRAM as before for an adjustment left at "Game setting". The saved
-- adjustments are untouched, and the test menu still shows and edits them.
-- T.E. 4.0 and SE 5.02. Values and their names are the service menu's (ADJUST.ASM).
mod = {
  name = "Operator settings",
  description = "The service menu's game settings as options, without changing the saved settings.",
  games = { "nbajamte", "nbajamse" },
}

-- id, label, adjustment number, then the values with their service menu names
local ADJ = {
  { "difficulty", "Difficulty", 13, { 1, "1 (extra easy)", 2, "2 (easy)", 3, "3 (normal)", 4, "4 (hard)", 5, "5 (extra hard)" } },
  { "timer_speed", "Game timer speed", 24, { 1, "1 (extra slow)", 2, "2 (slow)", 3, "3 (normal)", 4, "4 (fast)", 5, "5 (extra fast)" } },
  { "head_size", "Player head size", 25, { 1, "1 (normal)", 2, "2 (large)" } },
  { "assistance", "Computer assistance", 27, { 0, "Enabled", 1, "Disabled" } },
  { "tournament", "Tournament mode", 28, { 0, "Enabled", 1, "Disabled" } },
  { "full_game", "Credits for full game", 20, { 4, "4", 5, "5", 6, "6", 7, "7", 8, "8", 9, "9", 10, "10", 11, "11", 12, "12", 13, "13", 14, "14", 15, "15", 16, "16" } },
  { "win_bonus", "4 player win bonus", 26, { 0, "No bonus", 1, "Free game" } },
  { "attract_sound", "Attract mode sound", 16, { 0, "On", 1, "Off" } },
  { "free_play", "Free play", 18, { 0, "No", 1, "Yes" } },
  -- SE 5.02 only (its ROM, not its released source): 0 opens every team's extra players on
  -- team select (see unlock_players.lua expanded_rosters)
  { "se_rosters", "Expanded rosters (SE)", 29, { 0, "Open" }, se = true },
}

local options = {}
for _, a in ipairs(ADJ) do
  local values, labels = { -1 }, { "Game setting" }
  for k = 1, #a[4], 2 do values[#values + 1] = a[4][k]; labels[#labels + 1] = a[4][k + 1] end
  options[#options + 1] = { id = a[1], label = a[2], values = values, labels = labels, default = -1 }
end
mod.options = options

local STUB = 0xFFFE0000          -- in the FFFF fill after the program, on T.E. and SE
local TABLE = STUB + 0x200       -- 64 words, one per adjustment: the value + 1, or 0 = not set
local WORDS = 0x80               -- what the routine and the table take, in words

function on_load()
  local set = {}
  for _, a in ipairs(ADJ) do
    local v = nba.option(a[1])
    if v ~= -1 and not (a.se and nba.game ~= "nbajamse") then set[#set + 1] = { a = a, v = v } end
  end
  if #set == 0 then return end
  local ga, rc = nba.sym("GET_ADJ"), nba.sym("RC_LONG")
  -- GET_ADJ: MMTM; MMTM; CALLR ADJPOINT; CALLA RC_LONG (+0x40, target at +0x50)
  if not (ga and rc) or nba.rom_read16(ga + 0x40) & 0xFFE0 ~= 0x0D40 or nba.rom_read32(ga + 0x50) ~= rc then
    nba.log("GET_ADJ is not as expected: settings not applied"); return
  end
  for k = 0, WORDS - 1 do
    if nba.rom_read16(STUB + 16 * k) ~= 0xFFFF then nba.log("no free ROM space: settings not applied"); return end
  end
  -- Only A0 (the result) and A1 (saved and restored by GET_ADJ) are touched: callers keep
  -- their own values in the other registers across GET_ADJ (SE's team select keeps the pair
  -- number in A14). A7 still holds the NVRAM address for RC_LONG.
  local code = {
    0x4C01,                                  -- MOVE A0,A1        adjustment number
    0x2481,                                  -- SLL  4,A1         x 16 bits
    0x0B21, TABLE & 0xFFFF, TABLE >> 16,     -- ADDI TABLE,A1
    0x8421,                                  -- MOVE *A1,A1,0     value + 1, or 0
    0xCA03,                                  -- JREQ +3 words     not set: read NVRAM
    0x1421,                                  -- DEC  A1
    0x4C20,                                  -- MOVE A1,A0        the chosen value (and its Z flag)
    0x0960,                                  -- RETS
    0xC080, rc & 0xFFFF, rc >> 16,           -- JA   RC_LONG
  }
  for k, w in ipairs(code) do nba.rom_write16(STUB + 16 * (k - 1), w) end
  for n = 0, 63 do nba.rom_write16(TABLE + 16 * n, 0) end
  for _, s in ipairs(set) do
    nba.rom_write16(TABLE + 16 * s.a[3], s.v + 1)
    nba.log(string.format("%s: %d (adjustment %d)", s.a[1], s.v, s.a[3]))
  end
  nba.rom_write32(ga + 0x50, STUB)
end
