-- p1_options.lua - Phase 1 review, task 1.3: every option type reaches the script with the
-- value set, and the loser of a conflict reads its default while the winner is set.
mod = {
  name = "P1 options",
  options = {
    { id = "list", label = "List", values = { 1, 2, 3 }, default = 1 },
    { id = "flag", label = "Flag", type = "toggle", default = false, group = "Group A" },
    { id = "len", label = "Length", type = "range", min = 1, max = 5, step = 0.5, default = 1 },
    { id = "who", label = "Players", type = "players", default = 0, group = "Group B" },
    { id = "loser", label = "Loser", values = { 10, 20 }, default = 10, conflicts = { "flag" } },
    { id = "free", label = "Free", values = { 10, 20 }, default = 10 },
  },
}
function on_load()
  nba.log(string.format("list=%s flag=%s len=%s who=%s loser=%s free=%s",
    tostring(nba.option("list")), tostring(nba.option("flag")), tostring(nba.option("len")),
    tostring(nba.option("who")), tostring(nba.option("loser")), tostring(nba.option("free"))))
end
