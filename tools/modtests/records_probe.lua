-- records_probe.lua - loaded next to unlock_players.lua by nvram_unlock.expect (roadmap 4.4):
-- at frame 59000, after a whole scripted game, a hash of the NVRAM after the audit area
-- (T-unit CMOS words 0x24C-0x1FFF, the packed player records and the tables after them).
-- Scripts may read NVRAM; writing it is refused (nba.write*).
mod = { name = "Records probe" }
function on_frame(f)
  if f == 59000 then
    local h = 2166136261
    for w = 0x24C, 0x1FFF do h = ((h ~ (nba.read16(0x01400000 + 16 * w) & 0xFF)) * 16777619) & 0xFFFFFFFF end
    nba.log(string.format("records area %08x", h))
  end
end
