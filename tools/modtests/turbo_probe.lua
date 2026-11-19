-- turbo_probe.lua - loaded next to turbo_rules.lua by turbo_rules.expect. Player 1 holds turbo
-- (with up, so he runs) from 120 frames after tip-off for 600 frames, then lets go; logs his
-- meter (ply_turbo, 0x2F full) at 120, 300, 720 and 900 frames ("turbo <t>: <value>").
mod = { name = "Turbo probe" }
local procs, gs, inm, t
function on_load() procs = nba.sym("plyrproc_t"); gs = nba.sym("GAMSTATE"); inm = nba.sym("inmatchup") end
function on_frame()
  if nba.read16(gs) == 3 and nba.read16(inm) == 0 then t = (t or 0) + 1 end
  if not t then return end
  if t >= 120 and t < 720 then nba.press(1, { "turbo", "up" }, 1) end
  if t == 120 or t == 300 or t == 720 or t == 900 then
    local d = nba.read32(nba.read32(procs) + 0x130)
    nba.log(string.format("turbo %d: %d", t, nba.read16(d + 0x40)))
  end
end
