// statebench.cpp - save state size and speed (roadmap N1.3).
//
//   statebench <base> [frames] [games...]
//
// For each game, at two points (frame 1 500, in the attract mode, and frame 4 000, in a
// game with scripted two-player inputs) it saves a state every frame for [frames] frames
// (default 600) and reports:
//   * the raw size, and the size compressed with miniz at level 1 and level 6;
//   * a frame-to-frame delta (the bytes XOR the previous frame's state) compressed at
//     level 1, which is what sending a state every frame would cost;
//   * the time of a save, a load, a level-1 compression and a decompression, as the
//     median and the slowest over all frames;
//   * the sections of the last state, largest first, raw and compressed.
// Every load is of the state just saved, and every decompressed state must be the same
// bytes, so the run also checks that compression loses nothing. Exit status 0 when all
// of that holds for every game.
#include "games.h"
#include "miniz.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

static cabinet_inputs inputs_for(int f)
{
	cabinet_inputs in{};
	if (f >= 600 && f < 606) in.p[0].shoot = true;
	if (f >= 900 && f < 906) { in.p[0].start = true; in.p[1].start = true; }
	if (f > 960 && (f % 50) < 4) { in.p[0].shoot = true; in.p[1].shoot = true; }
	if (f > 1400 && (f % 200) < 60) in.p[0].right = true;
	if (f > 1400 && (f % 170) < 50) in.p[1].left = true;
	if (f > 1400 && (f % 90) < 5) in.p[0].turbo = true;
	if (f > 1400 && (f % 130) < 3) in.p[1].pass = true;
	return in;
}

using clk = std::chrono::steady_clock;
static double us(clk::time_point a, clk::time_point b) { return std::chrono::duration<double, std::micro>(b - a).count(); }

struct stat
{
	std::vector<double> v;
	void add(double x) { v.push_back(x); }
	double med() { if (v.empty()) return 0; std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
	double max() { return v.empty() ? 0 : *std::max_element(v.begin(), v.end()); }
	double mean() { double s = 0; for (double x : v) s += x; return v.empty() ? 0 : s / v.size(); }
};

static std::vector<u8> pack(const std::vector<u8> &in, int level)
{
	mz_ulong n = mz_compressBound(mz_ulong(in.size()));
	std::vector<u8> out(n);
	if (mz_compress2(out.data(), &n, in.data(), mz_ulong(in.size()), level) != MZ_OK) return {};
	out.resize(n);
	return out;
}

static bool unpack(const std::vector<u8> &in, size_t size, std::vector<u8> &out)
{
	out.resize(size);
	mz_ulong n = mz_ulong(size);
	return mz_uncompress(out.data(), &n, in.data(), mz_ulong(in.size())) == MZ_OK && n == size;
}

static void kb(char *b, double bytes) { snprintf(b, 32, "%.1f KB", bytes / 1024.0); }

static bool bench_at(arcade_machine &m, const char *gid, int &frame, int to, int frames)
{
	for (; frame < to; frame++) { m.set_inputs(inputs_for(frame + 1)); m.run_frame(); std::vector<float> a; m.take_audio(a); }
	stat save_t, load_t, pack_t, unpack_t, delta_t, raw_b, p1_b, delta_b;
	std::vector<u8> prev, st, out, x;
	bool good = true;
	for (int i = 0; i < frames; i++, frame++)
	{
		m.set_inputs(inputs_for(frame + 1)); m.run_frame();
		std::vector<float> a; m.take_audio(a);
		auto t0 = clk::now(); bool saved = m.save_state_to(st); auto t1 = clk::now();
		if (!saved) { printf("FAIL %s: save at frame %d: %s\n", gid, frame + 1, m.state_error().c_str()); return false; }
		auto t2 = clk::now(); bool ok = m.load_state(st); auto t3 = clk::now();
		if (!ok) { printf("FAIL %s: load at frame %d: %s\n", gid, frame + 1, m.state_error().c_str()); return false; }
		auto t4 = clk::now(); std::vector<u8> p = pack(st, 1); auto t5 = clk::now();
		auto t6 = clk::now(); bool same = unpack(p, st.size(), out) && out == st; auto t7 = clk::now();
		if (!same) { printf("FAIL %s: compression lost bytes at frame %d\n", gid, frame + 1); good = false; }
		save_t.add(us(t0, t1)); load_t.add(us(t2, t3)); pack_t.add(us(t4, t5)); unpack_t.add(us(t6, t7));
		raw_b.add(double(st.size())); p1_b.add(double(p.size()));
		if (!prev.empty())
		{
			// the bytes XOR the frame before's, as far as both go (a state's size can change
			// by a few bytes, the DCS queue), then the rest as it is
			auto t8 = clk::now();
			x.assign(st.begin(), st.end());
			for (size_t k = 0; k < st.size() && k < prev.size(); k++) x[k] ^= prev[k];
			std::vector<u8> d = pack(x, 1);
			auto t9 = clk::now();
			delta_b.add(double(d.size())); delta_t.add(us(t8, t9));
		}
		prev.assign(st.begin(), st.end());
	}
	st = prev;
	std::vector<u8> p6 = pack(st, 6);
	char r[32], c1[32], c6[32], dm[32], dx[32];
	kb(r, raw_b.mean()); kb(c1, p1_b.mean()); kb(c6, double(p6.size())); kb(dm, delta_b.mean()); kb(dx, delta_b.max());
	printf("  %s at frame %d (%d frames)\n", gid, to, frames);
	printf("    size: %s raw, %s at level 1, %s at level 6; delta from the frame before: %s on average, %s at most\n", r, c1, c6, dm, dx);
	printf("    time (median / slowest, us): save %.0f / %.0f, load %.0f / %.0f, compress %.0f / %.0f, decompress %.0f / %.0f, delta %.0f / %.0f\n",
		save_t.med(), save_t.max(), load_t.med(), load_t.max(), pack_t.med(), pack_t.max(), unpack_t.med(), unpack_t.max(), delta_t.med(), delta_t.max());

	// sections of the last state
	std::vector<std::pair<std::string, size_t>> marks;
	state_io::s_marks = &marks;
	std::vector<u8> traced = m.save_state();
	state_io::s_marks = nullptr;
	struct sec { std::string name; size_t raw, packed; };
	std::vector<sec> secs;
	for (size_t i = 0; i < marks.size(); i++)
	{
		size_t a = marks[i].second, b = i + 1 < marks.size() ? marks[i + 1].second : traced.size();
		if (b <= a) continue;
		std::vector<u8> part(traced.begin() + a, traced.begin() + b);
		secs.push_back({ marks[i].first, b - a, pack(part, 1).size() });
	}
	std::sort(secs.begin(), secs.end(), [](const sec &x, const sec &y) { return x.raw > y.raw; });
	printf("    sections (raw / level 1):");
	for (size_t i = 0; i < secs.size() && i < 8; i++)
	{
		kb(r, double(secs[i].raw)); kb(c1, double(secs[i].packed));
		printf("%s %s %s / %s", i ? "," : "", secs[i].name.c_str(), r, c1);
	}
	printf("\n");
	return good;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: statebench <base> [frames] [games...]\n"); return 2; }
	std::string base = argv[1];
	int frames = argc > 2 ? atoi(argv[2]) : 600;
	std::vector<std::string> want(argc > 3 ? argv + 3 : argv + argc, argv + argc);
	game_catalog cat(base);
	int pass = 0, fail = 0;
	for (auto &g : game_list())
	{
		if (!want.empty() && std::find(want.begin(), want.end(), g.id) == want.end()) continue;
		std::string why, err;
		if (!cat.available(g, why)) { if (!want.empty()) printf("SKIP %s: %s\n", g.id, why.c_str()); continue; }
		auto m = cat.create(g, err);
		if (!m) { printf("FAIL %s: %s\n", g.id, err.c_str()); fail++; continue; }
		dip_settings d; m->set_dips(d); m->reset();
		int frame = 0;
		bool ok = bench_at(*m, g.id, frame, 1500, frames);
		ok = ok && bench_at(*m, g.id, frame, 4000, frames);
		if (ok) { pass++; printf("PASS %s\n", g.id); } else fail++;
	}
	printf("%d passed, %d failed\n", pass, fail);
	return fail ? 1 : 0;
}
