-- turbo_rules.lua - turbo meter rules (roadmap Phase 6). T.E. 4.0 and SE 5.02.
--
-- Each player's meter is in his PxDATA block (GAME.EQU): ply_turbo (+0x40, 0 empty to 0x2F
-- full), ply_turbo_dl (+0x50, frames until the next unit is used), used_turbo (+0xE0, a short
-- delay after use before refilling) and the lit-up meter image (ply_meter_imgs+0x40, +0xC0),
-- whose OFSET hides as much of it as is used. The player process points at its block with
-- plyr_PDATA_p (PDATA+0x30).
-- - "Infinite turbo": the ticked players' meters are kept full, as the game's refill_turbo
--   does (ply_turbo 0x2F, OFSET 0).
-- - "Turbo drain": while turbo is held the game uses one unit every 8 frames (PLYR.ASM: MOVK
--   8,A2 into ply_turbo_dl); the script changes that 8 in memory (never in a file), found by its
--   place: SUBK 1,A2 / MOVE A2,*A0(ply_turbo_dl) / JRNZ / MOVK 8,A2 / MOVE ... / SUBK 1,A1.
-- - "Turbo refill": "Fast" adds a unit every 4 frames to a meter that is not in use (used_turbo
--   0), as replenish_turbo does; "Instant" fills it.
mod = {
  name = "Turbo rules",
  description = "Infinite turbo for chosen players, and how fast turbo drains and refills.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "infinite", label = "Infinite turbo", type = "players", default = 0 },
    { id = "drain", label = "Turbo drain", values = { 0, 2, 4, 16, 32 },
      labels = { "Game", "Very fast", "Fast", "Slow", "Very slow" }, default = 0 },
    { id = "refill", label = "Turbo refill", values = { 0, 1, 2 }, labels = { "Game", "Fast", "Instant" }, default = 0 },
    nba.include("lib/records.lua").OPTION,
  },
}

local FULL, TURBO, USED, LITUP, OFSET, PDATA_P = 0x2F, 0x40, 0xE0, 0xC0, 0x100, 0x130
local procs, gs, inm, infinite, refill, guard

local function fill(d)
  nba.write16(d + TURBO, FULL)
  local img = nba.read32(d + LITUP)
  if img ~= 0 then nba.write16(img + OFSET, 0) end
end

function on_load()
  procs, gs, inm = nba.sym("plyrproc_t"), nba.sym("GAMSTATE"), nba.sym("inmatchup")
  guard = nba.include("lib/records.lua").guard()
  infinite, refill = math.tointeger(nba.option("infinite")), math.tointeger(nba.option("refill"))
  local drain = math.tointeger(nba.option("drain"))
  if drain > 0 then
    local hits = nba.rom_find({ 0x1422, -1, 0x0050, -1, 0x1902, -1, 0x0050, 0x1421 }) or {}
    if #hits == 1 then
      nba.rom_write16(hits[1] + 16 * 4, 0x1800 | ((drain & 31) << 5) | 2)    -- MOVK drain,A2
      nba.log(string.format("turbo drain: a unit every %d frames (game 8)", drain))
    else
      nba.log("the turbo drain was not found: not changed")
    end
  end
  if infinite ~= 0 then nba.log(string.format("infinite turbo: players %d", infinite)) end
  if refill ~= 0 then nba.log("turbo refill: " .. (refill == 1 and "fast" or "instant")) end
end

function on_frame(f)
  guard.update()
  if not procs or nba.read16(gs) ~= 3 or nba.read16(inm) ~= 0 then return end
  if infinite == 0 and refill == 0 and math.tointeger(nba.option("drain")) == 0 then return end
  guard.mark()
  for k = 0, 3 do
    local p = nba.read32(procs + 32 * k)
    local d = p ~= 0 and nba.read32(p + PDATA_P) or 0
    if d ~= 0 then
      if infinite & (1 << k) ~= 0 then
        fill(d)
      elseif refill ~= 0 and nba.read16(d + USED) == 0 and nba.read16(d + TURBO) < FULL then
        if refill == 2 then fill(d)
        elseif f % 4 == 0 then
          local img = nba.read32(d + LITUP)
          if img ~= 0 and nba.read16(img + OFSET) > 0 then
            nba.write16(img + OFSET, nba.read16(img + OFSET) - 1)
            nba.write16(d + TURBO, nba.read16(d + TURBO) + 1)
          end
        end
      end
    end
  end
end
