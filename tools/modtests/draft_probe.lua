-- draft_probe.lua - loaded next to fantasy_draft.lua by fantasy_draft.expect (roadmap 6.F2, 6.F3).
-- Four players join (Start for players 1 to 4 from frame 1200, then again 200 frames later).
-- From frame 2000 every player presses Down every 20 frames and Shoot every 70; only the
-- player whose pick it is counts, so each pick lands a few places on from the last cursor.
-- At tip-off logs the four players' places in the player list ("tipoff places a b c d", from
-- each player's attribute table) and special_heads ("tipoff heads a b c d").
-- Option "setup" (modtest gives every script the same options):
--   0 nothing more; 1 saves a last draft "1:r85 2:r86 3:- 4:-" at frame 1 (for Replay);
--   2 saves "team 2 lost" at frame 1 (for Loser first); 3 never presses Down, so every
--   drafter's cursor stays on the first player.
mod = { name = "Draft probe", options = { { id = "setup", values = { 0, 1, 2, 3 }, default = 0 } } }
local gs, procs, atl, sh, tip
function on_load()
  gs, procs, atl, sh = nba.sym("GAMSTATE"), nba.sym("plyrproc_t"), nba.sym("ATL_t"), nba.sym("special_heads")
end
function on_frame(f)
  local setup = nba.option("setup")
  if f == 1 and setup == 1 then nba.save(nba.game .. "_draft_last.txt", "1:r85 2:r86 3:- 4:-\n") end
  if f == 1 and setup == 2 then nba.save(nba.game .. "_draft_loser.txt", "2\n") end
  for k = 1, 4 do
    if f == 1200 + 10 * k or f == 1400 + 10 * k then nba.press(k, "start", 4) end
    if f >= 2000 then
      if f % 20 == 0 and setup ~= 3 then nba.press(k, "down", 3) end
      if f % 70 == 35 then nba.press(k, "shoot", 3) end
    end
  end
  if not tip and nba.read16(gs) == 3 then
    tip = true
    local s, h = {}, {}
    for k = 0, 3 do
      s[#s + 1] = tostring((nba.read32(nba.read32(procs + 32 * k) + 0x150) - atl) // 368)
      local v = nba.read16(sh + 16 * k)
      h[#h + 1] = tostring(v >= 0x8000 and v - 0x10000 or v)
    end
    nba.log("tipoff places " .. table.concat(s, " "))
    nba.log("tipoff heads " .. table.concat(h, " "))
  end
end
