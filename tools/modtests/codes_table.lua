-- codes_table.lua - roadmap 2.1 check: lib/codes_<game>.lua loads, every id is unique,
-- and every symbol it names exists in the running revision's symbol file (unless the entry
-- says why not, in sym_note).
mod = { name = "Codes table check", games = { "nbajamte", "nbajamse", "nbamht", "nbahangt", "nbajam" } }
function on_load()
  local t = nba.include("lib/codes_" .. nba.game .. ".lua")
  local ids, bad, kinds = {}, {}, {}
  for _, c in ipairs(t) do
    if ids[c.id] then bad[#bad + 1] = "duplicate id " .. c.id end
    ids[c.id] = true
    kinds[c.kind] = (kinds[c.kind] or 0) + 1
    if not nba.sym(c.sym) and not c.sym_note then bad[#bad + 1] = c.id .. ": no symbol " .. c.sym end
    if not (c.label and c.label.en and c.label.es and c.original and c.when) then bad[#bad + 1] = c.id .. ": missing a field" end
  end
  local k = {}
  for n, v in pairs(kinds) do k[#k + 1] = n .. " " .. v end
  table.sort(k)
  nba.log(string.format("%d entries (%s), %d problems", #t, table.concat(k, ", "), #bad))
  for _, b in ipairs(bad) do nba.log("problem: " .. b) end
end
