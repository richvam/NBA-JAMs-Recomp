-- initials_probe.lua - loaded next to unlock_players.lua by nvram_initials.expect (roadmap
-- 4.4). Plays T.E./SE as player 1 with initials, so the game keeps a player record: start at
-- frames 1200 and 1400 (join), shoot on YES at 1900 ("enter initials for record keeping?"),
-- shoot five times from 2150, 30 frames apart (initials A, A, A at the cursor, then birth
-- month January and day 1), then shoot every 60 frames on team select to pick a team. The
-- game runs on to its end with player 1 idle. Logs "initials entered" when the game reports
-- the entry screen (can_enter_inits).
mod = { name = "Initials probe" }
local ce, sel, seen
function on_load() ce = nba.sym("can_enter_inits"); sel = nba.sym("in_team_select") end
function on_frame(f)
  if f == 1200 or f == 1400 then nba.press(1, "start", 4) end
  if f == 1900 then nba.press(1, "shoot", 4) end
  if f >= 2150 and f <= 2270 and (f - 2150) % 30 == 0 then nba.press(1, "shoot", 4) end
  if not seen and nba.read16(ce) ~= 0 then seen = true; nba.log("initials entered at frame " .. f) end
  -- the NVRAM after the audit area (CMOS words 0x24C on, bytes 0x498 on in the dump), where
  -- the packed player records are, hashed (scripts may read NVRAM, never write it)
  if f == 59000 then
    local h = 2166136261
    for w = 0x24C, 0x1FFF do h = ((h ~ (nba.read16(0x01400000 + 16 * w) & 0xFF)) * 16777619) & 0xFFFFFFFF end
    nba.log(string.format("records area %08x", h))
  end
  if f > 2400 and nba.read16(sel) ~= 0 and f % 60 == 0 then nba.press(1, "shoot", 4) end
end
