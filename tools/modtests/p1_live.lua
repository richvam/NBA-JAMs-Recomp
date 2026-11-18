-- p1_live.lua - Phase 1 review, task 1.4: a live option changed mid-game reaches on_option at
-- once and the script writes RAM; a non-live option does not call on_option.
mod = {
  name = "P1 live",
  options = {
    { id = "poke", label = "Poke", type = "toggle", default = false, live = true },
    { id = "slow", label = "Slow", values = { 1, 2 }, default = 1 },
  },
}
local scores
function on_load() scores = nba.sym("scores") end
function on_option(id, v)
  nba.log("on_option " .. id .. " " .. tostring(v) .. " at frame " .. nba.frame())
  if id == "poke" and v and scores then nba.write16(scores, 0x1234) end
end
