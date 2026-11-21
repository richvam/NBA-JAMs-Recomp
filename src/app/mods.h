// mods.h - Lua mod support. Scripts live in <app>/mods/*.lua; each one can be turned on
// for all games or for a chosen set of games, and may expose options for the mod menu.
// Settings are kept in <app>/mods.ini.
//
// A script declares itself with a global table and two optional hooks:
//
//   mod = {
//     name = "Quarter length", description = "...",
//     games = { "nbajamte", "nbajamse" },       -- optional: games the mod supports
//     options = { { id = "minutes", label = "Minutes per quarter",
//                   values = { 1, 2, 3, 4, 5 }, default = 3, format = "%d min" } },
//   }
// Option types (roadmap 1.3): type = "list" (the default: values/labels), "toggle" (on/off),
// "range" (min, max, step) and "players" (a tick per player 1 to 4, a bit mask). group = "..."
// puts a heading above the option; conflicts = { ids } greys it out while one of those options
// is away from its default, and then the script reads its default.
//   function on_load() ... end     -- game loaded, before the first reset (patch ROM here)
//   function on_frame(n) ... end   -- after every emulated frame
//   function on_option(id, v) ... end  -- a live = true option was changed during the game
//
// The `nba` table gives scripts access to the running game (see mods.cpp / mods/README.txt).
//
// Shared code (roadmap 1.1): nba.include("lib/name.lua") runs a file from mods/lib/ once per
// script and returns what it returns. Files in lib/ are not mods themselves.
// Symbols (roadmap 1.2): nba.sym("name") is the bit address of a name of the game's source
// for the running revision, from mods/sym/<game>_<program CRC-32>.lua, or nil.
// Saving (roadmap Phase 6, stats_export.lua): nba.save("name.csv", text) writes a file into
// <app>/stats/ and nothing else: a plain name (letters, digits, _ - .) ending .csv, .json,
// .jsonl or .txt, no folders, at most 1 MB, at most 16 saves per script in 600 frames. Scripts cannot
// read files back. { append = true, header = h } adds to the end instead (h first when it
// creates the file), up to 64 MB a file. nba.date() is the local date and time (nil online).
#pragma once
#include "machine.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

struct mod_option
{
	enum kind { list, toggle, range, players };
	kind type = list;
	std::string id, label, format, group;
	std::vector<std::string> conflicts;  // option ids that grey this one out when set
	bool live = false;                   // live = true: changed during a game through on_option (roadmap 1.4)
	std::vector<double> values;
	std::vector<std::string> labels;     // optional text for each value
	int def = 0;                         // index into values
	std::string value_text(int i) const;
};

// Drawing on the frontend overlay (roadmap 1.6), in game-screen pixels. The list is emptied
// before each on_frame, so a script draws again every frame; the game's frame buffer is never
// touched. Drawn by draw_mod_overlay (mod_draw.h).
struct mod_draw
{
	enum kind { text, rect, fill, line } type = text;
	int x = 0, y = 0, x2 = 0, y2 = 0, scale = 1;   // rect/fill: x2, y2 are the width and height
	u32 color = 0xffffffff;                        // 0xAARRGGBB
	std::string s;
};

struct mod_script
{
	std::string file;                    // file name inside mods/ (the settings key)
	std::string path;
	std::string name, description, error;
	std::vector<std::string> supported;  // empty = any game
	std::vector<mod_option> options;
	// found when the script is scanned (the 0.3 netplay guard used them; 1.7 replaced it)
	bool has_frame = false;              // defines on_frame
	bool writes_ram = false;             // mentions nba.write8 / write16 / write32
	// user settings
	bool enabled = false;
	bool all_games = true;
	std::vector<std::string> games;      // used when !all_games
	std::map<std::string, int> opt;      // option id -> index into values

	bool supports(const std::string &game) const;
	bool applies_to(const std::string &game) const;
	bool game_checked(const std::string &game) const;
	void set_game(const std::string &game, bool on);
	int option_index(const mod_option &o) const;    // the stored choice
	bool option_greyed(const mod_option &o) const;  // a conflicting option is away from its default
	int option_used(const mod_option &o) const;     // what the script reads: the default while greyed
	const mod_option *find_option(const std::string &id) const;
};

class mod_manager
{
public:
	void scan(const std::string &mods_dir, const std::string &ini_path);
	// the symbols of the running game (roadmap 1.2): loaded when a script first asks
	const std::map<std::string, u32> &symbols();
	std::string symbols_file() const;                            // the file they come from, for this game and revision
	static bool lib_path_ok(const std::string &rel);             // "lib/name.lua", nothing that leaves mods/lib/
	void save() const;
	std::vector<mod_script> &mods() { return m_mods; }

	// game session
	void start(const std::string &game_id, arcade_machine &m);  // runs on_load for every applicable mod
	void frame();                                                // runs on_frame
	// Changing options during a game (roadmap 1.4). Take used_values() before a change and
	// pass it to options_changed() after: on_option(id, value) runs for every live option
	// whose value as the script reads it has changed (a conflict can change another one).
	// Other options, and turning mods on or off, apply next game.
	static std::map<std::string, int> used_values(const mod_script &m);
	void options_changed(mod_script &m, const std::map<std::string, int> &before);
	bool is_running(const mod_script &m) const;
	// Input and drawing (roadmap 1.6). nba.press queues buttons; apply_presses ORs them into
	// the inputs of the next frame (before set_inputs) and counts them down. Online, pass
	// local_slot: only presses for that player are applied, into in.p[0], the local player's
	// input that the game sends to the others.
	void apply_presses(cabinet_inputs &in, int local_slot = -1);
	// After apply_presses and, online, after the players' inputs are merged (the same on every
	// computer), just before set_inputs: keeps each player's buttons for nba.input and clears
	// those of a player a script holds with nba.hold (roadmap 6.F2), so the game does not see them.
	void take_inputs(cabinet_inputs &in);
	const std::vector<mod_draw> &draws() const { return m_draw; }
	void stop();
	std::vector<std::string> take_log();                         // messages for the UI
	int active_count() const { return int(m_running.size()); }

	// Online games (roadmap 1.7; replaces the 0.3 guard). The host sends net_description()
	// with the game start: one line per script it runs for the game, with a hash of the
	// script and every library it names, and its option choices. A player calls net_apply()
	// before starting the game: exactly that set runs, with the host's options, or err names
	// the file that is missing or differs. Its own settings are kept aside, not saved, and
	// net_restore() puts them back after the game. set_netplay(true) gives scripts
	// nba.netplay = true.
	std::string net_description(const std::string &game_id) const;
	std::vector<std::string> active_names(const std::string &game_id) const;
	bool net_apply(const std::string &game_id, const std::string &desc, std::string &err);
	void net_restore();
	static u32 source_hash(const std::string &path, const std::string &dir);
	void set_netplay(bool on) { m_netplay = on; }
	// where nba.save writes: <mods folder>/../stats unless set (headless tests)
	void set_save_dir(const std::string &d) { m_save_dir = d; }
	const std::string &save_dir() const { return m_save_dir; }
	static bool save_name_ok(const std::string &name);

private:
	struct running
	{
		mod_script *mod;
		lua_State *L;
		bool has_frame;
	};
	std::vector<mod_script> m_mods;
	std::vector<running> m_running;
	std::string m_ini, m_dir;
	std::string m_save_dir;
	std::map<std::string, u32> m_syms;
	bool m_syms_loaded = false;
	std::string m_game;
	arcade_machine *m_mach = nullptr;
	u32 m_frame = 0;
	bool m_netplay = false;
	std::vector<std::string> m_log;
	struct press { int player; u32 buttons; int frames; };
	std::vector<press> m_press;
	u32 m_seen[4] = {};      // each player's buttons this frame (nba.input)
	int m_hold[4] = {};      // frames each player's buttons are still kept from the game (nba.hold)
	std::vector<mod_draw> m_draw;
	struct saved { bool enabled, all_games; std::vector<std::string> games; std::map<std::string, int> opt; };
	std::vector<saved> m_saved;      // a player's own settings during an online game

	lua_State *new_state(mod_script *mod, bool with_game);
	bool load_meta(mod_script &m);
	void load_settings();
	void report(const mod_script &m, const std::string &s);
	friend struct mod_api;
};
