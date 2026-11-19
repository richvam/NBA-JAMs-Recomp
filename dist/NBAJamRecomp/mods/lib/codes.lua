-- lib/codes.lua - turns a cabinet-code table (lib/codes_<game>.lua) into mod options and
-- forces the codes that are switched on (roadmap 3.1, 3.2).
--
--   local codes = nba.include("lib/codes.lua")
--   local T = codes.table("nbajamte")              -- lib/codes_nbajamte.lua
--   mod = { name = "...", games = { ... }, options = codes.options(T, { "tournament", "big_head" }) }
--   local drive
--   function on_load() drive = codes.driver(T) end
--   function on_frame() drive.update() end
--
-- options(): the option id is the code id and its label the English label.
--   flag             a toggle
--   players, taps    a players option (a tick per player)
--   team             a toggle (both teams)
--   number           a toggle (Maximum Hangtime: forced as 0xF, what a code entered by every
--                    player that has to enter it leaves)
--   "secret_players" (not a code id) one list option per player, secret_p1 to secret_p4: none,
--                    or one of the secret players of codes.secret_list(T, ...) (by name); pass
--                    the list as extra.secret_list when it should cover several tables
-- Conflicts are the table's, plus `extra` = { id = { other ids } }: an option is greyed while
-- one of the ids it lists is on.
--
-- How a code is forced: the game sets these variables itself and clears them too (the tables
-- say when). The driver writes the forced value on every frame of the code's window, so the
-- game reads it at whatever moment it looks, and writes nothing outside it:
--   game      from the start of the matchup screen until a little after tip-off (most codes)
--   select    while a team select screen is up (expanded roster)
--   halftime  while the halftime team select screen is up (team switch)
-- A code that is off is never written, so with every option off the game runs as unmodded.
local codes = {}

function codes.table(game)
  return nba.include("lib/codes_" .. game .. ".lua")
end

local function find(T, id)
  for _, c in ipairs(T) do if c.id == id then return c end end
end

local function initials(T)
  local out = {}
  for _, c in ipairs(T) do if c.kind == "initials" then out[#out + 1] = c end end
  return out
end

-- the secret players of several tables (T.E. and SE differ), each id once, by name: option
-- value k is the k-th of this list, so the same value means the same player on every game
function codes.secret_list(...)
  local seen, out = {}, {}
  for _, T in ipairs({ ... }) do
    for _, c in ipairs(initials(T)) do if not seen[c.id] then seen[c.id] = true; out[#out + 1] = c end end
  end
  table.sort(out, function(a, b) return a.label.en < b.label.en end)
  return out
end

local function merge(a, b)
  if not a and not b then return nil end
  local t = {}
  for _, v in ipairs(a or {}) do t[#t + 1] = v end
  for _, v in ipairs(b or {}) do t[#t + 1] = v end
  return t
end

function codes.options(T, ids, extra)
  extra = extra or {}
  local out = {}
  for _, id in ipairs(ids) do
    if id == "secret_players" then
      local list = extra.secret_list or codes.secret_list(T)
      local values, labels = { 0 }, { "None" }
      for k, c in ipairs(list) do values[k + 1] = k; labels[k + 1] = c.label.en end
      for p = 1, 4 do
        out[#out + 1] = { id = "secret_p" .. p, label = "Secret player, player " .. p, values = values,
          labels = labels, default = 0, conflicts = extra.secret_players }
      end
    else
      local c = assert(find(T, id), "no code " .. id)
      local o = { id = c.id, label = c.label.en, conflicts = merge(c.conflicts, extra[id]) }
      if c.kind == "players" or c.kind == "taps" then o.type = "players"; o.default = 0
      elseif c.kind == "flag" or c.kind == "team" or c.kind == "number" then o.type = "toggle"; o.default = false
      else error("codes.options: kind " .. c.kind .. " is not supported (" .. id .. ")") end
      out[#out + 1] = o
    end
  end
  return out
end

-- frames after tip-off during which the "game" window is still written
codes.HOLD = 60

-- The team select variables are local to SELECT.ASM, so the symbol files do not have them.
-- They follow special_heads in that file's .bss (SELECT.ASM: special_heads 4 words, then
-- bigroster1, bigroster2, tm1taps, tm2taps, dbnce1, dbnce2, tm1swrl, tm2swrl, toggle1_time,
-- toggle2_time, switch_1, switch_2, team1_switch, team2_switch); addresses are in bits.
codes.LOCAL = {
  bigroster1 = { "special_heads", 0x40 },
  team1_switch = { "special_heads", 0x100 },
}
function codes.sym(name)
  local l = codes.LOCAL[name]
  if l then local b = nba.sym(l[1]); return b and b + l[2] end
  return nba.sym(name)
end

local function value_of(v) if v == true then return 1 elseif v == false or v == nil then return 0 end return v end

-- the writes of code c with option value v: { addr, value or function, window }
local function writes(c, v, out, slots)
  local addr = codes.sym(c.sym)
  if not addr then return end
  local win = (c.when == "team_select" and "select") or (c.when == "halftime" and "halftime") or "game"
  if c.kind == "flag" then
    out[#out + 1] = { addr = addr, value = 1, win = win }
  elseif c.kind == "number" then
    out[#out + 1] = { addr = addr, value = 0xF, win = win }
  elseif c.kind == "players" then
    out[#out + 1] = { addr = addr, value = slots and function() return slots(v) end or v, win = win }
    if c.taps then
      local taps = nba.sym("p1taps")
      for k = 0, 3 do
        if v & (1 << k) ~= 0 then out[#out + 1] = { addr = taps + 16 * k, value = c.taps, win = win } end
      end
    end
  elseif c.kind == "taps" then
    for k = 0, 3 do
      if v & (1 << k) ~= 0 then out[#out + 1] = { addr = addr + 16 * k, value = c.value, win = win } end
    end
  elseif c.sym == "bigroster1" then
    -- the roster of the team each side is on: bigroster<n> = team<n>, once a team is shown
    for n = 0, 1 do
      local team = nba.sym("team" .. (n + 1))
      out[#out + 1] = { addr = addr + 16 * n, win = win, value = function()
        local t = nba.read16(team); if t ~= 0xFFFF then return t end end }
    end
  elseif c.sym == "team1_switch" then
    -- what the code leaves there: the frames pass and right were held (120)
    for n = 0, 1 do out[#out + 1] = { addr = addr + 16 * n, value = 120, win = win } end
  end
end

-- list: the secret player list the options were made with (codes.secret_list)
-- slots: function(mask) -> mask, from the option's cabinet players to the game's player bits,
--        read when written (Maximum Hangtime: they depend on the cabinet's player count)
-- holds: { id = frames } for a code the game checks later than codes.HOLD frames after
--        tip-off (NBA JAM 1993 clears assistance_off at its own check, 335 frames in)
function codes.driver(T, list, slots, holds)
  local d, on = {}, {}
  for _, c in ipairs(T) do
    if c.kind ~= "initials" then
      local ok, v = pcall(nba.option, c.id)
      v = ok and value_of(v) or 0
      if v ~= 0 then
        local w = {}
        writes(c, v, w, slots)
        local h = holds and holds[c.id]
        if h then for _, x in ipairs(w) do x.hold = h end end
        if #w > 0 then on[#on + 1] = { c = c, value = v, w = w } end
      end
    end
  end
  -- secret players: special_heads[player] = the code's value
  -- (option value k is list[k]; the code of that id in this game's table, if it has it)
  local list, sh = list or codes.secret_list(T), nba.sym("special_heads")
  for p = 1, 4 do
    local ok, v = pcall(nba.option, "secret_p" .. p)
    local c = ok and v and list[v] and find(T, list[v].id)
    if ok and v and list[v] and not c then nba.log("secret player " .. list[v].id .. " is not in this game") end
    if c and sh then
      on[#on + 1] = { c = c, value = c.value, player = p,
        w = { { addr = sh + 16 * (p - 1), value = c.value, win = "game" } } }
    end
  end
  d.on = on
  local inm, gs, sel, qtr, INGAME = nba.sym("inmatchup"), nba.sym("GAMSTATE"),
    nba.sym("in_team_select"), nba.sym("gmqrtr"), 3
  local phase, t = "wait", 0
  local longest = codes.HOLD
  for _, e in ipairs(on) do for _, w in ipairs(e.w) do if w.hold and w.hold > longest then longest = w.hold end end end
  local function put(win, t)
    for _, e in ipairs(on) do
      for _, w in ipairs(e.w) do
        if w.win == win and (not t or t <= (w.hold or codes.HOLD)) then
          local v = w.value
          if type(v) == "function" then v = v() end
          if v then nba.write16(w.addr, v) end
        end
      end
    end
  end
  function d.update()
    if #on == 0 or not inm or not gs then return end
    -- team select windows
    if sel and nba.read16(sel) ~= 0 then
      put("select")
      if qtr and nba.read16(qtr) ~= 0 then put("halftime") end
    end
    -- the game window: matchup screen to a little after tip-off
    if phase == "wait" or phase == "done" then
      if nba.read16(inm) ~= 0 then phase = "write"; t = 0 end
    end
    if phase == "write" then
      put("game", t)
      if nba.read16(gs) == INGAME and nba.read16(inm) == 0 then
        t = t + 1
        if t > longest then phase = "done" end
      end
    end
  end
  return d
end

return codes
