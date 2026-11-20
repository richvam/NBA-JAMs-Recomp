-- rules.lua - shot clock, goaltending and injuries (roadmap Phase 6). T.E. 4.0 and SE 5.02.
--
-- Shot clock: each possession the game starts a new clock24 process (sc_proc) and sets
-- shotimer to 1,0 (tens, ones: ten seconds); the process waits about 14 seconds, then shows
-- and counts those digits down, so 24 seconds = 14 hidden + 10 shown (SCORE.ASM shot_clock).
-- "Off" holds shotimer at 1,0 while a clock runs, so it never runs out; a length sets the
-- digits to length - 14 each time a new clock starts. The computer players read shotimer too.
-- Goaltending off: when the game sees a goaltend it lets a player with the goaltending code
-- (24 taps) get away with it 70% of the time (PLYR2.ASM: CMPI 24 / JRNE / MOVI 700 / CALLA
-- RNDPER); the script makes that apply to everyone at 100%: the JRNE becomes a NOP and the
-- 700 a 1000. In memory, never in a file; the place is found by the ADDI p1taps before it.
-- (Applied and checked instruction by instruction, but its effect is not seen in the headless
-- test games: the game reaches this check rarely, and none of them reached it.)
-- Injuries off: seq_injury (an animation call that adds to the player's injury stat, which
-- wears him down) returns at once.
-- Substitutions are not here: how T.E. decides to offer them is not traced yet.
mod = {
  name = "Game rules",
  description = "Shot clock off or another length, goaltending off, injuries off.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "shot_clock", label = "Shot clock", values = { 0, -1, 20, 30, 35 },
      labels = { "Game (24)", "Off", "20 seconds", "30 seconds", "35 seconds" }, default = 0 },
    { id = "goaltending", label = "Goaltending called", type = "toggle", default = true },
    { id = "injuries", label = "Injuries", type = "toggle", default = true },
    nba.include("lib/records.lua").OPTION,
  },
}

local st, sp, gs, clock, last, guard, changed

local function patch_goaltending()
  local t = nba.sym("p1taps")
  local hits = t and nba.rom_find({ 0x0B20, t & 0xFFFF, t >> 16, 0x8400, -1, -1, -1, 0x09C0, 0x02BC }) or {}
  if #hits ~= 1 or nba.rom_read16(hits[1] + 0x60) & 0xFF00 ~= 0xCB00 then return false end
  nba.rom_write16(hits[1] + 0x60, 0x0300)        -- JRNE -> NOP: everyone, not only the code
  nba.rom_write16(hits[1] + 0x80, 1000)          -- MOVI 700 -> 1000: always let go
  return true
end

local function patch_injuries()
  local a = nba.sym("seq_injury")
  if not a or nba.rom_read16(a) & 0xFC00 ~= 0x1800 then return false end   -- MOVK PS_INJURY,A0
  nba.rom_write16(a, 0x0960)                     -- RETS
  return true
end

function on_load()
  st, sp, gs = nba.sym("shotimer"), nba.sym("sc_proc"), nba.sym("GAMSTATE")
  guard = nba.include("lib/records.lua").guard()
  clock = math.tointeger(nba.option("shot_clock"))
  if clock ~= 0 then changed = true; nba.log(clock < 0 and "shot clock off" or ("shot clock " .. clock .. " seconds")) end
  if not nba.option("goaltending") then
    changed = true
    nba.log(patch_goaltending() and "goaltending off" or "the goaltending check was not found: not changed")
  end
  if not nba.option("injuries") then
    changed = true
    nba.log(patch_injuries() and "injuries off" or "seq_injury is not as expected: not changed")
  end
end

function on_frame()
  guard.update()
  if not changed or nba.read16(gs) ~= 3 then return end
  guard.mark()
  if clock == 0 or not st then return end
  local p = nba.read32(sp)
  if p == 0 then last = 0; return end
  if clock < 0 then
    nba.write16(st, 0); nba.write16(st + 16, 1)
  elseif p ~= last then
    local s = clock - 14
    nba.write16(st, s % 10); nba.write16(st + 16, s // 10)
  end
  last = p
end
