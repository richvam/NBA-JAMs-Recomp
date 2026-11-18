-- p1_include.lua - Phase 1 review, task 1.1: nba.include loads a library from mods/lib/ once
-- per script and refuses anything else.
mod = { name = "P1 include" }
local jam = nba.include("lib/jam.lua")

function on_load()
  nba.log("include lib/jam.lua: " .. type(jam) .. (type(jam) == "table" and jam.update and " with update" or ""))
  nba.log("included twice, same table: " .. tostring(nba.include("lib/jam.lua") == jam))
  for _, p in ipairs({ "../mods.ini", "lib/../p1_include.lua", "lib\\jam.lua", "lib//jam.lua", "sym/nbajamte_70646d2c.lua", "lib/jam.txt", "lib/missing.lua" }) do
    local ok = pcall(nba.include, p)
    nba.log("include " .. p .. ": " .. (ok and "ALLOWED" or "refused"))
  end
end
