-- show_info.lua - shooting, turbo and fire on the overlay during play (roadmap Phase 6).
-- T.E. 4.0 and SE 5.02.
--
-- One line per player, drawn on the program's overlay (never in the game's memory):
--   P1 FG 3/5  60% 3P 1/2 [#####   ] FIRE 2      (the bar is the turbo meter)
-- Shots: player_stats, 14 words per player (GAME.EQU PS_*): 2-point tries and makes,
-- 3-point tries and makes, ... A dunk is counted as a 2-point shot as well (checked against
-- the score: points = 2 x 2-point makes + 3 x 3-point makes), so FG = 2-point + 3-point.
-- Turbo: ply_turbo (+0x40 in the player's PxDATA block, through plyrproc_t; 0 to 0x2F full),
-- as turbo_rules.lua reads it.
-- Fire: the player on fire is plyrnumonfire (or -1); the baskets in a row are ballnumscored,
-- for the player ballpnumscored. "FIRE n" shows that count, "ON FIRE" the player on fire.
-- Shown only while the game is in play. The script changes nothing in the game.
mod = {
  name = "Show info",
  description = "Shooting, turbo and on-fire counters for each player on the overlay during play.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "place", label = "Place", values = { 0, 1 }, labels = { "Bottom", "Top" }, default = 0 },
  },
}

local ps, procs, gs, onfire, cnt, who
local FULL, TURBO, PDATA_P = 0x2F, 0x40, 0x130
local H = 10

function on_load()
  ps, procs, gs = nba.sym("player_stats"), nba.sym("plyrproc_t"), nba.sym("GAMSTATE")
  onfire, cnt, who = nba.sym("plyrnumonfire"), nba.sym("ballnumscored"), nba.sym("ballpnumscored")
end

local function stat(k, i) return nba.read16(ps + 16 * (14 * k + i)) end

local function signed(v) return v >= 0x8000 and v - 0x10000 or v end

function on_frame()
  if not (ps and procs and gs) or nba.read16(gs) ~= 3 then return end
  local y0 = nba.option("place") == 1 and 30 or 254 - 4 * H - 4
  nba.rect(2, y0 - 2, 340, 4 * H + 3, 0x80000000, true)
  local fire = onfire and signed(nba.read16(onfire)) or -1
  local streak, streaker = cnt and nba.read16(cnt) or 0, who and signed(nba.read16(who)) or -1
  for k = 0, 3 do
    local a2, m2, a3, m3 = stat(k, 0), stat(k, 1), stat(k, 2), stat(k, 3)
    local fa, fm = a2 + a3, m2 + m3
    local pct = fa > 0 and string.format("%3d%%", (fm * 100 + fa // 2) // fa) or "  -%"
    local p = nba.read32(procs + 32 * k)
    local d = p ~= 0 and nba.read32(p + PDATA_P) or 0
    local bar = ""
    if d ~= 0 then
      local t = math.min(nba.read16(d + TURBO), FULL)
      local n = (t * 8 + FULL // 2) // FULL
      bar = "[" .. string.rep("#", n) .. string.rep(" ", 8 - n) .. "]"
    end
    local f = fire == k and "ON FIRE" or (streaker == k and streak > 0 and ("FIRE " .. streak) or "")
    local line = string.format("P%d FG %d/%d %s 3P %d/%d %s %s", k + 1, fm, fa, pct, m3, a3, bar, f)
    nba.text(6, y0 + k * H, line, k < 2 and 0xffffff or 0xffff80)
  end
end
