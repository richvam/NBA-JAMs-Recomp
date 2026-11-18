-- codes_mht.lua - the cabinet codes of NBA Maximum Hangtime 1.3 as switches (roadmap 3.3).
-- Each switch forces what the code would set; the table is lib/codes_nbamht.lua.
-- Not here: the number code lock (pup_lockcombo only keeps the icons still while you enter a
-- number) and the special combo (it sets ABA ball, outdoor court, shot percentage and big head,
-- each of which has its own switch).
local codes = nba.include("lib/codes.lua")
local T = codes.table("nbamht")

-- tournament mode clears these in the game, so their switches are greyed while it is on
local BLOCKED = { "tournament" }
local IDS = { "tournament", "baby", "no_music", "goaltend", "max_block", "max_steal", "max_power", "max_speed",
  "hyper_speed", "stealth_turbo", "infinite_turbo", "no_push", "fast_pass", "big_head", "huge_head",
  "shot_percent", "no_tag", "no_drift", "no_assistance", "aba_ball", "outdoor_court" }
local extra = {}
for _, c in ipairs(T) do if c.tournament == "cleared" then extra[c.id] = BLOCKED end end

mod = {
  name = "Cabinet codes (Maximum Hangtime)",
  description = "Turns on Maximum Hangtime cabinet codes without entering them.",
  games = { "nbamht" },
  options = codes.options(T, IDS, extra),
}

-- the game's player numbers: on a four-player cabinet start 1-4 are players 0-3; on a
-- two-player one (TWOPLAYERS = 1) start 1 and 2 are players 1 and 2 (BB.ASM plyr_strtb1/2)
local function slots(mask)
  local two = nba.sym("TWOPLAYERS")
  if not (two and nba.read16(two) ~= 0) then return mask end
  return ((mask & 1) ~= 0 and 2 or 0) | ((mask & 2) ~= 0 and 4 or 0)
end

local drive
function on_load()
  drive = codes.driver(T, nil, slots)
  for _, e in ipairs(drive.on) do nba.log(string.format("forcing %s (%s)", e.c.id, e.c.sym)) end
end
function on_frame() drive.update() end
