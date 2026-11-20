-- fast_boot.lua - start the game without the power-on wait (roadmap Phase 6).
--
-- The long CPU and RAM self-test is already skipped: the program's default DIP switches turn
-- the power-up test off. What is left is POWERCMOS (DIAG.ASM in the sources): it checks the
-- NVRAM, shows the version and coinage on a black screen, then waits in a fixed loop
-- ("CTMP0: MOVI 200H,A0 ... DSJS A0,CTMP_WAIT") before the game starts attract mode.
-- The script makes that loop count 1 instead of 0x200.
--
-- When the NVRAM was blank or bad (the first start, or after the NVRAM was reset), the
-- game restores factory settings and instead waits for a button ("errors detected -- any
-- button to continue"). With "Error prompt: Continue" the script makes the jump past that
-- prompt unconditional ("JRZ CTMP0" becomes "JRUC CTMP0"), so the game goes on with the
-- factory settings it has already written, and without the error sound.
--
-- Both are changes to the program in memory, found by pattern in POWERCMOS, so they act
-- only when the script is on at power-on.
mod = {
  name = "Fast boot",
  description = "Skips the wait on the power-on screen and goes straight to attract mode.",
  games = { "nbajamte", "nbajamse", "nbajam", "nbahangt", "nbamht" },
  options = {
    { id = "errors", label = "Error prompt", values = { 0, 1 },
      labels = { "Wait for a button", "Continue" }, default = 0 },
  },
}

function on_load()
  local p = nba.sym("POWERCMOS")
  if not p then nba.log("fast boot: not available on this game"); return end
  local wait
  for a = p, p + 0x4000, 16 do
    if nba.rom_read16(a) == 0x09C0 and nba.rom_read16(a + 16) == 0x0200 then wait = a; break end
  end
  if not wait then nba.log("fast boot: power-on wait not found"); return end
  nba.rom_write16(wait + 16, 1)
  nba.log("fast boot: power-on wait shortened")
  if nba.option("errors") == 1 then
    -- the short JRZ (0xCAxx) before the loop whose target is the loop's MOVI
    for a = wait - 16, wait - 0x800, -16 do
      local w = nba.rom_read16(a)
      local off = w & 0xFF
      if off >= 0x80 then off = off - 0x100 end
      if (w & 0xFF00) == 0xCA00 and off ~= 0 and a + 16 + off * 16 == wait then
        nba.rom_write16(a, 0xC000 | (w & 0xFF))
        nba.log("fast boot: error prompt skipped")
        return
      end
    end
    nba.log("fast boot: error prompt not found")
  end
end
