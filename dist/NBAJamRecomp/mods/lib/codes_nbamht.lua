-- codes_nbamht.lua - the cabinet codes of NBA Maximum Hangtime 1.3 (roadmap 2.2), from the sources.
-- Written by tools/codes_from_source.py; checked in modtest by tools/modtests/codes_mht.lua.
-- kind "number": a 3-digit code on the versus screen (turbo, shoot, pass presses), value = the
-- digits. When the screen ends the variable is 0 or 0xF for every player; with humans on both
-- teams both teams must enter it (tournament: any one player, refused with one human).
-- kind "players": bit 0-3 = player 1-4, set at once; "flag": nonzero = on.
-- All are cleared at game start and at game over. Tournament mode clears the others than
-- those marked tournament = "kept". Spanish labels: roadmap 2.5.
-- Not in 1.3 although named in the source: show hot spots (pup_showhotspots is only cleared;
-- its number code 030 is commented out).
return {
  { id = "tournament", label = { en = "Tournament mode" }, original = "111", sym = "pup_tournament", kind = "number", value = 111, when = "matchup", tournament = "this is tournament mode" },
  { id = "baby", label = { en = "Baby-sized players" }, original = "025", sym = "pup_baby", kind = "number", value = 25, when = "matchup", tournament = "cleared" },
  { id = "no_music", label = { en = "No music during play" }, original = "048", sym = "pup_nomusic", kind = "number", value = 48, when = "matchup", tournament = "kept" },
  { id = "goaltend", label = { en = "Goaltending allowed" }, original = "937", sym = "pup_goaltend", kind = "number", value = 937, when = "matchup", tournament = "cleared" },
  { id = "max_block", label = { en = "Shot blocking" }, original = "616", sym = "pup_maxblock", kind = "number", value = 616, when = "matchup", tournament = "cleared" },
  { id = "max_steal", label = { en = "Maximum steals" }, original = "709", sym = "pup_maxsteal", kind = "number", value = 709, when = "matchup", tournament = "cleared" },
  { id = "max_power", label = { en = "Maximum power" }, original = "802", sym = "pup_maxpower", kind = "number", value = 802, when = "matchup", tournament = "cleared" },
  { id = "max_speed", label = { en = "Maximum speed" }, original = "284", sym = "pup_maxspeed", kind = "number", value = 284, when = "matchup", tournament = "cleared" },
  { id = "hyper_speed", label = { en = "Hyper speed" }, original = "552", sym = "pup_hypspeed", kind = "number", value = 552, when = "matchup", tournament = "cleared" },
  { id = "stealth_turbo", label = { en = "Stealth turbo" }, original = "273", sym = "pup_trbstealth", kind = "number", value = 273, when = "matchup", tournament = "cleared" },
  { id = "infinite_turbo", label = { en = "Infinite turbo" }, original = "461", sym = "pup_trbinfinite", kind = "number", value = 461, when = "matchup", tournament = "cleared" },
  { id = "no_push", label = { en = "No pushing" }, original = "390", sym = "pup_nopush", kind = "number", value = 390, when = "matchup", tournament = "cleared" },
  { id = "fast_pass", label = { en = "Fast turbo passes" }, original = "120", sym = "pup_fastpass", kind = "number", value = 120, when = "matchup", tournament = "cleared" },
  { id = "lock_combo", label = { en = "Lock the number code" }, original = "Press down on the versus screen: the player's three icons stop changing", sym = "pup_lockcombo", kind = "players", when = "matchup", tournament = "cleared", source = "BB3.ASM sp_lockcombo" },
  { id = "big_head", label = { en = "Big head" }, original = "Hold up, turbo and pass together", sym = "pup_bighead", kind = "players", when = "matchup", conflicts = { "huge_head" }, tournament = "kept", source = "BB3.ASM sp_bighead" },
  { id = "huge_head", label = { en = "Huge head" }, original = "Up, up, pass, turbo (within 1 s)", sym = "pup_hugehead", kind = "players", when = "matchup", conflicts = { "big_head" }, tournament = "kept", source = "BB3.ASM sp_hugehead" },
  { id = "shot_percent", label = { en = "Show shot percentage" }, original = "Turn the stick a full circle clockwise from up (within 1 s); only the first player to do it", sym = "pup_showshotper", kind = "players", when = "matchup", tournament = "kept", source = "BB3.ASM sp_showshotper" },
  { id = "no_tag", label = { en = "No tag arrows" }, original = "Left, left, pass, turbo (within 1 s)", sym = "pup_notag", kind = "players", when = "matchup", tournament = "cleared", source = "BB3.ASM sp_notag" },
  { id = "no_drift", label = { en = "No drift on block jumps" }, original = "Down, down, shoot, turbo (within 1 s)", sym = "pup_nodrift", kind = "players", when = "matchup", tournament = "cleared", source = "BB3.ASM sp_nodrift" },
  { id = "no_assistance", label = { en = "Computer assistance off" }, original = "Hold right and press pass twice (within 1 s)", sym = "pup_noassistance", kind = "flag", when = "matchup", tournament = "cleared", source = "BB3.ASM sp_noassistance" },
  { id = "aba_ball", label = { en = "ABA ball" }, original = "Hold right and press shoot, turbo, pass (within 1 s)", sym = "pup_aba", kind = "flag", when = "matchup", tournament = "kept", source = "BB3.ASM sp_aba" },
  { id = "outdoor_court", label = { en = "Outdoor court" }, original = "Hold left and press turbo twice (within 1 s)", sym = "pup_court", kind = "flag", when = "matchup", tournament = "kept", source = "BB3.ASM sp_court" },
  { id = "special", label = { en = "Special combo (ABA ball, outdoor court, shot percentage, big head)" }, original = "Shoot, shoot, then hold shoot and turn the stick a full circle clockwise from up (within 1.3 s)", sym = "pup_aba", kind = "flag", when = "matchup", tournament = "kept", source = "BB3.ASM sp_special" },
}
