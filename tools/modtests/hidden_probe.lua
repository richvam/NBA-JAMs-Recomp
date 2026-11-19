-- hidden_probe.lua - loaded next to unlock_players.lua by unlock_players.expect (roadmap 4.2):
-- 2 s into the game, each player's special_heads entry, attribute table pointer (PDATA+0x50)
-- and head image, and for player 1 which row of spechds_t its attributes are, when the game
-- has that symbol ("row n", from the row size given for the game below).
mod = { name = "Hidden probe" }
local SIZE = { nbajamte = 0x170, nbajamse = 0x170, nbajam = 0x140, nbahangt = 0x1F0, nbamht = 0x1F0 }
local gs, procs, sh, sp, t3
function on_load() gs = nba.sym("GAMSTATE"); procs = nba.sym("plyrproc_t"); sh = nba.sym("special_heads"); sp = nba.sym("spechds_t") end
function on_frame(f)
  if nba.read16(gs) == 3 and not t3 then t3 = f end
  if t3 and f == t3 + 120 then
    local out = {}
    for k = 0, 3 do
      local p = nba.read32(procs + 32 * k)
      local a = p ~= 0 and nba.read32(p + 0x150) or 0
      local h = p ~= 0 and nba.read32(p + 0x4B0) or 0
      out[#out + 1] = string.format("p%d sh=%d %08X/%08X", k + 1, sh and nba.read16(sh + 16 * k) or -2, a, h ~= 0 and nba.read32(h + 0x1C0) or 0)
    end
    local p1 = nba.read32(procs)
    local row = ""
    if sp and SIZE[nba.game] and p1 ~= 0 then
      local d = nba.read32(p1 + 0x150) - sp
      row = (d >= 0 and d % SIZE[nba.game] == 0) and (" p1 row " .. d // SIZE[nba.game]) or " p1 not a secret row"
    end
    nba.log("hidden: " .. table.concat(out, " ") .. row)
  end
end
