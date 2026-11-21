-- cvc_probe.lua - for cpu_vs_cpu.expect: logs a game that is in play with no player in it
-- (PSTATUS 0) for 1000 frames, and the period at the end-of-game screen (GAMSTATE 8).
mod = { name = "CVC probe" }
local gs, pst, q, run, last = nil, nil, nil, 0, -1
function on_load() gs, pst, q = nba.sym("GAMSTATE"), nba.sym("PSTATUS"), nba.sym("gmqrtr") end
function on_frame(f)
  local g = nba.read16(gs)
  if g == 3 and nba.read16(pst) == 0 then
    run = run + 1
    if run == 1000 then nba.log("in play with no player for 1000 frames") end
  else run = 0 end
  if g == 8 and last ~= 8 then
    nba.log(nba.read16(q) >= 4 and "end of game after period 4 or later" or ("end of game in period " .. nba.read16(q)))
  end
  if g ~= last and (g == 0 or g == 1) then nba.log("attract, PSTATUS " .. nba.read16(pst)) end
  last = g
end
