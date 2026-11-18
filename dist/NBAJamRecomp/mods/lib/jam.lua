-- lib/jam.lua - game events for Lua mods.
--
-- A script includes it and calls jam.update() from its on_frame:
--
--   local jam = nba.include("lib/jam.lua")
--   function on_frame() jam.update() end
--   function on_matchup()   nba.log("matchup screen") end
--   function on_game_start() nba.log("tip-off") end
--   function on_quarter(n)  nba.log("quarter " .. n) end   -- 5 and up are overtimes
--   function on_game_end()  nba.log("final buzzer") end
--
-- Each event is called once, in this order: on_matchup (the matchup screen before a game),
-- on_game_start (the game clock starts for the first time: the tip-off), on_quarter(n) (the
-- clock starts in quarter n; n = 1 right after on_game_start), on_game_end (the clock runs out
-- in the 4th quarter or an overtime with the scores apart, or the game is over).
-- A script defines only the events it wants. Attract-mode demo games give no events.
--
-- jam.available is false when the running game has no symbol file (mods/sym/): then no event
-- is called. jam.quarter() gives the current quarter (0 before tip-off), jam.scores() the two
-- teams' points.
--
-- The game's own state variables are read; nothing is written. One description per game
-- family, chosen by nba.game.

local jam = {}

-- GAMSTATE values from GAME.EQU (the same in both families)
local families = {
  jam      = { games = { nbajamte = true, nbajamse = true, nbajam = true }, INAMODE = 1, INGAME = 3, INGAMEOV = 8 },
  hangtime = { games = { nbamht = true, nbahangt = true },   INAMODE = 1, INGAME = 3, INGAMEOV = 8 },
}

local fam
for _, f in pairs(families) do if f.games[nba.game] then fam = f end end

local a = {}
if fam then
  for _, name in ipairs({ "GAMSTATE", "inmatchup", "gmqrtr", "game_time", "scores" }) do
    a[name] = nba.sym(name)
    if not a[name] then fam = nil end
  end
end
jam.available = fam ~= nil
if not fam and nba.game then    -- (no game while the mods page reads the scripts)
  nba.log("lib/jam.lua: no game events for " .. tostring(nba.game) .. " (no symbol file for this game or revision)")
end

local function s16(v) return v >= 0x8000 and v - 0x10000 or v end
local function call(name, ...)
  local f = _G[name]
  if type(f) == "function" then f(...) end
end

local phase = "idle"    -- idle, matchup, game, over
local quarter = 0
local prev_match, prev_time = 0, nil

function jam.quarter() return quarter end
function jam.scores()
  if not fam then return 0, 0 end
  return nba.read16(a.scores), nba.read16(a.scores + 16)
end

function jam.update()
  if not fam then return end
  local state = s16(nba.read16(a.GAMSTATE))
  local match = nba.read16(a.inmatchup)
  local q = nba.read16(a.gmqrtr)
  local t = nba.read32(a.game_time)

  -- the matchup screen: a new game is coming (closing one that never reached its end)
  if match ~= 0 and prev_match == 0 then
    if phase == "game" then phase = "over"; call("on_game_end") end
    phase = "matchup"
    quarter = 0
    call("on_matchup")
  end
  prev_match = match

  -- the clock counting down in a real game (not a demo). Its last tick to 0:00 is not a start:
  -- Maximum Hangtime moves gmqrtr on before the clock reaches 0.
  local running = state == fam.INGAME and prev_time ~= nil and t < prev_time and t ~= 0
  prev_time = t

  if running and phase == "matchup" then
    phase = "game"
    call("on_game_start")
  end
  if running and phase == "game" and q + 1 > quarter then
    quarter = q + 1
    call("on_quarter", quarter)
  end

  if phase == "game" then
    local s1, s2 = jam.scores()
    local buzzer = t == 0 and quarter >= 4 and s1 ~= s2
    if buzzer or state == fam.INGAMEOV or state == fam.INAMODE then
      phase = "over"
      call("on_game_end")
    end
  end
end

return jam
