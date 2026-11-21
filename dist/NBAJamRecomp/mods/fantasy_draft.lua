-- fantasy_draft.lua - a draft of players in place of picking a team's pair (roadmap 6.F2, 6.F3).
-- T.E. 4.0 and SE 5.02. Offline only for now.
--
-- On the first team select of a game the players who have joined draft players, on the
-- overlay, with their own controls: up and down move (left and right a page), Shoot picks.
-- The order is a snake (1 2 3 4, then 4 3 2 1). Each player picks for himself and, if his
-- teammate has not joined, for the teammate's place too; a side with nobody on it keeps its
-- team's own players. Each pick has a timer; when it runs out the player under the cursor is
-- taken (or the next one that can be). While the draft is on the game does not see the
-- players' buttons (nba.hold), and the team select countdown is off (as no_select_timer.lua).
-- After the draft the players still choose the team they play as (its name, court and logo).
-- A drafted player takes the court through the game's name table player_names in the ROM: the
-- chosen team's first pair is made the drafted players (roadmap 6.F1: a player's ratings,
-- head, uniform and name calls all follow that entry); a hidden player through
-- special_heads, as unlock_players.lua does. The table is put back after the game. A drafted
-- game is kept out of the saved records ("Protect records").
-- Options (6.F3):
--   Order       Fixed (player 1 first), Random, or the side that lost the last game first
--               (saved in stats/<game>_draft_loser.txt at each end of game).
--   Pool        Team rosters: the players of the teams' six pairs. All players: every player
--               of the game's player list (lib/draft_pool.lua), with the ones the expanded
--               rosters add. Everyone: and the hidden players (lib/hidden.lua).
--   Same player on both sides: a player taken by one side can still be taken by the other.
--   Draft       New draft, or Replay the last: the picks saved after the last draft
--               (stats/<game>_draft_last.txt) are used again without drafting.
-- Every draft is added to stats/<game>_drafts.jsonl.
-- Ladder (6.F5): the drafted pair takes on the computer's teams one game after another, as a
-- player with initials does in the stock game. The game keeps that run in the player's record:
-- PR_TEAMSDEF, one bit for each team beaten (RECORD.ASM), and picks the computer's opponent as
-- the first team of team_orders (SELECT2.ASM, from the weakest) not beaten yet (select_teams,
-- get_opponent_team, run just after team select); with all 27 beaten it gives a random team
-- with a hidden "superstar". Here the run belongs to the drafted pair, not to initials: the
-- script keeps the pair and its teams beaten in stats/<game>_ladder_<slot>.txt, and from the end
-- of the draft to the matchup screen writes that mask into the side's records (and initials 0 for
-- a player who has none, which select_teams needs), so the game itself picks the opponent. A
-- win adds the opponent's bit. The record is never saved ("Protect records" is forced on: the
-- guard clears the initials before the game saves). "Computer drafts too": the computer's
-- side takes the two best players left (sum of the six ratings) for each game. A new ladder
-- starts with a draft when the slot has no saved one.
-- Ladder slots (6.F6): four slots, each its own pair and teams beaten; choose the slot in the
-- mods menu ("Ladder slot") and the ladder goes on where that slot left off, also after the
-- program was closed. "Ladder: Start over" drafts a new pair into the slot (its old ladder is
-- replaced when the draft is done).
local records = nba.include("lib/records.lua")
local hidden = nba.include("lib/hidden.lua")
local PLAYERS = nba.include("lib/draft_pool.lua")

mod = {
  name = "Fantasy draft",
  description = "Draft players from every team before the game, in snake order, with a pick timer.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "timer", label = "Time for a pick", values = { 10, 20, 30, 60 }, labels = { "10 seconds", "20 seconds", "30 seconds", "60 seconds" }, default = 20 },
    { id = "order", label = "Order", values = { 0, 1, 2 }, labels = { "Fixed", "Random", "Loser first" }, default = 0 },
    { id = "pool", label = "Pool", values = { 0, 1, 2 }, labels = { "Team rosters", "All players", "Everyone" }, default = 1 },
    { id = "twice", label = "Same player on both sides", type = "toggle", default = false },
    { id = "draft", label = "Draft", values = { 0, 1 }, labels = { "New draft", "Replay the last" }, default = 0 },
    { id = "mode", label = "Mode", values = { 0, 1 }, labels = { "One game", "Ladder" }, default = 0 },
    { id = "cpu_draft", label = "Computer drafts too", type = "toggle", default = false },
    { id = "slot", label = "Ladder slot", values = { 1, 2, 3, 4 }, labels = { "Slot 1", "Slot 2", "Slot 3", "Slot 4" }, default = 1 },
    { id = "restart", label = "Ladder", values = { 0, 1 }, labels = { "Continue", "Start over" }, default = 0 },
    records.OPTION,
  },
}

-- attribute table fields (PLYR.EQU), bits from the start of a player's table
-- (shooting is kept as about 1000 to 1150: shown divided by 10)
local ATTR = { { "SHT", 0x30, 10 }, { "DNK", 0x40 }, { "BLK", 0x50 }, { "STL", 0x60 }, { "PAS", 0x140 }, { "POW", 0x150 } }
local PAT_SIZE = 368
local ROWS = 12
local HOLD = 60     -- frames after tip-off that special_heads is still written

local gs, sel, q, pst, team, set, names, sort, atl, spec, sh, inm, sc, guard
local POOL = {}      -- { kind = "r" (player list place) or "h" (hidden row), id, name, label }
local state = "idle" -- idle, wait, draft, done
local wait, drafters, owner, order, turn, picks, taken, cursor, timer, prev, rep
local patched = { {}, {} }   -- per side: ROM address -> original value
local held_team = {}
local after_tip, played = 0, false
local TEAMS = { "Atlanta", "Boston", "Charlotte", "Chicago", "Cleveland", "Dallas", "Denver",
  "Detroit", "Golden State", "Houston", "Indiana", "L.A. Clippers", "L.A. Lakers", "Miami",
  "Milwaukee", "Minnesota", "New Jersey", "New York", "Orlando", "Philadelphia", "Phoenix",
  "Portland", "Sacramento", "San Antonio", "Seattle", "Utah", "Washington" }
local pdata, orders           -- player<n>_data; team_orders (27 words) in the ROM
local ladder                  -- { side = 1 or 2, mask = teams beaten, games = n } in ladder mode
local opp_logged = false
local ALL = 0x7FFFFFF
local PR_INIT1, PR_TEAMSDEF = 0x40, 0x160

-- the team select countdown (no_select_timer.lua): its CREATE call is pointed at SUCIDE, so
-- the countdown ends as it starts. Halftime starts the same 30-second countdown (SELECT.ASM
-- subs_in), so the change is made only until the draft is over: set in attract mode and up to
-- the draft's end, put back after it (the team select already on stays without a clock).
local countdowns          -- { { addr, original, sucide } } of the longest select countdowns
local function find_countdowns()
  local W = 16
  local found, ticks = {}, 0
  for _, a in ipairs(nba.rom_find({ 0x09CA, -1, 0x09C1, -1, 0x09E7, -1, -1, 0x0D5F })) do
    local t = nba.rom_read16(a + W)
    local entry = nba.rom_read32(a + 5 * W)
    if t >= 20 * 53 and nba.rom_read16(entry) == 0x5600 then
      local fs = nba.rom_read32(entry + 2 * W)
      local tail = nba.rom_find({ 0x1820, 0x0580, fs & 0xffff, fs >> 16, 0x0D5F, -1, -1, 0xC080 }, entry, 1)
      if #tail > 0 then
        if t > ticks then found, ticks = {}, t end
        if t == ticks then found[#found + 1] = { a + 5 * W, entry, nba.rom_read32(tail[1] + 8 * W) } end
      end
    end
  end
  if #found > 0 then countdowns = found end
end
local countdown_is_off = false
local function countdown(off)
  if not countdowns or countdown_is_off == off then return end
  for _, c in ipairs(countdowns) do nba.rom_write32(c[1], off and c[3] or c[2]) end
  countdown_is_off = off
end

local function entry(t, slot) return names + (t * 20) * 64 + slot * 32 end

-- the pool for the Pool option
local function make_pool()
  local pool = nba.option("pool")
  local in_pairs = {}
  if pool == 0 then
    for t = 0, 26 do
      for p = 0, 5 do
        for s = 0, 1 do in_pairs[nba.rom_read32(names + (t * 20 + p) * 64 + s * 32)] = true end
      end
    end
  end
  for _, p in ipairs(PLAYERS) do
    if pool ~= 0 or in_pairs[nba.rom_read32(sort + 32 * p.place)] then
      POOL[#POOL + 1] = { kind = "r", id = p.place, name = p.name, label = p.label:gsub("_", " ") }
    end
  end
  if pool == 2 and spec and sh then
    for n, name in ipairs(hidden.rows(nba.game)) do
      POOL[#POOL + 1] = { kind = "h", id = n - 1, name = name, label = name:upper() }
    end
  end
end

function on_load()
  gs, sel, q, pst = nba.sym("GAMSTATE"), nba.sym("in_team_select"), nba.sym("gmqrtr"), nba.sym("PSTATUS")
  team = { nba.sym("team1"), nba.sym("team2") }
  set = { nba.sym("tm1set"), nba.sym("tm2set") }
  names, sort, atl = nba.sym("player_names"), nba.sym("name_sort"), nba.sym("ATL_t")
  spec, sh, inm, sc = nba.sym("spechds_t"), nba.sym("special_heads"), nba.sym("inmatchup"), nba.sym("scores")
  guard = records.guard()
  if nba.netplay then gs = nil; nba.log("fantasy draft: not in an online game yet"); return end
  if not (gs and sel and q and pst and team[2] and set[2] and names and sort and atl and inm) then
    gs = nil; nba.log("fantasy draft: not available on this game"); return
  end
  -- the player list must be the game's own name_sort, in order
  for _, p in ipairs(PLAYERS) do
    local ptr = nba.rom_read32(sort + 32 * p.place)
    for i = 0, p.place - 1 do
      if nba.rom_read32(sort + 32 * i) == ptr then gs = nil end
    end
    if ptr < nba.ROM_BASE then gs = nil end
  end
  if not gs then nba.log("fantasy draft: the player list does not match this game"); return end
  make_pool()
  pdata = {}
  for k = 1, 4 do pdata[k] = nba.sym("player" .. k .. "_data") end
  local o = nba.rom_find({ 5, 7, 14, 26, 22, 15 })
  if #o > 0 then
    orders = {}
    for i = 0, 26 do orders[i + 1] = nba.rom_read16(o[1] + 16 * i) end
  end
  nba.log(string.format("fantasy draft: %d players in the pool", #POOL))
  find_countdowns()
  if not countdowns then nba.log("fantasy draft: team select countdown not found; it is kept") end
  countdown(true)
end

local function s16(v) return v >= 0x8000 and v - 0x10000 or v end

local function rating(e, off)
  local base = e.kind == "r" and atl + e.id * PAT_SIZE or spec + e.id * PAT_SIZE
  return s16(nba.read16(base + off))
end

local function patch(s, a, v)
  if patched[s][a] == nil then patched[s][a] = nba.rom_read32(a) end
  nba.rom_write32(a, v)
end

local function restore(s)
  for a, v in pairs(patched[s]) do nba.rom_write32(a, v) end
  patched[s] = {}
end

local function side(k) return k <= 2 and 1 or 2 end

-- can drafter d take pool entry i?
local function can_take(d, i)
  local t = taken[i]
  if not t then return true end
  if not nba.option("twice") then return false end
  for _, who in ipairs(t) do if side(who) == side(d) then return false end end
  return true
end

-- the picks as a line of text: "1:r85 2:h12 3:- 4:-" (r: player list place, h: hidden row)
local function picks_text()
  local s = {}
  for k = 1, 4 do
    local e = picks[k] and POOL[picks[k] + 1]
    s[#s + 1] = k .. ":" .. (e and (e.kind .. e.id) or "-")
  end
  return table.concat(s, " ")
end

local function picks_names()
  local s = {}
  for k = 1, 4 do s[#s + 1] = picks[k] and string.format("P%d %s", k, POOL[picks[k] + 1].name) or string.format("P%d team", k) end
  return table.concat(s, ", ")
end

local function ladder_file() return string.format("%s_ladder_%d.txt", nba.game, nba.option("slot")) end

local function cpu_draft()
  if not nba.option("cpu_draft") then return end
  local has = { false, false }
  for k = 1, 4 do if picks[k] then has[side(k)] = true end end
  local used = {}
  for k = 1, 4 do if picks[k] then used[picks[k]] = true end end
  for s = 1, 2 do
    if not has[s] then
      for _, k in ipairs({ 2 * s - 1, 2 * s }) do
        local best, bv
        for i, e in ipairs(POOL) do
          if not used[i - 1] then
            local v = 0
            for _, a in ipairs(ATTR) do v = v + rating(e, a[2]) // (a[3] or 1) end
            if not bv or v > bv then best, bv = i - 1, v end
          end
        end
        picks[k], used[best] = best, true
        nba.log(string.format("fantasy draft: the computer takes %s for P%d", POOL[best + 1].name, k))
      end
    end
  end
end

local function finish()
  state = "done"
  nba.log("fantasy draft: done, " .. picks_names())
  local text = picks_text()
  nba.save(nba.game .. "_draft_last.txt", text .. "\n")
  nba.save(nba.game .. "_drafts.jsonl", string.format('{"date":"%s","picks":"%s","players":"%s"}\n',
    nba.date() or "", text, picks_names()), { append = true })
  if nba.option("mode") == 1 then
    local sides = {}
    for k = 1, 4 do if picks[k] then sides[side(k)] = true end end
    if sides[1] and sides[2] then
      nba.log("fantasy draft: a ladder is for one side against the computer; this is one game")
    else
      ladder = { side = sides[1] and 1 or 2, mask = 0, games = 0, picks = text }
      nba.save(ladder_file(), string.format("picks %s\nside %d\nmask %d\ngames 0\n", text, ladder.side, 0))
      nba.log(string.format("fantasy draft: a new ladder in slot %d for team %d", nba.option("slot"), ladder.side))
    end
  end
  cpu_draft()
end

-- Replay the last: the saved picks, if every one is in this pool
local function replay(text)
  text = text or nba.load(nba.game .. "_draft_last.txt")
  if not text then nba.log("fantasy draft: no saved draft to replay; drafting"); return false end
  local find = {}
  for i, e in ipairs(POOL) do find[e.kind .. e.id] = i - 1 end
  picks = {}
  for k, v in text:gmatch("(%d):([%w%-]+)") do
    k = tonumber(k)
    if v ~= "-" then
      if not find[v] then nba.log("fantasy draft: the saved draft has a player not in this pool; drafting"); return false end
      picks[k] = find[v]
    end
  end
  nba.log("fantasy draft: replaying the last draft, " .. picks_names())
  state = "done"
  return true
end

-- Ladder: the saved pair and its teams beaten
local function ladder_load()
  if nba.option("restart") == 1 then
    nba.log(string.format("fantasy draft: ladder slot %d starts over", nba.option("slot")))
    return false
  end
  local t = nba.load(ladder_file())
  if not t then return false end
  local p, sd = t:match("picks ([^\n]+)"), tonumber(t:match("side (%d)"))
  local m, g = tonumber(t:match("mask (%d+)")), tonumber(t:match("games (%d+)"))
  if not (p and sd and m) then nba.log("fantasy draft: the saved ladder cannot be read; drafting a new one"); return false end
  if not replay(p) then return false end
  ladder = { side = sd, mask = m, games = g or 0, picks = p }
  local n = 0
  for i = 0, 26 do if (m >> i) & 1 == 1 then n = n + 1 end end
  nba.log(string.format("fantasy draft: ladder slot %d, game %d for team %d, %d of 27 teams beaten", nba.option("slot"), ladder.games + 1, sd, n))
  cpu_draft()
  return true
end

local function ladder_save()
  nba.save(ladder_file(), string.format("picks %s\nside %d\nmask %d\ngames %d\n", ladder.picks, ladder.side, ladder.mask, ladder.games))
end

-- the players in the game make the draft order and own the places on court
local function begin()
  if nba.option("mode") == 1 and ladder_load() then return end
  if nba.option("draft") == 1 and nba.option("mode") == 0 and replay() then
    cpu_draft()
    return
  end
  local p = nba.read16(pst)
  drafters, owner = {}, {}
  for k = 0, 3 do if (p >> k) & 1 == 1 then drafters[#drafters + 1] = k + 1 end end
  for k = 1, 4 do
    local mate = k % 2 == 1 and k + 1 or k - 1
    if (p >> (k - 1)) & 1 == 1 then owner[k] = k elseif (p >> (mate - 1)) & 1 == 1 then owner[k] = mate end
  end
  local o = nba.option("order")
  if o == 1 then
    for i = #drafters, 2, -1 do local j = math.random(i); drafters[i], drafters[j] = drafters[j], drafters[i] end
  elseif o == 2 then
    local l = tonumber((nba.load(nba.game .. "_draft_loser.txt") or ""):match("%d") or "0")
    if l == 1 or l == 2 then
      local first, rest = {}, {}
      for _, d in ipairs(drafters) do if side(d) == l then first[#first + 1] = d else rest[#rest + 1] = d end end
      for _, d in ipairs(rest) do first[#first + 1] = d end
      drafters = first
      nba.log("fantasy draft: team " .. l .. " lost the last game and picks first")
    end
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
  local s = {}
  for _, d in ipairs(order) do s[#s + 1] = tostring(d) end
  nba.log(string.format("fantasy draft: %d drafters, %d picks, order %s", #drafters, #order, table.concat(s, " ")))
  if #order == 0 then state = "done" else state = "draft" end
end

local function free_from(d, c)
  for i = 0, #POOL - 1 do
    local k = (c + i) % #POOL
    if can_take(d, k) then return k end
  end
end

local function pick(d, i)
  taken[i] = taken[i] or {}
  taken[i][#taken[i] + 1] = d
  -- the first of the drafter's own places still open
  local slot
  for _, k in ipairs({ d, d % 2 == 1 and d + 1 or d - 1 }) do
    if owner[k] == d and not picks[k] then slot = k; break end
  end
  picks[slot] = i
  nba.log(string.format("fantasy draft: pick %d, player %d takes %s for P%d", turn, d, POOL[i + 1].name, slot))
  turn, timer = turn + 1, nba.option("timer") * 60
  if turn > #order then finish() end
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
  if down("shoot") and can_take(d, cursor[d]) then pick(d, cursor[d]) end
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
  for r = 0, math.min(ROWS, #POOL) - 1 do
    local k = top + r
    local e = POOL[k + 1]
    local line = string.format("%-17s", e.name:sub(1, 17))
    for _, a in ipairs(ATTR) do line = line .. string.format(" %3d", rating(e, a[2]) // (a[3] or 1)) end
    if taken[k] then for _, who in ipairs(taken[k]) do line = line .. " P" .. who end end
    local y = 48 + r * 10
    if k == c then nba.rect(6, y - 1, 388, 10, 0x80406080, true) end
    local ok = can_take(d, k)
    nba.text(8, y, line, not ok and 0x707070 or (e.kind == "h" and 0xffc080 or (k == c and 0xffffff or 0xc0c0c0)))
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
    -- (-1, read as 65535, while the side has no team yet)
    if t <= 26 and (picks[a] or picks[b]) then
      if held_team[s] ~= t then restore(s) end
      held_team[s] = t
      nba.write16(set[s] + 16 * t, 0)
      for j, k in ipairs({ a, b }) do
        local e = picks[k] and POOL[picks[k] + 1]
        if e and e.kind == "r" then patch(s, entry(t, j - 1), nba.rom_read32(sort + 32 * e.id)) end
      end
      guard.mark()
    end
  end
end

-- hidden players: special_heads from the matchup screen to a second after tip-off
local function hold_hidden(g)
  if not sh then return end
  if nba.read16(inm) ~= 0 then after_tip = 0 end
  if g == 3 then after_tip = after_tip + 1 end
  if after_tip > HOLD then return end
  for k = 1, 4 do
    local e = picks[k] and POOL[picks[k] + 1]
    if e and e.kind == "h" then nba.write16(sh + 16 * (k - 1), e.id) end
  end
end

function on_frame()
  guard.update()
  if not gs then return end
  local g = nba.read16(gs)
  if g == 0 or g == 1 then
    countdown(true)
    if state ~= "idle" then restore(2); restore(1); held_team = {}; state = "idle"; after_tip = 0; ladder = nil; opp_logged = false end
    played = false
    return
  end
  -- the loser of each game, for "Loser first"
  if g == 3 then played = true
  elseif g == 8 and played and sc then
    played = false
    local a, b = nba.read16(sc), nba.read16(sc + 16)
    nba.save(nba.game .. "_draft_loser.txt", (a < b and "1" or a > b and "2" or "0") .. "\n")
    if ladder then
      local mine, theirs = ladder.side == 1 and a or b, ladder.side == 1 and b or a
      local opp = nba.read16(team[3 - ladder.side])
      ladder.games = ladder.games + 1
      if mine > theirs and opp <= 26 then
        ladder.mask = ladder.mask | (1 << opp)
        local n = 0
        for i = 0, 26 do if (ladder.mask >> i) & 1 == 1 then n = n + 1 end end
        nba.log(string.format("fantasy draft: ladder: beat %s %d-%d, %d of 27 teams beaten%s", TEAMS[opp + 1], mine, theirs, n,
          ladder.mask == ALL and ", the ladder is won" or ""))
      else
        nba.log(string.format("fantasy draft: ladder: lost to %s %d-%d", TEAMS[opp + 1] or "?", mine, theirs))
      end
      ladder_save()
    end
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
      if timer <= 0 then pick(d, free_from(d, cursor[d])) end
    end
    if state == "draft" then draw() end
  end
  if state == "done" then
    countdown(false)
    if selecting or g == 2 or g == 3 then hold_pairs() end
    hold_hidden(g)
    if ladder then
      guard.mark()
      if nba.read16(q) == 0 and g ~= 3 and nba.read16(inm) == 0 then
        -- from the end of the draft to the matchup screen, every frame: the ladder's teams
        -- beaten in the records. select_teams reads them while in_team_select is still on, and
        -- loading a player's record (after his initials) replaces them, so they are written
        -- again in the next frame.
        local p = nba.read16(pst)
        for _, k in ipairs({ 2 * ladder.side - 1, 2 * ladder.side }) do
          if (p >> (k - 1)) & 1 == 1 and pdata[k] then
            if s16(nba.read16(pdata[k] + PR_INIT1)) < 0 then nba.write16(pdata[k] + PR_INIT1, 0) end
            nba.write32(pdata[k] + PR_TEAMSDEF, ladder.mask)
          end
        end
      end
      if nba.read16(inm) ~= 0 and not opp_logged then
        opp_logged = true
        local opp = nba.read16(team[3 - ladder.side])
        local stock
        if orders then for _, t in ipairs(orders) do if (ladder.mask >> t) & 1 == 0 then stock = t; break end end end
        nba.log(string.format("fantasy draft: ladder opponent %s, the stock game's next: %s", TEAMS[opp + 1] or "?",
          stock and TEAMS[stock + 1] or "(all beaten)"))
      end
    end
  end
end
