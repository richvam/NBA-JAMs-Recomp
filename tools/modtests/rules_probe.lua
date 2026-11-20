-- rules_probe.lua - loaded next to rules.lua by rules.expect. During play: the lowest shot
-- clock seen while one runs, the highest seen when a new one starts (shotimer tens*10+ones),
-- and at the end-of-game screen every player's injury stat (player_stats, PS_INJURY = 13, 14 a
-- player): "rules: shot clock low <n> high <n>, injuries a b c d".
mod = { name = "Rules probe" }
local st, sp, ps, gs, low, high, done = nil, nil, nil, nil, 99, 0, false
function on_load() st = nba.sym("shotimer"); sp = nba.sym("sc_proc"); ps = nba.sym("player_stats"); gs = nba.sym("GAMSTATE") end
function on_frame()
  local g = nba.read16(gs)
  if g == 3 then
    if nba.read32(sp) ~= 0 then
      local v = nba.read16(st + 16) * 10 + nba.read16(st)
      if v < low then low = v end
      if v > high then high = v end
    end
  end
  if g == 8 and not done then
    done = true
    local inj = {}
    for p = 0, 3 do inj[#inj + 1] = nba.read16(ps + 16 * (p * 14 + 13)) end
    nba.log(string.format("rules: shot clock low %d high %d, injuries %s", low, high, table.concat(inj, " ")))
  end
end
