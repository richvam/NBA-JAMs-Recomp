// goldentest.cpp - golden hashes (roadmap R5.2): the same game on every processor and compiler.
//
//   goldentest <base> <frames> [games...]
//
// For each game whose ROMs are in <base>: reset with the default switches, play <frames> frames
// of scripted two-player inputs (the same script as statetest), and every 600 frames print
//   <game> <frame> ram=<hash> pic=<hash> snd=<hash>
// ram is the main RAM (what the game itself computes, the netplay desync check); pic and snd
// are every picture and every sound sample since the last line. The last line is
//   <game> state=<hash> bytes=<n>
// the hash of a save state taken at the end (R5.3: a state must be the same bytes everywhere).
// Builds that agree print the same lines: compare the output of two builds with diff.
// GOLDEN_STATE_DIR=<folder> also writes each state to <folder>/<game>.state.
#include "games.h"
#include <cstdio>
#include <cstring>

static cabinet_inputs inputs_for(int f)
{
	cabinet_inputs in{};
	if (f >= 600 && f < 606) in.p[0].shoot = true;            // past an error screen
	if (f >= 900 && f < 906) { in.p[0].start = true; in.p[1].start = true; }
	if (f > 960 && (f % 50) < 4) { in.p[0].shoot = true; in.p[1].shoot = true; }
	if (f > 1400 && (f % 200) < 60) in.p[0].right = true;
	if (f > 1400 && (f % 170) < 50) in.p[1].left = true;
	if (f > 1400 && (f % 90) < 5) in.p[0].turbo = true;
	if (f > 1400 && (f % 130) < 3) in.p[1].pass = true;
	return in;
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: goldentest <base> <frames> [games...]\n"); return 2; }
	std::string base = argv[1];
	int frames = atoi(argv[2]);
	std::vector<std::string> want(argv + 3, argv + argc);
	game_catalog cat(base);
	int ran = 0;
	for (auto &g : game_list())
	{
		if (!want.empty() && std::find(want.begin(), want.end(), g.id) == want.end()) continue;
		std::string why, err;
		if (!cat.available(g, why)) { if (!want.empty()) printf("%s skipped: %s\n", g.id, why.c_str()); continue; }
		auto m = cat.create(g, err);
		if (!m) { printf("%s failed: %s\n", g.id, err.c_str()); return 1; }
		dip_settings d; m->set_dips(d); m->reset();
		u32 pic = 2166136261u, snd = 2166136261u;
		std::vector<float> a;
		for (int f = 1; f <= frames; f++)
		{
			m->set_inputs(inputs_for(f));
			m->run_frame();
			pic = fnv1a(m->frame(), size_t(m->width()) * m->height() * 4, pic);
			a.clear();
			m->take_audio(a);
			snd = fnv1a(a.data(), a.size() * sizeof(float), snd);
			if (f % 600 == 0)
			{
				printf("%s %d ram=%08x pic=%08x snd=%08x\n", g.id, f, m->state_hash(), pic, snd);
				pic = snd = 2166136261u;
			}
		}
		std::vector<u8> st = m->save_state();
		printf("%s state=%08x bytes=%zu\n", g.id, st.empty() ? 0u : fnv1a(st.data(), st.size()), st.size());
		// GOLDEN_STATE_DIR=<folder>: also write the state there, to compare two builds byte by byte
		if (const char *dir = getenv("GOLDEN_STATE_DIR"))
			if (FILE *f = fopen((std::string(dir) + "/" + g.id + ".state").c_str(), "wb")) { fwrite(st.data(), 1, st.size(), f); fclose(f); }
		fflush(stdout);
		ran++;
	}
	return ran > 0 ? 0 : 1;
}
