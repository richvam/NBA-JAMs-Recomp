-- lib/codes.lua - turns a cabinet-code table (lib/codes_<game>.lua) into mod options and
-- forces the codes that are switched on (roadmap 3.1).
--
--   local codes = nba.include("lib/codes.lua")
--   local T = codes.table("nbajamte")              -- lib/codes_nbajamte.lua
--   mod = { name = "...", games = { ... }, options = codes.options(T, { "tournament", "big_head" }) }
--   local drive
--   function on_load() drive = codes.driver(T) end
--   function on_frame() drive.update() end
--
-- options(): a "flag" code becomes a toggle; a "players" code becomes a players option (a tick
-- per player). The option id is the code id; its label is the English label; conflicts are
-- the table's.
--
-- How a code is forced: the game sets these variables itself on the matchup screen and at
-- tip-off, and clears them there too (the tables say when). The driver writes the forced value
-- on every frame from the start of the matchup screen until a little after tip-off, so the
-- game reads it at whatever moment it looks, and then stops: nothing is written during play.
-- A code that is off is never written, so with every option off the game runs as unmodded.
local codes = {}

function codes.table(game)
  return nba.include("lib/codes_" .. game .. ".lua")
end

local function find(T, id)
  for _, c in ipairs(T) do if c.id == id then return c end end
end

function codes.options(T, ids)
  local out = {}
  for _, id in ipairs(ids) do
    local c = assert(find(T, id), "no code " .. id)
    local o = { id = c.id, label = c.label.en, conflicts = c.conflicts }
    if c.kind == "players" then o.type = "players"; o.default = 0
    elseif c.kind == "flag" then o.type = "toggle"; o.default = false
    else error("codes.options: kind " .. c.kind .. " is not supported yet (" .. id .. ")") end
    out[#out + 1] = o
  end
  return out
end

-- frames after tip-off during which the forced values are still written
codes.HOLD = 60

function codes.driver(T)
  local d, on = {}, {}
  local inm, gs, INGAME = nba.sym("inmatchup"), nba.sym("GAMSTATE"), 3
  for _, c in ipairs(T) do
    local ok, v = pcall(nba.option, c.id)
    if ok and v ~= nil then
      local value = (c.kind == "players") and v or (v == true and 1 or 0)
      local addr = nba.sym(c.sym)
      if value ~= 0 and addr then on[#on + 1] = { c = c, addr = addr, value = value } end
    end
  end
  d.on = on
  local phase, t = "wait", 0
  function d.update()
    if #on == 0 or not inm or not gs then return end
    if phase == "wait" then
      if nba.read16(inm) ~= 0 then phase = "write" end
    end
    if phase == "write" then
      for _, w in ipairs(on) do nba.write16(w.addr, w.value) end
      if nba.read16(gs) == INGAME and nba.read16(inm) == 0 then
        t = t + 1
        if t > codes.HOLD then phase = "done" end
      end
    elseif phase == "done" then
      -- the next game: wait for its matchup screen
      if nba.read16(inm) ~= 0 then phase = "write"; t = 0 end
    end
  end
  return d
end

return codes
