-- lib/records.lua - keep a game that a script changed out of the players' saved records
-- (roadmap 5.3).
--
--   local records = nba.include("lib/records.lua")
--   local guard = records.guard()     -- once, in on_load (reads option protect_records)
--   guard.mark()                      -- whenever the script changes this game
--   guard.update()                    -- every frame
--   options: records.OPTION            -- the switch, "Protect records", on by default
--
-- At the end of every game the game saves the record of each player who entered initials
-- (T.E./SE: save_player_records, only when player<n>_data+PR_INIT1 is not negative; Hangtime:
-- the same with PR_NAME1, only when it is above 0; both are the fifth word of the record).
-- Once a game is marked, from the end of the fourth quarter (gmqrtr 4 or more and play
-- stopped: the save comes before the end-of-game screen, GAMSTATE 8) until attract mode, the
-- guard sets that word to -1 in the four players' in-RAM records, so the game skips them:
-- no game, win, loss or win streak is counted. Nothing is written to NVRAM. The game still
-- ages every saved record by one game ("last played"), as it does after any game.
-- Not on NBA JAM 1993: its symbol file does not name the player records.
local records = {}

records.OPTION = { id = "protect_records", label = "Protect records", type = "toggle", default = true }

local INIT1 = 0x40       -- bits: PR_COUNT, PR_WON, PR_LOST, PR_LASTPLAY, then PR_INIT1 / PR_NAME1
local INGAME, ATTRACT0, ATTRACT = 3, 0, 1

function records.guard()
  local g = { marked = false, done = false }
  local ok, on = pcall(nba.option, "protect_records")
  local enabled = not ok or on ~= false
  local gs, q = nba.sym("GAMSTATE"), nba.sym("gmqrtr")
  local p = {}
  for n = 1, 4 do p[n] = nba.sym("player" .. n .. "_data") end
  local usable = gs and q and p[1] and p[2] and p[3] and p[4] and nba.sym("save_player_records")
  if enabled and not usable then nba.log("protect records: not available on this game") end
  function g.mark() g.marked = true end
  function g.update()
    if not (enabled and usable) then return end
    local s = nba.read16(gs)
    if g.marked and s ~= INGAME and s ~= ATTRACT0 and s ~= ATTRACT and nba.read16(q) >= 4 then
      for n = 1, 4 do nba.write16(p[n] + INIT1, 0xFFFF) end
      if not g.done then g.done = true; nba.log("protect records: this game is not saved to the players' records") end
    elseif s == ATTRACT0 or s == ATTRACT then
      g.marked, g.done = false, false
    end
  end
  return g
end

return records
