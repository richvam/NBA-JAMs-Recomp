-- unlock_players.lua - play as any hidden player without entering initials or a PIN
-- (roadmap 4.2). One option per player: Normal, or a hidden player by name (lib/hidden.lua).
-- The game builds a player from special_heads[player] at tip-off; the script writes the
-- chosen number there from the matchup screen until a second after tip-off, every game. A
-- name the running game does not have is skipped (and logged).
local hidden = nba.include("lib/hidden.lua")
local ALL = hidden.all()

local values, labels = { 0 }, { "Normal" }
for k, n in ipairs(ALL) do values[k + 1] = k; labels[k + 1] = n end
local options = {}
for p = 1, 4 do
  options[p] = { id = "player" .. p, label = "Player " .. p, values = values, labels = labels, default = 0 }
end

mod = {
  name = "Unlock hidden players",
  description = "Pick any hidden player for each player slot, without entering initials or a PIN.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = options,
}

local HOLD = 60
local writes, sh, inm, gs = {}, nil, nil, nil
local phase, t = "wait", 0

function on_load()
  sh, inm, gs = nba.sym("special_heads"), nba.sym("inmatchup"), nba.sym("GAMSTATE")
  if not (sh and inm and gs) then nba.log("this game has no special_heads symbol: nothing to do"); return end
  local rows = hidden.rows(nba.game, hidden.is_mht103())
  local row = {}
  for n, name in ipairs(rows) do row[name] = n - 1 end
  for p = 1, 4 do
    local k = nba.option("player" .. p)
    local name = k and ALL[k]
    if name then
      if row[name] then
        writes[#writes + 1] = { addr = sh + 16 * (p - 1), value = row[name] }
        nba.log(string.format("player %d: %s (special_heads = %d)", p, name, row[name]))
      else
        nba.log(string.format("player %d: %s is not in this game", p, name))
      end
    end
  end
end

function on_frame()
  if #writes == 0 then return end
  if phase ~= "write" and nba.read16(inm) ~= 0 then phase = "write"; t = 0 end
  if phase == "write" then
    for _, w in ipairs(writes) do nba.write16(w.addr, w.value) end
    if nba.read16(gs) == 3 and nba.read16(inm) == 0 then
      t = t + 1
      if t > HOLD then phase = "done" end
    end
  end
end
