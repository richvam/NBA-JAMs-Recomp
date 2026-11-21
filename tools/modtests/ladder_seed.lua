-- ladder_seed.lua - loaded with fantasy_draft.lua by fantasy_slots.expect (roadmap 6.F6).
-- At frame 1 saves two ladders as fantasy_draft.lua writes them, as if the program had been
-- closed in the middle of each:
--   slot 1: Karl Malone and John Stockton (places 85, 86), nothing beaten yet
--   slot 2: Stacey Augmon and Danny Manning (places 1, 0), Dallas (team 5) beaten, 1 game played
mod = { name = "Ladder seed" }
function on_frame(f)
  if f == 1 then
    nba.save(nba.game .. "_ladder_1.txt", "picks 1:r85 2:r86 3:- 4:-\nside 1\nmask 0\ngames 0\n")
    nba.save(nba.game .. "_ladder_2.txt", "picks 1:r1 2:r0 3:- 4:-\nside 1\nmask 32\ngames 1\n")
  end
end
