-- codes_te.lua - the cabinet codes of NBA JAM Tournament Edition and T.E. Special Edition as
-- switches (roadmap 3.1, 3.2). Each switch forces what the code would set; the tables are in
-- lib/codes_nbajamte.lua and lib/codes_nbajamse.lua (the same codes; SE has more secret players).
local codes = nba.include("lib/codes.lua")
local T = codes.table("nbajamte")
local SECRET = codes.secret_list(T, codes.table("nbajamse"))

-- tournament mode blocks these in the game (SCORE.ASM tournament_on clears the taps and quick
-- hands, resets the secret players, and team select skips the roster and switch codes), so
-- their switches are greyed while it is on
local BLOCKED = { "tournament" }

mod = {
  name = "Cabinet codes (T.E.)",
  description = "Turns on T.E. cabinet codes without entering them.",
  games = { "nbajamte", "nbajamse" },
  options = codes.options(T, { "tournament", "big_head", "huge_head", "max_power", "quick_hands",
      "goaltend", "shot_percent", "assist_off", "baby", "big_roster", "team_switch", "secret_players" },
    { goaltend = { "tournament", "huge_head" }, huge_head = BLOCKED, quick_hands = BLOCKED,
      big_roster = BLOCKED, team_switch = BLOCKED, secret_players = BLOCKED, secret_list = SECRET }),
}

local drive
function on_load()
  drive = codes.driver(codes.table(nba.game), SECRET)
  for _, e in ipairs(drive.on) do
    if e.player then nba.log(string.format("forcing secret_p%d %s (special_heads = %d)", e.player, e.c.id, e.value))
    else nba.log(string.format("forcing %s (%s = %d)", e.c.id, e.c.sym, e.value)) end
  end
end
function on_frame() drive.update() end
