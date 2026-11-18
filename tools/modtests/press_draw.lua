-- press_draw.lua - roadmap 1.6 check: scripted start presses reach team select, and text,
-- a box and a line are drawn on the overlay. With a copy of dist/NBAJamRecomp/mods/sym:
--   SHOT=3100:/tmp/overlay.png build/linux/modtest <prog_lo> <prog_hi> <folder> nbajamte 3200
-- In modtest the game waits at power-on (GAMSTATE 0) until a button is seen, so it takes one
-- press to reach attract mode and one more to start.
mod = { name = "Press and draw" }
local sel, gs, presses, seen = nil, nil, 0, false

function on_load() sel = nba.sym("in_team_select"); gs = nba.sym("GAMSTATE") end

function on_frame(f)
  if not seen and f >= 1000 and f % 200 == 0 then
    nba.press(1, "start", 4)
    presses = presses + 1
    nba.log(string.format("frame %d: start pressed for player 1 (GAMSTATE %d)", f, gs and nba.read16(gs) or -1))
  end
  if sel and not seen and nba.read16(sel) ~= 0 then
    seen = true
    nba.log(string.format("team select reached at frame %d after %d presses", f, presses))
  end
  nba.rect(20, 20, 200, 40, 0x800000a0, true)   -- half-transparent blue box behind the text
  nba.rect(20, 20, 200, 40, 0xffff00)           -- yellow outline
  nba.text(28, 28, "MOD OVERLAY", 0xffffff, 2)
  nba.line(20, 70, 180, 110, 0x00ff00)
end
