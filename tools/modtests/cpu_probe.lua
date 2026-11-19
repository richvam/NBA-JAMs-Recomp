-- cpu_probe.lua - loaded next to cpu_difficulty.lua by cpu_difficulty.expect: each player's
-- plyr_d_skill and plyr_ptsdown 20 s and 40 s into the game, and the score at the end
-- ("cpu <frames>: p1 skill/ptsdown ...", "final a-b").
mod = { name = "CPU probe" }
local procs, gs, inm, sc, t, lg
function on_load() procs = nba.sym("plyrproc_t"); gs = nba.sym("GAMSTATE"); inm = nba.sym("inmatchup"); sc = nba.sym("scores") end
local function s16(v) return v >= 0x8000 and v - 0x10000 or v end
function on_frame(f)
  local g = nba.read16(gs)
  if g == 3 and nba.read16(inm) == 0 then t = (t or 0) + 1 end
  if t == 1200 or t == 2400 then
    local out = {}
    for k = 0, 3 do
      local p = nba.read32(procs + 32 * k)
      out[#out + 1] = string.format("p%d %d/%d", k + 1, s16(nba.read16(p + 0x460)), s16(nba.read16(p + 0x440)))
    end
    nba.log(string.format("cpu %d: %s", t, table.concat(out, " ")))
  end
  if g == 8 and lg ~= 8 then nba.log(string.format("final %d-%d", nba.read16(sc), nba.read16(sc + 16))) end
  lg = g
end
