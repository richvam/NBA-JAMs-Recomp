-- net_ram.lua - roadmap 1.7 check: a script that writes RAM every frame, with an option that
-- changes what it writes. Two nettest peers with different local settings must still end with
-- equal state hashes, because the player runs the host's set. With a copy of
-- dist/NBAJamRecomp/mods/sym next to it:
--   MODS=<folder> MODS_ON=net_ram.lua MODOPT=net_ram.lua:step=2 nettest <base> nbajamte host 7901 10000 4
--   MODS=<folder> MODS_ON=none nettest <base> nbajamte client 127.0.0.1 7901 10000
mod = {
  name = "Net RAM test",
  games = { "nbajamte", "nbajamse", "nbamht" },
  options = { { id = "step", label = "Step", values = { 1, 2, 3 }, default = 1 } },
}
local clock, step
function on_load()
  clock = nba.sym("game_time")
  step = nba.option("step")
  nba.log("online " .. tostring(nba.netplay) .. ", step " .. step)
end
function on_frame(f)
  -- a RAM word the game keeps: the clock, nudged differently for each step value
  if clock and f % 7 == 0 then nba.write16(clock, (f * step) & 0xffff) end
end
