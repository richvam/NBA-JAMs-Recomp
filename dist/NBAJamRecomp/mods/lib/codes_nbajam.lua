-- codes_nbajam.lua - the cabinet codes of NBA JAM 1993 rev 3.01 (roadmap 2.4).
-- No source: read from the disassembly (tools/dasm) at the places in "source"; written by
-- tools/codes_from_source.py; checked in modtest by tools/modtests/codes_te.lua (codes 3, 4,
-- 7, 10, 11). Kinds as in codes_nbajamte.lua. NBA JAM 1993 has no tournament mode, maximum
-- power, huge head or baby mode (T.E. added them). labels: es, en (roadmap 2.5).
return {
  { id = "shot_percent", label = { es = "Mostrar porcentaje de tiro", en = "Show shot percentage" }, original = "Tap any button 15 times or more, then turn the stick a full circle, on the matchup screen", sym = "show_shot", kind = "flag", when = "matchup", source = "disassembly FF906B80-FF906CA0 (OR of the stick directions = 0xF, taps >= 15)" },
  { id = "assist_off", label = { es = "Sin ayuda de la máquina", en = "Computer assistance off" }, original = "Tap any button exactly 10 times on the matchup screen", sym = "assistance_off", kind = "flag", when = "matchup", source = "disassembly FF908380-FF908530 (adjustment 0x1B, or a player's taps = 10)" },
  { id = "goaltend", label = { es = "Interferencia permitida", en = "Powerup goaltending" }, original = "Tap any button exactly 8 times on the matchup screen", sym = "p1taps", kind = "taps", value = 8, when = "matchup", effect = "the player's goaltends are let go 70% of the time", source = "disassembly FF8844F0-FF8845E0 (taps = 8, RNDPER 700)" },
  { id = "quick_hands", label = { es = "Manos rápidas", en = "Quick hands" }, original = "Tap any button 5 times or more on the matchup screen, then hold turbo, pass, shoot and down at tip-off", sym = "stlpower", kind = "players", when = "tipoff", source = "disassembly FF859270-FF859410 (controls = 0x72, taps >= 5)" },
  { id = "big_head", label = { es = "Cabeza grande", en = "Big head" }, original = "Hold up, turbo and pass at tip-off", sym = "plyrbighead", kind = "players", when = "tipoff", source = "disassembly FF859440-FF8594D0 (controls = 0x61)" },
}
