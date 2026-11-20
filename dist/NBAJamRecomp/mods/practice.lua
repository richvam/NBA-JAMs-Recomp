-- practice.lua - a practice game: frozen game clock, no shot clock (roadmap Phase 6).
--
-- Game clock: game_time holds the quarter clock (tenths, seconds, tens of seconds, minutes,
-- a byte each). On the first frame of play in each quarter the script keeps that value, and
-- writes it back on every frame of play, so the clock never runs down and the quarter never
-- ends. The game is ended by turning the script off (or by a reset).
-- Shot clock: as in rules.lua, while a shot clock process (sc_proc) runs the script holds
-- shotimer at 1,0 (tens, ones), so it never runs out.
-- A practice game is not a real game: "Protect records" (lib/records.lua) keeps it out of
-- the saved records.
mod = {
  name = "Practice",
  description = "Practice game: the game clock stands still and there is no shot clock.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = {
    { id = "clock", label = "Game clock runs", type = "toggle", default = false },
    { id = "shot_clock", label = "Shot clock on", type = "toggle", default = false },
    nba.include("lib/records.lua").OPTION,
  },
}

local gt, gs, q, st, sp, guard, freeze, noshot, held, heldq

function on_load()
  gt, gs, q = nba.sym("game_time"), nba.sym("GAMSTATE"), nba.sym("gmqrtr")
  st, sp = nba.sym("shotimer"), nba.sym("sc_proc")
  guard = nba.include("lib/records.lua").guard()
  freeze, noshot = not nba.option("clock"), not nba.option("shot_clock")
  if freeze then nba.log("practice: game clock frozen") end
  if noshot then nba.log("practice: no shot clock") end
end

function on_frame()
  guard.update()
  if not (gt and gs and q) then return end
  local g = nba.read16(gs)
  if g == 0 or g == 1 then heldq = nil end   -- attract: the next game holds its own clock
  if g ~= 3 then return end
  if not (freeze or noshot) then return end
  guard.mark()
  if freeze then
    local qq = nba.read16(q)
    if heldq ~= qq then held, heldq = nba.read32(gt), qq end
    if nba.read32(gt) ~= held then nba.write32(gt, held) end
  end
  if noshot and st and sp and nba.read32(sp) ~= 0 then
    nba.write16(st, 0); nba.write16(st + 16, 1)
  end
end
