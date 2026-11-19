-- roster_probe.lua - loaded next to unlock_players.lua by roster_te.expect (roadmap 4.3).
-- Goes to team select with start; then for six teams in turn presses turbo for player 1 25
-- times, 12 frames apart, and logs the highest pair number tm1set[team1] reached
-- ("roster: team T pairs up to N"), then moves the cursor down to the next team. The game
-- steps to the next pair on turbo and wraps after pair 5, or 19 when the roster power-up is
-- on that team and the team has more players.
mod = { name = "Roster probe" }
local sel, team1, tm1set, n, top, start, teams
function on_load() sel = nba.sym("in_team_select"); team1 = nba.sym("team1"); tm1set = nba.sym("tm1set"); n = 0; top = 0; teams = 0 end
function on_frame(f)
  if teams >= 6 then return end
  if nba.read16(sel) == 0 then
    if f >= 1000 and f % 200 == 0 then nba.press(1, "start", 4) end
    return
  end
  start = start or f + 120
  local t = nba.read16(team1)
  if f < start or t == 0xFFFF then return end
  local v = nba.read16(tm1set + 16 * t)
  if v > top then top = v end
  if (f - start) % 12 == 0 then
    if n == 25 then
      nba.log(string.format("roster: team %d pairs up to %d", t, top))
      teams = teams + 1; n = 0; top = 0
      nba.press(1, "down", 3); start = f + 30
      return
    end
    nba.press(1, "turbo", 3); n = n + 1
  end
end
