-- codes_probe_jam.lua - loaded next to codes_jam.lua by codes_force_jam.expect (roadmap 3.5):
-- the code variables 4 s and 20 s into the game ("late <frames>:"), and player 1's head image
-- 2 s in ("heads:", as head_probe.lua).
mod = { name = "Codes probe (NBA JAM)" }
local VARS = { "p1taps", "show_shot", "assistance_off", "stlpower", "plyrbighead" }
local S, gs, procs, t3 = {}, nil, nil, nil
function on_load() for _, n in ipairs(VARS) do S[n] = nba.sym(n) end; S.p2taps = S.p1taps + 16; table.insert(VARS, 2, "p2taps"); gs = nba.sym("GAMSTATE"); procs = nba.sym("plyrproc_t") end
function on_frame(f)
  if nba.read16(gs) == 3 and not t3 then t3 = f end
  if t3 and f == t3 + 120 and procs then
    local p = nba.read32(procs); local h = p ~= 0 and nba.read32(p + 0x4B0) or 0
    nba.log(string.format("heads: p1 %08X", h ~= 0 and nba.read32(h + 0x1C0) or 0))
  end
  if t3 and (f == t3 + 240 or f == t3 + 1200) then
    local out = {}
    for _, n in ipairs(VARS) do out[#out + 1] = string.format("%s=%d", n, nba.read16(S[n])) end
    nba.log(string.format("late %d: %s", f - t3, table.concat(out, " ")))
  end
end
