-- handicap.lua - a head start for either team, 0 to 50 points, given once at tip-off
-- (roadmap 5.1).
-- The points are written into the game's own score (scores, team 1 then team 2), so they are
-- real points: the scoreboard shows them, they count in the first quarter's box score (the
-- game takes quarter 1 as the score at the end of the quarter, PLYR.ASM, kp_qscrs) and the
-- quarters still add up to the final score; the late-game speed-up for the losing team
-- (PLYR.ASM, under 50 seconds) sees them too.
-- The scoreboard digits are objects whose image the game sets only when someone scores
-- (SCORE.ASM prt_top_scores: score_imgs[team] OSAG = LED_0[score]); the script does the same
-- once, with the addresses read from the start of prt_top_scores (checked against `scores`).
-- When: 60 frames after tip-off. Not before: when the tip-off sequence starts, the game
-- checks that both scores are 0 and stops (LOCKUP, trap 29) if not.
mod = {
  name = "Handicap",
  description = "Gives either team a head start of up to 50 points at tip-off.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "team1", label = "Team 1 head start", type = "range", min = 0, max = 50, step = 1, default = 0 },
    { id = "team2", label = "Team 2 head start", type = "range", min = 0, max = 50, step = 1, default = 0 },
  },
}

local AFTER = 60
local scores, gs, inm, h1, h2, imgs, led
local t, given = 0, false

function on_load()
  scores, gs, inm = nba.sym("scores"), nba.sym("GAMSTATE"), nba.sym("inmatchup")
  h1, h2 = nba.option("team1"), nba.option("team2")
  -- prt_top_scores: MMTM; MOVE @score_imgs,A8,L (+0x20, address at +0x30); MOVE @scores,A0
  -- (+0x50, address at +0x60); SLL; ADDI LED_0,A0 (+0x90, address at +0xA0)
  local p = nba.sym("prt_top_scores")
  if p and scores and nba.read32(p + 0x60) == scores then imgs, led = nba.read32(p + 0x30), nba.read32(p + 0xA0) end
  if not imgs then nba.log("scoreboard digits not found: the head start shows at the first basket") end
  if h1 > 0 or h2 > 0 then nba.log(string.format("head start: team 1 %d, team 2 %d", h1, h2)) end
end

function on_frame()
  if (h1 == 0 and h2 == 0) or not scores then return end
  local ingame = nba.read16(gs) == 3 and nba.read16(inm) == 0
  if not ingame then t = 0; given = false; return end
  t = t + 1
  if t == AFTER and not given then
    given = true
    -- only at the start of a game: both scores still 0
    if nba.read16(scores) == 0 and nba.read16(scores + 16) == 0 then
      nba.write16(scores, h1)
      nba.write16(scores + 16, h2)
      if imgs then
        for k, v in ipairs({ h1, h2 }) do
          local o = nba.read32(imgs + 32 * (k - 1))
          if o ~= 0 then nba.write32(o + 0x110, nba.read32(led + 32 * v)) end
        end
      end
      nba.log(string.format("head start given: %d - %d", h1, h2))
    end
  end
end
