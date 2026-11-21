-- random_matchup.lua - random teams and player pairs on team select (roadmap Phase 6).
-- T.E. 4.0 and SE 5.02.
--
-- On the first team select of a game (not at halftime) the script picks a random team for
-- each side it is set for, never the same team twice and never an excluded one, and a random
-- pair of that team's players. It holds them while team select is on: the team under the
-- cursor is team1 / team2, and the pair is tm1set / tm2set, one word per team (SELECT.ASM);
-- a team's pairs are the rows of player_heads that are not empty (SELECT2.ASM, 20 rows a
-- team; the toggle with Turbo goes through rows 0 to 5). Pressing a button then takes that
-- team and pair. A side is the players' when team1_control / team2_control is not negative.
-- The game still picks the computer's pair on its own for some teams when team select ends.
-- Choosing the computer's team changes the run against the computer, so then "Protect
-- records" keeps the game out of the saved records. Not in an online game.
local TEAMS = { "Atlanta", "Boston", "Charlotte", "Chicago", "Cleveland", "Dallas", "Denver",
  "Detroit", "Golden State", "Houston", "Indiana", "L.A. Clippers", "L.A. Lakers", "Miami",
  "Milwaukee", "Minnesota", "New Jersey", "New York", "Orlando", "Philadelphia", "Phoenix",
  "Portland", "Sacramento", "San Antonio", "Seattle", "Utah", "Washington" }

local records = nba.include("lib/records.lua")
local options = {
  { id = "sides", label = "Random teams for", values = { 0, 1, 2 }, labels = { "Players", "Computer", "Both" }, default = 0 },
  { id = "pairs", label = "Players", values = { 0, 1 }, labels = { "Random pair", "Team's first pair" }, default = 0 },
  records.OPTION,
}
for i, n in ipairs(TEAMS) do
  options[#options + 1] = { id = "ex" .. (i - 1), label = n, group = "Exclude", type = "toggle", default = false }
end

mod = {
  name = "Random matchup",
  description = "Random teams and player pairs on team select, with teams that can be left out.",
  games = { "nbajamte", "nbajamse" },
  options = options,
}

local sel, q, team, set, ctl, heads, guard
local pick        -- { [1] = { team, pair } or nil, [2] = ... } for this team select

function on_load()
  sel, q, heads = nba.sym("in_team_select"), nba.sym("gmqrtr"), nba.sym("player_heads")
  team = { nba.sym("team1"), nba.sym("team2") }
  set = { nba.sym("tm1set"), nba.sym("tm2set") }
  ctl = { nba.sym("team1_control"), nba.sym("team2_control") }
  guard = records.guard()
  if nba.netplay then sel = nil; nba.log("random matchup: not in an online game"); return end
  if not (sel and q and heads and team[2] and set[2] and ctl[2]) then sel = nil; nba.log("random matchup: not available on this game") end
end

local function s16(v) return v >= 0x8000 and v - 0x10000 or v end

local function pairs_of(t)
  local p = {}
  for r = 0, 5 do
    if nba.rom_read32(heads + (t * 20 + r) * 64) ~= 0 then p[#p + 1] = r end
  end
  return p
end

local function choose()
  local sides = nba.option("sides")
  local free = {}
  for t = 0, 26 do if not nba.option("ex" .. t) then free[#free + 1] = t end end
  pick = {}
  for s = 1, 2 do
    local human = s16(nba.read16(ctl[s])) >= 0
    if (sides == 2 or (sides == 0) == human) and #free > 0 then
      local i = math.random(#free)
      local t = table.remove(free, i)
      local p = pairs_of(t)
      local r = (nba.option("pairs") == 0 and #p > 0) and p[math.random(#p)] or 0
      pick[s] = { t, r, human }
      nba.log(string.format("random matchup: team %d %s, pair %d", s, TEAMS[t + 1], r))
    end
  end
  -- a team left to the cursor must not be one picked for the other side
  for s = 1, 2 do
    local o = pick[3 - s]
    if not pick[s] and o and nba.read16(team[s]) == o[1] then pick[3 - s] = nil end
  end
end

function on_frame()
  guard.update()
  if not sel then return end
  if nba.read16(sel) == 0 or nba.read16(q) ~= 0 then pick = nil; return end
  if not pick then choose() end
  for s = 1, 2 do
    local p = pick[s]
    if p then
      nba.write16(team[s], p[1])
      nba.write16(set[s] + 16 * p[1], p[2])
      if not p[3] then guard.mark() end
    end
  end
end
