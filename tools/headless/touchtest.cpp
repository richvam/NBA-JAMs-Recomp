// touchtest - the on-screen touch controls (roadmap R7.3) without a screen: fingers in,
// cabinet buttons out, arranging, the saved layout, and the drawn picture.
// usage: touchtest [out_dir]   (writes touch.png there when given)
#include "touch_pad.h"
#include <cmath>
#include <cstdio>
#include <string>

static int g_run = 0, g_fail = 0;
#define CHECK(c, what) do { g_run++; if (c) printf("ok   %s\n", what); else { g_fail++; printf("FAIL %s\n", what); } } while (0)

static const int W = 2340, H = 1080;   // a 19.5:9 phone in landscape (an S24+ is 3120 x 1440)

static cabinet_inputs::player read(const touch_pad &t) { cabinet_inputs::player p{}; t.apply(p); return p; }

// a point a given number of pixels from a part's centre
static void at(const touch_pad &t, touch_part part, float dx_px, float dy_px, float &x, float &y)
{
	x = t.layout.x[part] + dx_px / W;
	y = t.layout.y[part] + dy_px / H;
}

int main(int argc, char **argv)
{
	float x, y;
	{
		touch_pad t;
		cabinet_inputs::player p = read(t);
		CHECK(!p.up && !p.down && !p.left && !p.right && !p.shoot && !p.pass && !p.turbo && !p.start && !p.coin, "no fingers: nothing is held");
		at(t, tp_shoot, 0, 0, x, y); t.down(1, x, y, W, H);
		CHECK(read(t).shoot && !read(t).pass, "a finger on Shoot holds Shoot");
		at(t, tp_pass, 0, 0, x, y); t.move(1, x, y, W, H);
		CHECK(read(t).pass && !read(t).shoot, "sliding from Shoot to Pass lets go of Shoot and holds Pass");
		t.up(1);
		CHECK(!read(t).pass, "lifting the finger lets go");
		at(t, tp_turbo, 0, 0, x, y); t.down(2, x, y, W, H);
		at(t, tp_shoot, 0, 0, x, y); t.down(3, x, y, W, H);
		CHECK(read(t).turbo && read(t).shoot, "two fingers: Turbo and Shoot together");
		t.clear();
		at(t, tp_start, 0, 0, x, y); t.down(4, x, y, W, H);
		at(t, tp_coin, 0, 0, x, y); t.down(5, x, y, W, H);
		CHECK(read(t).start && read(t).coin, "Start and Coin");
	}
	{
		touch_pad t;
		float r = t.radius(tp_stick, W, H);
		struct dir { float dx, dy; bool u, d, l, r; const char *name; };
		const dir dirs[] = {
			{ 1, 0, 0, 0, 0, 1, "right" }, { -1, 0, 0, 0, 1, 0, "left" }, { 0, -1, 1, 0, 0, 0, "up" }, { 0, 1, 0, 1, 0, 0, "down" },
			{ 0.7f, -0.7f, 1, 0, 0, 1, "up-right" }, { -0.7f, 0.7f, 0, 1, 1, 0, "down-left" },
		};
		bool all = true;
		for (const dir &d : dirs)
		{
			t.clear();
			at(t, tp_stick, 0, 0, x, y); t.down(9, x, y, W, H);
			at(t, tp_stick, d.dx * r * 0.8f, d.dy * r * 0.8f, x, y); t.move(9, x, y, W, H);
			auto p = read(t);
			bool ok = p.up == d.u && p.down == d.d && p.left == d.l && p.right == d.r;
			if (!ok) printf("  %s: u%d d%d l%d r%d\n", d.name, p.up, p.down, p.left, p.right);
			all = all && ok;
		}
		CHECK(all, "stick: the four directions and the diagonals, measured in pixels on a wide screen");
		t.clear();
		at(t, tp_stick, 0, 0, x, y); t.down(9, x, y, W, H);
		at(t, tp_stick, r * 0.2f, 0, x, y); t.move(9, x, y, W, H);
		auto p = read(t);
		CHECK(!p.up && !p.down && !p.left && !p.right, "stick: a small move stays in the dead zone");
		at(t, tp_stick, r * 1.6f, 0, x, y); t.clear(); t.down(9, x, y, W, H);
		at(t, tp_stick, r * 2.5f, 0, x, y); t.move(9, x, y, W, H);
		CHECK(read(t).right, "stick: a thumb that lands just outside it still takes it, and keeps it when it slides away");
		t.clear();
		x = 0.5f; y = 0.5f; t.down(10, x, y, W, H);
		CHECK(!t.held(tp_stick) && !read(t).shoot, "the middle of the screen is not a control");
	}
	{
		touch_pad t;
		t.arranging = true;
		float ox = t.layout.x[tp_pass], oy = t.layout.y[tp_pass];
		at(t, tp_pass, 10, 5, x, y); t.down(1, x, y, W, H);
		t.move(1, x - 0.2f, y - 0.1f, W, H);
		t.up(1);
		CHECK(std::fabs(t.layout.x[tp_pass] - (ox - 0.2f)) < 0.002f && std::fabs(t.layout.y[tp_pass] - (oy - 0.1f)) < 0.002f, "arranging: a part is dragged where the finger goes, from where it was grabbed");
		at(t, tp_shoot, 0, 0, x, y); t.down(2, x, y, W, H);
		CHECK(!read(t).shoot, "arranging: nothing is pressed");
		t.move(2, 1.5f, -0.5f, W, H);
		CHECK(t.layout.x[tp_shoot] <= 0.98f && t.layout.y[tp_shoot] >= 0.02f, "arranging: a part stays on the screen");
	}
	{
		touch_pad t;
		t.layout.x[tp_turbo] = 0.333f; t.layout.size = 1.3f; t.layout.opacity = 0.7f;
		std::string s = t.layout.save();
		touch_layout l;
		CHECK(l.load(s) && std::fabs(l.x[tp_turbo] - 0.333f) < 0.001f && std::fabs(l.size - 1.3f) < 0.01f && std::fabs(l.opacity - 0.7f) < 0.01f, "layout: saved and read back");
		touch_layout d = touch_layout::defaults();
		touch_layout bad = d;
		CHECK(!bad.load("1,2;nonsense") && !bad.load(s + ";extra") && bad.x[tp_turbo] == d.x[tp_turbo], "layout: a damaged line is refused and changes nothing");
		std::string big = s.substr(0, s.rfind(';', s.rfind(';') - 1)) + ";9.00;0.50";
		CHECK(!bad.load(big), "layout: a size out of range is refused");
	}
	{
		touch_pad t;
		int w = W / 3, h = H / 3;
		std::vector<uint8_t> px;
		t.render(px, w, h);
		auto alpha = [&](float fx, float fy) { return px[(size_t(int(fy * h)) * w + size_t(int(fx * w))) * 4 + 3]; };
		bool parts = true;
		for (int i = 0; i < tp_count; i++) parts = parts && alpha(t.layout.x[i], t.layout.y[i]) > 0;
		CHECK(parts, "picture: every part is drawn where it is");
		CHECK(alpha(0.5f, 0.5f) == 0 && alpha(0.5f, 0.1f) == 0, "picture: the middle (the game) stays clear");
		int a1 = alpha(t.layout.x[tp_pass], t.layout.y[tp_pass]);
		at(t, tp_pass, 0, 0, x, y); t.down(1, x, y, W, H);
		t.render(px, w, h);
		CHECK(alpha(t.layout.x[tp_pass], t.layout.y[tp_pass]) > a1, "picture: a held button is drawn brighter");
		if (argc > 1)
		{
			// a PPM over a grey checker, to look at
			std::string f = std::string(argv[1]) + "/touch.ppm";
			if (FILE *o = fopen(f.c_str(), "wb"))
			{
				fprintf(o, "P6 %d %d 255\n", w, h);
				for (int yy = 0; yy < h; yy++)
					for (int xx = 0; xx < w; xx++)
					{
						const uint8_t *p = &px[(size_t(yy) * w + xx) * 4];
						int bg = ((xx / 16 + yy / 16) & 1) ? 70 : 40;
						for (int c = 0; c < 3; c++) fputc(int(p[c] * p[3] / 255 + bg * (255 - p[3]) / 255), o);
					}
				fclose(o);
				printf("  wrote %s\n", f.c_str());
			}
		}
	}
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
