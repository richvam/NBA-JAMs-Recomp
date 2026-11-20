-- practice_probe.lua - loaded next to practice.lua by practice.expect (roadmap Phase 6). Every
-- 2000 frames in play it logs "clock <game_time hex> quarter <gmqrtr> score a-b", and logs
-- "shot clock ran out" if shotimer is 0,0 while a shot clock process runs.
mod = { name = "Practice probe" }
local gt, gs, q, sc, st, sp, out
function on_load()
  gt, gs, q, sc = nba.sym("game_time"), nba.sym("GAMSTATE"), nba.sym("gmqrtr"), nba.sym("scores")
  st, sp = nba.sym("shotimer"), nba.sym("sc_proc")
end
function on_frame(f)
  if nba.read16(gs) ~= 3 then return end
  if f % 2000 == 0 then
    nba.log(string.format("clock %08X quarter %d score %d-%d", nba.read32(gt), nba.read16(q), nba.read16(sc), nba.read16(sc + 16)))
  end
  if not out and nba.read32(sp) ~= 0 and nba.read16(st) == 0 and nba.read16(st + 16) == 0 then
    out = true; nba.log("shot clock ran out at " .. f)
  end
end
