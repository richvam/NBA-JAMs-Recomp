-- matchup_probe.lua - loaded next to random_matchup.lua by random_matchup.expect.
-- Starts a game for player 1 as record_win_probe.lua does (start twice, YES, right, five
-- shoots for BBB), then on team select presses Shoot every 60 frames after 400 frames. At
-- tip-off (the first GAMSTATE 3) it logs "tipoff <team1> <team2> pairs <tm1set> <tm2set>".
mod = { name = "Matchup probe" }
local gs, sel, t1, t2, s1, s2, selt, done
function on_load()
  gs, sel = nba.sym("GAMSTATE"), nba.sym("in_team_select")
  t1, t2, s1, s2 = nba.sym("team1"), nba.sym("team2"), nba.sym("tm1set"), nba.sym("tm2set")
end
function on_frame(f)
  local k = f - 1200
  if k == 0 or k == 200 then nba.press(1, "start", 4) end
  if k == 700 then nba.press(1, "shoot", 4) end
  if k == 940 then nba.press(1, "right", 3) end
  if k >= 950 and k <= 1070 and (k - 950) % 30 == 0 then nba.press(1, "shoot", 4) end
  if k > 1200 and nba.read16(sel) ~= 0 then
    selt = selt or f
    if f - selt >= 400 and f % 60 == 0 then nba.press(1, "shoot", 4) end
  end
  if not done and nba.read16(gs) == 3 then
    done = true
    local a, b = nba.read16(t1), nba.read16(t2)
    nba.log(string.format("tipoff %d %d pairs %d %d", a, b, nba.read16(s1 + 16 * a), nba.read16(s2 + 16 * b)))
  end
end
