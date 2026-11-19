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
-- T.E.: the roster power-up on whatever team each side is on, so turbo on team select steps
-- through all the team's pairs (SELECT.ASM #toggle1: up to 19 instead of 5, on the teams
-- that have more players). Not SE 5.02: its ROM first asks the operator adjustment ADJROSTER
-- (29) and only opens the rosters cleanly when that is 0; with it on, the power-up path pops
-- a register twice and never reaches the extra players. Set the adjustment in the service
-- menu instead; this script does not write NVRAM.
options[5] = { id = "expanded_rosters", label = "Expanded rosters (T.E.)", type = "toggle", default = false }

mod = {
  name = "Unlock hidden players",
  description = "Pick any hidden player for each player slot, without entering initials or a PIN.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = options,
}

local HOLD = 60
local writes, sh, inm, gs = {}, nil, nil, nil
local roster   -- { sel, { bigroster1, team1 }, { bigroster2, team2 } } when expanded_rosters is on
local phase, t = "wait", 0

function on_load()
  sh, inm, gs = nba.sym("special_heads"), nba.sym("inmatchup"), nba.sym("GAMSTATE")
  if nba.option("expanded_rosters") and nba.game == "nbajamse" then
    nba.log("expanded rosters: not on SE (use the operator adjustment Expanded roster)")
  elseif nba.option("expanded_rosters") and nba.game == "nbajamte" then
    local codes = nba.include("lib/codes.lua")
    local b = codes.sym("bigroster1")
    roster = { nba.sym("in_team_select"), { b, nba.sym("team1") }, { b + 16, nba.sym("team2") } }
    nba.log("expanded rosters on")
  end
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
  -- on team select: bigroster<n> = team<n> once that side has a team
  if roster and roster[1] and nba.read16(roster[1]) ~= 0 then
    for n = 2, 3 do
      local t = nba.read16(roster[n][2])
      if t ~= 0xFFFF then nba.write16(roster[n][1], t) end
    end
  end
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
