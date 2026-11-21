-- stats_export.lua - the box score of every finished game, saved to stats/ (roadmap Phase 6).
-- T.E. 4.0 and SE 5.02.
--
-- When a game reaches its end-of-game screen (GAMSTATE 8, after play), the script adds:
--   stats/<game>_box.csv     one row per player: the teams, the score and the player's stats
--   stats/<game>_games.jsonl one line per game, the same box score as JSON (JSON Lines)
-- Both files grow, one game after another; the CSV header is written when the file is new.
-- Stats: player_stats, 14 words per player (GAME.EQU PS_*): 2-point and 3-point tries and
-- makes, dunks, blocks, offensive and defensive rebounds, steals, free throws, assists.
-- A dunk is counted as a 2-point shot as well. Points = 2 x 2-point + 3 x 3-point makes +
-- free throws made; the log says whether they add up to the game's score.
-- Players 1-2 are team 1 and 3-4 team 2 (team1, team2: the chosen team, numbered as in the
-- game's team list). "human": the player's PSTATUS bit was set at some time during play.
-- Team names come from the game's own list of team names in the ROM (RECORD.ASM), found by its
-- first name; if it is not found, teams are written as "TEAM n".
-- The date is the computer's local time; it is left empty in an online game (nba.date()).
-- The script writes nothing into the game.
mod = {
  name = "Stats export",
  description = "Saves the box score of each finished game to the stats folder as CSV and JSON.",
  games = { "nbajamte", "nbajamse" },
  options = {
    { id = "files", label = "Files", values = { 0, 1, 2 }, labels = { "CSV and JSON", "CSV only", "JSON only" }, default = 0 },
  },
}

local ps, gs, sc, t1, t2, q, pst, names
local played, number, humans = false, 0, 0

local COLS = "date,number,game,periods,team,team_name,team_score,opp_score,player,human," ..
             "pts,fgm,fga,fg3m,fg3a,dunks_made,dunks_tried,ftm,fta,oreb,dreb,ast,stl,blk\n"

-- a NUL-ended string in the ROM at bit address a
local function rom_string(a)
  local s = {}
  for i = 0, 63 do
    local b = a + 8 * i
    local w = nba.rom_read16(b & ~15)
    local c = (b & 8) ~= 0 and (w >> 8) or (w & 0xFF)
    if c == 0 then break end
    s[#s + 1] = string.char(c)
  end
  return table.concat(s)
end

-- RECORD.ASM #team_names: 27 pointers, the first to "ATLANTA HAWKS"
local function find_team_names()
  local at = nba.rom_find({ 0x5441, 0x414C, 0x544E, 0x2041, 0x4148, 0x4B57, 0x0053 })
  for _, s in ipairs(at) do
    for _, p in ipairs(nba.rom_find({ s & 0xFFFF, s >> 16 })) do
      local ok, t = true, {}
      for i = 0, 26 do
        local v = nba.rom_read32(p + 32 * i)
        if v < nba.ROM_BASE or (v & 7) ~= 0 then ok = false; break end
        t[i] = rom_string(v)
        if #t[i] < 4 then ok = false; break end
      end
      if ok then return t end
    end
  end
end

function on_load()
  ps, gs, sc = nba.sym("player_stats"), nba.sym("GAMSTATE"), nba.sym("scores")
  t1, t2, q, pst = nba.sym("team1"), nba.sym("team2"), nba.sym("gmqrtr"), nba.sym("PSTATUS")
  if not (ps and gs and sc) then nba.log("stats export: not available on this game"); return end
  names = find_team_names()
  nba.log(names and "stats export: team names found" or "stats export: team names not found")
end

local function stat(k, i) return nba.read16(ps + 16 * (14 * k + i)) end

local function csv_field(s)
  if s:find('[,"\n]') then return '"' .. s:gsub('"', '""') .. '"' end
  return s
end

local function json_str(s) return '"' .. s:gsub('[%c"\\]', function(c) return string.format("\\u%04x", c:byte()) end) .. '"' end

local function save()
  number = number + 1
  local date = nba.date() or ""
  local score = { nba.read16(sc), nba.read16(sc + 16) }
  local team = { t1 and nba.read16(t1) or -1, t2 and nba.read16(t2) or -1 }
  -- at the end-of-game screen gmqrtr has already moved past the last period: 4 after four quarters
  local periods = q and math.max(nba.read16(q), 1) or 4
  local st = humans
  local csv, teams, pts = {}, {}, { 0, 0 }
  for tm = 1, 2 do
    local tn = names and names[team[tm]] or ("TEAM " .. team[tm])
    local players = {}
    for j = 0, 1 do
      local k = 2 * (tm - 1) + j
      local s = {}
      for i = 0, 13 do s[i] = stat(k, i) end
      local p = {
        player = k + 1, human = (st >> k) & 1 == 1,
        pts = 2 * s[1] + 3 * s[3] + s[11], fgm = s[1] + s[3], fga = s[0] + s[2], fg3m = s[3], fg3a = s[2],
        dunks_made = s[5], dunks_tried = s[4], ftm = s[11], fta = s[10],
        oreb = s[7], dreb = s[8], ast = s[12], stl = s[9], blk = s[6],
      }
      pts[tm] = pts[tm] + p.pts
      csv[#csv + 1] = table.concat({ csv_field(date), number, nba.game, periods, tm, csv_field(tn), score[tm], score[3 - tm],
        p.player, p.human and 1 or 0, p.pts, p.fgm, p.fga, p.fg3m, p.fg3a, p.dunks_made, p.dunks_tried,
        p.ftm, p.fta, p.oreb, p.dreb, p.ast, p.stl, p.blk }, ",") .. "\n"
      players[#players + 1] = string.format('{"player":%d,"human":%s,"pts":%d,"fgm":%d,"fga":%d,"fg3m":%d,"fg3a":%d,' ..
        '"dunks_made":%d,"dunks_tried":%d,"ftm":%d,"fta":%d,"oreb":%d,"dreb":%d,"ast":%d,"stl":%d,"blk":%d}',
        p.player, tostring(p.human), p.pts, p.fgm, p.fga, p.fg3m, p.fg3a, p.dunks_made, p.dunks_tried,
        p.ftm, p.fta, p.oreb, p.dreb, p.ast, p.stl, p.blk)
    end
    teams[tm] = string.format('{"team":%d,"name":%s,"score":%d,"players":[%s]}', tm, json_str(tn), score[tm], table.concat(players, ","))
  end
  local files = nba.option("files")
  local ok, err = true, nil
  if files ~= 2 then ok, err = nba.save(nba.game .. "_box.csv", table.concat(csv), { append = true, header = COLS }) end
  if ok and files ~= 1 then
    ok, err = nba.save(nba.game .. "_games.jsonl", string.format('{"date":%s,"number":%d,"game":%s,"periods":%d,"teams":[%s]}\n',
      json_str(date), number, json_str(nba.game), periods, table.concat(teams, ",")), { append = true })
  end
  if not ok then nba.log("stats export: " .. tostring(err)); return end
  nba.log(string.format("stats export: game %d saved, %s %d - %d %s", number,
    names and names[team[1]] or "?", score[1], score[2], names and names[team[2]] or "?"))
  if pts[1] == score[1] and pts[2] == score[2] then nba.log("stats export: points add up to the score")
  else nba.log(string.format("stats export: points %d-%d differ from the score %d-%d", pts[1], pts[2], score[1], score[2])) end
end

function on_frame()
  if not (ps and gs and sc) then return end
  local g = nba.read16(gs)
  if g == 3 then
    if not played then humans = 0 end
    played = true
    -- PSTATUS during play (it is cleared by the end of the game): who joined at any time
    if pst then humans = humans | (nba.read16(pst) & 15) end
  elseif g == 8 and played then played = false; save() end
end
