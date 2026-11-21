-- ladder_probe.lua - loaded next to fantasy_draft.lua by fantasy_ladder.expect (roadmap 6.F5).
-- Plays game after game as player 1 with initials BBB, as record_win_probe.lua does (start
-- twice, YES, right, five shoots; on team select one Shoot for the first draft pick, more
-- starts for credit after the draft, then Shoot every 60 frames; in play Shoot and Pass by turns). In the fourth
-- quarter it puts team 1 ten points ahead (scores), so the drafted side wins every game.
-- 30 frames after each tip-off logs "tipoff <n> places a b c d": the four players' places in the player
-- list (from their attribute tables).
mod = { name = "Ladder probe" }
local gs, sel, scores, q, procs, atl
local s0, games, selt, lastgs, att, tip, inplay = nil, 0, nil, -1, nil, false, 0
function on_load()
  gs, sel, scores, q = nba.sym("GAMSTATE"), nba.sym("in_team_select"), nba.sym("scores"), nba.sym("gmqrtr")
  procs, atl = nba.sym("plyrproc_t"), nba.sym("ATL_t")
end
function on_frame(f)
  local g = nba.read16(gs)
  local idle = g == 0 or g == 1
  if idle and not (lastgs == 0 or lastgs == 1) then att = f end
  if f == 1 then att = 1 end
  lastgs = g
  if g == 3 then
    if f % 30 == 0 then nba.press(1, (f // 30) % 2 == 0 and "shoot" or "pass", 3) end
    -- 30 frames in: at the first frame of play the players' processes may not be set up yet
    inplay = inplay + 1
    if not tip and inplay == 30 then
      tip = true
      local s = {}
      for k = 0, 3 do s[#s + 1] = tostring((nba.read32(nba.read32(procs + 32 * k) + 0x150) - atl) // 368) end
      nba.log(string.format("tipoff %d places %s", games, table.concat(s, " ")))
    end
    if nba.read16(q) == 3 and f % 60 == 0 then nba.write16(scores, nba.read16(scores + 16) + 10) end
  elseif idle then tip, inplay = false, 0 end
  if not s0 and idle and att and f - att >= (games == 0 and 1199 or 600) then s0 = f; games = games + 1 end
  if s0 then
    local k = f - s0
    if k == 0 or k == 200 then nba.press(1, "start", 4) end
    if k == 700 then nba.press(1, "shoot", 4) end
    if k == 940 then nba.press(1, "right", 3) end
    if k >= 950 and k <= 1070 and (k - 950) % 30 == 0 then nba.press(1, "shoot", 4) end
    if k > 1200 then
      if nba.read16(sel) ~= 0 then
        selt = selt or f
        -- one Shoot for the first pick (the draft starts 120 frames in; a 10-second timer
        -- takes the second), Starts for credit once the draft is over (the draft holds them),
        -- then Shoot for the team
        local t = f - selt
        if t == 180 then nba.press(1, "shoot", 4) end
        if t >= 900 and t < 1300 and t % 60 == 30 then nba.press(1, "start", 4) end
        if t >= 1300 and f % 60 == 0 then nba.press(1, "shoot", 4) end
      elseif selt then
        s0, selt = nil, nil
      end
    end
  end
end
