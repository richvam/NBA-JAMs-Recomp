-- op_probe.lua - loaded next to operator_settings.lua by operator_settings.expect: 60 and 660
-- frames after tip-off, the game clock (game_time, 16:16 seconds), the head sizes
-- (plyrbighead) and the tournament-related flags ("op <frames>: ...").
mod = { name = "Operator probe" }
local gs, inm, gt, bh, ao, t
function on_load() gs = nba.sym("GAMSTATE"); inm = nba.sym("inmatchup"); gt = nba.sym("game_time"); bh = nba.sym("plyrbighead"); ao = nba.sym("assistance_off") end
function on_frame(f)
  if nba.read16(gs) == 3 and nba.read16(inm) == 0 then t = (t or 0) + 1 else t = nil end
  if t == 60 or t == 660 then
    nba.log(string.format("op %d: clock %08X bighead %X assistance_off %d", t, nba.read32(gt), nba.read16(bh), nba.read16(ao)))
  end
end
