-- p1_sym.lua - Phase 1 review, task 1.2: nba.sym gives the .map address for the running
-- revision, or nil (unknown names, and games without a symbol file).
mod = { name = "P1 symbols" }
function on_load()
  for _, n in ipairs({ "scores", "GAMSTATE", "game_time", "no_such_symbol_x" }) do
    local a = nba.sym(n)
    nba.log(n .. " " .. (a and string.format("0x%08X", a) or "nil"))
  end
end
