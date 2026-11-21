-- initials_auto_probe.lua - loaded next to auto_initials.lua by auto_initials.expect.
-- Starts a game for player 1 at frame 1200 (start twice, then Shoot on the YES question, as
-- record_win_probe.lua), leaves the initials grid to the script, and 900 frames after YES logs
-- player1_data PR_INIT1, PR_INIT2, PR_INIT3, PR_MONTH, PR_DAY: "fields a b c m d".
mod = { name = "Auto initials probe" }
local p1
function on_load() p1 = nba.sym("player1_data") end
function on_frame(f)
  if f == 1200 or f == 1400 then nba.press(1, "start", 4) end
  if f == 1900 then nba.press(1, "shoot", 4) end
  if f == 2800 then
    local v = {}
    for i = 0, 4 do v[#v + 1] = tostring(nba.read16(p1 + 0x40 + 16 * i)) end
    nba.log("fields " .. table.concat(v, " "))
  end
end
