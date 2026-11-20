-- fullgame_probe.lua - loaded next to full_game.lua by full_game.expect. Joins as player 1
-- with two start presses (which buy one quarter), picks a team with shoot, keeps him active in
-- play (shoot / pass every 30 frames) and logs each new quarter and the end of the game
-- ("quarter <n> starts" when gmqrtr changes in play, "game over in quarter <n>").
mod = { name = "Full game probe" }
local gs, q, lq, lg, sel
function on_load() gs = nba.sym("GAMSTATE"); q = nba.sym("gmqrtr"); sel = nba.sym("in_team_select") end
function on_frame(f)
  if f == 1200 or f == 1400 then nba.press(1, "start", 4) end
  local g = nba.read16(gs)
  if sel and nba.read16(sel) ~= 0 and f % 60 == 0 and f > 1600 then nba.press(1, "shoot", 4) end
  if not sel and g == 2 and f > 1600 and f < 5000 and f % 60 == 0 then nba.press(1, "shoot", 4) end
  if g == 3 and f % 30 == 0 then nba.press(1, (f // 30) % 2 == 0 and "shoot" or "pass", 3) end
  local qq = nba.read16(q)
  if g == 3 and qq ~= lq and qq < 4 then nba.log("quarter " .. (qq + 1) .. " starts"); lq = qq end
  if g == 8 and lg ~= 8 then nba.log("game over in quarter " .. qq) end
  lg = g
end
