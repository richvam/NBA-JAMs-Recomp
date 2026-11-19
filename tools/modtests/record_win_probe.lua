-- record_win_probe.lua - loaded next to handicap.lua by records_protect.expect (roadmap 5.3).
-- Plays game after game as player 1 with initials BBB 01/01 (as initials_probe.lua: start
-- twice, YES, right, five shoots, then shoot on team select; in play shoot and pass alternately
-- every 30 frames, or the game drops the idle player), the first at frame 1200, each next
-- one 600 frames after the game is back in attract mode (GAMSTATE 0 or 1). On each team select, 200 frames in, it logs the record the
-- game loaded for cabinet player 1 from NVRAM: "record game <n>: count <c> won <w> lost <l> streak <s>"
-- (player1_data PR_COUNT, PR_WON, PR_LOST, PR_WINSTREAK), and at each end of game the score.
mod = { name = "Record win probe" }
local gs, sel, p1, scores
local s0, games, selt, lastgs, att, inplay = nil, 0, nil, -1, nil, false
local WINSTREAK = 26 * 16      -- PR_WINSTREAK, word 26 of the in-RAM record
function on_load() gs = nba.sym("GAMSTATE"); sel = nba.sym("in_team_select"); p1 = nba.sym("player1_data"); scores = nba.sym("scores") end
function on_frame(f)
  local g = nba.read16(gs)
  local idle = g == 0 or g == 1     -- attract (0) or the attract/start screens after a game (1)
  if idle and not (lastgs == 0 or lastgs == 1) then att = f end
  if f == 1 then att = 1 end
  if g == 8 and lastgs ~= 8 then nba.log(string.format("end of game %d: %d-%d", games, nba.read16(scores), nba.read16(scores + 16))) end
  lastgs = g
  -- keep player 1 active in play (an idle player is dropped from the game)
  if g == 3 and f % 30 == 0 then nba.press(1, (f // 30) % 2 == 0 and "shoot" or "pass", 3) end
  -- (a game cut short after a quarter, for want of credit, never reaches the record save)
  if g == 3 then inplay = true elseif g == 8 or g == 0 or g == 1 then inplay = false end
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
        if f == selt + 200 then
          nba.log(string.format("record game %d: count %d won %d lost %d streak %d", games, nba.read16(p1), nba.read16(p1 + 16), nba.read16(p1 + 32), nba.read16(p1 + WINSTREAK)))
        end
        -- start a few more times first: in free play each start buys more of the game
        -- (two at the join buy one quarter); then shoot picks the team
        if f - selt < 400 then
          if (f - selt) % 60 == 30 then nba.press(1, "start", 4) end
        elseif f % 60 == 0 then nba.press(1, "shoot", 4) end
      elseif selt then
        s0, selt = nil, nil
      end
    end
  end
end
