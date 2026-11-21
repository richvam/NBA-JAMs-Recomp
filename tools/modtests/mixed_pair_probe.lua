-- mixed_pair_probe.lua - roadmap 6.F1: a hand-made pair from two teams plays a quarter.
-- Changes the ROM table player_names (SCORE2.ASM: 27 teams x 20 pairs x 2 name pointers) so
-- Chicago's first pair (team 3, pair 0) is Atlanta's first player and Utah's (team 25), then
-- starts a game for player 1 as matchup_probe.lua does, holding Chicago with pair 0 on team
-- select. Each player is found the way the game finds him (PLYRAT2.ASM plyr_getattributes,
-- SPEECH.ASM): the name pointer's place in name_sort gives the attribute table
-- ATL_t + place x 368 bits and the announcer's name call name_speech + place x 64 bits.
-- Logs at tip-off and at the end of the first quarter, for players 1 and 2:
--   "p<k> want <place> attrib <place from plyr_attrib_p> speech <place from spch_name_ram>"
-- and "quarter over at <frame>, gmqrtr <n>, score <a>-<b>, p1 <pts> p2 <pts>" (player_stats).
mod = { name = "Mixed pair probe" }
local gs, sel, t1, s1, names, sort, atl, spn, nsp, procs, ps, sc
local want, tip, q1
local function place(ptr)
  for i = 0, 200 do if nba.rom_read32(sort + 32 * i) == ptr then return i end end
  return -1
end
local function entry(t, p, s) return names + (t * 20 + p) * 64 + s * 32 end
function on_load()
  gs, sel, t1, s1 = nba.sym("GAMSTATE"), nba.sym("in_team_select"), nba.sym("team1"), nba.sym("tm1set")
  names, sort, atl = nba.sym("player_names"), nba.sym("name_sort"), nba.sym("ATL_t")
  spn, nsp, procs = nba.sym("spch_name_ram"), nba.sym("name_speech"), nba.sym("plyrproc_t")
  ps, sc = nba.sym("player_stats"), nba.sym("scores")
  local a, u = nba.rom_read32(entry(0, 0, 0)), nba.rom_read32(entry(25, 0, 0))
  nba.rom_write32(entry(3, 0, 0), a); nba.rom_write32(entry(3, 0, 1), u)
  want = { place(a), place(u) }
  nba.log(string.format("mixed pair: places %d %d", want[1], want[2]))
end
local function report(tag)
  for k = 0, 1 do
    local p = nba.read32(procs + 32 * k)
    local at = (nba.read32(p + 0x150) - atl) // 368
    local sp = (nba.read32(spn + 32 * k) - nsp) // 64
    nba.log(string.format("%s p%d want %d attrib %d speech %d %s", tag, k + 1, want[k + 1], at, sp,
      (at == want[k + 1] and sp == want[k + 1]) and "same" or "DIFFER"))
  end
end
local function pts(k)
  local function st(i) return nba.read16(ps + 16 * (14 * k + i)) end
  return 2 * st(1) + 3 * st(3) + st(11)
end
function on_frame(f)
  local k = f - 1200
  if k == 0 or k == 200 then nba.press(1, "start", 4) end
  if k == 700 then nba.press(1, "shoot", 4) end
  if k == 940 then nba.press(1, "right", 3) end
  if k >= 950 and k <= 1070 and (k - 950) % 30 == 0 then nba.press(1, "shoot", 4) end
  local g = nba.read16(gs)
  if k > 1200 and nba.read16(sel) ~= 0 then
    nba.write16(t1, 3); nba.write16(s1 + 16 * 3, 0)
    if f % 60 == 0 then nba.press(1, "shoot", 4) end
  end
  if g == 3 then
    if not tip then tip = true; report("tipoff") end
    if f % 30 == 0 then nba.press(1, (f // 30) % 2 == 0 and "shoot" or "pass", 3) end
  elseif tip and not q1 and g == 4 then
    q1 = true; report("q1end")
    nba.log(string.format("quarter over at %d, gmqrtr %d, score %d-%d, p1 %d p2 %d", f, nba.read16(nba.sym("gmqrtr")), nba.read16(sc), nba.read16(sc + 16), pts(0), pts(1)))
  end
end
