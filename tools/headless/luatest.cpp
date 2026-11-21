// luatest.cpp - tests of the Lua mod engine that need no game data at all.
// A synthetic, all-zero program ROM stands in for the game; the scripts are written to a
// temporary folder by the test itself. Covers: option parsing, the sandbox, the instruction
// budget, the ROM patch API, per-frame hooks, the mods.ini round trip, shared libraries
// (nba.include, roadmap 1.1) and symbols (nba.sym, roadmap 1.2).
// usage: luatest        (exit code 0 = all passed)
#include "tunit.h"
#include "mods.h"
#include "mod_draw.h"
#include "romload.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;

static int g_fail = 0, g_run = 0;
#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; printf("FAIL  %s  (%s:%d)\n", msg, __FILE__, __LINE__); } else printf("ok    %s\n", msg); } while (0)

static void write_file(const fs::path &p, const std::string &s) { std::ofstream(p, std::ios::binary) << s; }

static std::unique_ptr<tunit_machine> make_machine(u16 planted_at_word, const u16 *words, size_t n)
{
	tunit_roms r;
	r.program.assign(0x100000, 0);
	for (size_t i = 0; i < n; i++) { r.program[(planted_at_word + i) * 2] = u8(words[i]); r.program[(planted_at_word + i) * 2 + 1] = u8(words[i] >> 8); }
	r.gfx.assign(0xc00000, 0);
	r.snd_u3.assign(0x20000, 0xff); r.snd_u12.assign(0x80000, 0xff); r.snd_u13.assign(0x80000, 0xff);
	return std::make_unique<tunit_machine>(std::move(r), tunit_protection::nbajamte);
}

static mod_script *find(mod_manager &mm, const std::string &file)
{
	for (auto &s : mm.mods()) if (s.file == file) return &s;
	return nullptr;
}

int main()
{
	fs::path dir = fs::temp_directory_path() / "nbajam_luatest";
	fs::remove_all(dir);
	fs::create_directories(dir);
	std::string d = dir.string(), ini = (dir / "mods.ini").string();

	// ---- scripts ----
	write_file(dir / "opts.lua", R"(
mod = {
  name = "Options test", description = "d", games = { "nbajamte", "nbajamse" },
  options = {
    { id = "minutes", label = "Minutes", values = { 1, 2, 5, 12 }, default = 5, format = "%d min" },
    { id = "mode", values = { 0, 1 }, labels = { "Off", "On" } },
    { id = "empty", values = {} },
    { label = "no id", values = { 1 } },
  },
}
)");
	write_file(dir / "sandbox.lua", R"(
mod = { name = "Sandbox" }
assert(os == nil and io == nil and package == nil and debug == nil, "library leaked")
assert(require == nil and dofile == nil and loadfile == nil and load == nil, "loader leaked")
assert(string.format("%d", 3) == "3" and math.floor(2.5) == 2 and table.concat({ 1, 2 }) == "12", "base libraries missing")
)");
	write_file(dir / "escape.lua", "mod = { name = 'Escape' }\nos.exit(1)\n");
	write_file(dir / "forever.lua", "mod = { name = 'Forever' }\nwhile true do end\n");
	write_file(dir / "syntax.lua", "mod = { name = \n");
	write_file(dir / "noname.lua", "mod = { }\n");
	write_file(dir / "patch.lua", R"(
mod = { name = "Patch", options = { { id = "v", values = { 7, 9 }, default = 9 } } }
local hits
function on_load()
  hits = nba.rom_find({ 0x1234, 0x5678, -1, 0x9abc })
  nba.log("found " .. #hits)
  nba.rom_write16(hits[1], nba.option("v"))
  nba.rom_write32(hits[1] + 16, 0xdeadbeef)
  assert(nba.rom_read16(hits[1]) == 9)
  assert(nba.rom_read32(hits[1] + 16) == 0xdeadbeef)
  assert(not pcall(nba.rom_read16, 0x1000), "out-of-range read accepted")
  assert(not pcall(nba.rom_write16, nba.ROM_BASE + 8, 1), "unaligned write accepted")
end
)");
	write_file(dir / "frames.lua", R"(
mod = { name = "Frames" }
local last = 0
function on_frame(n) last = n; if n == 3 then nba.log("frame3") end end
)");
	write_file(dir / "frame_loop.lua", R"(
mod = { name = "FrameLoop" }
function on_frame(n) if n == 2 then while true do end end end
)");
	write_file(dir / "frame_err.lua", R"(
mod = { name = "FrameErr" }
function on_frame(n) if n == 2 then error("boom") end end
)");
	write_file(dir / "notes.txt", "not a script");

	mod_manager mm;
	mm.scan(d, ini);

	// ---- scanning and metadata ----
	CHECK(mm.mods().size() == 10, "only .lua files are listed");
	mod_script *o = find(mm, "opts.lua");
	CHECK(o && o->error.empty(), "options script loads");
	if (o)
	{
		CHECK(o->name == "Options test" && o->description == "d", "name and description parsed");
		CHECK(o->supported.size() == 2 && o->supports("nbajamte") && !o->supports("nbajam"), "supported games parsed");
		CHECK(o->options.size() == 2, "options with no values or no id are dropped");
		if (o->options.size() == 2)
		{
			CHECK(o->options[0].id == "minutes" && o->options[0].values.size() == 4, "value list parsed");
			CHECK(o->options[0].def == 2 && o->option_index(o->options[0]) == 2, "default value selects index 2 (5 min)");
			CHECK(o->options[0].format == "%d min", "format string kept");
			CHECK(o->options[1].label == "mode", "label falls back to the id");
			CHECK(o->options[1].labels.size() == 2 && o->options[1].value_text(1) == "On", "value labels used");
		}
	}
	CHECK(find(mm, "noname.lua") && find(mm, "noname.lua")->name == "noname", "missing name falls back to the file name");
	CHECK(find(mm, "syntax.lua") && !find(mm, "syntax.lua")->error.empty(), "syntax error is reported, not fatal");

	// ---- sandbox ----
	CHECK(find(mm, "sandbox.lua") && find(mm, "sandbox.lua")->error.empty(), "os, io, package, debug, require, load* are not available");
	CHECK(find(mm, "escape.lua") && !find(mm, "escape.lua")->error.empty(), "os.exit is an error, the program keeps running");

	// ---- instruction budget ----
	mod_script *fv = find(mm, "forever.lua");
	CHECK(fv && fv->error.find("budget") != std::string::npos, "endless loop at load stops at the instruction budget");

	// ---- running against a synthetic ROM ----
	const u16 pattern[4] = { 0x1234, 0x5678, 0x0001, 0x9abc };
	auto m = make_machine(100, pattern, 4);
	for (auto &s : mm.mods()) s.enabled = true;
	mm.start("nbajamte", *m);
	std::vector<std::string> log = mm.take_log();
	auto has = [&](const std::string &t) { for (auto &l : log) if (l.find(t) != std::string::npos) return true; return false; };
	CHECK(has("found 1"), "rom_find with a wildcard word finds the planted pattern once");
	const std::vector<u8> &rom = *m->program_rom();
	CHECK(rom[200] == 9 && rom[201] == 0, "rom_write16 changed the loaded ROM (option value 9)");
	CHECK(rom[202] == 0xef && rom[203] == 0xbe && rom[204] == 0xad && rom[205] == 0xde, "rom_write32 wrote two words");
	CHECK(has("[Patch]") && !has("[Patch] error"), "ROM script ran without error");
	CHECK(fv && !fv->applies_to("nbajamte"), "a script that failed to load is never started");
	// opts.lua supports only nbajamte/nbajamse, so it ran; a game it does not list is skipped
	m = make_machine(100, pattern, 4);
	mm.start("nbajam", *m);
	CHECK(mm.active_count() > 0, "scripts with no game list run on any game");
	bool opts_running_on_jam = false;
	for (auto &s : mm.mods()) if (s.file == "opts.lua" && s.applies_to("nbajam")) opts_running_on_jam = true;
	CHECK(!opts_running_on_jam, "a script is not applied to a game it does not list");
	m = make_machine(100, pattern, 4);
	mm.start("nbajamte", *m);
	mm.take_log();

	// ---- per-frame hooks and their budget ----
	for (int i = 0; i < 4; i++) mm.frame();
	log = mm.take_log();
	CHECK(has("frame3"), "on_frame receives the frame number");
	CHECK(has("[FrameLoop] error, mod stopped"), "endless on_frame loop is stopped by the budget");
	CHECK(has("[FrameErr] error, mod stopped: ") && has("boom"), "error in on_frame stops that mod only");
	int before = mm.active_count();
	mm.frame();
	CHECK(mm.active_count() == before && mm.active_count() >= 1, "other mods keep running after one fails");
	mm.stop();
	CHECK(mm.active_count() == 0, "stop closes every script");

	// ---- online games (roadmap 1.7; the 0.3 guard is gone) ----
	write_file(dir / "ram.lua", "mod = { name = 'Ram' }\nfunction on_load() nba.write16(0x01000000, 1); nba.log('online ' .. tostring(nba.netplay)) end\n");
	mod_manager g;
	g.scan(d, "");
	for (auto &s : g.mods()) s.enabled = s.file == "ram.lua" || s.file == "frames.lua";
	m = make_machine(100, pattern, 4);
	g.set_netplay(true);
	g.start("nbajamte", *m);
	for (int i = 0; i < 4; i++) g.frame();
	log = g.take_log();
	CHECK(!has("[Ram] error") && has("online true") && has("frame3"), "online: RAM writes and on_frame run, and nba.netplay is true");
	g.set_netplay(false);
	g.start("nbajamte", *m);
	log = g.take_log();
	CHECK(has("online false"), "offline: nba.netplay is false");
	g.stop();
	{
		// NVRAM is off limits (roadmap 4.4): a write there is an error, RAM next to it is fine
		write_file(dir / "nv.lua", "mod = { name = 'Nv' }\nfunction on_load() local a = pcall(nba.write16, 0x01400000, 1); local b = pcall(nba.write32, 0x013FFFF0, 1); local c = pcall(nba.write8, 0x014FFFF8, 1); local d = pcall(nba.write16, 0x013FFFF0, 1); nba.log('nv ' .. tostring(a) .. ' ' .. tostring(b) .. ' ' .. tostring(c) .. ' ' .. tostring(d)) end\n");
		mod_manager nvm;
		nvm.scan(d, "");
		for (auto &s : nvm.mods()) s.enabled = s.file == "nv.lua";
		auto nm = make_machine(100, pattern, 4);
		std::vector<u8> nv0 = nm->nvram();
		nvm.start("nbajamte", *nm);
		log = nvm.take_log();
		CHECK(has("nv false false false true") && nm->nvram() == nv0, "nba.write8/16/32 refuse NVRAM (0x01400000-0x014FFFFF) and leave it unchanged");
		nvm.stop();
		std::filesystem::remove(dir / "nv.lua");
	}
	{
		// the host's set, applied on a player whose own settings differ, then given back
		mod_manager hostm, pl;
		hostm.scan(d, ""); pl.scan(d, "");
		for (auto &s : hostm.mods()) s.enabled = s.file == "ram.lua" || s.file == "opts.lua";
		mod_script *ho = find(hostm, "opts.lua");
		if (ho && !ho->options.empty()) ho->opt[ho->options[0].id] = 3;
		std::string desc = hostm.net_description("nbajamte");
		for (auto &s : pl.mods()) s.enabled = s.file == "frames.lua";
		std::string err;
		bool ok = pl.net_apply("nbajamte", desc, err);
		mod_script *po = find(pl, "opts.lua");
		bool same = ok && find(pl, "ram.lua")->enabled && po && po->enabled && !find(pl, "frames.lua")->enabled && po->option_index(po->options[0]) == 3;
		CHECK(same, ("sync: the player runs exactly the host's scripts with the host's options " + err).c_str());
		pl.net_restore();
		CHECK(find(pl, "frames.lua")->enabled && !find(pl, "ram.lua")->enabled && !find(pl, "opts.lua")->enabled, "sync: the player's own settings come back after the game");
		write_file(dir / "ram.lua", "mod = { name = 'Ram' }\nfunction on_load() nba.write16(0x01000000, 2) end\n");
		ok = pl.net_apply("nbajamte", desc, err);
		CHECK(!ok && err.find("ram.lua") != std::string::npos && err.find("not the same") != std::string::npos && find(pl, "frames.lua")->enabled,
		      "sync: a script that differs from the host's is named, and the player's settings stay");
		err.clear();
		ok = pl.net_apply("nbajamte", "missing.lua\t00000000\t\n", err);
		CHECK(!ok && err.find("missing.lua") != std::string::npos && err.find("not in your mods folder") != std::string::npos, "sync: a script the player does not have is named");
		write_file(dir / "ram.lua", "mod = { name = 'Ram' }\nfunction on_load() nba.write16(0x01000000, 1); nba.log('online ' .. tostring(nba.netplay)) end\n");
		std::string crlf = "mod = { name = 'Ram' }\r\nfunction on_load() nba.write16(0x01000000, 1); nba.log('online ' .. tostring(nba.netplay)) end\r\n";
		u32 h1 = mod_manager::source_hash((dir / "ram.lua").string(), d);
		write_file(dir / "ram.lua", crlf);
		CHECK(h1 == mod_manager::source_hash((dir / "ram.lua").string(), d), "sync: Windows and Unix line ends give the same hash");
	}

	// ---- mods.ini round trip ----
	mod_script *p = find(mm, "patch.lua");
	for (auto &s : mm.mods()) s.enabled = false;
	if (p) { p->enabled = true; p->all_games = false; p->games = { "nbajamse", "nbahangt" }; p->opt["v"] = 0; }
	if (o) { o->enabled = true; o->opt["minutes"] = 3; o->opt["mode"] = 1; }
	mm.save();
	mod_manager m2;
	m2.scan(d, ini);
	mod_script *p2 = find(m2, "patch.lua"), *o2 = find(m2, "opts.lua");
	CHECK(p2 && p2->enabled && !p2->all_games, "enabled flag and game list restored");
	CHECK(p2 && p2->games.size() == 2 && p2->games[0] == "nbajamse" && p2->games[1] == "nbahangt", "game list order restored");
	CHECK(p2 && p2->option_index(p2->options[0]) == 0, "option value restored (7)");
	CHECK(o2 && o2->option_index(o2->options[0]) == 3 && o2->option_index(o2->options[1]) == 1, "other script's options restored");
	CHECK(find(m2, "escape.lua") && !find(m2, "escape.lua")->enabled, "disabled scripts stay disabled");
	// an unknown option value in the ini is ignored, the default stays
	write_file(ini, "[opts.lua]\nenabled=1\nminutes=99\nmode=1\n");
	mod_manager m3;
	m3.scan(d, ini);
	mod_script *o3 = find(m3, "opts.lua");
	CHECK(o3 && o3->enabled && o3->option_index(o3->options[0]) == 2 && o3->option_index(o3->options[1]) == 1, "invalid option value falls back to the default");

	// ---- option types (roadmap 1.3) ----
	{
		fs::path td = fs::temp_directory_path() / "nbajam_luatest_types";
		fs::remove_all(td);
		fs::create_directories(td);
		std::string tini = (td / "mods.ini").string();
		write_file(td / "types.lua", R"(
mod = { name = "Types", options = {
  { id = "ot", type = "toggle", label = "Overtime", default = true, group = "Length" },
  { id = "speed", type = "range", min = 0.5, max = 2, step = 0.1, default = 1, group = "Rules" },
  { id = "fire", type = "players", default = 5, group = "Rules" },
  { id = "nofire", type = "toggle", conflicts = { "fire2" } },
  { id = "fire2", type = "players" },
  { id = "list", values = { 3, 4 }, default = 4 },
  { id = "badrange", type = "range", min = 5, max = 1 },
} }
function on_load()
  nba.log("v", tostring(nba.option("ot")), nba.option("speed"), nba.option("fire"), tostring(nba.option("nofire")), nba.option("list"))
end
)");
		mod_manager tm;
		tm.scan(td.string(), tini);
		mod_script *t = find(tm, "types.lua");
		CHECK(t && t->error.empty() && t->options.size() == 6, "types: toggle, range, players and list parse; a range with no values is left out");
		if (t && t->options.size() == 6)
		{
			auto &o = t->options;
			CHECK(o[0].type == mod_option::toggle && o[0].values.size() == 2 && t->option_index(o[0]) == 1 && o[0].group == "Length", "types: a toggle is { 0, 1 }, default true is on, its group is kept");
			CHECK(o[1].type == mod_option::range && o[1].values.size() == 16 && t->option_index(o[1]) == 5 && o[1].value_text(15) == "2", "types: a range has min..max by step (0.5 to 2 by 0.1 is 16 values), its default found");
			CHECK(o[2].type == mod_option::players && o[2].values.size() == 16 && t->option_index(o[2]) == 5 && o[2].value_text(5) == "1, 3" && o[2].value_text(0) == "None", "types: players is a bit mask 0 to 15, shown as the players ticked");
			CHECK(!t->option_greyed(o[3]), "types: conflicts: not greyed while the other option is at its default");
			t->opt["nofire"] = 1; t->opt["fire2"] = 2;
			CHECK(t->option_greyed(o[3]) && t->option_used(o[3]) == 0 && t->option_index(o[3]) == 1, "types: conflicts: greyed once the other is set; the script reads the default, the choice is kept");
			// every type round-trips through mods.ini
			t->enabled = true; t->opt["ot"] = 0; t->opt["speed"] = 12; t->opt["fire"] = 10; t->opt["list"] = 0;
			tm.save();
			mod_manager t2;
			t2.scan(td.string(), tini);
			mod_script *u = find(t2, "types.lua");
			CHECK(u && u->option_index(u->options[0]) == 0 && u->option_index(u->options[1]) == 12 && u->option_index(u->options[2]) == 10 &&
			      u->option_index(u->options[3]) == 1 && u->option_index(u->options[4]) == 2 && u->option_index(u->options[5]) == 0,
			      "types: mods.ini round-trips every type (toggle, range with a 0.1 step, players, list)");
			auto tm8 = make_machine(10, nullptr, 0);
			for (auto &x : t2.mods()) x.enabled = x.file == "types.lua";
			t2.start("nbajamte", *tm8);
			bool ok = false;
			for (auto &x : t2.take_log()) ok = ok || x.find("v false 1.7 10 false 3") != std::string::npos;
			CHECK(ok, "types: nba.option gives a toggle as a boolean, a range as a number, players as the mask, a greyed option as its default");
			t2.stop();
		}
		fs::remove_all(td);
	}

	// ---- live options (roadmap 1.4) ----
	{
		fs::path ld = fs::temp_directory_path() / "nbajam_luatest_live";
		fs::remove_all(ld);
		fs::create_directories(ld);
		write_file(ld / "live.lua", R"(
mod = { name = "Live", options = {
  { id = "fast", type = "toggle", live = true },
  { id = "who", type = "players", live = true, conflicts = { "block" } },
  { id = "block", type = "toggle" },
  { id = "later", values = { 1, 2 } },
} }
function on_option(id, v)
  nba.log("opt", id, tostring(v))
  if id == "fast" then nba.write16(0x01000000, v and 7 or 3) end
end
)");
		mod_manager lm;
		lm.scan(ld.string(), "");
		mod_script *l = find(lm, "live.lua");
		auto mach = make_machine(10, nullptr, 0);
		if (l)
		{
			l->enabled = true;
			lm.start("nbajamte", *mach);
			lm.take_log();
			auto set = [&](const char *id, int v) { auto b = mod_manager::used_values(*l); l->opt[id] = v; lm.options_changed(*l, b); return lm.take_log(); };
			auto lg = set("fast", 1);
			CHECK(lg.size() == 1 && lg[0].find("opt fast true") != std::string::npos && mach->peek16(0x01000000) == 7, "live: a live option calls on_option(id, value) at once, and its write lands");
			lg = set("later", 1);
			CHECK(lg.empty(), "live: an option without live = true does not call on_option (it applies next game)");
			set("who", 6);
			lg = set("block", 1);
			CHECK(lg.size() == 1 && lg[0].find("opt who 0") != std::string::npos, "live: a live option greyed out by a conflict reports its default");
			lg = set("block", 0);
			CHECK(lg.size() == 1 && lg[0].find("opt who 6") != std::string::npos, "live: and its kept choice when the conflict ends");
			CHECK(lm.is_running(*l), "live: the script keeps running");
			lm.stop();
		}
		CHECK(l != nullptr, "live: script found");
		fs::remove_all(ld);
	}

	// ---- shared libraries (roadmap 1.1) ----
	{
		fs::path ld = fs::temp_directory_path() / "nbajam_luatest_lib";
		fs::remove_all(ld);
		fs::create_directories(ld / "lib" / "sub");
		std::string l = ld.string();
		write_file(ld / "lib" / "util.lua", "loads = (loads or 0) + 1\nlocal M = {}\nfunction M.twice(x) return 2 * x end\nreturn M\n");
		write_file(ld / "lib" / "sub" / "deep.lua", "return { v = 7 }\n");
		write_file(ld / "lib" / "noreturn.lua", "x_from_lib = 5\n");
		write_file(ld / "lib" / "ramlib.lua", "local M = {}\nfunction M.poke() nba.write16(0x01000000, 1) end\nreturn M\n");
		write_file(ld / "lib" / "loop_a.lua", "return nba.include('lib/loop_b.lua')\n");
		write_file(ld / "lib" / "loop_b.lua", "return nba.include('lib/loop_a.lua')\n");
		write_file(ld / "outside.lua", "mod = { name = 'Outside' }\nreturn 1\n");
		write_file(ld / "user.lua", R"(
mod = { name = "User" }
local u = nba.include("lib/util.lua")
local u2 = nba.include("lib/util.lua")
assert(u.twice(21) == 42, "library function works")
assert(u == u2 and loads == 1, "a library runs once per script")
assert(nba.include("lib/sub/deep.lua").v == 7, "sub-folders of lib/ work")
assert(nba.include("lib/noreturn.lua") == true and x_from_lib == 5, "a library that returns nothing gives true")
for _, bad in ipairs({ "../outside.lua", "lib/../outside.lua", "outside.lua", "/etc/passwd", "lib/x.txt", "lib\\util.lua", "C:/x/lib/util.lua", "lib//util.lua" }) do
  assert(not pcall(nba.include, bad), "path accepted: " .. bad)
end
assert(not pcall(nba.include, "lib/missing.lua"), "a missing library is an error")
assert(nba.include("lib/loop_a.lua") ~= nil, "libraries that include each other stop")
)");
		write_file(ld / "uses_ramlib.lua", "mod = { name = 'UsesRamLib' }\nlocal r = nba.include(\"lib/ramlib.lua\")\n");
		mod_manager lm;
		lm.scan(l, "");
		bool listed_lib = false;
		for (auto &s : lm.mods()) listed_lib = listed_lib || s.file == "util.lua" || s.file == "deep.lua" || s.file == "noreturn.lua";
		CHECK(lm.mods().size() == 3 && !listed_lib, "include: files in mods/lib/ are not listed as mods");
		mod_script *u = find(lm, "user.lua");
		CHECK(u && u->error.empty(), ("include: a script uses a library, once, with sub-folders; ../, absolute and non-.lua paths are refused" + std::string(u && !u->error.empty() ? " - " + u->error : "")).c_str());
		mod_script *ur = find(lm, "uses_ramlib.lua");
		u32 before = ur ? mod_manager::source_hash(ur->path, l) : 0;
		write_file(ld / "lib" / "ramlib.lua", "local M = {}\nfunction M.poke() nba.write16(0x01000000, 2) end\nreturn M\n");
		CHECK(ur && before != mod_manager::source_hash(ur->path, l), "include: a change in a library changes the hash of the script that includes it (online check)");
		fs::remove_all(ld);
	}

	// ---- symbols (roadmap 1.2) ----
	{
		fs::path sd = fs::temp_directory_path() / "nbajam_luatest_sym";
		fs::remove_all(sd);
		fs::create_directories(sd / "sym");
		const u16 code[4] = { 0x09e0, 0x1111, 0x2222, 0x0300 };
		auto sm = make_machine(200, code, 4);
		const std::vector<u8> &prog = *sm->program_rom();
		char crc[16];
		snprintf(crc, sizeof crc, "%08x", crc32_of(prog.data(), prog.size()));
		const unsigned at = 0xff800000u + 200 * 16;
		char tbl[512];
		snprintf(tbl, sizeof tbl, "return {\n  scores = 0x010F2F30,\n  found = { addr = 0x01001000, pattern = { 0x09E0, -1, 0x2222 }, at = 0x%08X },\n"
		                          "  wrong = { addr = 0x01002000, pattern = { 0x09E0, 0x5555 }, at = 0x%08X },\n  [\"odd name\"] = 0x10,\n}\n", at, at);
		write_file(sd / "sym" / ("nbajamte_" + std::string(crc) + ".lua"), tbl);
		write_file(sd / "sym" / "nbajamte_00000000.lua", "return { scores = 1 }\n");
		write_file(sd / "symuser.lua", R"(
mod = { name = "SymUser" }
function on_load()
  assert(nba.sym("scores") == 0x010F2F30, "plain entry")
  assert(nba.sym("found") == 0x01001000, "entry whose pattern is in the ROM")
  assert(nba.sym("wrong") == nil, "entry whose pattern is not in the ROM is left out")
  assert(nba.sym("odd name") == 0x10 and nba.sym("nothing") == nil, "any name; unknown is nil")
  nba.log("symbols ok")
end
)");
		mod_manager sm2;
		sm2.scan(sd.string(), "");
		for (auto &s : sm2.mods()) s.enabled = true;
		sm2.start("nbajamte", *sm);
		auto lg = sm2.take_log();
		bool ok = false;
		std::string why;
		for (auto &x : lg) { ok = ok || x.find("symbols ok") != std::string::npos; if (x.find("error") != std::string::npos) why = x; }
		CHECK(ok, ("sym: the file is chosen by the program's CRC-32; plain and pattern-checked entries; a pattern not in the ROM is left out " + why).c_str());
		CHECK(sm2.symbols_file().find("nbajamte_" + std::string(crc)) != std::string::npos, "sym: the file name is <game>_<CRC-32 of the program>.lua");
		sm2.stop();
		// another revision (other CRC): no file, so no symbols, and no error
		const u16 other[1] = { 0x4242 };
		auto om = make_machine(10, other, 1);
		write_file(sd / "symnone.lua", "mod = { name = 'SymNone' }\nfunction on_load() assert(nba.sym('scores') == nil); nba.log('none ok') end\n");
		mod_manager sm3;
		sm3.scan(sd.string(), "");
		for (auto &s : sm3.mods()) s.enabled = s.file == "symnone.lua";
		sm3.start("nbajamte", *om);
		lg = sm3.take_log();
		ok = false;
		for (auto &x : lg) ok = ok || x.find("none ok") != std::string::npos;
		CHECK(ok, "sym: a revision with no symbol file gives nil, not an error");
		sm3.stop();
		fs::remove_all(sd);
	}

	// ---- game events, lib/jam.lua (roadmap 1.5) ----
	// The shipped library, a symbol file for a made-up program, and a driver script that writes
	// a game's state into RAM frame by frame: a demo, then matchup, tip-off, four quarters, the
	// buzzer, and a second game.
	{
		fs::path ed = fs::temp_directory_path() / "nbajam_luatest_events";
		fs::remove_all(ed);
		fs::create_directories(ed / "lib");
		fs::create_directories(ed / "sym");
		std::ifstream jf("dist/NBAJamRecomp/mods/lib/jam.lua", std::ios::binary);
		std::stringstream js; js << jf.rdbuf();
		write_file(ed / "lib" / "jam.lua", js.str());
		const u16 code[1] = { 0x5151 };
		auto em = make_machine(10, code, 1);
		const std::vector<u8> &prog = *em->program_rom();
		char crc[16];
		snprintf(crc, sizeof crc, "%08x", crc32_of(prog.data(), prog.size()));
		write_file(ed / "sym" / ("nbajamte_" + std::string(crc) + ".lua"),
		           "return { GAMSTATE = 0x01000000, inmatchup = 0x01000010, gmqrtr = 0x01000020, game_time = 0x01000040, scores = 0x01000080 }\n");
		write_file(ed / "driver.lua", R"(
mod = { name = "Driver" }
local jam = nba.include("lib/jam.lua")
local got = {}
function on_matchup() got[#got + 1] = "M" end
function on_game_start() got[#got + 1] = "S" end
function on_quarter(n) got[#got + 1] = "Q" .. n end
function on_game_end() got[#got + 1] = "E" end
-- { GAMSTATE, inmatchup, gmqrtr, game_time, score 1, score 2 } per frame
local T = 0x03000000
local seq = {
  { 1, 0, 0, T, 0, 0 }, { 1, 0, 0, T - 1, 0, 0 }, { 1, 0, 0, T - 2, 0, 0 },     -- demo game: nothing
  { 2, 1, 0, T, 0, 0 }, { 2, 1, 0, T, 0, 0 }, { 3, 0, 0, T, 0, 0 },             -- matchup screen
  { 3, 0, 0, T - 1, 0, 0 }, { 3, 0, 0, T - 2, 2, 0 }, { 3, 0, 0, 0, 2, 0 },     -- quarter 1
  { 3, 0, 1, 0, 2, 0 }, { 4, 0, 1, T, 2, 0 }, { 3, 0, 1, T, 2, 0 },             -- break (halftime-like screen)
  { 3, 0, 1, T - 1, 2, 0 }, { 3, 0, 1, 1, 2, 2 }, { 3, 0, 2, 0, 2, 2 },         -- quarter 2; counter moves at 0:00
  { 3, 0, 2, T, 2, 2 }, { 3, 0, 2, T - 1, 2, 2 }, { 3, 0, 2, 0, 4, 4 },         -- quarter 3
  { 3, 0, 3, T, 4, 4 }, { 3, 0, 3, T - 1, 4, 4 }, { 3, 0, 3, 0, 6, 6 },         -- quarter 4, tied
  { 3, 0, 4, T, 6, 6 }, { 3, 0, 4, T - 1, 6, 6 }, { 3, 0, 4, 0, 9, 6 },         -- overtime: buzzer, apart
  { 3, 0, 4, 0, 9, 6 }, { 1, 0, 4, 0, 9, 6 },                                   -- nothing more
  { 2, 1, 0, T, 0, 0 }, { 3, 0, 0, T, 0, 0 }, { 3, 0, 0, T - 1, 0, 0 },         -- a second game
  { 8, 0, 0, T - 1, 0, 0 },                                                     -- game over (left)
}
local i = 0
function on_frame()
  i = i + 1
  local s = seq[i]
  if not s then return end
  nba.write16(0x01000000, s[1] & 0xffff); nba.write16(0x01000010, s[2]); nba.write16(0x01000020, s[3])
  nba.write32(0x01000040, s[4]); nba.write16(0x01000080, s[5]); nba.write16(0x01000090, s[6])
  jam.update()
  if i == #seq then nba.log("events " .. table.concat(got, " ") .. (jam.available and "" or " unavailable")) end
end
)");
		mod_manager evm;
		evm.scan(ed.string(), "");
		CHECK(evm.mods().size() == 1, "events: lib/jam.lua is a library, not listed as a mod");
		for (auto &s : evm.mods()) s.enabled = true;
		evm.start("nbajamte", *em);
		for (int f = 0; f < 40; f++) evm.frame();
		auto lg = evm.take_log();
		std::string ev;
		for (auto &x : lg) if (x.find("events ") != std::string::npos) ev = x.substr(x.find("events ") + 7);
		CHECK(ev == "M S Q1 Q2 Q3 Q4 Q5 E M S Q1 E",
		      ("events: a demo gives none; matchup, tip-off, each quarter once (the clock's last tick is not a start), overtime, the buzzer with the scores apart, a second game, game over (got \"" + ev + "\")").c_str());
		evm.stop();
		// a game without a symbol file: no events, said once, no error
		const u16 other[1] = { 0x6161 };
		auto om = make_machine(10, other, 1);
		mod_manager evn;
		evn.scan(ed.string(), "");
		for (auto &s : evn.mods()) s.enabled = true;
		evn.start("nbajamte", *om);
		for (int f = 0; f < 40; f++) evn.frame();
		lg = evn.take_log();
		bool said = false, unavailable = false, err = false;
		for (auto &x : lg)
		{
			said = said || x.find("no game events for nbajamte") != std::string::npos;
			unavailable = unavailable || x.find("events  unavailable") != std::string::npos;
			err = err || x.find("error") != std::string::npos;
		}
		CHECK(said && unavailable && !err, "events: a revision without a symbol file says so, calls nothing, and is not an error");
		evn.stop();
		fs::remove_all(ed);
	}

	// ---- input and drawing (roadmap 1.6) ----
	{
		fs::path pd = fs::temp_directory_path() / "nbajam_luatest_press";
		fs::remove_all(pd);
		fs::create_directories(pd);
		write_file(pd / "pd.lua", R"(
mod = { name = "PD" }
function on_frame(f)
  if f == 1 then
    nba.press(1, "start", 3)
    nba.press(3, { "shoot", "turbo", "left" }, 1)
    assert(not pcall(nba.press, 1, "jump"), "unknown button")
    assert(not pcall(nba.press, 5, "start"), "player 5")
    assert(not pcall(nba.press, 1, "start", 0), "0 frames")
    nba.text(10, 20, "HELLO", 0xff0000, 2)
    nba.rect(1, 2, 30, 40, 0x80112233, true)
    nba.rect(1, 2, 30, 40, 0x00ff00)
    nba.line(0, 0, 9, 4)
    nba.log("queued")
  end
end
)");
		const u16 code[1] = { 0x7171 };
		auto pm = make_machine(10, code, 1);
		mod_manager pmm;
		pmm.scan(pd.string(), "");
		for (auto &s : pmm.mods()) s.enabled = true;
		pmm.start("nbajamte", *pm);
		pmm.frame();
		auto lg = pmm.take_log();
		bool queued = false;
		for (auto &x : lg) queued = queued || x.find("[PD] queued") != std::string::npos;
		CHECK(queued, "press: a name or a table of names; unknown buttons, player 5 and 0 frames are errors");
		int start_frames = 0;
		bool p3 = false, p3_once = true, others_clear = true;
		for (int f = 0; f < 5; f++)
		{
			cabinet_inputs in{};
			pmm.apply_presses(in);
			if (in.p[0].start) start_frames++;
			if (f == 0) p3 = in.p[2].shoot && in.p[2].turbo && in.p[2].left && !in.p[2].pass;
			else p3_once = p3_once && !in.p[2].shoot;
			others_clear = others_clear && !in.p[1].start && !in.p[3].start && !in.p[0].shoot;
		}
		CHECK(start_frames == 3 && p3 && p3_once && others_clear, "press: buttons are held for their frames on the right player, then let go");
		const auto &dl = pmm.draws();
		bool draws = dl.size() == 4 && dl[0].type == mod_draw::text && dl[0].s == "HELLO" && dl[0].color == 0xffff0000u && dl[0].scale == 2 &&
		             dl[1].type == mod_draw::fill && dl[1].color == 0x80112233u && dl[1].x2 == 30 && dl[1].y2 == 40 &&
		             dl[2].type == mod_draw::rect && dl[2].color == 0xff00ff00u && dl[3].type == mod_draw::line && dl[3].x2 == 9 && dl[3].y2 == 4;
		CHECK(draws, "draw: text, a filled half-transparent box, an outline and a line are queued (0xRRGGBB is opaque)");
		canvas cv(64, 48);
		cv.fill(0, 0, 64, 48, 0xff000000u);
		draw_mod_overlay(cv, dl);
		auto at = [&](int x, int y) { return cv.px[size_t(y) * 64 + size_t(x)]; };
		bool pix = at(1, 2) == 0xff00ff00u && at(30, 41) == 0xff00ff00u && at(5, 5) != 0xff000000u && at(5, 5) != 0xff00ff00u && at(9, 4) == 0xffffffffu && at(0, 0) == 0xffffffffu;
		CHECK(pix, "draw: the outline, the blended box and both ends of the line land on the overlay canvas");
		pmm.frame();
		CHECK(pmm.draws().empty(), "draw: the list is emptied before each on_frame (a script draws every frame)");
		// online: only presses for this computer's player, into its own input
		pmm.stop();
		pmm.start("nbajamte", *pm);
		pmm.frame();
		cabinet_inputs on{};
		pmm.apply_presses(on, 2);
		CHECK(on.p[0].shoot && on.p[0].turbo && !on.p[0].start && !on.p[2].shoot, "press online: only the local player's presses, sent as that player's own input");
		pmm.stop();
		fs::remove_all(pd);
	}

	// ---- reading and holding a player's buttons (nba.input, nba.hold; roadmap 6.F2) ----
	{
		fs::path hd = fs::temp_directory_path() / "nbajam_luatest_hold";
		fs::remove_all(hd);
		fs::create_directories(hd);
		write_file(hd / "hd.lua", R"(
mod = { name = "HD" }
function on_frame(f)
  local i = nba.input(2)
  nba.log(string.format("f%d shoot %s up %s coin %s", f, tostring(i.shoot), tostring(i.up), tostring(i.coin)))
  if f == 1 then
    nba.hold(2, 2)
    assert(not pcall(nba.hold, 0), "player 0")
    assert(not pcall(nba.hold, 2, 0), "0 frames")
    assert(not pcall(nba.input, 5), "player 5")
  end
end
)");
		const u16 code[1] = { 0x7171 };
		auto hm = make_machine(10, code, 1);
		mod_manager hmm;
		hmm.scan(hd.string(), "");
		for (auto &s : hmm.mods()) s.enabled = true;
		hmm.start("nbajamte", *hm);
		bool seen_p2 = true, held = true, others = true, after = false;
		for (int f = 0; f < 4; f++)
		{
			cabinet_inputs in{};
			in.p[1].shoot = true; in.p[1].up = true; in.p[1].coin = true; in.p[0].shoot = true;
			hmm.apply_presses(in);
			hmm.take_inputs(in);
			// frame() for f == 0 is the first on_frame (f1): it holds player 2 for the next two frames
			if (f == 1 || f == 2) held = held && !in.p[1].shoot && !in.p[1].up && in.p[1].coin;
			if (f == 3) after = in.p[1].shoot && in.p[1].up;
			others = others && in.p[0].shoot;
			hmm.frame();
		}
		auto lg = hmm.take_log();
		int seen = 0;
		bool err = false;
		for (auto &x : lg) { if (x.find("shoot true up true coin true") != std::string::npos) seen++; err = err || x.find("error") != std::string::npos; }
		seen_p2 = seen == 4;
		CHECK(seen_p2 && !err, "input: a script reads a player's buttons every frame, also while they are held; bad players and frames are errors");
		CHECK(held && after && others, "hold: the game does not get the held player's buttons (coin excepted) for the frames asked, then gets them; other players are not touched");
		hmm.stop();
		fs::remove_all(hd);
	}

	// ---- saving (nba.save, roadmap Phase 6: stats_export.lua) ----
	{
		fs::path sd = fs::temp_directory_path() / "nbajam_luatest_save";
		fs::remove_all(sd);
		fs::create_directories(sd / "mods");
		write_file(sd / "mods" / "sv.lua", R"(
mod = { name = "Save" }
function on_frame(f)
  if f == 1 then
    assert(nba.save("box_1.csv", "team,pts\n1,12\n") == true, "save failed")
    assert(nba.save("box_1.csv", "team,pts\n1,14\n") == true, "overwrite failed")
    assert(nba.save("game.json", "{}") == true, "json failed")
    for _, bad in ipairs({ "../up.csv", "sub/x.csv", "sub\\x.csv", ".hidden.csv", "run.exe", "noext", "", "a b.csv",
                           string.rep("a", 61) .. ".csv", "C:x.csv" }) do
      assert(not pcall(nba.save, bad, "x"), "bad name accepted: " .. bad)
    end
    assert(not pcall(nba.save, "big.txt", string.rep("x", 1048577)), "too big accepted")
    assert(nba.save("log.csv", "1\n", { append = true, header = "n\n" }))
    assert(nba.save("log.csv", "2\n", { append = true, header = "n\n" }))
    assert(type(nba.date()) == "string" and nba.date():match("^%d%d%d%d%-%d%d%-%d%d %d%d:%d%d:%d%d$"), "date")
    for i = 1, 11 do assert(nba.save("n" .. i .. ".txt", "x")) end
    nba.log(pcall(nba.save, "over.txt", "x") and "17th allowed" or "17th refused")
  end
  if f == 700 then nba.log(pcall(nba.save, "later.txt", "x") and "later allowed" or "later refused") end
end
)");
		const u16 code[1] = { 0x7171 };
		auto sm = make_machine(10, code, 1);
		mod_manager smm;
		smm.scan((sd / "mods").string(), "");
		bool loaded = smm.mods().size() == 1 && smm.mods()[0].error.empty();
		for (auto &s : smm.mods()) { s.enabled = true; if (!s.error.empty()) printf("save script: %s\n", s.error.c_str()); }
		smm.start("nbajamte", *sm);
		for (int f = 0; f < 701; f++) { sm->run_frame(); smm.frame(); }
		std::string all;
		for (auto &l : smm.take_log()) all += l + "\n";
		auto rd = [&](const char *n) { std::ifstream f(sd / "stats" / n, std::ios::binary); std::stringstream b; b << f.rdbuf(); return b.str(); };
		CHECK(loaded && all.find("rror") == std::string::npos && all.find("failed") == std::string::npos, "save: no script errors");
		CHECK(rd("box_1.csv") == "team,pts\n1,14\n" && rd("game.json") == "{}" && rd("log.csv") == "n\n1\n2\n", "save: files land in stats/ next to mods/, an overwrite replaces the file, append writes the header once");
		size_t nfiles = 0; bool tmp = false;
		std::error_code dec;
		for (auto &e : fs::directory_iterator(sd / "stats", dec)) { nfiles++; tmp = tmp || e.path().extension() == ".tmp"; }
		CHECK(nfiles == 15 && !tmp, "save: only files within the rules are written (15), no .tmp is left");
		CHECK(all.find("17th refused") != std::string::npos && all.find("later allowed") != std::string::npos, "save: at most 16 saves in 600 frames, then allowed again");
		bool outside = false;
		for (auto &e : fs::recursive_directory_iterator(sd, dec))
		{
			std::string r = fs::relative(e.path(), sd).string();
			if (r.rfind("mods", 0) != 0 && r.rfind("stats", 0) != 0) outside = true;
		}
		CHECK(!outside, "save: nothing is written outside stats/");
		smm.stop();
		fs::remove_all(sd);
	}

	fs::remove_all(dir);
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
