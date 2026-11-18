-- codes_mht.lua - roadmap 2.2 check: enter one Maximum Hangtime code for player 1 on the
-- versus screen, built from lib/codes_<game>.lua (number codes from their digits, stick codes
-- from STICK below), and report every power-up variable 4 s into the game.
-- The script plays by itself: start presses to team select, shoot to pick, then the code.
-- Option "code": 0 = none, 1-23 = the table's entries in order.
mod = {
  name = "Codes check (Maximum Hangtime)",
  games = { "nbamht", "nbahangt" },
  options = { { id = "code", label = "Code", type = "range", min = 0, max = 23, step = 1, default = 0 },
    -- two = 1: cabinet player 2 joins too (the other team: cabinet players 1 and 2 are the
    -- game's player slots 1 and 2) and enters nothing; two = 2: it enters the same code
    { id = "two", label = "Second player", values = { 0, 1, 2 }, default = 0 } },
}
local codes   -- lib/codes_<game>.lua, read in on_load (nba.game is not set while the folder is scanned)

-- stick codes, step by step: each step is held 3 frames; "rel" steps release everything
local UR, DR, DL, UL = { "up", "right" }, { "down", "right" }, { "down", "left" }, { "up", "left" }
local SWIRL = { { "up" }, UR, { "right" }, DR, { "down" }, DL, { "left" }, UL, { "up" } }
local R = "rel"
local STICK = {
  lock_combo = { { "down" }, R },
  big_head = { { "up", "turbo", "pass" }, R },
  huge_head = { { "up" }, R, { "up" }, R, { "pass" }, R, { "turbo" }, R },
  shot_percent = SWIRL,
  no_tag = { { "left" }, R, { "left" }, R, { "pass" }, R, { "turbo" }, R },
  no_drift = { { "down" }, R, { "down" }, R, { "shoot" }, R, { "turbo" }, R },
  no_assistance = { { "right" }, { "right", "pass" }, { "right" }, { "right", "pass" }, R },
  aba_ball = { { "right" }, { "right", "shoot" }, { "right" }, { "right", "turbo" }, { "right" }, { "right", "pass" }, R },
  outdoor_court = { { "left" }, { "left", "turbo" }, { "left" }, { "left", "turbo" }, R },
  special = { { "shoot" }, R, { "shoot" }, R, { "up", "shoot" }, { "up", "right", "shoot" }, { "right", "shoot" }, { "down", "right", "shoot" },
    { "down", "shoot" }, { "down", "left", "shoot" }, { "left", "shoot" }, { "up", "left", "shoot" }, { "up", "shoot" }, R },
}
local VARS = { "pup_tournament", "pup_baby", "pup_nomusic", "pup_goaltend", "pup_maxblock", "pup_maxsteal", "pup_maxpower",
  "pup_maxspeed", "pup_hypspeed", "pup_trbstealth", "pup_trbinfinite", "pup_nopush", "pup_fastpass", "pup_lockcombo",
  "pup_bighead", "pup_hugehead", "pup_showshotper", "pup_notag", "pup_nodrift", "pup_noassistance", "pup_aba", "pup_court" }

local S, steps = {}, {}
DELAY = 120
local entry, phase, t0, t3, two = nil, "boot", 0, nil, 0
local function r(n) return S[n] and nba.read16(S[n]) or -1 end

function on_load()
  for _, n in ipairs(VARS) do S[n] = nba.sym(n) end
  for _, n in ipairs({ "GAMSTATE", "in_team_select", "inmatchup" }) do S[n] = nba.sym(n) end
  codes = nba.include("lib/codes_" .. nba.game .. ".lua")
  local k = nba.option("code")
  two = nba.option("two")
  entry = codes[k]
  if entry and entry.kind == "number" then
    -- digits: turbo, shoot, pass presses
    local d = string.format("%03d", entry.value)
    for i, b in ipairs({ "turbo", "shoot", "pass" }) do
      for _ = 1, tonumber(d:sub(i, i)) do steps[#steps + 1] = { b }; steps[#steps + 1] = R end
    end
  elseif entry then
    steps = STICK[entry.id] or {}
  end
  nba.log((two == 1 and "two players, one code: " or two == 2 and "two players, both enter it: " or "") .. "code " .. (entry and (entry.id .. " (" .. entry.original .. ")") or "none") .. ", " .. #steps .. " steps")
end

function on_frame(f)
  if phase == "boot" then
    if r("in_team_select") ~= 0 then phase = "select"
    elseif f >= 1000 and f % 200 == 0 then nba.press(1, "start", 4); if two > 0 then nba.press(2, "start", 4) end end
  elseif phase == "select" then
    if r("inmatchup") ~= 0 then phase = "matchup"; t0 = f; nba.log("versus screen at frame " .. f)
    elseif f % 60 == 0 then nba.press(1, "shoot", 4); if two > 0 then nba.press(2, "shoot", 4) end end
  elseif phase == "matchup" then
    local t = f - t0 - DELAY         -- the code readers start after the screen transition
    if t >= 0 and t % 3 == 0 then
      local s = steps[t // 3 + 1]
      if s and s ~= R then nba.press(1, s, 3); if two == 2 then nba.press(2, s, 3) end end
    end
    if r("inmatchup") == 0 then phase = "game" end
  elseif phase == "game" then
    if r("GAMSTATE") == 3 and not t3 then t3 = f end
    if t3 and f - t3 >= 240 then
      phase = "done"
      -- one line per variable that is set, so expectations can name the mode and the variable
      local mode = ({ [0] = "one player", "two players, one code", "two players, both" })[two]
      local any = false
      for _, n in ipairs(VARS) do
        if r(n) ~= 0 then any = true; nba.log(string.format("result %s: %s=%X", mode, n:sub(5), r(n))) end
      end
      if not any then nba.log("result " .. mode .. ": none set") end
    end
  end
end
