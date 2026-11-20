-- full_game.lua - a started game is paid through all four quarters (roadmap Phase 6).
--
-- The game keeps the quarters each player has bought in qtr_purchased (a word per player);
-- its own monitor_fullgame (SELECT.ASM in T.E.) treats a player with 4 as having bought the
-- full game: it sets the purchased bit, gives him a full timer and shows the "full game
-- purchased" plaque. The script writes 4 for every player who has joined (PSTATUS), from the
-- start of team select to the end of the game, so no buy-in screen comes between quarters.
-- Credits are not touched and no audit is changed by the script (the game counts the full
-- game in its "full games purchased" audit, as for a bought one).
mod = {
  name = "Full game",
  description = "A started game is paid through all four quarters: no buy-in screens.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
}

local qp, ps, gs

function on_load()
  qp, ps, gs = nba.sym("qtr_purchased"), nba.sym("PSTATUS"), nba.sym("GAMSTATE")
  if not (qp and ps and gs) then nba.log("full game: not available on this game") end
end

function on_frame()
  if not (qp and ps and gs) then return end
  local g = nba.read16(gs)
  if g == 0 or g == 1 then return end      -- attract
  local st = nba.read16(ps)
  for k = 0, 3 do
    if st & (1 << k) ~= 0 and nba.read16(qp + 16 * k) < 4 then nba.write16(qp + 16 * k, 4) end
  end
end
