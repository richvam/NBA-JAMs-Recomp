-- hitboxes.lua - fighting-game style boxes on the overlay (roadmap Phase 6). T.E. 4.0, SE 5.02.
--
-- Everything is drawn from what the game itself tests, in its world units (X across the
-- court, Y up, negative, Z into the court), and put on the screen the way the display code
-- does it (NDSP1.ASM dma_objlst, 3D objects):
--   screen x = (X - (WORLDTLX + 200)) * (Z - 75) / 819.2 + 200
--   screen y = Y - WORLDTLY + (Z - 894) * 3/8 + 76
-- Shown for each player (blue team 1, red team 2):
--   body box   plyr_chkpcollide (PLYR.ASM): two players bump when their boxes overlap, X
--              within 3/8 of the image width of the center (1/4 for large art), Z within
--              13 each (or the image's ICBZ). Drawn from the top of the player to his feet,
--              with the Z depth as a box on the floor.
--   ball reach ball_chkpcollide (PLYR2.ASM): the ball touches a player when its center is
--              within half the image width in X and within a Z distance from the player's
--              defense skill (tbl1; tbl2 for a low, slow ball). A steal is that touch during
--              the steal move (and a roll on the steal skill); a block, during a block or
--              rebound. Drawn on the floor, yellow; filled while the steal or block move is on.
--   push reach seq_push (PLYR2.ASM): a push hits an opponent within 50 in front, up to 90
--              degrees to either side of where the pusher faces. Drawn while the push move is on.
-- And the ball object (green, with a line down to the floor; while a player holds the ball
-- the game keeps it near his hands, so it does not always sit on the ball in his art) and the center of both rims (cyan;
-- HOOPLX/HOOPRX, HOOPY, Z 1170). The game is never changed.
mod = {
  name = "Hitboxes",
  description = "Draws each player's body box, ball reach and push reach, the ball and the rims.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "body", label = "Body boxes", type = "toggle", default = true },
    { id = "reach", label = "Ball and push reach", type = "toggle", default = true },
    { id = "ball", label = "Ball and rims", type = "toggle", default = true },
  },
}

-- object (DISP.EQU) and player process (PLYR.EQU) fields, bit offsets
local OXVAL, OYPOS, OZPOS, OSIZEX, OSIZEY, OIMG, ODATA, OXANI, OMISC =
  0x80, 0xb0, 0xd0, 0x130, 0x140, 0x170, 0x1c0, 0x1e0, 0x210
local IFLAGS, ICBZ = 0xc0, 0x110
local P_ATTRIB, P_SEQ, P_DIR = 0x150, 0x170, 0x3d0
local PAT_DEFSKILL = 0x50
local BLOCK_SEQ, STEAL_SEQ, PUSH_SEQ = 17, 43, 44
local tbl1 = { [0] = 16, 17, 18, 18, 18, 18, 18, 19, 20, 21, 22, 23 }
local tbl2 = { [0] = 32, 33, 34, 35, 35, 35, 35, 36, 37, 38, 39, 40 }
local TEAM = { 0x40a0ff, 0xff5050 }

local gs, objs, procs, wx, wy, ballp

function on_load()
  gs, objs, procs = nba.sym("GAMSTATE"), nba.sym("plyrobj_t"), nba.sym("plyrproc_t")
  wx, wy, ballp = nba.sym("WORLDTLX"), nba.sym("WORLDTLY"), nba.sym("ballobj_p")
  if not (gs and objs and procs and wx and wy) then gs = nil; nba.log("hitboxes: not available on this game") end
end

local function s16(v) return v >= 0x8000 and v - 0x10000 or v end
local function s32(v) return v >= 0x80000000 and v - 0x100000000 or v end

local camx, camy
local function scr(x, y, z)
  local sx = (x - camx) * (z - 75) / 819.2 + 200
  local zz = math.max(z - 894, 0)
  return math.floor(sx + 0.5), y - camy + zz // 2 - zz // 8 + 76
end

-- a box on the floor (Y = 0) from X x1..x2, Z z1..z2, as four lines (it is a trapezoid)
local function floor_box(x1, x2, z1, z2, color, y)
  y = y or 0
  local ax, ay = scr(x1, y, z1); local bx, by = scr(x2, y, z1)
  local cx, cy = scr(x2, y, z2); local dx, dy = scr(x1, y, z2)
  nba.line(ax, ay, bx, by, color); nba.line(bx, by, cx, cy, color)
  nba.line(cx, cy, dx, dy, color); nba.line(dx, dy, ax, ay, color)
end

-- the scale the display uses at this depth (ODATA_p scale table: 0x100 = full size)
local function scale_y(o, z)
  local t = nba.read32(o + ODATA)
  if t == 0 then return 1 end
  local v = nba.read16(t + ((z >> 4) << 6) + 16)
  return v > 0 and 256 / v or 1
end

local function draw_player(k, o, p)
  local x = s16(nba.read16(o + OXVAL + 16)) + s16(nba.read16(o + OXANI + 16))
  local y = s16(nba.read16(o + OYPOS))
  local z = s16(nba.read16(o + OZPOS))
  local zd = z + s16(nba.read16(o + OMISC)) - 75
  local w, h = nba.read16(o + OSIZEX), nba.read16(o + OSIZEY)
  local img = nba.read32(o + OIMG)
  local large = img ~= 0 and nba.read16(img + IFLAGS) >= 0x8000
  local hw = large and w / 4 or w * 3 / 8
  local rz = img ~= 0 and nba.read16(img + ICBZ) or 0
  if rz == 0 then rz = 13 end
  local color = TEAM[k < 2 and 1 or 2]
  local seq = nba.read16(p + P_SEQ)
  if nba.option("body") then
    local lx, ty = scr(x - hw, y, z)
    local rx = scr(x + hw, y, z)
    local bot = ty + math.floor(h * scale_y(o, zd) + 0.5)
    nba.rect(lx, ty, math.max(rx - lx, 1), math.max(bot - ty, 1), color)
    floor_box(x - hw, x + hw, z - rz, z + rz, color)
  end
  if nba.option("reach") then
    local at = nba.read32(p + P_ATTRIB)
    local def = at ~= 0 and math.min(nba.read16(at + PAT_DEFSKILL), 11) or 5
    local active = seq == STEAL_SEQ or seq == BLOCK_SEQ
    local r = tbl1[def] or 18
    floor_box(x - w / 2, x + w / 2, z - r, z + r, active and 0xffff00 or 0x80ffff00)
    if active then
      local ax, ay = scr(x - w / 2, 0, z - r); local bx, by = scr(x + w / 2, 0, z + r)
      nba.rect(ax, ay, math.max(bx - ax, 1), math.max(by - ay, 1), 0x50ffff00, true)
      local r2 = tbl2[def] or 35   -- the larger reach for a low, slow ball
      floor_box(x - w / 2, x + w / 2, z - r2, z + r2, 0x80ffff00)
    end
    if seq == PUSH_SEQ then
      local a0 = nba.read16(p + P_DIR) * math.pi / 64
      local px, py = scr(x, 0, z)
      local lastx, lasty = px, py
      for i = -6, 6 do
        local a = a0 + i * math.pi / 12
        local qx, qy = scr(x + 50 * math.sin(a), 0, z - 50 * math.cos(a))
        nba.line(lastx, lasty, qx, qy, 0xff80ff)
        lastx, lasty = qx, qy
      end
      nba.line(lastx, lasty, px, py, 0xff80ff)
    end
  end
end

function on_frame()
  if not gs or nba.read16(gs) ~= 3 then return end
  camx = s16(nba.read16(wx + 16)) + 200
  camy = s16(nba.read16(wy + 16))
  for k = 0, 3 do
    local o, p = nba.read32(objs + 32 * k), nba.read32(procs + 32 * k)
    if o ~= 0 and p ~= 0 then draw_player(k, o, p) end
  end
  if nba.option("ball") then
    local b = ballp and nba.read32(ballp) or 0
    if b ~= 0 then
      local x = s16(nba.read16(b + OXVAL + 16)) + s16(nba.read16(b + OXANI + 16))
      local y, z = s16(nba.read16(b + OYPOS)), s16(nba.read16(b + OZPOS))
      local bx, by = scr(x, y, z)
      local gx, gy = scr(x, 0, z)
      nba.rect(bx - 3, by - 3, 7, 7, 0x40ff40)
      nba.line(bx, by + 3, gx, gy, 0x8040ff40)   -- a drop line to the floor below the ball
    end
    for _, hx in ipairs({ 0x2000 - 320, 0x2000 + 320 }) do
      local cx, cy = scr(hx, -120, 1170)
      nba.line(cx - 10, cy, cx + 10, cy, 0x00ffff); nba.line(cx, cy - 4, cx, cy + 4, 0x00ffff)
    end
  end
end
