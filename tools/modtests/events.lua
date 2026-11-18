-- events.lua - logs every event of lib/jam.lua with its frame (roadmap 1.5 check).
-- Copy it next to a copy of dist/NBAJamRecomp/mods/lib and mods/sym, then:
--   PLAY=1 build/linux/modtest <prog_lo> <prog_hi> <that folder> nbajamte 60000
mod = { name = "Event log" }
local jam = nba.include("lib/jam.lua")
local function say(s) nba.log(string.format("EVENT frame %d: %s", nba.frame(), s)) end
function on_frame() jam.update() end
function on_matchup() say("matchup") end
function on_game_start() say("game start") end
function on_quarter(n) say("quarter " .. n) end
function on_game_end() local a, b = jam.scores(); say("game end " .. a .. "-" .. b) end
