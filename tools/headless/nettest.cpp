// nettest - run one side of a netplay session headless with scripted inputs.
// MODS=<dir> loads the scripts in <dir>, as the app does with the user's mods. This peer's own
// settings: MODS_ON=<file,file> (default: every script) and MODOPT=<file>:<id>=<index>. The host
// sends its set with the game start; the client runs exactly that set (roadmap 1.7) or stops
// with exit code 3 and names the file that differs.
// WIDE=<columns> (roadmap W3.5): this peer also draws the wide gameplay picture (wide_view, T.E.
// games), with the symbol file from WIDE_SYMS=<dir>, as the app does with Widescreen on. The two
// peers must stay in sync whatever each one shows.
// usage: nettest <base> <game> host <port> <frames> <delay> [loss]
//        nettest <base> <game> client <ip> <port> <frames> [loss]
#include "games.h"
#include "netplay.h"
#include "mods.h"
#include "romload.h"
#include "wide_view.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static cabinet_inputs::player script(int slot, u32 f)
{
	cabinet_inputs::player p{};
	u32 h = (u32(slot) * 2654435761u) ^ ((f / 12) * 40503u);
	h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
	p.up = (h & 3) == 0; p.down = (h & 3) == 1; p.left = (h >> 2 & 3) == 0; p.right = (h >> 2 & 3) == 1;
	p.shoot = (h >> 4 & 7) == 0; p.pass = (h >> 7 & 7) == 0; p.turbo = (h >> 10 & 1);
	u32 g = f % 600;
	p.coin = g >= 60u + u32(slot) * 8 && g < 64u + u32(slot) * 8;
	p.start = g >= 120u + u32(slot) * 8 && g < 126u + u32(slot) * 8;
	return p;
}

// "  NAME = 0x01234567," from a symbol file
static u32 symbol(const std::string &file, const std::string &name)
{
	std::ifstream in(file);
	std::string l;
	while (std::getline(in, l))
	{
		std::istringstream w(l);
		std::string n, eq, v;
		if (!(w >> n >> eq >> v) || n != name || eq != "=") continue;
		if (v.rfind("0x", 0) == 0) return u32(strtoul(v.c_str(), nullptr, 16));
		// NAME = { addr = 0x..., pattern = {...} }: the address (the app also checks the pattern
		// against the ROM; these tests only run the owner's revisions the files were made from)
		size_t const a = l.find("addr = 0x");
		if (v == "{" && a != std::string::npos) return u32(strtoul(l.c_str() + a + 7, nullptr, 16));
	}
	return 0;
}

static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage\n"); return 1; }
	std::string base = argv[1], id = argv[2], role = argv[3];
	bool host = role == "host";
	mod_manager mods;
	if (const char *md = getenv("MODS"))
	{
		mods.scan(md, "");
		std::string on = getenv("MODS_ON") ? getenv("MODS_ON") : "";
		for (auto &s : mods.mods()) s.enabled = on.empty() || ("," + on + ",").find("," + s.file + ",") != std::string::npos;
		if (const char *mo = getenv("MODOPT"))
		{
			std::string kv = mo;
			size_t c = kv.find(':'), e = kv.find('=');
			for (auto &s : mods.mods())
				if (c != std::string::npos && e != std::string::npos && s.file == kv.substr(0, c)) s.opt[kv.substr(c + 1, e - c - 1)] = atoi(kv.c_str() + e + 1);
		}
		std::string names;
		for (auto &n : mods.active_names(id)) names += (names.empty() ? "" : ", ") + n;
		fprintf(stderr, "%s: own Lua mods on: %s\n", role.c_str(), names.empty() ? "none" : names.c_str());
		mods.set_netplay(true);
	}
	netplay np;
	std::string err;
	int frames, delay = 4;
	double loss = 0;
	if (host)
	{
		frames = atoi(argv[5]); delay = atoi(argv[6]); if (argc > 7) loss = atof(argv[7]);
		if (!np.host(u16(atoi(argv[4])), err)) { fprintf(stderr, "host: %s\n", err.c_str()); return 1; }
	}
	else
	{
		frames = atoi(argv[6]); if (argc > 7) loss = atof(argv[7]);
		if (!np.join(argv[4], u16(atoi(argv[5])), err)) { fprintf(stderr, "join: %s\n", err.c_str()); return 1; }
	}
	np.socket().loss = loss;
	int want_players = getenv("PLAYERS") ? atoi(getenv("PLAYERS")) : 2;

	game_catalog cat(base);
	const game_info *gi = nullptr;
	for (auto &g : game_list()) if (id == g.id) gi = &g;
	if (!gi) { fprintf(stderr, "unknown game\n"); return 1; }
	std::unique_ptr<arcade_machine> m;

	auto t0 = net_now_ms();
	if (host)
	{
		while (np.players() < want_players)
		{
			np.poll(); sleep_ms(5);
			if (net_now_ms() - t0 > 30000) { fprintf(stderr, "no client\n"); return 1; }
		}
		m = cat.create(*gi, err);
		if (!m) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
		dip_settings d;
		m->set_dips(d);
		mods.start(id, *m);
		m->reset();
		np.host_start(id, m->fingerprint(), d, m->nvram(), delay, mods.net_description(id));
	}
	else
	{
		while (!np.start_received())
		{
			np.poll(); sleep_ms(5);
			if (!np.active()) { fprintf(stderr, "client: %s\n", np.error().c_str()); return 1; }
		}
		m = cat.create(*gi, err);
		if (!m) { np.client_ready(false, err); return 1; }
		// NOSYNC=1 (a control for the test): keep this peer's own mods instead of the host's
		if (!getenv("NOSYNC") && !mods.net_apply(id, np.start_mods(), err)) { np.client_ready(false, err); np.poll(); fprintf(stderr, "client: refused: %s\n", err.c_str()); return 3; }
		std::string names;
		for (auto &n : mods.active_names(id)) names += (names.empty() ? "" : ", ") + n;
		fprintf(stderr, "client: running the host's Lua mods: %s\n", names.empty() ? "none" : names.c_str());
		mods.start(id, *m);
		if (m->fingerprint() != np.start_fingerprint()) { np.client_ready(false, "different ROMs"); np.poll(); fprintf(stderr, "fingerprint mismatch\n"); return 1; }
		m->set_nvram(np.start_nvram());
		m->set_dips(np.start_dips());
		m->reset();
		np.client_ready(true, "");
	}
	std::unique_ptr<wide_view> wide;
	long wide_frames = 0;
	if (const char *wc = getenv("WIDE"))
	{
		std::vector<u8> *prog = m->program_rom();
		char fp[16];
		snprintf(fp, sizeof fp, "%08x", prog ? crc32_of(prog->data(), prog->size()) : 0u);
		std::string const f = std::string(getenv("WIDE_SYMS") ? getenv("WIDE_SYMS") : "mods/sym") + "/" + id + "_" + fp + ".lua";
		wide_symbols ws;
		ws.display = symbol(f, "DISPLAY"); ws.objlst = symbol(f, "OBJLST"); ws.worldtlx = symbol(f, "WORLDTLX");
		ws.worldtly = symbol(f, "WORLDTLY"); ws.gndx = symbol(f, "gndx"); ws.gndpos_t = symbol(f, "gndpos_t");
		ws.dtype = symbol(f, "dtype"); ws.dpage = symbol(f, "dpage"); ws.pcnt = symbol(f, "PCNT"); ws.scrntl = symbol(f, "SCRNTL"); ws.scrnlr = symbol(f, "SCRNLR");
		if (!ws.complete()) { fprintf(stderr, "%s: WIDE: no symbols in %s\n", role.c_str(), f.c_str()); return 1; }
		const wide_game *wg = wide_game_for(id.c_str());
		if (!wg) { fprintf(stderr, "%s: WIDE: no wide view for %s\n", role.c_str(), id.c_str()); return 1; }
		wide = std::make_unique<wide_view>(*m, ws, atoi(wc), *wg);
		fprintf(stderr, "%s: drawing the wide view, %d columns\n", role.c_str(), wide->columns());
	}
	fprintf(stderr, "%s: slot %d, players %d, start\n", role.c_str(), np.local_slot(), np.players());
	u32 stalls = 0;
	auto tstart = net_now_ms();
	while (np.frame() < u32(frames))
	{
		np.poll();
		if (!np.active()) { fprintf(stderr, "%s: ended: %s at frame %u\n", role.c_str(), np.error().c_str(), np.frame()); return 1; }
		if (!np.can_run()) { stalls++; sleep_ms(1); continue; }
		cabinet_inputs::player local = script(np.local_slot(), np.frame());
		// MENU=<frame>: this peer has the pause menu open for 600 frames from that frame. The
		// app then sends nothing pressed (app::menu_inputs) and the game goes on (U5.3).
		if (const char *mf = getenv("MENU"))
			if (np.frame() >= u32(atoi(mf)) && np.frame() < u32(atoi(mf)) + 600) local = cabinet_inputs::player{};
		// as the app does online (app::step_game): nba.press for this peer's own player, sent like
		// the pad; then nba.input / nba.hold on the merged inputs
		cabinet_inputs li{};
		li.p[0] = local;
		mods.apply_presses(li, np.local_slot());
		local = li.p[0];
		cabinet_inputs in = np.frame_inputs(local);
		mods.take_inputs(in);
		if (getenv("CHEAT") && np.frame() >= 400 && np.frame() < 460) in.p[0].left = true;
		m->set_inputs(in);
		m->run_frame();
		std::vector<float> a; m->take_audio(a);
		mods.frame();
		if (wide && wide->show_wide()) wide_frames++;
		u32 h = m->state_hash();
		if (np.frame() % 300 == 0) printf("%u %08x\n", np.frame(), h);
		np.frame_done(h);
	}
	// keep serving the peer for a moment so it can finish too
	auto te = net_now_ms();
	while (net_now_ms() - te < 1500) { np.poll(); sleep_ms(2); }
	fprintf(stderr, "%s: done %d frames in %u ms, stall polls %u, desync=%d ping=%d\n", role.c_str(), frames, net_now_ms() - tstart, stalls, np.desynced(), np.ping_ms());
	if (wide) fprintf(stderr, "%s: the wide view was shown in %ld frames\n", role.c_str(), wide_frames);
	np.stop();
	return np.desynced() ? 2 : 0;
}
