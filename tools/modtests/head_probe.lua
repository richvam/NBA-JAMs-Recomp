-- head_probe.lua - roadmap 3.1 check: each player's head object image pointer (ODATA_p of the
-- object at plyr_headobj_p, offset 0x4B0 of the player process), 2 s into the game. A big
-- head is a different image pointer, the one the cabinet code gives.
mod = { name = "Head probe" }
local procs, gs, t
function on_load() procs = nba.sym("plyrproc_t"); gs = nba.sym("GAMSTATE") end
function on_frame(f)
  if not procs then return end
  if nba.read16(gs) == 3 and not t then t = f end
  if t and f == t + 120 then
    local out = {}
    for k = 0, 3 do
      local p = nba.read32(procs + 32 * k)
      local h = p ~= 0 and nba.read32(p + 0x4B0) or 0
      local d = h ~= 0 and nba.read32(h + 0x1C0) or 0
      out[#out + 1] = string.format("p%d %08X", k + 1, d)
    end
    nba.log("heads: " .. table.concat(out, " "))
  end
end
