-- fantasy_draft.lua - a draft of players in place of picking a team's pair (roadmap 6.F2).
-- T.E. 4.0 and SE 5.02. Offline only for now.
--
-- On the first team select of a game the players who have joined draft players from every
-- team, on the overlay, with their own controls: up and down move (left and right a page),
-- Shoot picks. The order is a snake (1 2 3 4, then 4 3 2 1). Each player picks for himself
-- and, if his teammate has not joined, for the teammate's place too; a side with nobody on it
-- keeps its team's own players. Each pick has a timer; when it runs out the player under the
-- cursor is taken (or the first one left). While the draft is on the game does not see the
-- players' buttons (nba.hold), and the team select countdown is off (as no_select_timer.lua).
-- After the draft the players still choose the team they play as (its name, court and
-- logo); the script holds that team's first pair and makes it the drafted players by
-- changing the game's name table player_names in the ROM (roadmap 6.F1: a player's ratings,
-- head, uniform and name calls all follow that entry). The table is put back after the
-- game. A drafted game is kept out of the saved records ("Protect records").
local records = nba.include("lib/records.lua")
local POOL = nba.include("lib/draft_pool.lua")

mod = {
  name = "Fantasy draft",
  description = "Draft players from every team before the game, in snake order, with a pick timer.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "timer", label = "Time for a pick", values = { 10, 20, 30, 60 }, labels = { "10 seconds", "20 seconds", "30 seconds", "60 seconds" }, default = 20 },
    records.OPTION,
  },
}

-- attribute table fields (PLYR.EQU), bits from the start of a player's table
-- (shooting is kept as about 1000 to 1150: shown divided by 10)
local ATTR = { { "SHT", 0x30, 10 }, { "DNK", 0x40 }, { "BLK", 0x50 }, { "STL", 0x60 }, { "PAS", 0x140 }, { "POW", 0x150 } }
local PAT_SIZE = 368
local ROWS = 12

local gs, sel, q, pst, team, set, names, sort, atl, guard
local state = "idle"   -- idle, wait, draft, done
local wait, drafters, owner, order, turn, picks, taken, cursor, timer, prev, rep
local patched = { {}, {} }   -- per side: ROM address -> original value
local held_team = {}

-- the team select countdown (no_select_timer.lua): its CREATE call is pointed at SUCIDE
local function countdown_off()
  local W = 16
  local best, ticks
  for _, a in ipairs(nba.rom_find({ 0x09CA, -1, 0x09C1, -1, 0x09E7, -1, -1, 0x0D5F })) do
    local t = nba.rom_read16(a + W)
    if t >= 20 * 53 and (not ticks or t > ticks) then best, ticks = a, t end
  end
  if not best then return false end
  local entry = nba.rom_read32(best + 5 * W)
  if nba.rom_read16(entry) ~= 0x5600 then return false end
  local fs = nba.rom_read32(entry + 2 * W)
  local tail = nba.rom_find({ 0x1820, 0x0580, fs & 0xffff, fs >> 16, 0x0D5F, -1, -1, 0xC080 }, entry, 1)
  if #tail == 0 then return false end
  nba.rom_write32(best + 5 * W, nba.rom_read32(tail[1] + 8 * W))
  return true
end

function on_load()
  gs, sel, q, pst = nba.sym("GAMSTATE"), nba.sym("in_team_select"), nba.sym("gmqrtr"), nba.sym("PSTATUS")
  team = { nba.sym("team1"), nba.sym("team2") }
  set = { nba.sym("tm1set"), nba.sym("tm2set") }
  names, sort, atl = nba.sym("player_names"), nba.sym("name_sort"), nba.sym("ATL_t")
  guard = records.guard()
  if nba.netplay then gs = nil; nba.log("fantasy draft: not in an online game yet"); return end
  if not (gs and sel and q and pst and team[2] and set[2] and names and sort and atl) then
    gs = nil; nba.log("fantasy draft: not available on this game"); return
  end
  -- the pool must be the game's own name_sort, in order
  for _, p in ipairs(POOL) do
    local ptr = nba.rom_read32(sort + 32 * p.place)
    for i = 0, p.place - 1 do
      if nba.rom_read32(sort + 32 * i) == ptr then gs = nil end
    end
    if ptr < nba.ROM_BASE then gs = nil end
  end
  if not gs then nba.log("fantasy draft: the player list does not match this game"); return end
  if not countdown_off() then nba.log("fantasy draft: team select countdown not found; it is kept") end
end

local function s16(v) return v >= 0x8000 and v - 0x10000 or v end

local function rating(place, off) return s16(nba.read16(atl + place * PAT_SIZE + off)) end

local function entry(t, slot) return names + (t * 20) * 64 + slot * 32 end

local function patch(s, a, v)
  if patched[s][a] == nil then patched[s][a] = nba.rom_read32(a) end
  nba.rom_write32(a, v)
end

local function restore(s)
  for a, v in pairs(patched[s]) do nba.rom_write32(a, v) end
  patched[s] = {}
end

-- the players in the game make the draft order and own the places on court
local function begin()
  local p = nba.read16(pst)
  drafters, owner = {}, {}
  for k = 0, 3 do if (p >> k) & 1 == 1 then drafters[#drafters + 1] = k + 1 end end
  for k = 1, 4 do
    local mate = k % 2 == 1 and k + 1 or k - 1
    if (p >> (k - 1)) & 1 == 1 then owner[k] = k elseif (p >> (mate - 1)) & 1 == 1 then owner[k] = mate end
  end
  order = {}
  local need = {}
  for k = 1, 4 do if owner[k] then need[owner[k]] = (need[owner[k]] or 0) + 1 end end
  for r = 1, 2 do
    local list = {}
    for _, d in ipairs(drafters) do list[#list + 1] = d end
    if r == 2 then for i = 1, #list // 2 do list[i], list[#list + 1 - i] = list[#list + 1 - i], list[i] end end
    for _, d in ipairs(list) do if need[d] >= r then order[#order + 1] = d end end
  end
  picks, taken, cursor, prev, rep = {}, {}, {}, {}, {}
  for _, d in ipairs(drafters) do cursor[d], prev[d], rep[d] = 0, {}, 0 end
  turn, timer = 1, nba.option("timer") * 60
  state = #order > 0 and "draft" or "done"
  nba.log(string.format("fantasy draft: %d drafters, %d picks", #drafters, #order))
end

local function free_from(c)
  for i = 0, #POOL - 1 do
    local k = (c + i) % #POOL
    if not taken[k] then return k end
  end
end

local function pick(d, place)
  taken[place] = d
  -- the first of the drafter's own places still open
  local slot
  for _, k in ipairs({ d, d % 2 == 1 and d + 1 or d - 1 }) do
    if owner[k] == d and not picks[k] then slot = k; break end
  end
  picks[slot] = place
  nba.log(string.format("fantasy draft: pick %d, player %d takes %s for P%d", turn, d, POOL[place + 1].name, slot))
  turn, timer = turn + 1, nba.option("timer") * 60
  if turn > #order then
    state = "done"
    local s = {}
    for k = 1, 4 do s[#s + 1] = picks[k] and string.format("P%d %s", k, POOL[picks[k] + 1].name) or string.format("P%d team", k) end
    nba.log("fantasy draft: done, " .. table.concat(s, ", "))
  end
end

local function step(d)
  local i = nba.input(d)
  local p = prev[d]
  local function down(b) return i[b] and not p[b] end
  local n = #POOL
  local held = (i.up or i.down) and rep[d] + 1 or 0
  rep[d] = held
  local auto = held > 20 and held % 4 == 0
  if down("up") or (auto and i.up) then cursor[d] = (cursor[d] - 1) % n end
  if down("down") or (auto and i.down) then cursor[d] = (cursor[d] + 1) % n end
  if down("left") then cursor[d] = (cursor[d] - ROWS) % n end
  if down("right") then cursor[d] = (cursor[d] + ROWS) % n end
  prev[d] = i
  if down("shoot") and not taken[cursor[d]] then pick(d, cursor[d]) end
end

local function draw()
  local d = order[turn]
  local c = cursor[d]
  nba.rect(4, 18, 392, 218, 0xe0000000, true)
  nba.text(8, 22, string.format("FANTASY DRAFT  PICK %d OF %d  PLAYER %d  %d", turn, #order, d, (timer + 59) // 60), 0xffff80)
  local head = string.format("%-17s", "PLAYER")
  for _, a in ipairs(ATTR) do head = head .. string.format(" %3s", a[1]) end
  nba.text(8, 36, head, 0x80c0ff)
  local top = math.max(0, math.min(c - ROWS // 2, #POOL - ROWS))
  for r = 0, ROWS - 1 do
    local k = top + r
    local line = string.format("%-17s", POOL[k + 1].name:sub(1, 17))
    for _, a in ipairs(ATTR) do line = line .. string.format(" %3d", rating(k, a[2]) // (a[3] or 1)) end
    if taken[k] then line = line .. " P" .. taken[k] end
    local y = 48 + r * 10
    if k == c then nba.rect(6, y - 1, 388, 10, 0x80406080, true) end
    nba.text(8, y, line, taken[k] and 0x707070 or (k == c and 0xffffff or 0xc0c0c0))
  end
  local s = {}
  for k = 1, 4 do if picks[k] then s[#s + 1] = string.format("P%d %s", k, POOL[picks[k] + 1].label) end end
  nba.text(8, 196, table.concat(s, "  "), 0x80ff80)
  nba.text(8, 206, "UP/DOWN MOVE  LEFT/RIGHT PAGE  SHOOT PICK", 0x808080)
end

-- after the draft: the team each side plays as gets its first pair made the drafted players
local function hold_pairs()
  for s = 1, 2 do
    local t = nba.read16(team[s])
    local a, b = 2 * s - 1, 2 * s
    if picks[a] or picks[b] then
      if held_team[s] ~= t then restore(s) end
      held_team[s] = t
      nba.write16(set[s] + 16 * t, 0)
      for j, k in ipairs({ a, b }) do
        if picks[k] then patch(s, entry(t, j - 1), nba.rom_read32(sort + 32 * picks[k])) end
      end
      guard.mark()
    end
  end
end

function on_frame()
  guard.update()
  if not gs then return end
  local g = nba.read16(gs)
  if g == 0 or g == 1 then
    if state ~= "idle" then restore(2); restore(1); held_team = {}; state = "idle" end
    return
  end
  local selecting = nba.read16(sel) ~= 0 and nba.read16(q) == 0
  if state == "idle" and selecting then state, wait = "wait", 0 end
  if state == "wait" then
    -- a moment for the players to join first
    wait = wait + 1
    if wait >= 120 then begin() end
  end
  if state == "draft" then
    for k = 1, 4 do nba.hold(k, 2) end
    local d = order[turn]
    step(d)
    if state == "draft" then
      timer = timer - 1
      if timer <= 0 then pick(d, free_from(cursor[d])) end
    end
    if state == "draft" then draw() end
  end
  if state == "done" then
    if selecting or g == 2 or g == 3 then hold_pairs() end
  end
end
