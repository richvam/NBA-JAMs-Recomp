-- boot_probe.lua - loaded next to fast_boot.lua by fast_boot.expect (roadmap Phase 6). Logs
-- "attract at <frame>" on the first frame on which the process list (ACTIVE) is not empty:
-- POWERCMOS runs before any process exists, and amode_start is the first one created after it.
-- NBA JAM 1993 has no ACTIVE symbol: amode_start begins with "MOVI ACTIVE,A0", read from there.
mod = { name = "Boot probe" }
local act, seen
function on_load()
  act = nba.sym("ACTIVE")
  local a = nba.sym("amode_start")
  if not act and a and nba.rom_read16(a) == 0x09E0 then act = nba.rom_read32(a + 16) end
end
function on_frame(f)
  if act and not seen and nba.read32(act) ~= 0 then seen = true; nba.log("attract at " .. f) end
end
