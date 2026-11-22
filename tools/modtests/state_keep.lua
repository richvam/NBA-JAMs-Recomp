-- state_keep.lua: a script that keeps state in every way a script can (roadmap N1.2):
-- a global, locals shared by two functions, a table with a metatable, a table reached two
-- ways, math.random, a ROM patch that changes over time and queued button presses. Every
-- 25 frames it logs all of it; modtest STATE=<a>:<b> loads a state and checks that the
-- messages after the load are the same as before it.
mod = { name = "state keep (test)", games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" } }

count = 0                                   -- a global
local ticks, last = 0, -1                   -- locals that two functions share
local bag = setmetatable({ n = 0, list = {} }, { __index = function(_, k) return "no " .. k end })
local alias = bag.list                      -- the same table reached a second way
local patch_at, patch_orig

local function step()
  ticks = ticks + 1
  last = math.random(1000)
end

local function note()
  bag.n = bag.n + 1
  alias[#alias + 1] = last
  if #alias > 4 then table.remove(alias, 1) end
end

function on_load()
  math.randomseed(1234)
  patch_at = nba.rom_find({ 0x09CA })[1]
  if patch_at then patch_orig = nba.rom_read16(patch_at) end
end

function on_frame(f)
  count = count + 1
  step()
  if f % 7 == 0 then note() end
  if patch_at then nba.rom_write16(patch_at, (patch_orig + count) & 0xffff) end
  if f % 40 == 0 then nba.press(1, "start", 3) end
  if f % 25 == 0 then
    nba.log(string.format("keep: frame %d count %d ticks %d last %d bag %d [%s] %s rom %s",
      f, count, ticks, last, bag.n, table.concat(alias, ","), bag.missing,
      patch_at and string.format("%04x", nba.rom_read16(patch_at)) or "-"))
  end
end
