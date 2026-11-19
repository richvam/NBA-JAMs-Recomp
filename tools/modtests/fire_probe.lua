-- fire_probe.lua - loaded next to fire_rules.lua by fire_rules.expect: every change of
-- plyrnumonfire during play ("fire <frame>: <player or -1>"), and at the end the number of
-- times somebody caught fire and the frames on fire per team ("fire total: ...").
mod = { name = "Fire probe" }
local of, gs, last, fires, t1, t2, logged = nil, nil, 0xFFFF, 0, 0, 0, 0
function on_load() of = nba.sym("plyrnumonfire"); gs = nba.sym("GAMSTATE") end
function on_frame(f)
  local g = nba.read16(gs)
  if g == 3 then
    local p = nba.read16(of)
    if p ~= last then
      if p ~= 0xFFFF then fires = fires + 1 end
      if logged < 12 then nba.log(string.format("fire %d: %d", f, p == 0xFFFF and -1 or p)); logged = logged + 1 end
      last = p
    end
    if p == 0 or p == 1 then t1 = t1 + 1 elseif p == 2 or p == 3 then t2 = t2 + 1 end
  end
  if g == 8 and last ~= -2 then nba.log(string.format("fire total: %d fires, team 1 %d frames, team 2 %d frames", fires, t1, t2)); last = -2 end
end
