// widetest.cpp - the widescreen engine hooks change nothing (roadmap W3.1).
//
//   widetest <base> <sym_dir> <frames> [games...]
//
// For each game whose ROMs are in <base>, two machines play the same scripted inputs side by
// side. The second has every widescreen hook on:
//   * a watch on the game's display routine (DISPLAY, from the symbol file in <sym_dir>),
//     which walks the object list each time the routine starts, reading only;
//   * an observer on the blitter that draws every command the game issues a second time,
//     into a 512-pixel-wide picture and into a 1024-pixel-wide one (blit_to).
// Checked: every frame's hash (main RAM, picture, sound) is the same in both machines; the
// watch ran; and the two copies hold the same pixels in the columns both have (0 to 511),
// so the wide picture draws exactly what the board draws. Exit status 0 when all pass.
// With WIDETEST_PNG=<dir>, at four frames it writes the board's picture (<game>_<frame>_board.png)
// and that frame's commands drawn into the 1024 x 512 picture (<game>_<frame>_wide.png): both
// video pages, the 56-pixel pad, and black where nothing was drawn this frame.
#include "games.h"
#include "romload.h"
#include "miniz.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

static void write_png(const std::string &path, int w, int h, const std::vector<u32> &px)
{
	std::vector<u8> rgb(size_t(w) * h * 3);
	for (size_t i = 0; i < size_t(w) * h; i++) { rgb[i * 3] = u8(px[i] >> 16); rgb[i * 3 + 1] = u8(px[i] >> 8); rgb[i * 3 + 2] = u8(px[i]); }
	size_t len = 0;
	void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &len);
	if (FILE *f = fopen(path.c_str(), "wb")) { fwrite(png, 1, len, f); fclose(f); }
	mz_free(png);
}

static cabinet_inputs inputs_for(int f)
{
	cabinet_inputs in{};
	if (f >= 600 && f < 606) in.p[0].shoot = true;
	if (f >= 900 && f < 906) { in.p[0].coin = true; }
	if (f >= 960 && f < 966) { in.p[0].start = true; }
	if (f > 1000 && (f % 50) < 4) in.p[0].shoot = true;
	if (f > 1400 && (f % 200) < 60) in.p[0].right = true;
	if (f > 1400 && (f % 90) < 5) in.p[0].turbo = true;
	return in;
}

static u32 frame_hash(arcade_machine &m)
{
	std::vector<float> a;
	u32 h = m.state_hash();
	h = fnv1a(m.frame(), size_t(m.width()) * m.height() * 4, h);
	m.take_audio(a);
	return fnv1a(a.data(), a.size() * sizeof(float), h);
}

// "  NAME = 0x01234567," from the game's symbol file
static u32 symbol(const std::string &file, const std::string &name)
{
	std::ifstream in(file);
	std::string l;
	while (std::getline(in, l))
	{
		std::istringstream w(l);
		std::string n, eq, v;
		if (w >> n >> eq >> v && n == name && eq == "=" && v.rfind("0x", 0) == 0) return u32(strtoul(v.c_str(), nullptr, 16));
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: widetest <base> <sym_dir> <frames> [games...]\n"); return 2; }
	std::string base = argv[1], symdir = argv[2];
	int frames = atoi(argv[3]);
	std::vector<std::string> want(argv + 4, argv + argc);
	game_catalog cat(base);
	int pass = 0, fail = 0;
	for (auto &g : game_list())
	{
		if (!want.empty() && std::find(want.begin(), want.end(), g.id) == want.end()) continue;
		std::string why, err;
		if (!cat.available(g, why)) { if (!want.empty()) printf("SKIP %s: %s\n", g.id, why.c_str()); continue; }
		auto a = cat.create(g, err), b = cat.create(g, err);
		if (!a || !b) { printf("FAIL %s: %s\n", g.id, err.c_str()); fail++; continue; }
		dip_settings d;
		a->set_dips(d); a->reset();
		b->set_dips(d); b->reset();

		char fp[16];
		std::vector<u8> *prog = b->program_rom();     // named as the Lua mods name it (mods.cpp)
		snprintf(fp, sizeof fp, "%08x", prog ? crc32_of(prog->data(), prog->size()) : 0u);
		std::string sym = symdir + "/" + g.id + "_" + fp + ".lua";
		u32 display = symbol(sym, "DISPLAY"), objlst = symbol(sym, "OBJLST");

		long watched = 0, objects = 0;
		if (display && objlst)
			b->watch_pc(display, [&]() {
				watched++;
				u32 o = u32(b->peek16(objlst)) | u32(b->peek16(objlst + 16)) << 16;
				for (int n = 0; o && n < 600; n++) { objects++; o = u32(b->peek16(o)) | u32(b->peek16(o + 16)) << 16; }
			});
		std::vector<u16> narrow(size_t(512) * 512, 0), wide(size_t(1024) * 512, 0);
		long dmas = 0;
		b->observe_dma([&](const u16 *regs) {
			dmas++;
			b->wide_blit(regs, narrow.data(), 512);
			b->wide_blit(regs, wide.data(), 1024);
		});

		const char *pngdir = getenv("WIDETEST_PNG");
		auto shot = [&](int f) { return pngdir && (f == 600 || f == 1500 || f == frames * 3 / 4 || f == frames); };
		int first_bad = -1, pix_bad_frames = 0;
		for (int f = 1; f <= frames; f++)
		{
			if (shot(f)) { std::fill(narrow.begin(), narrow.end(), 0); std::fill(wide.begin(), wide.end(), 0); }
			a->set_inputs(inputs_for(f)); a->run_frame();
			b->set_inputs(inputs_for(f)); b->run_frame();
			if (first_bad < 0 && frame_hash(*a) != frame_hash(*b)) first_bad = f;
			bool same = true;
			for (int y = 0; y < 512 && same; y++) same = memcmp(&narrow[size_t(y) * 512], &wide[size_t(y) * 1024], 512 * sizeof(u16)) == 0;
			if (!same) pix_bad_frames++;
			if (shot(f))
			{
				std::string stem = std::string(pngdir) + "/" + g.id + "_" + std::to_string(f);
				std::vector<u32> board(a->frame(), a->frame() + size_t(a->width()) * a->height()), w(size_t(1024) * 512);
				write_png(stem + "_board.png", a->width(), a->height(), board);
				const u32 *pens = b->pens();
				for (size_t i = 0; i < w.size(); i++) w[i] = pens ? pens[wide[i] & 0x7fff] : 0;
				write_png(stem + "_wide.png", 1024, 512, w);
			}
		}
		bool ok = first_bad < 0 && pix_bad_frames == 0 && dmas > 0 && (!display || watched > 0);
		printf("%s %s: %d frames, hashes %s, %ld blitter commands drawn again (%s), display routine %s\n",
		       ok ? "ok  " : "FAIL", g.id, frames, first_bad < 0 ? "equal" : ("differ from frame " + std::to_string(first_bad)).c_str(), dmas,
		       pix_bad_frames ? (std::to_string(pix_bad_frames) + " frames with different pixels").c_str() : "same pixels in 512 and 1024 wide",
		       display ? ("watched " + std::to_string(watched) + " times, " + std::to_string(objects) + " objects read").c_str() : "not watched (no symbol file)");
		(ok ? pass : fail)++;
	}
	printf("\n%d passed, %d failed\n", pass, fail);
	return fail ? 1 : 0;
}
