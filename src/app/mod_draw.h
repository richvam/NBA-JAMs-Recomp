// mod_draw.h - draws the Lua mods' overlay (nba.text, nba.rect, nba.line; roadmap 1.6) on the
// frontend overlay canvas, which has the game's own size, so game-screen pixels map 1:1.
#pragma once
#include "mods.h"
#include "ui/canvas.h"
#include <cstdlib>

inline void draw_mod_overlay(canvas &cv, const std::vector<mod_draw> &list)
{
	auto plot = [&](int x, int y, u32 c) {
		if (x < 0 || y < 0 || x >= cv.w || y >= cv.h) return;
		int a = int(c >> 24);
		if (a >= 255) cv.px[size_t(y) * size_t(cv.w) + size_t(x)] = c;
		else if (a > 0) cv.blend(x, y, 1, 1, c | 0xff000000u, a);
	};
	for (const auto &d : list)
	{
		int a = int(d.color >> 24);
		switch (d.type)
		{
			case mod_draw::text: cv.text(d.x, d.y, d.s, d.color | 0xff000000u, d.scale); break;
			case mod_draw::fill:
				if (a >= 255) cv.fill(d.x, d.y, d.x2, d.y2, d.color);
				else cv.blend(d.x, d.y, d.x2, d.y2, d.color | 0xff000000u, a);
				break;
			case mod_draw::rect:
				if (d.x2 <= 0 || d.y2 <= 0) break;
				for (int x = d.x; x < d.x + d.x2; x++) { plot(x, d.y, d.color); plot(x, d.y + d.y2 - 1, d.color); }
				for (int y = d.y + 1; y < d.y + d.y2 - 1; y++) { plot(d.x, y, d.color); plot(d.x + d.x2 - 1, y, d.color); }
				break;
			case mod_draw::line:
			{
				int x = d.x, y = d.y, dx = std::abs(d.x2 - d.x), dy = -std::abs(d.y2 - d.y);
				int sx = d.x < d.x2 ? 1 : -1, sy = d.y < d.y2 ? 1 : -1, err = dx + dy;
				for (int n = 0; n < 4096; n++)
				{
					plot(x, y, d.color);
					if (x == d.x2 && y == d.y2) break;
					int e2 = 2 * err;
					if (e2 >= dy) { err += dy; x += sx; }
					if (e2 <= dx) { err += dx; y += sy; }
				}
				break;
			}
		}
	}
}
