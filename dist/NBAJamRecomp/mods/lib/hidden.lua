-- lib/hidden.lua - the hidden (secret) players of each game, by the number the game keeps in
-- special_heads[player] (roadmap 4.1, 4.2). The game builds a player whose special_heads
-- entry is n from row n of its secret player table (spechds_t), head and attributes; -1 is a
-- normal player.
--   T.E. 4.0, SE 5.02   the initials table of SELECT2.ASM (lib/codes_nbajamte/se.lua, kind
--                       "initials"): 61 and 68 rows
--   NBA JAM 1993 3.01   13 rows, read from the ROM (FF963740): 11 are T.E.'s rows 0-9 and 11,
--                       rows 10 and 12 are its own, named only by their initials and date
--   Hangtime L1.1, L1.3,
--   Max. Hangtime 1.3   SELECT2.ASM #team_data, 61 rows (names from #team_snds); the same
--                       table in all three ROMs
--   Max. Hangtime 1.03  64 rows read from the ROM: the 36 staff rows, four of its own, then
--                       the guest stars without Spud Webb
local hidden = {}

local HT_STAFF = { "Jeff Johnson", "Divita", "Turmell", "Thompson", "Geer", "Carlton", "Hedrick", "Hey",
  "Morris", "Bardo", "Minifee", "Martinez", "Pesina", "Liptak", "Eddie", "Mike V.", "Jamie R.", "Nick E.",
  "J. Root", "Mednick", "Dan R.", "Pat F.", "Boon", "Tobias", "Oursler", "Jason S.", "Quinn", "M. Perry",
  "N. Funk", "MDoc", "Bud", "Marius", "Munday", "North", "Amrich", "Jiggets" }
local HT_STARS = { "Pippen", "Rodman", "L. Johnson", "G. Rice", "Kidd", "Mutombo", "G. Hill", "Olajuwon",
  "R. Miller", "Smits", "Mourning", "G. Robinson", "Spud Webb", "Ewing", "Starks", "A. Hardaway", "H. Grant",
  "Stackhouse", "C. Robinson", "D. Robinson", "S. Elliott", "Kemp", "Malone", "Webber", "Muresan" }

local function cat(...)
  local t = {}
  for _, l in ipairs({ ... }) do for _, v in ipairs(l) do t[#t + 1] = v end end
  return t
end

local function from_codes(game)
  local t = {}
  for _, c in ipairs(nba.include("lib/codes_" .. game .. ".lua")) do
    if c.kind == "initials" then t[c.value + 1] = c.label.en end
  end
  return t
end

-- names in row order (row n = names[n + 1]) for a game; rev103: Maximum Hangtime 1.03
function hidden.rows(game, rev103)
  if game == "nbajamte" or game == "nbajamse" then return from_codes(game) end
  if game == "nbajam" then
    local te = from_codes("nbajamte")
    local t = {}
    for n = 0, 12 do t[n + 1] = te[n + 1] end
    t[11] = "HOW (07/15)"; t[13] = "TON (07/03)"
    return t
  end
  if game == "nbamht" and rev103 then
    local stars = {}
    for _, s in ipairs(HT_STARS) do if s ~= "Spud Webb" then stars[#stars + 1] = s end end
    return cat(HT_STAFF, { "Zirin", "Heith", "Matt B.", "K. Day" }, stars)
  end
  if game == "nbamht" or game == "nbahangt" then return cat(HT_STAFF, HT_STARS) end
  return {}
end

-- every name of every game, each once, by name: the option list (value k = names[k])
function hidden.all()
  local seen, out = {}, {}
  for _, l in ipairs({ hidden.rows("nbajamte"), hidden.rows("nbajamse"), hidden.rows("nbajam"),
      hidden.rows("nbahangt"), hidden.rows("nbamht", true) }) do
    for _, n in ipairs(l) do if not seen[n] then seen[n] = true; out[#out + 1] = n end end
  end
  table.sort(out)
  return out
end

-- Maximum Hangtime 1.03: its table starts at FFAAF360 (1.3 has it at FFAB9200); the first
-- row is JAPPLE, J = 10
function hidden.is_mht103()
  return nba.game == "nbamht" and nba.read16(0xFFAAF360) == 10 and nba.read16(0xFFAB9200) ~= 10
end

return hidden
