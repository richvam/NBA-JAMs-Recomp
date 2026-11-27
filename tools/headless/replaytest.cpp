// replaytest - the replay file (roadmap N5.4), no SDL, no ROMs: every input of every frame
// comes back as recorded, the header round-trips, and damaged files are refused.
// (A full game recorded and played back is checked with ROMs: tools/audit/i_replay.pad.)
#include <cstdio>
#include <string>
#include "replay.h"

static int g_run = 0, g_fail = 0;
#define CHECK(c, what) do { g_run++; if (c) printf("ok   %s\n", what); else { g_fail++; printf("FAIL %s\n", what); } } while (0)

static bool same(const cabinet_inputs &a, const cabinet_inputs &b)
{
	for (int s = 0; s < 4; s++)
	{
		const auto &x = a.p[s], &y = b.p[s];
		if (x.up != y.up || x.down != y.down || x.left != y.left || x.right != y.right || x.shoot != y.shoot || x.pass != y.pass ||
		    x.turbo != y.turbo || x.start != y.start || x.coin != y.coin) return false;
	}
	return a.service == b.service && a.test == b.test && a.volume_up == b.volume_up && a.volume_down == b.volume_down;
}

int main()
{
	// a recording of 30 000 frames (about nine minutes) of busy inputs
	replay r;
	r.build = "2026.11.06-300-gabcdef0"; r.game = "nbahangt"; r.mods = "full_game.lua\x01on";
	r.saved_at = 1795000000; r.players = 2; r.online = true; r.start_hash = 0x12345678; r.end_hash = 0x9abcdef0;
	for (int i = 0; i < 300000; i++) r.state.push_back(uint8_t(i * 7 + (i >> 8)));
	std::vector<cabinet_inputs> want;
	uint32_t x = 1;
	for (int f = 0; f < 30000; f++)
	{
		cabinet_inputs in{};
		for (int s = 0; s < 4; s++)
		{
			x = x * 1103515245u + 12345u;
			auto &p = in.p[s];
			p.up = x >> 16 & 1; p.down = x >> 17 & 1; p.left = x >> 18 & 1; p.right = x >> 19 & 1; p.shoot = x >> 20 & 1;
			p.pass = x >> 21 & 1; p.turbo = x >> 22 & 1; p.start = x >> 23 & 1; p.coin = x >> 24 & 1;
		}
		in.service = f % 97 == 0; in.test = f % 1001 == 0; in.volume_up = f % 13 == 0; in.volume_down = f % 17 == 0;
		r.add(in);
		want.push_back(in);
	}
	CHECK(r.count() == 30000, "replay: 30 000 frames recorded");
	bool all = true;
	for (uint32_t f = 0; f < r.count(); f++) all = all && same(r.at(f), want[f]);
	CHECK(all, "replay: every input of every frame reads back as recorded (four players, service, test, volume)");

	std::vector<uint8_t> file = r.encode();
	printf("  file: %zu bytes for %u frames and a %zu-byte state\n", file.size(), r.count(), r.state.size());
	replay b;
	std::string err;
	bool ok = replay::decode(file, b, err);
	CHECK(ok && b.build == r.build && b.game == r.game && b.mods == r.mods && b.saved_at == r.saved_at && b.players == 2 && b.online &&
	      b.start_hash == r.start_hash && b.end_hash == r.end_hash && b.state == r.state && b.frames == r.frames,
	      "replay: the file reads back the same (header, state and frames)");
	all = ok;
	for (uint32_t f = 0; ok && f < b.count(); f++) all = all && same(b.at(f), want[f]);
	CHECK(all, "replay: every frame from the file is the input recorded");
	CHECK(file.size() < r.frames.size() + r.state.size(), "replay: the file is compressed");

	// damaged files are refused, never read wrongly
	auto refused = [&](std::vector<uint8_t> f) { replay z; std::string e; return !replay::decode(f, z, e) && !e.empty(); };
	std::vector<uint8_t> t = file; t.resize(t.size() / 2);
	CHECK(refused(t), "replay: a cut file is refused");
	t = file; t[0] = 'X';
	CHECK(refused(t), "replay: a file that is not a replay is refused");
	t = file; t[4] = 9;
	CHECK(refused(t), "replay: a replay of another format version is refused");
	t = file; t[t.size() / 2] ^= 0x5a;
	CHECK(refused(t), "replay: a changed byte is refused (the compressed data's own check)");
	CHECK(refused({}), "replay: an empty file is refused");
	CHECK(r.at(99999).p[0].up == false, "replay: a frame past the end is neutral");

	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
