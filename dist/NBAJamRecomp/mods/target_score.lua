-- target_score.lua - first to N points wins, and a mercy rule (roadmap Phase 6).
--
-- When a team reaches the target score, or leads by the mercy margin, during play, the script
-- ends the game the game's own way: it makes the quarter the fourth (gmqrtr 3) and sets the
-- clock (game_time: tenths, seconds, tens of seconds, minutes, a byte each) to one tenth of
-- a second, so the game's own clock runs it out and the game goes to its end-of-game screens.
-- (Not to 0: Hangtime ends a quarter only when its clock ticks down to 0 itself.) It keeps both there during
-- play until the game is over: written once, a quarter that ends on its own at that moment
-- starts the fourth quarter with a new clock (seen on SE). The score at that moment
-- has a leader, so there is no (further) overtime. In overtime (gmqrtr 4 or more) it ends
-- that overtime period the same way. The quarter table (box score by quarter) shows the
-- points in the quarter that was being played.
mod = {
  name = "Target score",
  description = "First to a target score wins; an optional mercy rule ends a one-sided game.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = {
    { id = "target", label = "First to", values = { 0, 11, 15, 21, 30, 50, 75, 100 },
      labels = { "Off", "11", "15", "21", "30", "50", "75", "100" }, default = 0 },
    { id = "mercy", label = "Mercy rule", values = { 0, 10, 15, 20, 30 },
      labels = { "Off", "10 points ahead", "15 points ahead", "20 points ahead", "30 points ahead" }, default = 0 },
    nba.include("lib/records.lua").OPTION,
  },
}

local sc, gs, inm, q, gt, target, mercy, ended, endq, guard

function on_load()
  sc, gs, inm, q, gt = nba.sym("scores"), nba.sym("GAMSTATE"), nba.sym("inmatchup"), nba.sym("gmqrtr"), nba.sym("game_time")
  target, mercy = math.tointeger(nba.option("target")), math.tointeger(nba.option("mercy"))
  guard = nba.include("lib/records.lua").guard()
  if target > 0 then nba.log("first to " .. target) end
  if mercy > 0 then nba.log("mercy rule: " .. mercy .. " points ahead") end
end

function on_frame()
  guard.update()
  if (target == 0 and mercy == 0) or not (sc and gs and q and gt) then return end
  local g = nba.read16(gs)
  if g ~= 3 or nba.read16(inm) ~= 0 then
    if g == 0 or g == 1 then ended = false end
    return
  end
  guard.mark()
  if ended then
    -- the clock is held only in the period the rule ended (the fourth quarter, or the
    -- overtime it triggered in); the quarter is never put back: the game moves it on when it
    -- ends the period (Hangtime does so while still in play, GAMSTATE 3)
    if nba.read16(q) < endq then nba.write16(q, endq) end
    if nba.read16(q) == endq and nba.read32(gt) > 1 then nba.write32(gt, 1) end
    return
  end
  local a, b = nba.read16(sc), nba.read16(sc + 16)
  local why
  if target > 0 and (a >= target or b >= target) and a ~= b then why = "target"
  elseif mercy > 0 and math.abs(a - b) >= mercy then why = "mercy" end
  if why then
    ended = true
    endq = math.max(nba.read16(q), 3)      -- 3: the fourth quarter; 4 and on: overtime
    if nba.read16(q) < endq then nba.write16(q, endq) end
    nba.write32(gt, 1)
    nba.log(string.format("game ended by the %s rule at %d-%d", why, a, b))
  end
end
