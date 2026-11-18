-- codes_probe_mht.lua - loaded next to codes_mht.lua by codes_force_mht.expect (roadmap 3.3):
-- logs every power-up variable that is not 0, 4 s and 20 s into the game ("late <frames>:").
mod = { name = "Codes probe (Maximum Hangtime)" }
local VARS = { "pup_tournament", "pup_baby", "pup_nomusic", "pup_goaltend", "pup_maxblock", "pup_maxsteal", "pup_maxpower",
  "pup_maxspeed", "pup_hypspeed", "pup_trbstealth", "pup_trbinfinite", "pup_nopush", "pup_fastpass", "pup_lockcombo",
  "pup_bighead", "pup_hugehead", "pup_showshotper", "pup_notag", "pup_nodrift", "pup_noassistance", "pup_aba", "pup_court" }
local S, gs, t3 = {}, nil, nil
function on_load() for _, n in ipairs(VARS) do S[n] = nba.sym(n) end; gs = nba.sym("GAMSTATE") end
function on_frame(f)
  if nba.read16(gs) == 3 and not t3 then t3 = f end
  if t3 and (f == t3 + 240 or f == t3 + 1200) then
    local out = {}
    for _, n in ipairs(VARS) do local v = nba.read16(S[n]); if v ~= 0 then out[#out + 1] = string.format("%s=%X", n:sub(5), v) end end
    nba.log(string.format("late %d: %s", f - t3, #out > 0 and table.concat(out, " ") or "none"))
  end
end
