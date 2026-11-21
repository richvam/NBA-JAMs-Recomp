// mods.cpp - Lua mod support (see mods.h). Scripts run in a sandbox: only the base, table,
// string, math and utf8 libraries are opened (no io, os, package or debug), and every call
// into a script has an instruction budget so a broken loop cannot hang the emulator.
#include "mods.h"
#include "romload.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// UTF-8 paths on every platform (as in romload.cpp)
static std::string u8s(const fs::path &p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
static fs::path P(const std::string &s) { return fs::path(std::u8string(s.begin(), s.end())); }

// load a script through our own file reader, so non-ASCII folders work on Windows too
static int load_script(lua_State *L, const std::string &path, const std::string &name)
{
	std::vector<u8> d;
	if (!read_whole_file(path, d)) { lua_pushstring(L, ("cannot read " + path).c_str()); return LUA_ERRFILE; }
	return luaL_loadbufferx(L, reinterpret_cast<const char *>(d.data()), d.size(), ("@" + name).c_str(), "t");
}

static const u32 kRomBase = 0xff800000;
static const int kLoadBudget = 200000000;   // instructions for on_load / metadata
static const int kFrameBudget = 5000000;    // instructions per on_frame

// ---------------------------------------------------------------------------
// option / script helpers
// ---------------------------------------------------------------------------
std::string mod_option::value_text(int i) const
{
	if (i < 0 || i >= int(values.size())) return "?";
	if (i < int(labels.size()) && !labels[size_t(i)].empty()) return labels[size_t(i)];
	char b[64];
	double v = values[size_t(i)];
	if (type == toggle) return v != 0 ? "On" : "Off";
	if (type == players)
	{
		std::string t;
		for (int p = 0; p < 4; p++) if (int(v) & (1 << p)) t += (t.empty() ? "" : ", ") + std::to_string(p + 1);
		return t.empty() ? "None" : t;
	}
	if (!format.empty())
	{
		if (format.find("%d") != std::string::npos || format.find("%i") != std::string::npos) snprintf(b, sizeof b, format.c_str(), int(v));
		else snprintf(b, sizeof b, format.c_str(), v);
	}
	else if (v == static_cast<double>(static_cast<long long>(v))) snprintf(b, sizeof b, "%lld", (long long)v);
	else snprintf(b, sizeof b, "%g", v);
	return b;
}

bool mod_script::supports(const std::string &g) const
{
	return supported.empty() || std::find(supported.begin(), supported.end(), g) != supported.end();
}

bool mod_script::game_checked(const std::string &g) const
{
	return std::find(games.begin(), games.end(), g) != games.end();
}

bool mod_script::applies_to(const std::string &g) const
{
	if (!enabled || !error.empty() || !supports(g)) return false;
	return all_games || game_checked(g);
}

void mod_script::set_game(const std::string &g, bool on)
{
	auto it = std::find(games.begin(), games.end(), g);
	if (on && it == games.end()) games.push_back(g);
	if (!on && it != games.end()) games.erase(it);
}

int mod_script::option_index(const mod_option &o) const
{
	auto it = opt.find(o.id);
	int i = it == opt.end() ? o.def : it->second;
	return std::clamp(i, 0, std::max(0, int(o.values.size()) - 1));
}

const mod_option *mod_script::find_option(const std::string &id) const
{
	for (const auto &o : options) if (o.id == id) return &o;
	return nullptr;
}

bool mod_script::option_greyed(const mod_option &o) const
{
	for (const auto &c : o.conflicts)
		if (const mod_option *w = find_option(c))
			if (w != &o && option_index(*w) != w->def) return true;
	return false;
}

int mod_script::option_used(const mod_option &o) const
{
	return option_greyed(o) ? std::clamp(o.def, 0, std::max(0, int(o.values.size()) - 1)) : option_index(o);
}

// the value list entry closest to v, if it is v up to rounding (a range step such as 0.1
// written to mods.ini and read back); -1 if none
static int value_slot(const std::vector<double> &vals, double v)
{
	for (size_t i = 0; i < vals.size(); i++)
		if (std::fabs(vals[i] - v) <= 1e-9 * std::max(1.0, std::fabs(v))) return int(i);
	return -1;
}

// ---------------------------------------------------------------------------
// the `nba` API seen by scripts
// ---------------------------------------------------------------------------
struct mod_api
{
	static mod_manager *mgr(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_mgr");
		auto *m = static_cast<mod_manager *>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return m;
	}
	static mod_script *script(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_mod");
		auto *m = static_cast<mod_script *>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return m;
	}
	static arcade_machine &mach(lua_State *L)
	{
		mod_manager *m = mgr(L);
		if (!m || !m->m_mach) luaL_error(L, "no game is running");
		return *m->m_mach;
	}
	static std::vector<u8> &rom(lua_State *L)
	{
		std::vector<u8> *r = mach(L).program_rom();
		if (!r || r->empty()) luaL_error(L, "this game has no program ROM access");
		return *r;
	}
	static size_t rom_word(lua_State *L, int arg)
	{
		lua_Integer a = luaL_checkinteger(L, arg);
		u32 addr = u32(a);
		std::vector<u8> &r = rom(L);
		if (addr < kRomBase || (addr & 15) || (size_t(addr - kRomBase) >> 4) >= r.size() / 2)
			luaL_error(L, "ROM address 0x%08x out of range (bit addresses 0xff800000.., word aligned)", addr);
		return size_t(addr - kRomBase) >> 4;
	}
	static u16 rw(std::vector<u8> &r, size_t w) { return u16(r[w * 2] | (r[w * 2 + 1] << 8)); }
	static void ww(std::vector<u8> &r, size_t w, u16 v) { r[w * 2] = u8(v); r[w * 2 + 1] = u8(v >> 8); }

	static int log(lua_State *L)
	{
		int n = lua_gettop(L);
		std::string s;
		for (int i = 1; i <= n; i++) { if (i > 1) s += " "; s += luaL_tolstring(L, i, nullptr); lua_pop(L, 1); }
		if (mod_manager *m = mgr(L)) m->report(*script(L), s);
		return 0;
	}
public:
	// an option's value as the script reads it: toggle a boolean, players the mask, else a number
	static void push_option(lua_State *L, const mod_script &s, const mod_option &o)
	{
		double v = o.values.empty() ? 0 : o.values[size_t(s.option_used(o))];
		if (o.type == mod_option::toggle) lua_pushboolean(L, v != 0);
		else if (o.type == mod_option::players) lua_pushinteger(L, lua_Integer(v));
		else lua_pushnumber(L, v);
	}
	static int option(lua_State *L)
	{
		const char *id = luaL_checkstring(L, 1);
		mod_script *s = script(L);
		for (const auto &o : s->options)
			if (o.id == id) { push_option(L, *s, o); return 1; }
		return luaL_error(L, "unknown option '%s'", id);
	}
	static int frame(lua_State *L) { lua_pushinteger(L, mgr(L)->m_frame); return 1; }

	// ---- input and drawing (roadmap 1.6) ----
	static u32 button_bit(lua_State *L, const char *b)
	{
		static const char *names[] = { "up", "down", "left", "right", "shoot", "pass", "turbo", "start", "coin" };
		for (u32 i = 0; i < 9; i++) if (strcmp(b, names[i]) == 0) return 1u << i;
		luaL_error(L, "nba.press: unknown button '%s' (up, down, left, right, shoot, pass, turbo, start, coin)", b);
		return 0;
	}
	// nba.press(player, buttons, frames): player 1-4; buttons a name or a table of names; held
	// for `frames` frames (default 1), from the next frame on
	static int press(lua_State *L)
	{
		int player = int(luaL_checkinteger(L, 1));
		if (player < 1 || player > 4) return luaL_error(L, "nba.press: player is 1 to 4");
		u32 bits = 0;
		if (lua_istable(L, 2))
		{
			lua_Integer n = luaL_len(L, 2);
			for (lua_Integer i = 1; i <= n; i++) { lua_geti(L, 2, i); bits |= button_bit(L, luaL_checkstring(L, -1)); lua_pop(L, 1); }
		}
		else bits = button_bit(L, luaL_checkstring(L, 2));
		int frames = int(luaL_optinteger(L, 3, 1));
		if (frames < 1 || frames > 36000) return luaL_error(L, "nba.press: frames is 1 to 36000");
		mgr(L)->m_press.push_back({ player - 1, bits, frames });
		return 0;
	}
	static u32 color_arg(lua_State *L, int i)
	{
		lua_Integer c = luaL_optinteger(L, i, 0xffffff);
		u32 v = u32(c);
		return v <= 0xffffff ? (0xff000000u | v) : v;   // 0xRRGGBB is opaque
	}
	static int text(lua_State *L)
	{
		mod_draw d;
		d.type = mod_draw::text;
		d.x = int(luaL_checkinteger(L, 1)); d.y = int(luaL_checkinteger(L, 2));
		d.s = luaL_tolstring(L, 3, nullptr); lua_pop(L, 1);
		d.color = color_arg(L, 4);
		d.scale = int(luaL_optinteger(L, 5, 1));
		if (d.scale < 1 || d.scale > 4) return luaL_error(L, "nba.text: scale is 1 to 4");
		push_draw(L, d);
		return 0;
	}
	static int rect(lua_State *L)
	{
		mod_draw d;
		d.type = lua_toboolean(L, 6) ? mod_draw::fill : mod_draw::rect;
		d.x = int(luaL_checkinteger(L, 1)); d.y = int(luaL_checkinteger(L, 2));
		d.x2 = int(luaL_checkinteger(L, 3)); d.y2 = int(luaL_checkinteger(L, 4));
		d.color = color_arg(L, 5);
		push_draw(L, d);
		return 0;
	}
	static int line(lua_State *L)
	{
		mod_draw d;
		d.type = mod_draw::line;
		d.x = int(luaL_checkinteger(L, 1)); d.y = int(luaL_checkinteger(L, 2));
		d.x2 = int(luaL_checkinteger(L, 3)); d.y2 = int(luaL_checkinteger(L, 4));
		d.color = color_arg(L, 5);
		push_draw(L, d);
		return 0;
	}
	static void push_draw(lua_State *L, const mod_draw &d)
	{
		mod_manager *m = mgr(L);
		if (m->m_draw.size() >= 4096) luaL_error(L, "too many drawings in one frame (4096)");
		m->m_draw.push_back(d);
	}
	// nba.include("lib/name.lua"): a shared file from mods/lib/, run once per script; returns
	// what the file returns (true when it returns nothing), as require does
	static int include(lua_State *L)
	{
		std::string rel = luaL_checkstring(L, 1);
		if (!mod_manager::lib_path_ok(rel)) return luaL_error(L, "nba.include: only \"lib/<name>.lua\" files from mods/lib/ (got \"%s\")", rel.c_str());
		mod_manager *m = mgr(L);
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_included");
		if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setfield(L, LUA_REGISTRYINDEX, "nba_included"); }
		int cache = lua_gettop(L);
		lua_getfield(L, cache, rel.c_str());
		if (!lua_isnil(L, -1)) return 1;
		lua_pop(L, 1);
		// marked while it runs, so two files that include each other stop instead of looping
		lua_pushboolean(L, 1);
		lua_setfield(L, cache, rel.c_str());
		if (load_script(L, m->m_dir + "/" + rel, rel) != LUA_OK) return lua_error(L);
		lua_call(L, 0, 1);
		if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushboolean(L, 1); }
		lua_pushvalue(L, -1);
		lua_setfield(L, cache, rel.c_str());
		return 1;
	}
	// nba.save(name, text [, { append = true, header = "..." }]): write text to
	// <app>/stats/<name> (see mods.h), replacing the file, or added at its end with append;
	// header is written first when append creates the file. A bad name or too much text is a
	// script error; a failed write returns nil and the reason.
	static int save(lua_State *L)
	{
		std::string name = luaL_checkstring(L, 1);
		size_t len = 0;
		const char *text = luaL_checklstring(L, 2, &len);
		bool append = false;
		std::string header;
		if (!lua_isnoneornil(L, 3))
		{
			luaL_checktype(L, 3, LUA_TTABLE);
			lua_getfield(L, 3, "append"); append = lua_toboolean(L, -1); lua_pop(L, 1);
			lua_getfield(L, 3, "header"); if (lua_isstring(L, -1)) header = lua_tostring(L, -1); lua_pop(L, 1);
		}
		if (!mod_manager::save_name_ok(name))
			return luaL_error(L, "nba.save: the name is a plain file name ending .csv, .json, .jsonl or .txt (letters, digits, _ - .), got \"%s\"", name.c_str());
		if (len > (1u << 20)) return luaL_error(L, "nba.save: at most 1 MB of text");
		mod_manager *m = mgr(L);
		// at most 16 saves in 600 frames (10 seconds), so a script cannot fill the disk
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_save_from");
		lua_getfield(L, LUA_REGISTRYINDEX, "nba_saves");
		lua_Integer from = lua_isinteger(L, -2) ? lua_tointeger(L, -2) : -1000000, n = lua_tointeger(L, -1);
		lua_pop(L, 2);
		if (lua_Integer(m->m_frame) - from >= 600 || lua_Integer(m->m_frame) < from) { from = m->m_frame; n = 0; }
		if (n >= 16) return luaL_error(L, "nba.save: at most 16 files in 600 frames");
		lua_pushinteger(L, from); lua_setfield(L, LUA_REGISTRYINDEX, "nba_save_from");
		lua_pushinteger(L, n + 1); lua_setfield(L, LUA_REGISTRYINDEX, "nba_saves");
		std::error_code ec;
		fs::path dir = P(m->m_save_dir);
		fs::create_directories(dir, ec);
		fs::path to = dir / P(name), tmp = dir / P(name + ".tmp");
		bool ok;
		if (append)
		{
			std::uintmax_t had = fs::exists(to, ec) ? fs::file_size(to, ec) : 0;
			if (ec) had = 0;
			if (had + len + header.size() > (64u << 20)) return luaL_error(L, "nba.save: stats/%s would pass 64 MB", name.c_str());
			std::ofstream f(to, std::ios::binary | std::ios::app);
			if (had == 0) f.write(header.data(), std::streamsize(header.size()));
			f.write(text, std::streamsize(len));
			ok = bool(f);
			if (!ok) { lua_pushnil(L); lua_pushstring(L, ("cannot write stats/" + name).c_str()); return 2; }
			lua_pushboolean(L, 1);
			return 1;
		}
		{
			std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
			f.write(text, std::streamsize(len));
			ok = bool(f);
		}
		if (ok) { fs::rename(tmp, to, ec); ok = !ec; }
		if (!ok) { fs::remove(tmp, ec); lua_pushnil(L); lua_pushstring(L, ("cannot write stats/" + name).c_str()); return 2; }
		lua_pushboolean(L, 1);
		return 1;
	}
	// nba.date(): the local date and time as "YYYY-MM-DD HH:MM:SS", for saved files; nil in an
	// online game, so no script can make its game depend on the clock
	static int date(lua_State *L)
	{
		if (mgr(L)->m_netplay) { lua_pushnil(L); return 1; }
		std::time_t t = std::time(nullptr);
		std::tm tm{};
#ifdef _WIN32
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif
		char b[32];
		std::strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &tm);
		lua_pushstring(L, b);
		return 1;
	}
	// nba.sym("name"): the bit address of a symbol of the running revision, or nil
	static int sym(lua_State *L)
	{
		const char *n = luaL_checkstring(L, 1);
		mod_manager *m = mgr(L);
		if (!m || !m->m_mach) return luaL_error(L, "nba.sym: no game is running");
		auto &t = m->symbols();
		auto it = t.find(n);
		if (it == t.end()) lua_pushnil(L); else lua_pushinteger(L, lua_Integer(it->second));
		return 1;
	}
	static int rom_size(lua_State *L) { lua_pushinteger(L, lua_Integer(rom(L).size() / 2)); return 1; }
	static int rom_read16(lua_State *L) { size_t w = rom_word(L, 1); lua_pushinteger(L, rw(rom(L), w)); return 1; }
	static int rom_read32(lua_State *L)
	{
		size_t w = rom_word(L, 1);
		std::vector<u8> &r = rom(L);
		if (w + 1 >= r.size() / 2) return luaL_error(L, "ROM address out of range");
		lua_pushinteger(L, lua_Integer(rw(r, w) | (u32(rw(r, w + 1)) << 16)));
		return 1;
	}
	static int rom_write16(lua_State *L)
	{
		size_t w = rom_word(L, 1);
		ww(rom(L), w, u16(luaL_checkinteger(L, 2)));
		return 0;
	}
	static int rom_write32(lua_State *L)
	{
		size_t w = rom_word(L, 1);
		std::vector<u8> &r = rom(L);
		if (w + 1 >= r.size() / 2) return luaL_error(L, "ROM address out of range");
		u32 v = u32(luaL_checkinteger(L, 2));
		ww(r, w, u16(v));
		ww(r, w + 1, u16(v >> 16));
		return 0;
	}
	// rom_find({0x09e0, 0x0000, 0x0300, -1, ...} [, start_addr [, max]]) -> { addr, ... }
	// -1 / false matches any word
	static int rom_find(lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE);
		std::vector<int> pat;
		lua_Integer n = luaL_len(L, 1);
		for (lua_Integer i = 1; i <= n; i++)
		{
			lua_geti(L, 1, i);
			int v = lua_isboolean(L, -1) ? -1 : int(luaL_checkinteger(L, -1));
			pat.push_back(v < 0 ? -1 : (v & 0xffff));
			lua_pop(L, 1);
		}
		std::vector<u8> &r = rom(L);
		size_t words = r.size() / 2;
		size_t start = lua_isnoneornil(L, 2) ? 0 : rom_word(L, 2);
		lua_Integer max = luaL_optinteger(L, 3, 1000);
		lua_newtable(L);
		if (pat.empty()) return 1;
		lua_Integer found = 0;
		for (size_t w = start; w + pat.size() <= words && found < max; w++)
		{
			size_t k = 0;
			while (k < pat.size() && (pat[k] < 0 || rw(r, w + k) == pat[k])) k++;
			if (k == pat.size())
			{
				lua_pushinteger(L, lua_Integer(kRomBase + (u32(w) << 4)));
				lua_rawseti(L, -2, ++found);
			}
		}
		return 1;
	}
	static int read16(lua_State *L) { lua_pushinteger(L, mach(L).peek16(u32(luaL_checkinteger(L, 1)))); return 1; }
	// NVRAM (and its write-enable latch) is off limits to scripts: 0x01400000-0x014FFFFF on
	// both boards. Saved player records, audits and adjustments live there (roadmap 4.4).
	static u32 ram_addr(lua_State *L, u32 a)
	{
		if (a >= 0x01400000u && a <= 0x014fffffu) luaL_error(L, "nba.write: 0x%08X is NVRAM, which scripts may not write", (unsigned)a);
		return a;
	}
	static int write16(lua_State *L) { mach(L).poke16(ram_addr(L, u32(luaL_checkinteger(L, 1))), u16(luaL_checkinteger(L, 2))); return 0; }
	static int read32(lua_State *L)
	{
		u32 a = u32(luaL_checkinteger(L, 1));
		arcade_machine &m = mach(L);
		lua_pushinteger(L, lua_Integer(m.peek16(a) | (u32(m.peek16(a + 16)) << 16)));
		return 1;
	}
	static int write32(lua_State *L)
	{
		u32 a = ram_addr(L, u32(luaL_checkinteger(L, 1))), v = u32(luaL_checkinteger(L, 2));
		ram_addr(L, a + 16);
		arcade_machine &m = mach(L);
		m.poke16(a, u16(v));
		m.poke16(a + 16, u16(v >> 16));
		return 0;
	}
	static int read8(lua_State *L)
	{
		u32 a = u32(luaL_checkinteger(L, 1));
		lua_pushinteger(L, (mach(L).peek16(a & ~15u) >> (a & 8)) & 0xff);
		return 1;
	}
	static int write8(lua_State *L)
	{
		u32 a = ram_addr(L, u32(luaL_checkinteger(L, 1)));
		u16 v = u16(luaL_checkinteger(L, 2) & 0xff);
		arcade_machine &m = mach(L);
		u16 w = m.peek16(a & ~15u);
		w = (a & 8) ? u16((w & 0x00ff) | (v << 8)) : u16((w & 0xff00) | v);
		m.poke16(a & ~15u, w);
		return 0;
	}
};

static void budget_hook(lua_State *L, lua_Debug *)
{
	luaL_error(L, "script took too long (instruction budget exceeded)");
}

static bool call(lua_State *L, int nargs, int budget, std::string &err)
{
	lua_sethook(L, budget_hook, LUA_MASKCOUNT, budget);
	int r = lua_pcall(L, nargs, 0, 0);
	lua_sethook(L, nullptr, 0, 0);
	if (r == LUA_OK) return true;
	err = lua_isstring(L, -1) ? lua_tostring(L, -1) : "error";
	lua_pop(L, 1);
	return false;
}

lua_State *mod_manager::new_state(mod_script *mod, bool with_game)
{
	lua_State *L = luaL_newstate();
	if (!L) return nullptr;
	static const luaL_Reg libs[] = {
		{ LUA_GNAME, luaopen_base }, { LUA_TABLIBNAME, luaopen_table }, { LUA_STRLIBNAME, luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 },
	};
	for (const auto &l : libs) { luaL_requiref(L, l.name, l.func, 1); lua_pop(L, 1); }
	// no loading of other files / bytecode from a sandboxed script
	for (const char *g : { "dofile", "loadfile", "load" }) { lua_pushnil(L); lua_setglobal(L, g); }
	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "nba_mgr");
	lua_pushlightuserdata(L, mod);
	lua_setfield(L, LUA_REGISTRYINDEX, "nba_mod");

	static const luaL_Reg api[] = {
		{ "log", mod_api::log }, { "option", mod_api::option }, { "frame", mod_api::frame }, { "include", mod_api::include }, { "sym", mod_api::sym }, { "save", mod_api::save }, { "date", mod_api::date },
		{ "rom_size", mod_api::rom_size }, { "rom_read16", mod_api::rom_read16 }, { "rom_read32", mod_api::rom_read32 },
		{ "rom_write16", mod_api::rom_write16 }, { "rom_write32", mod_api::rom_write32 }, { "rom_find", mod_api::rom_find },
		{ "read8", mod_api::read8 }, { "write8", mod_api::write8 }, { "read16", mod_api::read16 }, { "write16", mod_api::write16 },
		{ "read32", mod_api::read32 }, { "write32", mod_api::write32 },
		{ "press", mod_api::press }, { "text", mod_api::text }, { "rect", mod_api::rect }, { "line", mod_api::line },
		{ nullptr, nullptr },
	};
	luaL_newlib(L, api);
	lua_pushinteger(L, kRomBase);
	lua_setfield(L, -2, "ROM_BASE");
	if (with_game) lua_pushstring(L, m_game.c_str()); else lua_pushnil(L);
	lua_setfield(L, -2, "game");
	lua_pushboolean(L, with_game && m_netplay);   // an online game (roadmap 1.7): a script may refuse to run
	lua_setfield(L, -2, "netplay");
	lua_setglobal(L, "nba");
	return L;
}

// ---------------------------------------------------------------------------
// scanning: read each script's `mod` table
// ---------------------------------------------------------------------------
static std::string field_str(lua_State *L, int t, const char *k)
{
	lua_getfield(L, t, k);
	std::string s = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
	lua_pop(L, 1);
	return s;
}

// Does the script text mention write8 / write16 / write32 other than as rom_write*? A text
// scan on purpose: it also catches writes behind an alias (local w = nba.write16) or inside
// a branch that has not run yet. A mention in a comment counts too; that errs on the safe side.
// Libraries a script includes count as part of it: every "lib/....lua" named in its text is
// scanned too (an include through a variable is not followed; a name in a comment is).
static bool source_writes_ram(const std::string &path, const std::string &dir, std::vector<std::string> &seen)
{
	std::ifstream f(P(path), std::ios::binary);
	std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	for (const std::string w : { "write8", "write16", "write32" })
		for (size_t p = s.find(w); p != std::string::npos; p = s.find(w, p + 1))
		{
			char after = p + w.size() < s.size() ? s[p + w.size()] : ' ';
			bool rom = p >= 4 && s.compare(p - 4, 4, "rom_") == 0;
			if (!rom && !isalnum(static_cast<unsigned char>(after)) && after != '_') return true;
		}
	for (size_t p = s.find("lib/"); p != std::string::npos; p = s.find("lib/", p + 1))
	{
		size_t e = s.find(".lua", p);
		if (e == std::string::npos) break;
		std::string rel = s.substr(p, e + 4 - p);
		if (!mod_manager::lib_path_ok(rel) || std::find(seen.begin(), seen.end(), rel) != seen.end()) continue;
		seen.push_back(rel);
		if (source_writes_ram(dir + "/" + rel, dir, seen)) return true;
	}
	return false;
}

bool mod_manager::load_meta(mod_script &m)
{
	lua_State *L = new_state(&m, false);
	if (!L) { m.error = "out of memory"; return false; }
	std::string err;
	if (load_script(L, m.path, m.file) != LUA_OK)
	{
		m.error = lua_tostring(L, -1);
		lua_close(L);
		return false;
	}
	if (!call(L, 0, kLoadBudget, err)) { m.error = err; lua_close(L); return false; }
	lua_getglobal(L, "mod");
	if (lua_istable(L, -1))
	{
		int t = lua_gettop(L);
		m.name = field_str(L, t, "name");
		m.description = field_str(L, t, "description");
		lua_getfield(L, t, "games");
		if (lua_istable(L, -1))
			for (lua_Integer i = 1; i <= luaL_len(L, -1); i++)
			{
				lua_geti(L, -1, i);
				if (lua_isstring(L, -1)) m.supported.push_back(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		lua_pop(L, 1);
		lua_getfield(L, t, "options");
		if (lua_istable(L, -1))
		{
			int ot = lua_gettop(L);
			for (lua_Integer i = 1; i <= luaL_len(L, ot); i++)
			{
				lua_geti(L, ot, i);
				if (lua_istable(L, -1))
				{
					int o = lua_gettop(L);
					mod_option op;
					op.id = field_str(L, o, "id");
					op.label = field_str(L, o, "label");
					op.format = field_str(L, o, "format");
					if (op.label.empty()) op.label = op.id;
					op.group = field_str(L, o, "group");
					const std::string ty = field_str(L, o, "type");
					lua_getfield(L, o, "conflicts");
					if (lua_istable(L, -1))
						for (lua_Integer k = 1; k <= luaL_len(L, -1); k++) { lua_geti(L, -1, k); if (lua_isstring(L, -1)) op.conflicts.push_back(lua_tostring(L, -1)); lua_pop(L, 1); }
					lua_pop(L, 1);
					auto num = [&](const char *f, double d) { lua_getfield(L, o, f); double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : d; lua_pop(L, 1); return v; };
					lua_getfield(L, o, "live");
					op.live = lua_toboolean(L, -1);
					lua_pop(L, 1);
					if (ty == "toggle") { op.type = mod_option::toggle; op.values = { 0, 1 }; }
					else if (ty == "players") { op.type = mod_option::players; for (int k = 0; k < 16; k++) op.values.push_back(k); }
					else if (ty == "range")
					{
						op.type = mod_option::range;
						const double mn = num("min", 0), mx = num("max", mn), st = num("step", 1);
						if (st > 0 && mx >= mn)
						{
							const long long n = (long long)std::floor((mx - mn) / st + 1e-9) + 1;
							for (long long k = 0; k < std::min(n, 1001LL); k++) op.values.push_back(mn + double(k) * st);
						}
					}
					else if (ty.empty() || ty == "list")
					{
						lua_getfield(L, o, "values");
						if (lua_istable(L, -1))
							for (lua_Integer k = 1; k <= luaL_len(L, -1); k++) { lua_geti(L, -1, k); op.values.push_back(lua_tonumber(L, -1)); lua_pop(L, 1); }
						lua_pop(L, 1);
					}
					lua_getfield(L, o, "labels");
					if (lua_istable(L, -1))
						for (lua_Integer k = 1; k <= luaL_len(L, -1); k++) { lua_geti(L, -1, k); op.labels.push_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : ""); lua_pop(L, 1); }
					lua_pop(L, 1);
					lua_getfield(L, o, "default");
					if (lua_isnumber(L, -1) || lua_isboolean(L, -1))
					{
						double d = lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? 1 : 0) : lua_tonumber(L, -1);
						int k = value_slot(op.values, d);
						if (k >= 0) op.def = k;
					}
					lua_pop(L, 1);
					if (!op.id.empty() && !op.values.empty()) m.options.push_back(op);
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	if (m.name.empty()) m.name = u8s(P(m.file).stem());
	lua_getglobal(L, "on_frame");
	m.has_frame = lua_isfunction(L, -1);
	lua_pop(L, 1);
	lua_close(L);
	std::vector<std::string> seen;
	m.writes_ram = source_writes_ram(m.path, m_dir, seen);
	return true;
}

bool mod_manager::save_name_ok(const std::string &name)
{
	if (name.empty() || name.size() > 64 || name[0] == '.') return false;
	for (char c : name)
		if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')) return false;
	for (const char *e : { ".csv", ".json", ".jsonl", ".txt" })
	{
		size_t k = strlen(e);
		if (name.size() > k && name.compare(name.size() - k, k, e) == 0) return true;
	}
	return false;
}

bool mod_manager::lib_path_ok(const std::string &rel)
{
	if (rel.size() < 9 || rel.size() > 128 || rel.compare(0, 4, "lib/") != 0 || rel.compare(rel.size() - 4, 4, ".lua") != 0) return false;
	if (rel.find("..") != std::string::npos || rel.find("//") != std::string::npos) return false;
	for (char c : rel)
		if (!(isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '/')) return false;
	return true;
}

std::string mod_manager::symbols_file() const
{
	if (!m_mach) return "";
	std::vector<u8> *r = m_mach->program_rom();
	if (!r || r->empty()) return "";
	char crc[16];
	snprintf(crc, sizeof crc, "%08x", crc32_of(r->data(), r->size()));
	return m_dir + "/sym/" + m_game + "_" + crc + ".lua";
}

// mods/sym/<game>_<crc>.lua returns a table: name = address, or name = { addr = a, pattern =
// { words... }, at = b } for an address found by hand, which is used only when the ROM holds
// those words at `at` (default: addr). It runs in a sandbox with nothing but the base library.
const std::map<std::string, u32> &mod_manager::symbols()
{
	if (m_syms_loaded) return m_syms;
	m_syms_loaded = true;
	m_syms.clear();
	std::string file = symbols_file();
	if (file.empty()) return m_syms;
	lua_State *L = luaL_newstate();
	if (!L) return m_syms;
	luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
	lua_pop(L, 1);
	for (const char *g : { "dofile", "loadfile", "load" }) { lua_pushnil(L); lua_setglobal(L, g); }
	std::string name = file.substr(file.find_last_of('/') + 1);
	if (load_script(L, file, "sym/" + name) != LUA_OK) { lua_close(L); return m_syms; }      // no file: no symbols
	lua_sethook(L, budget_hook, LUA_MASKCOUNT, kLoadBudget);
	if (lua_pcall(L, 0, 1, 0) != LUA_OK || !lua_istable(L, -1)) { fprintf(stderr, "mod: %s: not a symbol table\n", name.c_str()); lua_close(L); return m_syms; }
	lua_sethook(L, nullptr, 0, 0);
	std::vector<u8> *rom = m_mach->program_rom();
	int t = lua_gettop(L), dropped = 0;
	lua_pushnil(L);
	while (lua_next(L, t))
	{
		if (lua_type(L, -2) == LUA_TSTRING)
		{
			std::string k = lua_tostring(L, -2);
			if (lua_isinteger(L, -1)) m_syms[k] = u32(lua_tointeger(L, -1));
			else if (lua_istable(L, -1))
			{
				int e = lua_gettop(L);
				lua_getfield(L, e, "addr");
				bool have = lua_isinteger(L, -1);
				u32 addr = have ? u32(lua_tointeger(L, -1)) : 0;
				lua_pop(L, 1);
				lua_getfield(L, e, "at");
				u32 at = lua_isinteger(L, -1) ? u32(lua_tointeger(L, -1)) : addr;
				lua_pop(L, 1);
				lua_getfield(L, e, "pattern");
				if (have && lua_istable(L, -1) && rom)
				{
					// the words must be in the ROM at `at`, or the entry is not used
					lua_Integer n = luaL_len(L, -1);
					size_t w0 = at >= kRomBase ? size_t(at - kRomBase) >> 4 : SIZE_MAX;
					for (lua_Integer i = 1; have && i <= n; i++)
					{
						lua_geti(L, -1, i);
						size_t w = w0 + size_t(i - 1);
						long v = lua_isinteger(L, -1) ? long(lua_tointeger(L, -1)) : -1;
						if (v >= 0 && (w0 == SIZE_MAX || w >= rom->size() / 2 || u16((*rom)[w * 2] | ((*rom)[w * 2 + 1] << 8)) != u16(v))) have = false;
						lua_pop(L, 1);
					}
					if (!have) dropped++;
				}
				lua_pop(L, 1);
				if (have) m_syms[k] = addr;
			}
		}
		lua_pop(L, 1);
	}
	lua_close(L);
	fprintf(stderr, "mod: %zu symbols from %s%s\n", m_syms.size(), name.c_str(), dropped ? (" (" + std::to_string(dropped) + " left out: their code is not in this ROM)").c_str() : "");
	return m_syms;
}

// ---------------------------------------------------------------------------
// online games (roadmap 1.7)
// ---------------------------------------------------------------------------
// FNV-1a of the script and of every "lib/....lua" it names (and they name), in the order
// first named, each with its name: a change in any of them gives another hash
static void hash_source(const std::string &path, const std::string &dir, std::vector<std::string> &seen, u32 &h)
{
	std::ifstream f(P(path), std::ios::binary);
	std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());   // the same file with Windows line ends
	h = fnv1a(s.data(), s.size(), h);
	for (size_t p = s.find("lib/"); p != std::string::npos; p = s.find("lib/", p + 1))
	{
		size_t e = s.find(".lua", p);
		if (e == std::string::npos) break;
		std::string rel = s.substr(p, e + 4 - p);
		if (!mod_manager::lib_path_ok(rel) || std::find(seen.begin(), seen.end(), rel) != seen.end()) continue;
		seen.push_back(rel);
		h = fnv1a(rel.data(), rel.size(), h);
		hash_source(dir + "/" + rel, dir, seen, h);
	}
}

u32 mod_manager::source_hash(const std::string &path, const std::string &dir)
{
	std::vector<std::string> seen;
	u32 h = 2166136261u;
	hash_source(path, dir, seen, h);
	return h;
}

// "file<TAB>hash<TAB>id=index,id=index" per line
std::string mod_manager::net_description(const std::string &game_id) const
{
	std::string d;
	for (const auto &m : m_mods)
	{
		if (!m.applies_to(game_id) || !m.error.empty()) continue;
		char hx[16];
		snprintf(hx, sizeof hx, "%08x", source_hash(m.path, m_dir));
		d += m.file + "\t" + hx + "\t";
		bool first = true;
		for (const auto &o : m.options) { d += (first ? "" : ",") + o.id + "=" + std::to_string(m.option_index(o)); first = false; }
		d += "\n";
	}
	return d;
}

std::vector<std::string> mod_manager::active_names(const std::string &game_id) const
{
	std::vector<std::string> r;
	for (const auto &m : m_mods) if (m.applies_to(game_id) && m.error.empty()) r.push_back(m.name);
	return r;
}

bool mod_manager::net_apply(const std::string &game_id, const std::string &desc, std::string &err)
{
	net_restore();
	for (const auto &m : m_mods) m_saved.push_back({ m.enabled, m.all_games, m.games, m.opt });
	for (auto &m : m_mods) m.enabled = false;
	std::stringstream ss(desc);
	std::string line;
	while (std::getline(ss, line))
	{
		if (line.empty()) continue;
		size_t t1 = line.find('\t'), t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
		if (t2 == std::string::npos) { err = "the host sent an unreadable Lua mods list"; net_restore(); return false; }
		std::string file = line.substr(0, t1), hash = line.substr(t1 + 1, t2 - t1 - 1), opts = line.substr(t2 + 1);
		mod_script *m = nullptr;
		for (auto &x : m_mods) if (x.file == file) m = &x;
		if (!m) { err = "the host's Lua mod " + file + " is not in your mods folder"; net_restore(); return false; }
		char hx[16];
		snprintf(hx, sizeof hx, "%08x", source_hash(m->path, m_dir));
		if (hash != hx) { err = "your Lua mod " + file + " (or a library it uses) is not the same as the host's"; net_restore(); return false; }
		if (!m->error.empty()) { err = "your Lua mod " + file + " does not load: " + m->error; net_restore(); return false; }
		m->enabled = true;
		m->all_games = true;
		std::stringstream os(opts);
		std::string kv;
		while (std::getline(os, kv, ','))
		{
			size_t eq = kv.find('=');
			if (eq == std::string::npos) continue;
			const mod_option *o = m->find_option(kv.substr(0, eq));
			int i = atoi(kv.c_str() + eq + 1);
			if (o && i >= 0 && i < int(o->values.size())) m->opt[o->id] = i;
		}
	}
	(void)game_id;
	return true;
}

void mod_manager::net_restore()
{
	if (m_saved.size() != m_mods.size()) { m_saved.clear(); return; }
	for (size_t i = 0; i < m_mods.size(); i++)
	{
		m_mods[i].enabled = m_saved[i].enabled;
		m_mods[i].all_games = m_saved[i].all_games;
		m_mods[i].games = m_saved[i].games;
		m_mods[i].opt = m_saved[i].opt;
	}
	m_saved.clear();
}

void mod_manager::scan(const std::string &dir, const std::string &ini)
{
	stop();
	m_mods.clear();
	m_ini = ini;
	m_dir = dir;
	m_save_dir = u8s(P(dir).parent_path() / "stats");
	std::error_code ec;
	std::vector<fs::path> files;
	for (auto it = fs::directory_iterator(P(dir), ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		std::string ext = it->path().extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (it->is_regular_file(ec) && ext == ".lua") files.push_back(it->path());
	}
	std::sort(files.begin(), files.end());
	for (const auto &p : files)
	{
		mod_script m;
		m.file = u8s(p.filename());
		m.path = u8s(p);
		load_meta(m);
		m_mods.push_back(std::move(m));
	}
	load_settings();
}

// ---------------------------------------------------------------------------
// settings: mods.ini
//   [quarter_length.lua]
//   enabled=1
//   games=all            (or a comma separated list of game ids)
//   minutes=5            (option values, by value)
// ---------------------------------------------------------------------------
static std::string trim(std::string s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

void mod_manager::load_settings()
{
	std::ifstream f(P(m_ini));
	if (!f) return;
	std::string line;
	mod_script *cur = nullptr;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;
		if (line[0] == '[')
		{
			std::string name = trim(line.substr(1, line.find(']') - 1));
			cur = nullptr;
			for (auto &m : m_mods) if (m.file == name) cur = &m;
			continue;
		}
		if (!cur) continue;
		auto eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
		if (k == "enabled") cur->enabled = atoi(v.c_str()) != 0;
		else if (k == "games")
		{
			cur->all_games = v == "all" || v.empty();
			cur->games.clear();
			if (!cur->all_games)
			{
				std::stringstream ss(v);
				std::string g;
				while (std::getline(ss, g, ',')) if (!trim(g).empty()) cur->games.push_back(trim(g));
			}
		}
		else
			for (const auto &o : cur->options)
				if (o.id == k)
				{
					int i = value_slot(o.values, atof(v.c_str()));
					if (i >= 0) cur->opt[k] = i;
				}
	}
}

void mod_manager::save() const
{
	if (m_ini.empty()) return;
	std::ofstream g(P(m_ini));
	g.precision(15);
	g << "# NBA JAM Recomp - Lua mods (scripts in mods/). games=all or a list: nbajam,nbajamte,nbajamse,nbahangt,nbamht\n";
	for (const auto &m : m_mods)
	{
		g << "\n[" << m.file << "]\nenabled=" << (m.enabled ? 1 : 0) << "\ngames=";
		if (m.all_games) g << "all";
		else for (size_t i = 0; i < m.games.size(); i++) g << (i ? "," : "") << m.games[i];
		g << "\n";
		for (const auto &o : m.options) g << o.id << "=" << o.values[size_t(m.option_index(o))] << "\n";
	}
}

// ---------------------------------------------------------------------------
// game session
// ---------------------------------------------------------------------------
void mod_manager::report(const mod_script &m, const std::string &s)
{
	std::string line = "[" + m.name + "] " + s;
	fprintf(stderr, "mod: %s\n", line.c_str());
	m_log.push_back(line);
}

std::vector<std::string> mod_manager::take_log()
{
	std::vector<std::string> r;
	r.swap(m_log);
	return r;
}

void mod_manager::start(const std::string &game_id, arcade_machine &mach)
{
	stop();
	m_game = game_id;
	m_mach = &mach;
	m_frame = 0;
	m_syms_loaded = false;
	for (auto &m : m_mods)
	{
		if (!m.applies_to(game_id)) continue;
		lua_State *L = new_state(&m, true);
		if (!L) continue;
		std::string err;
		bool ok = load_script(L, m.path, m.file) == LUA_OK;
		if (!ok) { err = lua_tostring(L, -1); }
		else ok = call(L, 0, kLoadBudget, err);
		if (ok)
		{
			lua_getglobal(L, "on_load");
			if (lua_isfunction(L, -1)) ok = call(L, 0, kLoadBudget, err);
			else lua_pop(L, 1);
		}
		if (!ok)
		{
			report(m, "error: " + err);
			lua_close(L);
			continue;
		}
		lua_getglobal(L, "on_frame");
		bool hf = lua_isfunction(L, -1);
		lua_pop(L, 1);
		m_running.push_back({ &m, L, hf });
	}
}

void mod_manager::apply_presses(cabinet_inputs &in, int local_slot)
{
	for (size_t i = 0; i < m_press.size();)
	{
		press &p = m_press[i];
		if (local_slot < 0 || p.player == local_slot)
		{
			cabinet_inputs::player &q = in.p[local_slot < 0 ? p.player : 0];
			bool *b[9] = { &q.up, &q.down, &q.left, &q.right, &q.shoot, &q.pass, &q.turbo, &q.start, &q.coin };
			for (int k = 0; k < 9; k++) if (p.buttons & (1u << k)) *b[k] = true;
		}
		if (--p.frames <= 0) { m_press.erase(m_press.begin() + long(i)); continue; }
		i++;
	}
}

void mod_manager::frame()
{
	m_frame++;
	m_draw.clear();
	for (size_t i = 0; i < m_running.size();)
	{
		running &r = m_running[i];
		if (r.has_frame)
		{
			lua_getglobal(r.L, "on_frame");
			lua_pushinteger(r.L, m_frame);
			std::string err;
			if (!call(r.L, 1, kFrameBudget, err))
			{
				report(*r.mod, "error, mod stopped: " + err);
				lua_close(r.L);
				m_running.erase(m_running.begin() + long(i));
				continue;
			}
		}
		i++;
	}
}

std::map<std::string, int> mod_manager::used_values(const mod_script &m)
{
	std::map<std::string, int> r;
	for (const auto &o : m.options) r[o.id] = m.option_used(o);
	return r;
}

bool mod_manager::is_running(const mod_script &m) const
{
	for (const auto &r : m_running) if (r.mod == &m) return true;
	return false;
}

void mod_manager::options_changed(mod_script &m, const std::map<std::string, int> &before)
{
	for (size_t i = 0; i < m_running.size(); i++)
	{
		running &r = m_running[i];
		if (r.mod != &m) continue;
		for (const auto &o : m.options)
		{
			auto b = before.find(o.id);
			if (!o.live || (b != before.end() && b->second == m.option_used(o))) continue;
			lua_getglobal(r.L, "on_option");
			if (!lua_isfunction(r.L, -1)) { lua_pop(r.L, 1); return; }
			lua_pushstring(r.L, o.id.c_str());
			mod_api::push_option(r.L, m, o);
			std::string err;
			if (!call(r.L, 2, kFrameBudget, err))
			{
				report(m, "error, mod stopped: " + err);
				lua_close(r.L);
				m_running.erase(m_running.begin() + long(i));
				return;
			}
		}
		return;
	}
}

void mod_manager::stop()
{
	for (auto &r : m_running) lua_close(r.L);
	m_running.clear();
	m_press.clear();
	m_draw.clear();
	m_mach = nullptr;
	m_syms.clear();
	m_syms_loaded = false;
}
