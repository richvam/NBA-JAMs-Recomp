-- auto_initials.lua - enters each player's initials and birthday on the record screen
-- (roadmap Phase 6). T.E. 4.0 and SE 5.02.
--
-- When a player joins, the game asks for three initials, a month and a day (SELECT.ASM
-- player_cursor), so it can load that player's saved record. The script presses the
-- player's own controls, as a person would (nba.press): it moves the cursor to each
-- letter, the month and the day set below, and presses Shoot on each. It writes nothing
-- into the game.
-- How it knows where the cursor is: while a choice is being made, the game keeps the
-- cursor's place in the record's field for it (save_selection: player<n>_data PR_INIT1,
-- PR_INIT2, PR_INIT3, PR_MONTH, PR_DAY, -1 until reached). The grids, from SELECT.ASM:
-- letters A-Z then space and delete, 4 a row; months 2 a row; days 1-31, 4 a row. The
-- cursor does not wrap at the edges.
-- Initials AAA with January 1 are not kept by the game, so the defaults are BBB.
-- Not in an online game.
local LETTERS = {}
for i = 0, 25 do LETTERS[i + 1] = string.char(65 + i) end
local LV = {}
for i = 0, 25 do LV[i + 1] = i end
local MONTHS = { "January", "February", "March", "April", "May", "June", "July", "August",
                 "September", "October", "November", "December" }
local MV, DV, DAYS = {}, {}, {}
for i = 0, 11 do MV[i + 1] = i end
for i = 0, 30 do DV[i + 1] = i; DAYS[i + 1] = tostring(i + 1) end

local options = {
  { id = "players", label = "Players", type = "players", default = 1 },
}
for k = 1, 4 do
  local g = "Player " .. k
  for j = 1, 3 do
    options[#options + 1] = { id = "p" .. k .. "_i" .. j, label = "Initial " .. j, group = g,
      values = LV, labels = LETTERS, default = 1 }
  end
  options[#options + 1] = { id = "p" .. k .. "_month", label = "Month", group = g, values = MV, labels = MONTHS, default = 0 }
  options[#options + 1] = { id = "p" .. k .. "_day", label = "Day", group = g, values = DV, labels = DAYS, default = 0 }
end

mod = {
  name = "Auto initials",
  description = "Enters each player's initials and birthday on the record screen, so the saved record loads.",
  games = { "nbajamte", "nbajamse" },
  options = options,
}

local WIDTH = { 4, 4, 4, 2, 4 }   -- grid width for each of the five choices
local FIELD = 0x40                 -- PR_INIT1, bits; then PR_INIT2, PR_INIT3, PR_MONTH, PR_DAY
local data, can_enter, pst
local busy = { 0, 0, 0, 0 }        -- frames to wait before the next press, per player
local done = { false, false, false, false }

function on_load()
  pst, can_enter = nba.sym("PSTATUS"), nba.sym("can_enter_inits")
  data = {}
  for k = 1, 4 do data[k] = nba.sym("player" .. k .. "_data") end
  if nba.netplay then pst = nil; nba.log("auto initials: not in an online game"); return end
  if not (pst and can_enter and data[1] and data[4]) then pst = nil; nba.log("auto initials: not available on this game") end
end

local function s16(v) return v >= 0x8000 and v - 0x10000 or v end

local function target(k, c)
  if c <= 3 then return nba.option("p" .. k .. "_i" .. c) end
  if c == 4 then return nba.option("p" .. k .. "_month") end
  return nba.option("p" .. k .. "_day")
end

local function player(k)
  local slot = {}
  for i = 1, 5 do slot[i] = s16(nba.read16(data[k] + FIELD + 16 * (i - 1))) end
  -- the choice being made is the last field that is not -1
  local c = 0
  for i = 5, 1, -1 do if slot[i] >= 0 then c = i; break end end
  if c == 0 then done[k] = false; return end      -- not on the grid (yet, or again)
  if done[k] then return end
  if busy[k] > 0 then busy[k] = busy[k] - 1; return end
  local at, want, w = slot[c], target(k, c), WIDTH[c]
  local dr = want // w - at // w
  local dc = want % w - at % w
  local dir = dr < 0 and "up" or dr > 0 and "down" or dc < 0 and "left" or dc > 0 and "right"
  if dir then
    nba.press(k, dir, 3)
  else
    nba.press(k, "shoot", 3)
    if c == 5 then
      done[k] = true
      nba.log(string.format("auto initials: player %d entered %s%s%s %s %d", k,
        LETTERS[slot[1] + 1], LETTERS[slot[2] + 1], LETTERS[slot[3] + 1], MONTHS[slot[4] + 1], want + 1))
    end
  end
  busy[k] = 8
end

function on_frame()
  if not pst or nba.read16(can_enter) == 0 then return end
  local p, who = nba.read16(pst), nba.option("players")
  for k = 1, 4 do
    if (p >> (k - 1)) & 1 == 1 and (who >> (k - 1)) & 1 == 1 then player(k) else done[k] = false end
  end
end
