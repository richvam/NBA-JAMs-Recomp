-- codes_te.lua - the cabinet codes of NBA JAM Tournament Edition and T.E. Special Edition as
-- switches (roadmap 3.1). Each switch forces what the code would set; the tables are in
-- lib/codes_nbajamte.lua and lib/codes_nbajamse.lua (the same codes).
local codes = nba.include("lib/codes.lua")
local T = codes.table("nbajamte")

mod = {
  name = "Cabinet codes (T.E.)",
  description = "Turns on T.E. cabinet codes without entering them.",
  games = { "nbajamte", "nbajamse" },
  options = codes.options(T, { "tournament", "big_head", "max_power" }),
}

local drive
function on_load()
  drive = codes.driver(codes.table(nba.game))
  for _, w in ipairs(drive.on) do nba.log(string.format("forcing %s (%s = %d)", w.c.id, w.c.sym, w.value)) end
end
function on_frame() drive.update() end
