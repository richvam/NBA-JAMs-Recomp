-- codes_jam.lua - the cabinet codes of NBA JAM (1993, rev 3.01) as switches (roadmap 3.5).
-- Each switch forces what the code would set; the table is lib/codes_nbajam.lua. NBA JAM 1993
-- has no tournament mode, maximum power, huge head or baby mode (T.E. added them).
local codes = nba.include("lib/codes.lua")
local T = codes.table("nbajam")

mod = {
  name = "Cabinet codes (NBA JAM)",
  description = "Turns on NBA JAM cabinet codes without entering them.",
  games = { "nbajam" },
  options = (function() local o = codes.options(T, { "big_head", "quick_hands", "goaltend", "shot_percent", "assist_off" }); o[#o + 1] = nba.include("lib/records.lua").OPTION; return o end)(),
}

local drive, guard
function on_load()
  guard = nba.include("lib/records.lua").guard()
  -- the game checks computer assistance about 335 frames after tip-off and clears it there
  drive = codes.driver(T, nil, nil, { assist_off = 600 })
  for _, e in ipairs(drive.on) do nba.log(string.format("forcing %s (%s = %d)", e.c.id, e.c.sym, e.value)) end
end
function on_frame()
  drive.update()
  -- a game with a code forced does not count for the records
  if #drive.on > 0 then guard.mark() end
  guard.update()
end
