-- handicap.lua - a head start for either team, 0 to 50 points, given once at tip-off
-- (roadmap 5.1, 5.2), on every game.
-- The points are written into the game's own score (scores, team 1 then team 2), so they are
-- real points: the scoreboard shows them, they count in the first quarter's box score (the
-- game takes quarter 1 as the score at the end of the quarter, PLYR.ASM, kp_qscrs) and the
-- quarters still add up to the final score; the late-game speed-up for the losing team
-- (PLYR.ASM, under 50 seconds) sees them too.
-- The scoreboard digits are objects whose image the game sets only when someone scores
-- (SCORE.ASM prt_top_scores: score_imgs[team] OSAG = LED_0[score]); the script does the same
-- once. The addresses come from prt_top_scores (in the symbol file, or found by its first
-- instructions on NBA JAM 1993): the score_imgs load comes just before the `scores` load,
-- and LED_0 is the address of the first ADDI after it. (Hangtime first caps the score at 151,
-- so the offsets differ.)
-- "Only against the CPU": a team gets its head start only when nobody plays on the other
-- team (PSTATUS bits 0-1 are team 1's players, 2-3 team 2's), so the option can stay on
-- and leave games between people alone.
-- When: 60 frames after tip-off. Not before: when the tip-off sequence starts, the game
-- checks that both scores are 0 and stops (LOCKUP, trap 29) if not.
mod = {
  name = "Handicap",
  description = "Gives either team a head start of up to 50 points at tip-off.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = {
    { id = "team1", label = "Team 1 head start", type = "range", min = 0, max = 50, step = 1, default = 0 },
    { id = "team2", label = "Team 2 head start", type = "range", min = 0, max = 50, step = 1, default = 0 },
    { id = "cpu_only", label = "Only against the CPU", type = "toggle", default = true },
    nba.include("lib/records.lua").OPTION,
  },
}

local AFTER = 60
local guard
local scores, gs, inm, ps, h1, h2, cpu_only, imgs, led

-- the scoreboard digit objects and the digit image table, from prt_top_scores
local function find_scoreboard()
  local p = nba.sym("prt_top_scores")
  if not p then
    -- T-unit: MMTM SP,A2-A8; MOVE @score_imgs,A8,L; MOVE @scores,A0
    local hits = nba.rom_find({ 0x098F, 0x3F80, 0x07A8, -1, -1, 0x05A0, scores & 0xFFFF, scores >> 16 })
    p = hits and hits[1]
  end
  if not p then return end
  for k = 0, 0x200, 16 do
    if nba.read32(p + k) == scores then
      local im = nba.read32(p + k - 0x30)
      for j = k + 0x20, k + 0x200, 16 do
        local v = nba.read32(p + j)
        if (nba.read16(p + j - 16) & 0xFFF0) == 0x0B20 and v >= 0xFF800000 then return im, v end
      end
    end
  end
end
local t, given = 0, false

function on_load()
  scores, gs, inm = nba.sym("scores"), nba.sym("GAMSTATE"), nba.sym("inmatchup")
  guard = nba.include("lib/records.lua").guard()
  ps = nba.sym("PSTATUS")
  h1, h2, cpu_only = nba.option("team1"), nba.option("team2"), nba.option("cpu_only")
  if scores then imgs, led = find_scoreboard() end
  if not imgs then nba.log("scoreboard digits not found: the head start shows at the first basket") end
  if h1 > 0 or h2 > 0 then nba.log(string.format("head start: team 1 %d, team 2 %d", h1, h2)) end
end

function on_frame()
  if guard then guard.update() end
  if (h1 == 0 and h2 == 0) or not scores then return end
  local ingame = nba.read16(gs) == 3 and nba.read16(inm) == 0
  if not ingame then t = 0; given = false; return end
  t = t + 1
  if t == AFTER and not given then
    given = true
    -- only at the start of a game: both scores still 0
    if nba.read16(scores) == 0 and nba.read16(scores + 16) == 0 then
      local a, b = h1, h2
      if cpu_only and ps then
        local st = nba.read16(ps)
        if st & 0xC ~= 0 then a = 0 end   -- people on team 2: no head start for team 1
        if st & 0x3 ~= 0 then b = 0 end   -- people on team 1: none for team 2
      end
      if a > 0 or b > 0 then guard.mark() end
      nba.write16(scores, a)
      nba.write16(scores + 16, b)
      if imgs then
        for k, v in ipairs({ a, b }) do
          local o = nba.read32(imgs + 32 * (k - 1))
          if o ~= 0 then nba.write32(o + 0x110, nba.read32(led + 32 * v)) end
        end
      end
      nba.log(string.format("head start given: %d - %d", a, b))
    end
  end
end
