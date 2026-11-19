-- fire_rules.lua - when a player catches fire (roadmap Phase 6). T.E. 4.0 and SE 5.02.
--
-- The game counts a player's baskets in a row (ballnumscored, for ballpnumscored) and sets
-- plyrnumonfire (the player on fire, 0-3, or -1 for nobody) at 3 in a row, or 4 for a player
-- who has been on fire before (PLYR2.ASM: SUBK 3 / SUBK 4 on the count). Only one player is
-- on fire at a time.
-- "Baskets to catch fire" changes those two numbers in memory (the instruction constants,
-- found by their place after the load of ballnumscored; never in a file): N, and N + 1 after a
-- first fire. "Always on fire" keeps plyrnumonfire on a player of that team during play
-- (the team's first player, or the one already on fire); "Never on fire" keeps that team's
-- count of baskets in a row at 0 (ballnumscored, when ballpnumscored is on the team) and
-- clears plyrnumonfire if it falls on the team: clearing plyrnumonfire alone is not enough,
-- the game sets it again from the count on the next frame. If both teams are "always", team 1 has it.
mod = {
  name = "Fire rules",
  description = "Baskets needed to catch fire, and always or never on fire for each team.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "baskets", label = "Baskets to catch fire", values = { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
      labels = { "Game (3)", "1", "2", "3", "4", "5", "6", "7", "8" }, default = 0 },
    { id = "team1", label = "Team 1 fire", values = { 0, 1, 2 }, labels = { "Game", "Always on fire", "Never on fire" }, default = 0 },
    { id = "team2", label = "Team 2 fire", values = { 0, 1, 2 }, labels = { "Game", "Always on fire", "Never on fire" }, default = 0 },
    nba.include("lib/records.lua").OPTION,
  },
}

local ALWAYS, NEVER = 1, 2
local onfire, gs, inm, cnt, who, rule, guard

local function subk(k, rd) return 0x1400 | ((k & 31) << 5) | rd end

function on_load()
  onfire, gs, inm = nba.sym("plyrnumonfire"), nba.sym("GAMSTATE"), nba.sym("inmatchup")
  cnt, who = nba.sym("ballnumscored"), nba.sym("ballpnumscored")
  guard = nba.include("lib/records.lua").guard()
  rule = { math.tointeger(nba.option("team1")), math.tointeger(nba.option("team2")) }
  local n = math.tointeger(nba.option("baskets"))
  if n > 0 then
    local bn = nba.sym("ballnumscored")
    local hits = bn and nba.rom_find({ -1, bn & 0xFFFF, bn >> 16 }) or {}
    local done = false
    for _, a in ipairs(hits) do
      if nba.rom_read16(a) & 0xFFE0 == 0x05A0 then        -- MOVE @ballnumscored,Rd
        for k = 3, 20 do
          if nba.rom_read16(a + 16 * k) == subk(4, 0) and nba.rom_read16(a + 16 * (k + 2)) == subk(3, 0) then
            nba.rom_write16(a + 16 * k, subk(n + 1, 0))
            nba.rom_write16(a + 16 * (k + 2), subk(n, 0))
            done = true
            nba.log(string.format("baskets to catch fire: %d (then %d)", n, n + 1))
            break
          end
        end
      end
      if done then break end
    end
    if not done then nba.log("the fire count was not found: baskets not changed") end
  end
  for t = 1, 2 do
    if rule[t] ~= 0 then nba.log(string.format("team %d: %s", t, rule[t] == ALWAYS and "always on fire" or "never on fire")) end
  end
end

function on_frame()
  guard.update()
  if not onfire or nba.read16(gs) ~= 3 or nba.read16(inm) ~= 0 then return end
  if rule[1] ~= 0 or rule[2] ~= 0 or math.tointeger(nba.option("baskets")) > 0 then guard.mark() end
  local w = nba.read16(who)
  if w <= 3 and rule[w // 2 + 1] == NEVER and nba.read16(cnt) ~= 0 then nba.write16(cnt, 0) end
  local p = nba.read16(onfire)
  local team = p ~= 0xFFFF and (p // 2) + 1 or nil
  if team and rule[team] == NEVER then nba.write16(onfire, 0xFFFF); p = 0xFFFF; team = nil end
  for t = 1, 2 do
    if rule[t] == ALWAYS and team ~= t and not (t == 2 and rule[1] == ALWAYS) then
      nba.write16(onfire, (t - 1) * 2); break
    end
  end
end
