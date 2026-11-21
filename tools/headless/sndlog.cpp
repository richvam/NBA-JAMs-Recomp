// sndlog.cpp - the sound commands of a game, from the user's ROMs (roadmap U6.1).
//
//   sndlog <data_dir> <game_id> log [frames]
//       runs the game with a scripted coin, start and moves on the team select screen and
//       prints each byte the main CPU sends the sound board, with its frame and the inputs
//       pressed at that frame, so the menu music and the cursor, confirm and back sounds can
//       be told apart.
//   sndlog <data_dir> <game_id> menu
//       the check of U6.1 and U6.2 through the menus' own player (menu_sound): the tune
//       plays, each of the cursor, confirm and back sounds plays with the music off, and
//       nothing at all comes out with both off. Exit status 0 when all of that holds.
//   sndlog <data_dir> <game_id> play <frames> <byte> [byte...]   (w<n>: wait n frames)
//       the sound board on its own (main CPU held), the bytes sent after it has started, and
//       the loudness of what it plays, a tenth of a second at a time.
//
// Nothing is written to disk.
#include "games.h"
#include "menu_sound.h"
#include <chrono>
#include <mutex>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static const char *pressed(int f, cabinet_inputs &in)
{
	// a button to pass the power-on screens, then (free play) start, then right on team select
	// every 40 frames, down, shoot, pass and turbo. T0=<frame> moves the start.
	static const int t0 = getenv("T0") ? atoi(getenv("T0")) : 1600;
	auto on = [&](int at, int len) { return f >= at && f < at + len; };
	in = cabinet_inputs{};
	if (on(500, 5) || on(900, 5)) { in.p[0].shoot = true; return "shoot (power-on)"; }
	if (on(t0, 6) || on(t0 + 200, 6)) { in.p[0].start = true; return "start"; }
	for (int k = 0; k < 8; k++) if (on(t0 + 600 + k * 40, 5)) { in.p[0].right = true; return "right"; }
	for (int k = 0; k < 4; k++) if (on(t0 + 1000 + k * 40, 5)) { in.p[0].down = true; return "down"; }
	if (on(t0 + 1300, 5)) { in.p[0].shoot = true; return "shoot"; }
	if (on(t0 + 1400, 5)) { in.p[0].pass = true; return "pass"; }
	if (on(t0 + 1500, 5)) { in.p[0].turbo = true; return "turbo"; }
	return "";
}

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: sndlog <data_dir> <game_id> log [frames] | play <frames> <byte>...\n"); return 1; }
	std::string base = argv[1], id = argv[2], mode = argv[3];
	game_catalog cat(base);
	const game_info *gi = nullptr;
	for (auto &g : game_list()) if (id == g.id) gi = &g;
	if (!gi) { fprintf(stderr, "unknown game %s\n", id.c_str()); return 1; }
	std::string err;
	auto m = cat.create(*gi, err);
	if (!m) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
	dip_settings d;
	m->set_dips(d);
	m->reset();
	std::vector<float> a;
	if (mode == "log")
	{
		int frames = argc > 4 ? atoi(argv[4]) : 2400, f = 0;
		const char *now = "";
		m->on_sound_command = [&](u8 b) { printf("%5d %02x %s\n", f, b, now); };
		for (f = 0; f < frames; f++)
		{
			cabinet_inputs in;
			now = pressed(f, in);
			m->set_inputs(in);
			m->run_frame();
			m->take_audio(a);
			// SHOTS=<dir>: a picture every 100 frames, to see where the script is (not for the repository)
			if (const char *dir = getenv("SHOTS"))
				if (f % 100 == 0)
				{
					char name[512];
					snprintf(name, sizeof(name), "%s/%s_%05d.ppm", dir, id.c_str(), f);
					if (FILE *o = fopen(name, "wb"))
					{
						fprintf(o, "P6 %d %d 255\n", m->width(), m->height());
						const u32 *px = m->frame();
						for (int i = 0; i < m->width() * m->height(); i++) { u8 rgb[3] = { u8(px[i] >> 16), u8(px[i] >> 8), u8(px[i]) }; fwrite(rgb, 1, 3, o); }
						fclose(o);
					}
				}
		}
		return 0;
	}
	if (mode == "play" && argc > 5)
	{
		int frames = atoi(argv[4]);
		m->set_sound_only(true);
		// let the sound board start (it plays nothing until asked)
		for (int f = 0; f < (getenv("WARM") ? atoi(getenv("WARM")) : 120); f++) { m->run_frame(); m->take_audio(a); }
		a.clear();
		std::vector<float> all;
		for (int i = 5; i < argc; i++)
		{
			// "w<n>": n frames with nothing sent
			const int n = argv[i][0] == 'w' ? atoi(argv[i] + 1) : 1;
			if (argv[i][0] != 'w') m->sound_command(u8(strtol(argv[i], nullptr, 16)));
			for (int k = 0; k < n; k++) { m->run_frame(); m->take_audio(a); all.insert(all.end(), a.begin(), a.end()); a.clear(); }
		}
		for (int f = 0; f < frames; f++) { m->run_frame(); m->take_audio(a); all.insert(all.end(), a.begin(), a.end()); a.clear(); }
		const size_t step = size_t(m->audio_rate() / 10);
		double peak_all = 0;
		printf("loudness (rms x1000) every 0.1 s:");
		for (size_t i = 0; i + step <= all.size(); i += step)
		{
			// without the steady level the DAC rests at: what changes is what is heard
			double mean = 0, s = 0;
			for (size_t k = i; k < i + step; k++) mean += all[k];
			mean /= double(step);
			for (size_t k = i; k < i + step; k++) s += (all[k] - mean) * (all[k] - mean);
			double r = std::sqrt(s / double(step));
			peak_all = std::max(peak_all, r);
			printf(" %d", int(r * 1000));
		}
		printf("\npeak %d\n", int(peak_all * 1000));
		return peak_all > 0.005 ? 0 : 2;
	}
	if (mode == "menu")
	{
		m.reset();                                   // the menu player makes its own
		std::mutex mx;
		std::vector<float> got;
		menu_sound ms([&](const std::string &) { std::string e; return cat.create(*gi, e); },
		              [] { return size_t(0); },
		              [&](const float *p, size_t n) { std::lock_guard<std::mutex> l(mx); got.insert(got.end(), p, p + n); });
		const size_t sec = 48000;
		auto size = [&] { std::lock_guard<std::mutex> l(mx); return got.size(); };
		auto wait_for = [&](size_t n) { for (int i = 0; i < 6000 && size() < n; i++) std::this_thread::sleep_for(std::chrono::milliseconds(5)); };
		auto wait_ready = [&] { for (int i = 0; i < 6000 && !ms.ready(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(5)); };
		auto loud = [&](size_t from, size_t to)
		{
			std::lock_guard<std::mutex> l(mx);
			double peak = 0;
			for (size_t i = from; i + sec / 10 <= std::min(to, got.size()); i += sec / 10)
			{
				double mean = 0, s2 = 0;
				for (size_t k = i; k < i + sec / 10; k++) mean += got[k];
				mean /= double(sec / 10);
				for (size_t k = i; k < i + sec / 10; k++) s2 += (got[k] - mean) * (got[k] - mean);
				peak = std::max(peak, std::sqrt(s2 / double(sec / 10)));
			}
			return int(peak * 1000);
		};
		bool ok = true;
		ms.want(id, true, true, 1.0f);
		wait_ready();
		size_t at = size();
		wait_for(at + 10 * sec);
		int tune = loud(at + 5 * sec, at + 10 * sec);
		printf("music, 5 to 10 s after it starts: %d\n", tune);
		ok = ok && tune > 5;
		ms.want(id, false, true, 1.0f);              // music off, sounds on
		wait_ready();
		at = size();
		wait_for(at + sec);
		int quiet = loud(at + sec / 2, at + sec);
		printf("music off: %d\n", quiet);
		ok = ok && quiet <= 2;
		static const char *names[3] = { "cursor", "confirm", "back" };
		for (int c = 0; c < 3; c++)
		{
			at = size();
			ms.play(menu_sound::cue(c));
			wait_for(at + sec);
			int r = loud(at, at + sec);
			printf("%s sound: %d\n", names[c], r);
			ok = ok && r > 5;
			wait_for(size() + sec);                  // let it end
		}
		ms.want(id, false, false, 1.0f);             // both off
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
		at = size();
		std::this_thread::sleep_for(std::chrono::milliseconds(700));
		printf("both off: %zu samples in 0.7 s\n", size() - at);
		ok = ok && size() == at;
		printf("%s\n", ok ? "OK" : "FAILED");
		return ok ? 0 : 2;
	}
	fprintf(stderr, "bad mode\n");
	return 1;
}
