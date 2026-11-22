// statetest.cpp - save state check (roadmap N1.1).
//
//   statetest <base> <frames-before> <frames-after> [games...]
//
// For each game: play scripted inputs to frame N, save the state, play <frames-after> more
// frames and record a hash of every frame (main RAM, the picture and the sound of that
// frame); then load the state and play the same frames again, once in the same machine and
// once in a second machine made fresh for the game, and compare every hash. The second
// machine shows that the state holds everything (nothing left over in the first machine).
// Prints the size of a state and how long a save and a load take. Exit status 0 when every
// game passes.
#include "games.h"
#include "audio_join.h"
#include <cmath>
#include <chrono>
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

// the sound as played: the largest step between two samples, and the last sample
static float g_last = 0, g_step = 0;
static bool g_any = false;
static void heard(const std::vector<float> &a)
{
	for (float v : a) { if (g_any) g_step = std::max(g_step, std::fabs(v - g_last)); g_last = v; g_any = true; }
}

static u32 frame_hash(arcade_machine &m, std::vector<float> &a)
{
	u32 h = m.state_hash();
	h = fnv1a(m.frame(), size_t(m.width()) * m.height() * 4, h);
	a.clear();
	m.take_audio(a);
	heard(a);
	return fnv1a(a.data(), a.size() * sizeof(float), h);
}

static std::vector<u32> play(arcade_machine &m, int from, int n)
{
	std::vector<u32> out;
	std::vector<float> a;
	for (int f = from; f < from + n; f++)
	{
		m.set_inputs(inputs_for(f));
		m.run_frame();
		out.push_back(frame_hash(m, a));
	}
	return out;
}

static int first_diff(const std::vector<u32> &a, const std::vector<u32> &b)
{
	for (size_t i = 0; i < a.size() && i < b.size(); i++)
		if (a[i] != b[i]) return int(i);
	return a.size() == b.size() ? -1 : int(std::min(a.size(), b.size()));
}

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: statetest <base> <frames-before> <frames-after> [games...]\n"); return 2; }
	std::string base = argv[1];
	int before = atoi(argv[2]), after = atoi(argv[3]);
	std::vector<std::string> want(argv + 4, argv + argc);
	game_catalog cat(base);
	int pass = 0, fail = 0;
	for (auto &g : game_list())
	{
		if (!want.empty() && std::find(want.begin(), want.end(), g.id) == want.end()) continue;
		std::string why, err;
		if (!cat.available(g, why)) { if (!want.empty()) { printf("SKIP %s: %s\n", g.id, why.c_str()); } continue; }
		auto m = cat.create(g, err);
		if (!m) { printf("FAIL %s: %s\n", g.id, err.c_str()); fail++; continue; }
		dip_settings d; m->set_dips(d); m->reset();
		std::vector<float> a;
		for (int f = 1; f <= before; f++) { m->set_inputs(inputs_for(f)); m->run_frame(); a.clear(); m->take_audio(a); }

		auto t0 = std::chrono::steady_clock::now();
		std::vector<u8> st = m->save_state();
		auto t1 = std::chrono::steady_clock::now();
		if (st.empty()) { printf("FAIL %s: save: %s\n", g.id, m->state_error().c_str()); fail++; continue; }
		g_any = false; g_step = 0;
		std::vector<u32> ref = play(*m, before + 1, after);
		float normal_step = g_step, last_played = g_last;

		auto t2 = std::chrono::steady_clock::now();
		bool ok1 = m->load_state(st);
		auto t3 = std::chrono::steady_clock::now();
		if (!ok1) { printf("FAIL %s: load: %s\n", g.id, m->state_error().c_str()); fail++; continue; }
		// the sound at the jump back (roadmap N1.2): the step from the last sample played to
		// the first one after the load, as it is and with audio_join
		float raw_step = 0, joined_step = 0;
		{
			arcade_machine &mm = *m;
			std::vector<u8> here = mm.save_state();
			mm.set_inputs(inputs_for(before + 1)); mm.run_frame();
			std::vector<float> b; mm.take_audio(b);
			if (!b.empty()) raw_step = std::fabs(b[0] - last_played);
			audio_join j;
			j.played({ last_played });
			j.jump();
			j.apply(b);
			float prev = last_played;
			for (float v : b) { joined_step = std::max(joined_step, std::fabs(v - prev)); prev = v; }
			mm.load_state(here);
		}
		g_any = false;
		std::vector<u32> same = play(*m, before + 1, after);

		auto m2 = cat.create(g, err);
		m2->set_dips(d); m2->reset();
		if (!m2->load_state(st)) { printf("FAIL %s: load in a new machine: %s\n", g.id, m2->state_error().c_str()); fail++; continue; }
		std::vector<u32> fresh = play(*m2, before + 1, after);

		// the state saved again right after a load is the same bytes
		m2->load_state(st);
		std::vector<u8> again = m2->save_state();

		int d1 = first_diff(ref, same), d2 = first_diff(ref, fresh);
		bool bytes_same = again == st;
		auto us = [](auto a, auto b) { return (long long)std::chrono::duration_cast<std::chrono::microseconds>(b - a).count(); };
		bool smooth = joined_step <= normal_step + 1e-6f;
		if (d1 < 0 && d2 < 0 && bytes_same && smooth)
		{
			pass++;
			printf("PASS %s: saved at frame %d, %d frames equal after a load (same machine and a new one); state %zu bytes, save %lld us, load %lld us; "
				"sound at the jump back: step %.4f as it is, %.4f joined (largest in play %.4f)\n",
				g.id, before, after, st.size(), us(t0, t1), us(t2, t3), raw_step, joined_step, normal_step);
		}
		else
		{
			fail++;
			printf("FAIL %s:", g.id);
			if (d1 >= 0) printf(" same machine differs from frame %d;", before + 1 + d1);
			if (!smooth) printf(" the joined sound steps %.4f, more than %.4f in play;", joined_step, normal_step);
			if (d2 >= 0) printf(" new machine differs from frame %d;", before + 1 + d2);
			if (!bytes_same)
			{
				size_t i = 0;
				while (i < st.size() && i < again.size() && st[i] == again[i]) i++;
				printf(" saved again after a load, the bytes differ at %zu of %zu;", i, st.size());
			}
			printf("\n");
		}
		fflush(stdout);
	}
	printf("state test: %d passed, %d failed\n", pass, fail);
	return fail ? 1 : 0;
}
