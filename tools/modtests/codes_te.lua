-- codes_te.lua - roadmap 2.1 check: enter one T.E./SE cabinet code for player 1 the way the
-- cabinet takes it (taps and held buttons on the matchup screen, held buttons at tip-off)
-- and report, at tip-off, the variable tools/../mods/lib/codes_nbajamte.lua names for it.
-- The script plays by itself: start presses to team select, shoot to pick, then the code.
-- Option "code" picks the code (see CODES below). Run with run_all.sh (codes_te.expect).
mod = {
  name = "Codes check (T.E.)",
  games = { "nbajamte", "nbajamse", "nbajam" },
  options = { { id = "code", label = "Code", values = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, default = 0 } },
}

-- each: taps = shoot taps on the matchup screen; hold = held through the matchup screen;
-- seq = { stick, buttons... } pressed one at a time with the stick held; circle = stick
-- round after the taps; tip = held from the end of the matchup screen to tip-off.
local CODES = {
  [0] = { name = "none" },
  [1] = { name = "tournament", hold = { "turbo", "pass", "shoot", "right" } },
  [2] = { name = "maxpower", hold = { "turbo", "pass", "shoot", "down" } },
  [3] = { name = "shot_percent", taps = 15, circle = true },
  [4] = { name = "assist_off", taps = 10 },
  [5] = { name = "goaltend", taps = 24 },
  [6] = { name = "quick_hands", taps = 5, tip = { "shoot", "down" } },
  [7] = { name = "big_head", tip = { "up", "turbo", "pass" } },
  [8] = { name = "huge_head", taps = 6, tip = { "up", "turbo", "pass" } },
  -- NBA Jam 1993 (roadmap 2.4): goaltending takes 8 taps, quick hands all three buttons and down
  [10] = { name = "goaltend_8", taps = 8 },
  [11] = { name = "quick_hands_1993", taps = 5, tip = { "turbo", "pass", "shoot", "down" } },
  [9] = { name = "baby", seq = { { "down", "right" }, "turbo", "shoot", "pass", "turbo", "shoot", "pass", "turbo", "shoot", "pass" } },
}

local S = {}
local code, phase, t0, step, t3 = nil, "boot", 0, 0, nil
local function r(n) return S[n] and nba.read16(S[n]) or -1 end
local function cat(a, b) local t = {} for _, v in ipairs(a) do t[#t + 1] = v end for _, v in ipairs(b) do t[#t + 1] = v end return t end

function on_load()
  for _, n in ipairs({ "GAMSTATE", "in_team_select", "inmatchup", "p1taps", "plyr_tourn", "plyr_maxpower",
      "show_shot", "assistance_off", "stlpower", "plyrbighead", "baby_flag" }) do S[n] = nba.sym(n) end
  code = CODES[nba.option("code")]
  nba.log("code " .. code.name)
end

function on_frame(f)
  if phase == "boot" then
    -- NBA Jam 1993 has no in_team_select symbol: two start presses reach it by frame 2400
    if r("in_team_select") > 0 or (not S.in_team_select and f >= 2400) then phase = "select"
    elseif f >= 1000 and f % 200 == 0 then nba.press(1, "start", 4) end
  elseif phase == "select" then
    if r("inmatchup") ~= 0 then phase = "matchup"; t0 = f; step = 0; nba.log("matchup at frame " .. f)
    elseif f % 60 == 0 then nba.press(1, "shoot", 4) end
  elseif phase == "matchup" then
    local t = f - t0 - 20          -- the game starts reading a moment after inmatchup is set
    if t < 0 then return end
    if code.hold then nba.press(1, code.hold, 2) end
    local taps = code.taps or 0
    if t >= 4 and t % 4 == 0 and step < taps then nba.press(1, "shoot", 2); step = step + 1 end
    if code.circle and step >= taps and t % 4 == 0 then
      local dirs = { "up", "right", "down", "left" }
      nba.press(1, dirs[(t // 4) % 4 + 1], 3)
    end
    if code.seq then
      local k = t // 8 + 1     -- one button per 8 frames: 4 down, 4 up, stick held throughout
      nba.press(1, code.seq[1], 2)
      if k >= 2 and k <= #code.seq and t % 8 < 4 then nba.press(1, cat(code.seq[1], { code.seq[k] }), 2) end
    end
    if r("inmatchup") == 0 then phase = "tip"; t0 = f end
  elseif phase == "tip" then
    if code.tip and r("GAMSTATE") ~= 3 then nba.press(1, code.tip, 2) end
    if r("GAMSTATE") == 3 and not t3 then t3 = f end
    -- read 4 s into the game: some checks (assist_off) run a little after tip-off
    if t3 and f - t3 >= 240 then
      phase = "done"
      nba.log(string.format("4 s after tip-off (frame %d): p1taps=%d plyr_tourn=%d plyr_maxpower=%d show_shot=%d assistance_off=%d stlpower=%d plyrbighead=%d baby_flag=%d",
        f, r("p1taps"), r("plyr_tourn"), r("plyr_maxpower"), r("show_shot"), r("assistance_off"), r("stlpower"), r("plyrbighead"), r("baby_flag")))
    end
  end
end
