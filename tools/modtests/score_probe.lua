-- score_probe.lua - loaded next to handicap.lua by handicap.expect (roadmap 5.1). Logs the
-- score and the quarter table kp_qscrs whenever GAMSTATE or the quarter changes ("f=<frame>
-- gs=<state> q=<quarter> score a-b quarters q1 q2 q3 q4"), and 100 frames after tip-off which
-- number each scoreboard digit object shows ("scoreboard a-b"): its image (OSAG) looked up in
-- the game's LED_0 table, both addresses read from prt_top_scores as handicap.lua does.
mod = { name = "Score probe" }
local s, gs, q, k, lg, lq, t3, imgs, led
function on_load()
  s = nba.sym("scores"); gs = nba.sym("GAMSTATE"); q = nba.sym("gmqrtr"); k = nba.sym("kp_qscrs")
  local p = nba.sym("prt_top_scores")
  if p and nba.read32(p + 0x60) == s then imgs, led = nba.read32(p + 0x30), nba.read32(p + 0xA0) end
end
local function shown(team)
  local o = nba.read32(imgs + 32 * team)
  if o == 0 then return -1 end
  local img = nba.read32(o + 0x110)
  for n = 0, 199 do if nba.read32(led + 32 * n) == img then return n end end
  return -1
end
function on_frame(f)
  local g, qq = nba.read16(gs), nba.read16(q)
  if g ~= lg or qq ~= lq then
    local t = {}
    for i = 0, 3 do t[#t + 1] = nba.read16(k + 32 * i) .. "-" .. nba.read16(k + 32 * i + 16) end
    nba.log(string.format("f=%d gs=%d q=%d score %d-%d quarters %s", f, g, qq, nba.read16(s), nba.read16(s + 16), table.concat(t, " ")))
    lg, lq = g, qq
  end
  if g == 3 and not t3 then t3 = f end
  if t3 and f == t3 + 100 and imgs then nba.log(string.format("scoreboard %d-%d", shown(0), shown(1))) end
end
