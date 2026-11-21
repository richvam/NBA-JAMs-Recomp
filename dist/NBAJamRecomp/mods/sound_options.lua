-- sound_options.lua - in-game music off and announcer off (roadmap Phase 6). T.E. 4.0, SE 5.02.
--
-- These games have no code or setting for either. A sound is a table in the ROM: a priority
-- word with the channel in bits 8-10, a time, the sound code sent to the sound board, and so
-- on (SOUNDS.ASM snd_play); the script changes the code in those tables when the game is
-- loaded, never the game's memory.
--   Music off: the tunes of the four quarters and overtime ({ F3FF, 8, code, 0 }, codes 002,
--   006, 007, 008, 00B: BB.ASM, SCORE.ASM) send the game's own music off code instead
--   (musicoff_snd), so play is silent apart from the crowd and the sounds of the game.
--   Announcer off: every sound on channel 1, the announcer's, with a speech code (100-1FF)
--   sends 17F, which the sound board plays as nothing and which leaves the music playing
--   (measured with sndlog). The referee's whistle (148) is on that channel too and is kept.
-- The team select, halftime and end-of-game tunes are kept. An option takes effect from the
-- next game.
mod = {
  name = "Sound options",
  description = "Turns the music during play, or the announcer, off.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "music", label = "Music during play", type = "toggle", default = true },
    { id = "announcer", label = "Announcer", type = "toggle", default = true },
  },
}

local TUNES = { 0x8002, 0x8006, 0x8007, 0x8008, 0x800B }
local SILENT, WHISTLE = 0x817F, 0x8148

function on_load()
  local off = nba.sym("musicoff_snd")
  if not nba.option("music") then
    if not off then nba.log("sound options: music off is not available on this game")
    else
      local code, n = nba.rom_read16(off + 32), 0
      for _, t in ipairs(TUNES) do
        for _, a in ipairs(nba.rom_find({ 0xF3FF, 0x0008, t, 0x0000 })) do nba.rom_write16(a + 32, code); n = n + 1 end
      end
      nba.log(string.format("sound options: music off, %d tunes", n))
    end
  end
  if not nba.option("announcer") then
    local n = 0
    for w = 0, nba.rom_size() - 3 do
      local a = nba.ROM_BASE + 16 * w
      local p = nba.rom_read16(a)
      if p >> 12 == 0xF and (p >> 8) & 7 == 1 then
        local c = nba.rom_read16(a + 32)
        if c >> 8 == 0x81 and c ~= WHISTLE and c ~= SILENT and nba.rom_read16(a + 16) < 0x400 then
          nba.rom_write16(a + 32, SILENT); n = n + 1
        end
      end
    end
    nba.log(string.format("sound options: announcer off, %d sounds", n))
  end
end
