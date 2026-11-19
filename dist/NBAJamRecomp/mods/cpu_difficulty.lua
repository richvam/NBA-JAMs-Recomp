-- cpu_difficulty.lua - the computer players' skill, and the catch-up help, as options
-- (roadmap Phase 6). T.E. 4.0 and SE 5.02.
--
-- Each player process keeps (PLYR.EQU, from PDATA = 0x100):
--   plyr_ptsdown (+0x440)  points the player's team is losing by (negative when winning);
--                          the "computer assistance" catch-up reads it (shots, speed)
--   plyr_d_skill (+0x460)  a computer player's skill: 0 average, - poorer, + better. The game
--                          sets it again and again in play (DRONE.ASM) from the difficulty
--                          adjustment, the quarter and the score gap, so the computer gets
--                          better when it is behind.
-- With the default difficulty the game keeps the opponents at about -15 (T.E.) or -6 (SE) and
-- a person's computer teammate at 0 (seen in modtest). "CPU skill" holds plyr_d_skill of the computer players (PSTATUS shows no person in that
-- slot) at the chosen value during play. "Catch-up off" holds plyr_ptsdown at 0 for every
-- player, people and computer, so nobody gets help from the score gap; the game's own
-- computer assistance code (10 taps) and adjustment only switch it off for people.
mod = {
  name = "CPU difficulty",
  description = "Sets the computer players' skill and turns the catch-up help off.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "skill", label = "CPU skill", values = { 99, -15, -10, -6, -3, -1, 0, 1, 3, 6, 10, 15 },
      labels = { "Game", "-15 (poorest)", "-10", "-6", "-3", "-1", "0 (average)", "+1", "+3", "+6", "+10", "+15 (best)" }, default = 99 },
    { id = "catch_up", label = "Catch-up help", type = "toggle", default = true },
    nba.include("lib/records.lua").OPTION,
  },
}

local PTSDOWN, SKILL = 0x440, 0x460
local procs, ps, gs, skill, catch_up, guard

function on_load()
  procs, ps, gs = nba.sym("plyrproc_t"), nba.sym("PSTATUS"), nba.sym("GAMSTATE")
  skill, catch_up = math.tointeger(nba.option("skill")), nba.option("catch_up")
  guard = nba.include("lib/records.lua").guard()
  if skill ~= 99 then nba.log("cpu skill " .. skill) end
  if not catch_up then nba.log("catch-up off") end
end

function on_frame()
  guard.update()
  if (skill == 99 and catch_up) or not procs or nba.read16(gs) ~= 3 then return end
  guard.mark()
  local st = nba.read16(ps)
  for k = 0, 3 do
    local p = nba.read32(procs + 32 * k)
    if p ~= 0 then
      if skill ~= 99 and st & (1 << k) == 0 then nba.write16(p + SKILL, skill & 0xFFFF) end
      if not catch_up then nba.write16(p + PTSDOWN, 0) end
    end
  end
end
