-- ot_probe.lua - loaded next to target_score.lua by target_score_ot.expect. Forces overtime:
-- from five seconds before the end of the fourth quarter (gmqrtr 3) it holds the score at
-- 73-73 until the quarter is over, so regulation ends tied, and presses start on the screen
-- before overtime (state 2), which buys it as start buys a quarter in free play. Logs "tied 73-73 at <frame>", every change of quarter in
-- play ("period <gmqrtr>") and the end ("final <a>-<b> in period <gmqrtr> at <frame>").
mod = { name = "Overtime probe" }
local sc, gs, q, gt, tied, lq, done, lg, s2
function on_load() sc = nba.sym("scores"); gs = nba.sym("GAMSTATE"); q = nba.sym("gmqrtr"); gt = nba.sym("game_time") end
function on_frame(f)
  local g, qq = nba.read16(gs), nba.read16(q)
  -- hold the tie to the buzzer: a basket in the last seconds would decide regulation
  if qq == 3 and (tied or (g == 3 and nba.read32(gt) <= 0x0500 and nba.read32(gt) > 0)) then
    nba.write16(sc, 73); nba.write16(sc + 16, 73)
    if not tied then tied = true; nba.log("tied 73-73 at " .. f) end
  end
  if g == 3 and qq ~= lq then nba.log("period " .. qq); lq = qq end
  if tied and g ~= lg then nba.log(string.format("state %d period %d clock %08X at %d", g, qq, nba.read32(gt), f)) end
  lg = g
  -- in free play overtime is bought like a quarter: press start on the screen before it
  -- once per screen: repeated presses restart its countdown
  if tied and qq >= 4 and g == 2 then
    s2 = s2 or f
    if f == s2 + 60 then nba.press(1, "start", 4) end
  else s2 = nil end
  if g == 8 and not done then done = true; nba.log(string.format("final %d-%d in period %d at %d", nba.read16(sc), nba.read16(sc + 16), qq, f)) end
end
