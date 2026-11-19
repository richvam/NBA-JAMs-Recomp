-- codes_probe.lua - loaded next to codes_te.lua by codes_force_te2.expect (roadmap 3.2).
-- Logs what the forced codes change, read from the game:
--   "select: ..."  bigroster1/2, team1/2 and team1/2_switch on a team select screen, the
--                  first time it has lasted 200 frames (and again at halftime)
--   "attrib: ..."  each player's attribute table pointer (plyr_attrib_p, PDATA + 0x50) and
--                  head image, 2 s into the game; a secret player's table is spechds_t + n * size
--   "late: ..."    the code variables 4 s and 20 s into the game
mod = { name = "Codes probe" }
local codes = nba.include("lib/codes.lua")
local S, t3, sel, qtr0 = {}, nil, 0, -1
local function r(n) return S[n] and nba.read16(S[n]) or -1 end
function on_load()
  for _, n in ipairs({ "GAMSTATE", "in_team_select", "gmqrtr", "team1", "team2", "p1taps", "p2taps",
      "show_shot", "assistance_off", "stlpower", "plyrbighead", "baby_flag", "plyr_tourn", "plyr_maxpower" }) do
    S[n] = nba.sym(n)
  end
  S.bigroster1 = codes.sym("bigroster1"); S.bigroster2 = S.bigroster1 + 16
  S.team1_switch = codes.sym("team1_switch"); S.team2_switch = S.team1_switch + 16
  S.procs = nba.sym("plyrproc_t"); S.spechds_t = nba.sym("spechds_t")
end
function on_frame(f)
  if r("in_team_select") ~= 0 then
    sel = sel + 1
    if sel == 200 and r("gmqrtr") ~= qtr0 then
      qtr0 = r("gmqrtr")
      nba.log(string.format("select: gmqrtr=%d team1=%d team2=%d bigroster1=%d bigroster2=%d team1_switch=%d team2_switch=%d",
        r("gmqrtr"), r("team1"), r("team2"), r("bigroster1"), r("bigroster2"), r("team1_switch"), r("team2_switch")))
    end
  else sel = 0 end
  if r("GAMSTATE") == 3 and not t3 then t3 = f end
  if t3 and f == t3 + 120 then
    local out = {}
    for k = 0, 3 do
      local p = nba.read32(S.procs + 32 * k)
      local a = p ~= 0 and nba.read32(p + 0x150) or 0
      local h = p ~= 0 and nba.read32(p + 0x4B0) or 0
      out[#out + 1] = string.format("p%d %08X/%08X", k + 1, a, h ~= 0 and nba.read32(h + 0x1C0) or 0)
    end
    nba.log(string.format("attrib: spechds_t %08X %s", S.spechds_t, table.concat(out, " ")))
  end
  if t3 and (f == t3 + 240 or f == t3 + 1200) then
    nba.log(string.format("late %d: p1taps=%d p2taps=%d show_shot=%d assistance_off=%d stlpower=%d plyrbighead=%d baby_flag=%d plyr_tourn=%d plyr_maxpower=%d",
      f - t3, r("p1taps"), r("p2taps"), r("show_shot"), r("assistance_off"), r("stlpower"), r("plyrbighead"), r("baby_flag"), r("plyr_tourn"), r("plyr_maxpower")))
  end
end
