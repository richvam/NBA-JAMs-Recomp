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
//
// W3.2: a third machine draws the wide gameplay picture (wide_view, WIDETEST_COLS columns,
// 533 by default). Checked: its frame hashes equal the plain machine's too, and in every frame
// of play every column of the wide view holds a drawn pixel. At WIDETEST_COLS=400 the view must
// be exactly the game's own picture of the same display pass wherever its camera is the game's. With WIDETEST_PNG the same four
// frames are written as <game>_<frame>_view.png.
#include "games.h"
#include "romload.h"
#include "wide_view.h"
#include "miniz.h"
#include <algorithm>
#include <memory>
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
	int cols = getenv("WIDETEST_COLS") ? atoi(getenv("WIDETEST_COLS")) : 533;
	std::vector<std::string> want(argv + 4, argv + argc);
	game_catalog cat(base);
	int pass = 0, fail = 0;
	for (auto &g : game_list())
	{
		if (!want.empty() && std::find(want.begin(), want.end(), g.id) == want.end()) continue;
		std::string why, err;
		if (!cat.available(g, why)) { if (!want.empty()) printf("SKIP %s: %s\n", g.id, why.c_str()); continue; }
		auto a = cat.create(g, err), b = cat.create(g, err), c = cat.create(g, err);
		if (!a || !b || !c) { printf("FAIL %s: %s\n", g.id, err.c_str()); fail++; continue; }
		dip_settings d;
		a->set_dips(d); a->reset();
		b->set_dips(d); b->reset();
		c->set_dips(d); c->reset();

		char fp[16];
		std::vector<u8> *prog = b->program_rom();     // named as the Lua mods name it (mods.cpp)
		snprintf(fp, sizeof fp, "%08x", prog ? crc32_of(prog->data(), prog->size()) : 0u);
		std::string sym = symdir + "/" + g.id + "_" + fp + ".lua";
		u32 display = symbol(sym, "DISPLAY"), objlst = symbol(sym, "OBJLST");
		wide_symbols ws;
		ws.display = display; ws.objlst = objlst;
		ws.worldtlx = symbol(sym, "WORLDTLX"); ws.worldtly = symbol(sym, "WORLDTLY");
		ws.gndx = symbol(sym, "gndx"); ws.gndpos_t = symbol(sym, "gndpos_t"); ws.dtype = symbol(sym, "dtype"); ws.dpage = symbol(sym, "dpage");
		std::unique_ptr<wide_view> view;
		// the wide view knows the Tournament Edition court (W3.2); Hangtime's is W4.2
		bool const te = std::string(g.id) == "nbajamte" || std::string(g.id) == "nbajamse";
		if (te && ws.complete()) view = std::make_unique<wide_view>(*c, ws, cols);
		long play = 0, play_full = 0, play_same_cam = 0, same400 = 0, ref_frames = 0;
		std::vector<u16> ref_work(size_t(512) * 512, 0), ref_done(ref_work.size(), 0);
		int ref_page = 0, ref_done_page = 0;
		u32 const dpage = symbol(sym, "dpage");
		int view_bad = -1;

		long watched = 0, objects = 0;
		if (display && objlst)
			b->watch_pc(display, [&]() {
				if (watched++) { ref_done.swap(ref_work); ref_done_page = ref_page; ref_frames++; }
				std::fill(ref_work.begin(), ref_work.end(), 0);
				ref_page = dpage && b->peek16(dpage) ? 0 : 256;
				u32 o = u32(b->peek16(objlst)) | u32(b->peek16(objlst + 16)) << 16;
				for (int n = 0; o && n < 600; n++) { objects++; o = u32(b->peek16(o)) | u32(b->peek16(o + 16)) << 16; }
			});
		std::vector<u16> narrow(size_t(512) * 512, 0), wide(size_t(1024) * 512, 0);
		long dmas = 0;
		b->observe_dma([&](const u16 *regs) {
			dmas++;
			b->wide_blit(regs, ref_work.data(), 512);
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
			c->set_inputs(inputs_for(f)); c->run_frame();
			u32 const ha = frame_hash(*a);
			if (first_bad < 0 && ha != frame_hash(*b)) first_bad = f;
			if (view_bad < 0 && ha != frame_hash(*c)) view_bad = f;
			if (view && view->is_wide() && view->floor_lines() > 0)
			{
				play++;
				// every column of the view holds something drawn (no empty strip at a side)
				int empty = 0;
				for (int x = 0; x < view->columns(); x++)
				{
					bool any = false;
					for (int y = 0; y < wide_view::HEIGHT && !any; y++) any = view->picture()[size_t(y) * view->pitch() + x] != 0;
					if (!any) empty++;
				}
				if (!empty) play_full++;
				if (view->render_centre() == view->game_centre()) play_same_cam++;
				// at 400 columns, wherever the wide camera is the game's, the view must hold exactly what the
				// game drew in the same display pass (machine b draws the game's own commands of each pass)
				if (cols == 400 && view->render_centre() == view->game_centre() && view->frames() == ref_frames)
				{
					bool same = true;
					for (int y = 0; y < wide_view::HEIGHT && same; y++)
						same = memcmp(&view->picture()[size_t(y) * view->pitch()], &ref_done[size_t(ref_done_page + y) * 512 + 56], 400 * sizeof(u16)) == 0;
					if (same) same400++;
				}
			}
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
				if (view)
				{
					std::vector<u32> v(size_t(view->columns()) * wide_view::HEIGHT);
					for (int y = 0; y < wide_view::HEIGHT; y++)
						for (int x = 0; x < view->columns(); x++)
							v[size_t(y) * view->columns() + x] = pens ? pens[view->picture()[size_t(y) * view->pitch() + x] & 0x7fff] : 0;
					write_png(stem + "_view.png", view->columns(), wide_view::HEIGHT, v);
				}
			}
		}
		bool ok = first_bad < 0 && pix_bad_frames == 0 && dmas > 0 && (!display || watched > 0) && view_bad < 0 && play_full == play && (cols != 400 || same400 == play_same_cam);
		if (view)
			printf("     %s wide view %d columns: hashes %s, %ld frames in play, %ld with no empty column%s\n", g.id, cols,
			       view_bad < 0 ? "equal" : ("differ from frame " + std::to_string(view_bad)).c_str(), play, play_full,
			       cols == 400 ? (", " + std::to_string(same400) + " of " + std::to_string(play_same_cam) + " with the game's camera the same as the game's own picture").c_str() : "");
		printf("%s %s: %d frames, hashes %s, %ld blitter commands drawn again (%s), display routine %s\n",
		       ok ? "ok  " : "FAIL", g.id, frames, first_bad < 0 ? "equal" : ("differ from frame " + std::to_string(first_bad)).c_str(), dmas,
		       pix_bad_frames ? (std::to_string(pix_bad_frames) + " frames with different pixels").c_str() : "same pixels in 512 and 1024 wide",
		       display ? ("watched " + std::to_string(watched) + " times, " + std::to_string(objects) + " objects read").c_str() : "not watched (no symbol file)");
		(ok ? pass : fail)++;
	}
	printf("\n%d passed, %d failed\n", pass, fail);
	return fail ? 1 : 0;
}
