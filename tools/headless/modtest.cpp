// modtest.cpp - load a T-unit program ROM, run the Lua mods in a folder on it and show
// what they changed. usage: modtest [--expect file] prog_lo prog_hi mods_dir game_id [frames] [opt=value...]
// --expect (roadmap 1.8): lines "expect <frame> <symbol or 0xaddress>[:8|:16|:32] <op> <value>",
// op one of == != < > <= >=; after that frame the value is read and compared. Other lines
// "log <text>": a line of the mods' log contains the text, after the line the previous
// log/once matched (so they check the order); "once <text>": the same, and exactly one line
// contains it; "nolog <text>": no line does. These are checked at the end of the run.
// A line may start with "<game id>:" to apply only to that game. The log also holds a line
// "listed <file>" for each script the mods folder lists.
// (game, frames, env, opt: read by tools/modtests/run_all.sh) and # comments are skipped.
// Exit code 4 when an expectation fails or its symbol is unknown.
#include "tunit.h"
#include "wunit.h"
#include <memory>
#include "mods.h"
#include "mod_draw.h"
#include "miniz.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

static std::vector<u8> load(const std::string &p)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) { fprintf(stderr, "missing %s\n", p.c_str()); exit(1); }
	return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct expectation { int frame; std::string name, op; int bits; long long value; int line; bool done = false; };

static bool compare(long long a, const std::string &op, long long b)
{
	if (op == "==") return a == b;
	if (op == "!=") return a != b;
	if (op == "<") return a < b;
	if (op == ">") return a > b;
	if (op == "<=") return a <= b;
	return a >= b;
}

int main(int argc, char **argv)
{
	std::vector<expectation> expects;
	struct log_expectation { std::string kind, text; int line; };
	std::vector<log_expectation> log_expects;
	std::vector<std::string> log;
	const char *expect_file = nullptr;
	if (argc > 2 && !strcmp(argv[1], "--expect"))
	{
		expect_file = argv[2];
		std::ifstream f(expect_file);
		if (!f) { fprintf(stderr, "missing %s\n", expect_file); return 1; }
		std::string l; int n = 0;
		while (std::getline(f, l))
		{
			n++;
			if (!l.empty() && l.back() == '\r') l.pop_back();
			std::istringstream is(l); std::string kw; is >> kw;
			if (kw.size() > 1 && kw.back() == ':')
			{
				if (argc < 7 || kw.substr(0, kw.size() - 1) != argv[6]) continue;   // argv[6]: game_id before the shift
				is >> kw;
			}
			if (kw == "log" || kw == "once" || kw == "nolog")
			{
				std::string t; std::getline(is, t);
				t.erase(0, t.find_first_not_of(" \t"));
				log_expects.push_back({ kw, t, n });
				continue;
			}
			if (kw != "expect") continue;
			expectation e; std::string v; e.line = n; e.bits = 16;
			if (!(is >> e.frame >> e.name >> e.op >> v) || (e.op != "==" && e.op != "!=" && e.op != "<" && e.op != ">" && e.op != "<=" && e.op != ">="))
			{ fprintf(stderr, "%s:%d: expected \"expect <frame> <symbol>[:bits] <op> <value>\"\n", expect_file, n); return 1; }
			if (auto c = e.name.find(':'); c != std::string::npos) { e.bits = atoi(e.name.c_str() + c + 1); e.name.resize(c); }
			if (e.bits != 8 && e.bits != 16 && e.bits != 32) { fprintf(stderr, "%s:%d: width must be 8, 16 or 32\n", expect_file, n); return 1; }
			e.value = strtoll(v.c_str(), nullptr, 0);
			expects.push_back(e);
		}
		argv += 2; argc -= 2;
	}
	if (argc < 5) { fprintf(stderr, "usage: modtest [--expect file] prog_lo prog_hi mods_dir game_id [frames] [opt=value...]\n"); return 1; }
	auto lo = load(argv[1]), hi = load(argv[2]);
	std::vector<u8> prog(0x100000);
	for (size_t i = 0; i < 0x80000; i++) { prog[i * 2] = lo[i]; prog[i * 2 + 1] = hi[i]; }
	std::vector<u8> orig = prog;
	// Wolf-unit games (nbahangt / nbamht) get a Wolf-unit board; PIC=<n> as in headless_w
	std::string gid = argv[4];
	std::unique_ptr<arcade_machine> mp;
	if (gid == "nbahangt" || gid == "nbamht")
	{
		wunit_roms r;
		r.program = prog;
		r.gfx.assign(0x800000, 0);
		r.dcs.assign(0x700000, 0xff);
		mp = std::make_unique<wunit_machine>(std::move(r), getenv("PIC") ? atoi(getenv("PIC")) : 528);
	}
	else
	{
		tunit_roms r;
		r.program = prog;
		r.gfx.assign(0xc00000, 0);
		r.snd_u3.assign(0x20000, 0xff); r.snd_u12.assign(0x80000, 0xff); r.snd_u13.assign(0x80000, 0xff);
		mp = std::make_unique<tunit_machine>(std::move(r), gid == "nbajam" ? tunit_protection::nbajam : tunit_protection::nbajamte);
	}
	arcade_machine &m = *mp;
	mod_manager mods;
	mods.scan(argv[3], "");
	int frames = argc > 5 ? atoi(argv[5]) : 0;
	for (auto &s : mods.mods())
	{
		s.enabled = true;
		log.push_back("listed " + s.file);
		printf("mod %s: \"%s\" %s\n", s.file.c_str(), s.name.c_str(), s.error.empty() ? "" : ("ERROR " + s.error).c_str());
		for (int a = 6; a < argc; a++)
		{
			std::string kv = argv[a]; auto eq = kv.find('=');
			for (auto &o : s.options)
				if (o.id == kv.substr(0, eq))
					for (size_t i = 0; i < o.values.size(); i++) if (o.values[i] == atof(kv.c_str() + eq + 1)) s.opt[o.id] = int(i);
		}
	}
	mods.start(argv[4], m);
	for (auto &l : mods.take_log()) log.push_back(l);
	const std::vector<u8> &now = *m.program_rom();
	for (size_t w = 0; w < now.size() / 2; w++)
		if (now[w * 2] != orig[w * 2] || now[w * 2 + 1] != orig[w * 2 + 1])
			printf("  patched %08X: %04X -> %04X\n", 0xff800000u + u32(w << 4), orig[w * 2] | orig[w * 2 + 1] << 8, now[w * 2] | now[w * 2 + 1] << 8);
	m.set_dips(dip_settings{});
	m.reset();
	// optional: WATCH=<bit address> prints that long every 500 frames; PLAY=1 presses
	// start then shoot/pass periodically to get through the menus into a game
	const char *watch = getenv("WATCH");
	bool play = getenv("PLAY") != nullptr;
	int p0 = getenv("PLAYAT") ? atoi(getenv("PLAYAT")) : 400;   // first frame of the presses
	// LIVE=<frame>:<id>=<value> changes an option at that frame, as the pause menu does (roadmap 1.4)
	const char *live = getenv("LIVE");
	int live_at = live ? atoi(live) : -1;
	int failed = 0;
	for (int f = 0; f < frames; f++)
	{
		cabinet_inputs in{};
		if (play && f > p0)
		{
			int t = f % 60;
			in.p[0].start = f < p0 + 800 && t < 4;
			in.p[0].shoot = getenv("NOPICK") == nullptr && f >= p0 + 800 && t >= 20 && t < 24;
		}
		mods.apply_presses(in);   // nba.press (roadmap 1.6)
		m.set_inputs(in);
		if (f == live_at)
		{
			std::string kv = strchr(live, ':') + 1;
			auto eq = kv.find('=');
			for (auto &sc : mods.mods())
				for (auto &o : sc.options)
					if (o.id == kv.substr(0, eq))
						for (size_t i = 0; i < o.values.size(); i++)
							if (o.values[i] == atof(kv.c_str() + eq + 1))
							{
								auto before = mod_manager::used_values(sc);
								sc.opt[o.id] = int(i);
								mods.options_changed(sc, before);
								printf("  frame %6d  option %s set to %g%s\n", f, o.id.c_str(), o.values[i], o.live ? " (live)" : " (next game)");
							}
		}
		m.run_frame();
		mods.frame();
		for (auto &l : mods.take_log()) log.push_back(l);
		// SHOT=<frame>:<file.png>: the picture of that frame with the mods' overlay (roadmap 1.6)
		if (const char *shot = getenv("SHOT"); shot && f == atoi(shot) && strchr(shot, ':'))
		{
			int w = m.width(), h = m.height();
			canvas cv(w, h);
			std::memcpy(cv.px.data(), m.frame(), size_t(w) * h * 4);
			draw_mod_overlay(cv, mods.draws());
			std::vector<u8> rgb(size_t(w) * h * 3);
			for (size_t i = 0; i < size_t(w) * h; i++) { u32 c = cv.px[i]; rgb[i * 3] = u8(c >> 16); rgb[i * 3 + 1] = u8(c >> 8); rgb[i * 3 + 2] = u8(c); }
			size_t len = 0;
			void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &len);
			if (FILE *fp = fopen(strchr(shot, ':') + 1, "wb")) { fwrite(png, 1, len, fp); fclose(fp); printf("  frame %6d  picture with the overlay: %s (%d draws)\n", f, strchr(shot, ':') + 1, int(mods.draws().size())); log.push_back("picture with " + std::to_string(mods.draws().size()) + " draws"); }
			mz_free(png);
		}
		for (auto &e : expects)
			if (e.frame == f)
			{
				e.done = true;
				long long a = -1;
				if (e.name.rfind("0x", 0) == 0) a = (long long)strtoul(e.name.c_str(), nullptr, 0);
				else if (auto it = mods.symbols().find(e.name); it != mods.symbols().end()) a = it->second;
				if (a < 0) { printf("FAIL %s:%d: frame %d: %s is not a symbol of %s (%s)\n", expect_file, e.line, f, e.name.c_str(), gid.c_str(), mods.symbols_file().c_str()); failed++; continue; }
				u32 ad = u32(a);
				long long v = e.bits == 8 ? (m.peek16(ad & ~15u) >> (ad & 8)) & 0xff : e.bits == 16 ? m.peek16(ad) : (long long)(m.peek16(ad) | (u32(m.peek16(ad + 16)) << 16));
				bool ok = compare(v, e.op, e.value);
				printf("%s %s:%d: frame %d: %s = %lld (0x%llX), expected %s %lld\n", ok ? "PASS" : "FAIL", expect_file, e.line, f, e.name.c_str(), v, v, e.op.c_str(), e.value);
				if (!ok) failed++;
			}
		if (watch && f % 500 == 0)
		{
			u32 a = u32(strtoul(watch, nullptr, 0));
			printf("  frame %6d  [%08X] = %08X  pc=%08X\n", f, a, m.peek16(a) | (u32(m.peek16(a + 16)) << 16), m.pc());
		}
	}
	if (frames) printf("ran %d frames, pc=%08X\n", frames, m.pc());
	mods.stop();
	for (auto &e : expects)
		if (!e.done) { printf("FAIL %s:%d: frame %d was not reached (ran %d frames)\n", expect_file, e.line, e.frame, frames); failed++; }
	for (auto &l : mods.take_log()) log.push_back(l);
	size_t from = 0;
	for (auto &e : log_expects)
	{
		int count = 0; size_t first = log.size();
		for (size_t i = 0; i < log.size(); i++)
			if (log[i].find(e.text) != std::string::npos) { count++; if (i >= from && first == log.size()) first = i; }
		bool ok;
		std::string why;
		if (e.kind == "nolog") { ok = count == 0; why = ok ? "not in the log" : "in the log " + std::to_string(count) + " times"; }
		else
		{
			ok = first < log.size() && (e.kind == "log" || count == 1);
			why = first == log.size() ? (count ? "only before the previous match" : "not in the log") : e.kind == "once" && count != 1 ? "in the log " + std::to_string(count) + " times" : "log line " + std::to_string(first + 1) + ": " + log[first];
			if (first < log.size()) from = first + 1;
		}
		printf("%s %s:%d: %s \"%s\": %s\n", ok ? "PASS" : "FAIL", expect_file, e.line, e.kind.c_str(), e.text.c_str(), why.c_str());
		if (!ok) failed++;
	}
	int total = int(expects.size() + log_expects.size());
	if (expect_file) printf("%s: %d of %d expectations passed\n", expect_file, total - failed, total);
	return failed ? 4 : 0;
}
