-- cpu_vs_cpu.lua - a whole four-quarter game between two computer teams, for watching
-- (roadmap Phase 6).
--
-- The attract-mode demo is only a 60-second show (ATTRACT.ASM demogame), so the script
-- plays a real game instead: from attract mode it presses Start for player 1 (and adds a
-- coin if that starts nothing), then Shoot until the game is in play, which picks the team
-- under the cursor and skips the initials. "Team: Random" first moves the cursor a random
-- number of steps on the team select screen (in_team_select; not on NBA JAM 1993, whose
-- symbol file does not have it: there the team under the cursor is taken). During play it
-- clears PSTATUS, the bits of the players in the game, so the game's own computer players
-- (drones) run all four, on every game; between quarters it puts the
-- bit back, or the game would end for lack of players. When the game is over the game
-- goes back to attract mode, and with "After the game: Play another" the next one starts.
-- A computer game is not a real game: "Protect records" (lib/records.lua) keeps it out of
-- the saved records. Not in an online game. Turn the script off to play yourself.
--
-- "Label players: CPU" (Hangtime and Maximum Hangtime): those games put a player number
-- over a player whose PSTATUS bit is set, and a CPU tag over a drone, when the tags are made
-- at the tip-off of each period. The bit put back between quarters (or still set from team
-- select) then made player 1 show as a person. With the option the bit is also cleared on the
-- match-up screen and again as soon as the next period's tip-off comes (inmatchup, or
-- GAMSTATE back to play), so the tags are made for four drones; the period-end check still
-- finds the bit. The overlay also says "CPU VS CPU" at the top during play, on every game.
mod = {
  name = "CPU vs CPU",
  description = "Starts whole games between two computer teams, for watching.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = {
    { id = "team", label = "Team", values = { 0, 1 }, labels = { "Under the cursor", "Random" }, default = 1 },
    { id = "again", label = "After the game", values = { 0, 1 }, labels = { "Back to attract", "Play another" }, default = 1 },
    { id = "labels", label = "Label players: CPU", values = { 0, 1 }, labels = { "Off", "On" }, default = 1 },
    nba.include("lib/records.lua").OPTION,
  },
}

local gs, pst, sel, guard, inm
local hangtime = nba.game == "nbahangt" or nba.game == "nbamht"
local saved          -- PSTATUS when play began: put back between quarters
local played = false -- this game has been in play
local started = 0    -- games started by the script
local idle = 0       -- frames in attract mode since the last try
local steps          -- random cursor steps still to press on the team select screen

function on_load()
  gs, pst, sel, inm = nba.sym("GAMSTATE"), nba.sym("PSTATUS"), nba.sym("in_team_select"), nba.sym("inmatchup")
  guard = nba.include("lib/records.lua").guard()
  if nba.netplay then gs = nil; nba.log("cpu vs cpu: not in an online game"); return end
  if not (gs and pst) then gs = nil; nba.log("cpu vs cpu: not available on this game") end
end

function on_frame(f)
  guard.update()
  if not gs then return end
  local g = nba.read16(gs)
  if g == 0 or g == 1 then
    -- attract mode: start a game (the first after 3 seconds; another only if asked)
    if played then played, saved = false, nil end
    steps = nil
    if started > 0 and nba.option("again") == 0 then return end
    idle = idle + 1
    if idle == 180 then nba.press(1, "start", 4)
    elseif idle == 300 then nba.press(1, "coin", 4)
    elseif idle == 330 then nba.press(1, "start", 4)
    elseif idle >= 600 then idle = 170 end
    return
  end
  if idle > 0 then idle = 0; started = started + 1; nba.log("cpu vs cpu: game " .. started .. " started") end
  guard.mark()
  local labels = hangtime and nba.option("labels") == 1
  local matchup = inm and nba.read16(inm) ~= 0
  if g == 3 then
    local p = nba.read16(pst)
    if p ~= 0 then saved = p; nba.write16(pst, 0) end
    played = true
    if nba.option("labels") == 1 then
      nba.rect(160, 2, 80, 11, 0x80000000, true)
      nba.text(168, 4, "CPU VS CPU", 0xffff80)
    end
  elseif labels and matchup then
    -- the match-up before a tip-off: the tags are made next, for drones
    local p = nba.read16(pst)
    if p ~= 0 then saved = saved or p; nba.write16(pst, 0) end
  elseif g ~= 8 and saved and nba.read16(pst) == 0 then
    nba.write16(pst, saved)   -- between quarters: a player is in the game, so it goes on
  elseif not played then
    -- before the first tip-off: team select and initials
    local picking = sel and nba.read16(sel) ~= 0
    if picking and steps == nil then
      steps = nba.option("team") == 1 and math.random(0, 26) or 0
      if steps > 0 then nba.log("cpu vs cpu: " .. steps .. " random steps on team select") end
    end
    if picking and steps > 0 then
      -- one step every 12 frames, down or right
      if f % 12 == 0 then nba.press(1, math.random(2) == 1 and "down" or "right", 3); steps = steps - 1 end
    elseif f % 60 == 20 then nba.press(1, "shoot", 4) end
  end
end
