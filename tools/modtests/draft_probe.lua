-- draft_probe.lua - loaded next to fantasy_draft.lua by fantasy_draft.expect (roadmap 6.F2).
-- Four players join (Start for players 1 to 4 from frame 1200, then again 200 frames later).
-- From frame 2000 every player presses Down every 20 frames and Shoot every 70; only the
-- player whose pick it is counts, so each pick lands a few places on from the last cursor.
-- Logs "team select <in_team_select>" every 600 frames: the game stays on team select while
-- the players' buttons are held. At tip-off logs the four players' places in the player list
-- ("tipoff places a b c d", from each player's attribute table).
mod = { name = "Draft probe" }
local sel, gs, procs, atl, tip
function on_load()
  sel, gs, procs, atl = nba.sym("in_team_select"), nba.sym("GAMSTATE"), nba.sym("plyrproc_t"), nba.sym("ATL_t")
end
function on_frame(f)
  for k = 1, 4 do
    if f == 1200 + 10 * k or f == 1400 + 10 * k then nba.press(k, "start", 4) end
    if f >= 2000 then
      if f % 20 == 0 then nba.press(k, "down", 3) end
      if f % 70 == 35 then nba.press(k, "shoot", 3) end
    end
  end
  if f % 600 == 0 then nba.log("team select " .. nba.read16(sel)) end
  -- at tip-off: each player's place in the player list, from his attribute table (6.F1)
  if not tip and nba.read16(gs) == 3 then
    tip = true
    local s = {}
    for k = 0, 3 do s[#s + 1] = tostring((nba.read32(nba.read32(procs + 32 * k) + 0x150) - atl) // 368) end
    nba.log("tipoff places " .. table.concat(s, " "))
  end
end
