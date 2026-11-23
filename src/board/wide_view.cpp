// wide_view.cpp - the wide gameplay picture (roadmap W3.2); see wide_view.h.
// Object fields, flags, the projection and the floor drawing follow the Tournament Edition
// sources: DISP.EQU (STRUCT OBJ, OFLAGS bits) and NDSP1.ASM (DISPLAY, dma_objlst, gnd_dodma).
#include "wide_view.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <cstdlib>

namespace {
// object fields (bit offsets, DISP.EQU)
constexpr u32 OXVAL = 0x80, OXPOS = 0x90, OYPOS = 0xb0, OZPOS = 0xd0, OFLAGS = 0xe0, OCTRL = 0xf0, OFSET = 0x100,
              OSAG = 0x110, OSIZEX = 0x130, OSIZEY = 0x140, OPAL = 0x150, OCONST = 0x160, ODATA_P = 0x1c0,
              OXANI = 0x1e0, OMISC = 0x210;
// OFLAGS and OCTRL bits
constexpr u16 M_CHARGEN = 0x4000;   // Hangtime: a text object drawn its own way
constexpr u16 M_SHAD = 0x200, M_NOSCALE = 0x1000;
constexpr u16 M_FLIPH = 0x10, M_FLIPV = 0x20, M_3DQ = 0x40, M_ZEROCOMP = 0x80, M_3D = 0x100, M_SCRNREL = 0x2000;
// the court floor (NDSP1.ASM gnd_dodma): the court picture is gndi_w x gndi_h pixels, a line is
// drawn 400 wide from gndi_w/2-200 plus gndpos_t[40 + line], line 0 at screen y gndi_y - WORLDTLY
constexpr int FLOOR_FIRST = 40;
// blitter registers (midway_video.h)
enum { R_LRSKIP, R_COMMAND, R_OFFSETLO, R_OFFSETHI, R_XSTART, R_YSTART, R_WIDTH, R_HEIGHT, R_PALETTE, R_COLOR,
       R_SCALE_X, R_SCALE_Y, R_TOPCLIP, R_BOTCLIP, R_UNKNOWN_E, R_CONFIG, R_LEFTCLIP, R_RIGHTCLIP };
int bits_per_pixel(u16 ctrl) { int b = (ctrl >> 12) & 7; return b ? b : 8; }
}

// T.E. (and NBA JAM 1993, whose display code its symbol file matches to T.E.'s by pattern;
// its court is the same 1277 x 138, at 6 bits a pixel): NDSP1.ASM GNDI_Y 116, PLYR.EQU GZBASE 894, the art and camera limits measured in W2.3
// (backdrop mid-court -512 to +675, camera to +-310 so the view to +-510).
// Hangtime and Maximum Hangtime: WORLD.EQU (GNDI_W 1762, GNDI_H 144, GNDI_Y 254-144,
// GND_H GNDI_H+40, GZBASE CZMIN-40*8/3, MAX_VIEW1 375 so the view to +-575).
const wide_game *wide_game_for(const char *id)
{
	static const wide_game te{1277, 138, 116, 178, 894, false, -512, 675, 510};
	static const wide_game ht{1762, 144, 110, 184, 894, true, -575, 575, 575};
	std::string const g = id ? id : "";
	if (g == "nbajamte" || g == "nbajamse" || g == "nbajam") return &te;
	if (g == "nbahangt" || g == "nbamht") return &ht;
	return nullptr;
}

wide_view::wide_view(arcade_machine &mach, const wide_symbols &sym, int columns, const wide_game &game)
	: m(mach), s(sym), g(game), m_cols(std::clamp(columns & ~1, 400, 1000)), m_left(((PITCH - m_cols) / 2) & ~1),
	  m_work(size_t(PITCH) * ROWS, 0), m_done(size_t(PITCH) * ROWS, 0)
{
	m.watch_pc(s.display, [this]() { frame_start(); });
	m.observe_dma([this](const u16 *regs) { on_dma(regs); });
}

wide_view::~wide_view()
{
	m.watch_pc(0, nullptr);
	m.observe_dma(nullptr);
}

// gnd_dodma: gndpos_t[i] = (gndx + i * step) >> 16, gndx 16:16. T.E.: step = gndx / 0x120.
// Hangtime: step = (gndx << DIVRES) / DIVSLP, DIVRES 6, DIVSLP = 64*1594*144/612 - 64*184 = 12227.
int wide_view::gndpos(s32 gndx, int line) const
{
	s32 const step = g.hangtime ? s32(u32(gndx) << 6) / 12227 : gndx / 0x120;
	return int(s32(gndx + s32(line) * step) >> 16);
}

void wide_view::frame_start()
{
	auto r16 = [&](u32 a) { return m.peek16(a); };
	auto r32 = [&](u32 a) { return u32(r16(a)) | u32(r16(a + 16)) << 16; };

	// the previous frame is finished
	if (m_started)
	{
		if (!m_floor_done) draw_flat();
		draw_skipped(m_objs.size());
		m_done.swap(m_work);
		m_done_page = m_page;
		m_done_wide = m_wide;
		m_steady = m_wide ? m_steady + 1 : 0;
		m_done_rc = m_rc;
		m_done_gc = m_gc;
		m_done_fl = m_floor_line;
		m_done_ff = m_floor_full;
		m_done_own = m_own;
		m_frames++;
	}
	m_started = true;
	std::fill(m_work.begin(), m_work.end(), 0);

	// DISPLAY draws page 1 (256 rows down) when dpage is 0
	m_page = r16(s.dpage) ? 0 : 256;
	m_wide = s16(r16(s.dtype)) > 0;
	m_gndx = s32(r32(s.gndx));
	m_gc = m_gndx >> 16;
	int const half = m_cols / 2;
	int const lo = std::max(g.art_left, -g.game_edge) + half, hi = std::min(g.art_right, g.game_edge) - half;
	m_rc = !m_wide ? m_gc : lo <= hi ? std::clamp(m_gc, lo, hi) : (lo + hi) / 2;
	// board column 56 is the game's screen x 0. m_base moves the game's 400 columns to the middle
	// of the view; a world object also moves by the difference between the two cameras.
	m_base = m_left - LEFT + half - 200;
	int const d = m_rc - m_gc;
	m_off = m_base - d;
	m_rgndx = m_gndx + s32(u32(d) << 16);
	m_floor_line = 0;
	m_floor_full = 0;
	m_own = 0;
	m_floor_done = false;
	m_objs.clear();
	m_next = 0;
	if (!m_wide) return;

	int const tlx = s16(r16(s.worldtlx + 16)), tly = s16(r16(s.worldtly + 16));
	m_tly = tly;
	// the rows the game draws flat objects into (SCRNTL / SCRNLR, Y in the high word): Hangtime
	// lowers the top at times, and leaves out what is above it
	m_scr_top = s.scrntl ? std::max(0, int(s16(r16(s.scrntl + 16)))) : 0;
	m_scr_bot = s.scrnlr ? std::min(HEIGHT - 1, int(s16(r16(s.scrnlr + 16))) - 1) : HEIGHT - 1;
	// the 3D projection (dma_objlst #3d), with C = (WORLDTLX + 200) << 16 the camera centre.
	// T.E.: x = ((X - C + ani) >> 11) * (z + 819 - GZBASE) * 5/8 >> 14, + 200 - ani.
	// Hangtime (PRJX2Z): x = ((X - C + ani) >> 5) * (z + 503 - GZBASE) * 65/128 >> 19, + 200 - ani.
	auto proj = [&](u32 o, s32 c) {
		s32 a7 = s32(r32(o + OXVAL)) - c;
		s32 const ani = s32(r32(o + OXANI));
		s32 const z = s16(r16(o + OZPOS)) + s16(r16(o + OMISC));
		if (g.hangtime)
		{
			a7 = (a7 + ani) >> 5;
			a7 *= (z + 503 - g.gzbase);
			a7 = (a7 >> 1) + (a7 >> 7);
			return (a7 >> 19) + 200 - (ani >> 16);
		}
		a7 = (a7 + ani) >> 11;
		a7 *= (z + 819 - g.gzbase);
		s32 a3 = a7 >> 2;
		a7 -= a3;
		a3 >>= 1;
		a7 -= a3;
		return (a7 >> 14) + 200 - (ani >> 16);
	};
	s32 const cam = s32(u32(tlx + 200) << 16);

	u32 o = r32(s.objlst);
	for (int n = 0; o && n < 600; n++, o = r32(o))
	{
		u16 const flags = r16(o + OFLAGS);
		u16 const ctrl = r16(o + OCTRL);
		int const w = r16(o + OSIZEX);
		int h = r16(o + OSIZEY);

		// where the game's own command for this object goes: its keys, and how far it moves
		obj_shift k{ctrl, r16(o + OPAL), r16(o + OCONST), m_off};
		k.w = u16(w);
		k.img = r32(o + OSAG);
		k.img_end = k.img + u32(w) * u32(h) * u32(bits_per_pixel(ctrl));
		int q = -1;                    // quick perspective: the gndpos_t entry
		if (flags & M_SCRNREL)
			k.shift = hud_shift(s16(r16(o + OXPOS)) + (ctrl & M_FLIPH ? 0 : w / 2), s16(r16(o + OYPOS)));   // names, turbo bars, messages
		else if (flags & M_3DQ)
		{
			q = int((s32(r32(o + ODATA_P)) - s32(s.gndpos_t)) / 16);
			if (q >= 0 && q < g.gnd_h) k.shift = m_base - (gndpos(m_rgndx, q) - gndpos(m_gndx, q));
		}
		else if (flags & M_3D)
		{
			k.shift = m_base + proj(o, cam + s32(u32(d) << 16)) - proj(o, cam);
			// the whole object for the wide camera (dma_objlst #3d): screen x from the projection,
			// y = OYPOS - WORLDTLY + Z/2 - Z/8 + GND_Y with Z from GZBASE (not below 0), the scale
			// from the object's scale table by Z/16; shadows and unscaled objects at 1:1
			s32 const zz = s16(r16(o + OZPOS)) + s16(r16(o + OMISC));
			s32 const zr = std::max<s32>(0, zz - g.gzbase);
			int x = proj(o, cam + s32(u32(d) << 16));
			int const y = s16(r16(o + OYPOS)) - tly + (zr >> 1) - (zr >> 3) + (g.gndi_y - 40);
			bool const scaled = !(flags & (M_NOSCALE | M_SHAD));
			// shadows shimmer: x's low bit is PCNT's (dma_objlst: srl 1 / srl 1,PCNT / addc)
			if (flags & M_SHAD) x = (x & ~1) | (s.pcnt ? r16(s.pcnt) & 1 : 0);
			u16 sx = 0x100, sy = 0x100;
			if (scaled)
			{
				u32 const t = r32(o + ODATA_P) + u32(zr >> 4) * 64;
				sx = r16(t);
				sy = r16(t + 16);
			}
			int yy = y;
			if (!scaled && (ctrl & M_FLIPV)) yy += h - 1;
			if (ctrl & M_FLIPH) x += w - 1;
			k.sag = r32(o + OSAG);
			k.h = u16(h);
			k.own = (ctrl & 0x8000) && w > 0 && h > 0 && w <= 1023 && h <= 511 && sx && sy;   // without the go bit the blitter draws nothing
			k.exact = d == 0;
			k.regs[R_LRSKIP] = r16(o + OFSET);
			k.regs[R_COMMAND] = ctrl;
			k.regs[R_OFFSETLO] = u16(k.sag);
			k.regs[R_OFFSETHI] = u16(k.sag >> 16);
			k.regs[R_XSTART] = u16(x + LEFT + m_base);
			k.regs[R_YSTART] = u16(yy + m_page);
			k.regs[R_WIDTH] = u16(w);
			k.regs[R_HEIGHT] = u16(h);
			k.regs[R_PALETTE] = k.pal;
			k.regs[R_COLOR] = k.cons;
			k.regs[R_SCALE_X] = sx;
			k.regs[R_SCALE_Y] = sy;
			// wholly outside the view (the game's own test, with the unscaled size)
			int const vx = x + LEFT + m_base - (ctrl & M_FLIPH ? w - 1 : 0);
			if (vx >= m_left + m_cols || vx + w <= m_left) k.own = false;
		}
		// where the game's own (uncut) command for it starts: told apart from a mirrored twin with
		// the same image (the crowd on both sides) by its place
		{
			int gx = 0, gy = s16(r16(o + OYPOS));
			k.known = true;
			if (flags & M_SCRNREL) gx = s16(r16(o + OXPOS));
			else if (flags & M_3D)
			{
				s32 const zr = std::max<s32>(0, s16(r16(o + OZPOS)) + s16(r16(o + OMISC)) - g.gzbase);
				gx = proj(o, cam);
				gy += -tly + (zr >> 1) - (zr >> 3) + (g.gndi_y - 40);
				if ((flags & (M_NOSCALE | M_SHAD)) && (ctrl & M_FLIPV)) gy += h - 1;
			}
			else if (flags & M_3DQ)
			{
				if (q >= 0 && q < g.gnd_h) gx = s16(r16(o + OXPOS)) - gndpos(m_gndx, q); else k.known = false;
				gy -= tly;
			}
			else { gx = s16(r16(o + OXPOS)) - tlx; gy -= tly; }
			if (!(flags & M_3D) && (ctrl & M_FLIPV)) gy += h - 1;
			if (ctrl & M_FLIPH) gx += w - 1;
			k.gx = gx + LEFT;
			k.gy = gy + m_page;
		}
		m_objs.push_back(k);

		// flat objects (stands, banners, the courtside crowd and tables): drawn whole by the view
		// at their place in the list order, so the parts beyond the game's 400 columns appear at
		// the sides and the game's own copy (cut at its edge, or for its own camera) is not used
		if (flags & (M_3D | M_SCRNREL | M_CHARGEN)) continue;
		if (w <= 0 || h <= 0 || w > 1023 || h > 511) continue;   // the view cannot draw it: the game's copy is used
		int x = s16(r16(o + OXPOS)), y = s16(r16(o + OYPOS)) - tly;
		if (flags & M_3DQ)
		{
			// x = OXPOS - gndpos_t[q], the floor line the object stands on
			if (q < 0 || q >= g.gnd_h) continue;   // the view cannot draw it: the game's copy is used
			x += LEFT + m_base - gndpos(m_rgndx, q);
		}
		else
			x += LEFT + m_off - tlx;
		u32 sag = r32(o + OSAG);
		if (!(ctrl & M_ZEROCOMP))
		{
			// cut off the rows outside the game's screen box, as the game does (dma_objlst: the
			// rows above start the image further in; with a vertical flip they are at its end)
			int const top = std::max(0, m_scr_top - y), bot = std::max(0, y + h - 1 - m_scr_bot);
			if (top + bot >= h) { m_objs.back().skip = true; continue; }
			int const cut_start = ctrl & M_FLIPV ? bot : top;
			sag += u32(cut_start) * u32(w) * u32(bits_per_pixel(ctrl));
			h -= top + bot;
			y += top;
		}
		obj_shift &f = m_objs.back();
		f.sag = sag;
		f.h = u16(h);
		f.exact = d == 0;
		// wholly outside the view: nothing to draw, and the game's copy is not used either
		f.own = ctrl & 0x8000;   // an object without the go bit is hidden: the blitter draws nothing
		if (x >= m_left + m_cols || x + w <= m_left) { f.own = false; f.skip = true; continue; }
		if (ctrl & M_FLIPH) x += w - 1;
		if (ctrl & M_FLIPV) y += h - 1;
		f.regs[R_LRSKIP] = r16(o + OFSET);
		f.regs[R_COMMAND] = ctrl;
		f.regs[R_OFFSETLO] = u16(sag);
		f.regs[R_OFFSETHI] = u16(sag >> 16);
		f.regs[R_XSTART] = u16(x);
		f.regs[R_YSTART] = u16(y + m_page);
		f.regs[R_WIDTH] = u16(w);
		f.regs[R_HEIGHT] = u16(h);
		f.regs[R_PALETTE] = k.pal;
		f.regs[R_COLOR] = k.cons;
		f.regs[R_SCALE_X] = f.regs[R_SCALE_Y] = 0x100;
	}
}

// into the wide picture: the view's columns, the rows of the page being drawn. Anything the
// blitter wraps past column 1023 or row 511 lands outside these and is not drawn.
void wide_view::blit(u16 *regs)
{
	// Cut the command at the view's left and right edges as the game cuts at its screen edge
	// (dma_objlst: start the source further in and skip the same count at the end of each row),
	// so nothing reaches column 1023 and wraps round to the other side. A zero-compressed image
	// (each line packed) cannot be cut this way, as in the game; the clip window still holds it
	// unless it is wider than the space beside the view.
	int const cmd = regs[R_COMMAND];
	if (!(cmd & 0xc0))
	{
		int const width = regs[R_WIDTH] & 0x3ff;
		int skip = regs[R_LRSKIP];
		int const step = regs[R_SCALE_X] ? regs[R_SCALE_X] : 0x100;
		int x = s16(regs[R_XSTART]);
		int const dw = (width - skip) * 0x100 / step;  // drawn width
		if (dw <= 0) return;
		// a scaled image is cut only where it would wrap: cut at the view's edges its pixels could
		// land one column off the blitter's own clipping
		int const lo = step == 0x100 ? m_left : 0, hi = step == 0x100 ? m_left + m_cols - 1 : 0x3ff;
		bool const flip = cmd & M_FLIPH;
		int const first = flip ? x - dw + 1 : x, last = flip ? x : x + dw - 1;
		if (last < lo || first > hi) return;
		int const cut_lo = std::max(0, lo - first), cut_hi = std::max(0, last - hi);
		int const start_cut = flip ? cut_hi : cut_lo;   // dest pixels left out at the start of a row
		int const end_cut = flip ? cut_lo : cut_hi;     // and at its end
		int const s0 = (start_cut * step + 0xff) >> 8, s1 = (end_cut * step + 0xff) >> 8;
		if (s0 || s1)
		{
			if (skip + s0 + s1 >= width) return;
			u32 const sag = (u32(regs[R_OFFSETLO]) | u32(regs[R_OFFSETHI]) << 16) + u32(s0) * u32(bits_per_pixel(u16(cmd)));
			regs[R_OFFSETLO] = u16(sag);
			regs[R_OFFSETHI] = u16(sag >> 16);
			regs[R_LRSKIP] = u16(skip + s0 + s1);
			x += flip ? -(s0 * 0x100 / step) : s0 * 0x100 / step;
			regs[R_XSTART] = u16(x);
		}
	}
	regs[R_XSTART] &= 0x3ff;
	regs[R_TOPCLIP] = u16(m_page);
	regs[R_BOTCLIP] = u16(m_page + HEIGHT - 1);
	regs[R_LEFTCLIP] = u16(m_left);
	regs[R_RIGHTCLIP] = u16(m_left + m_cols - 1);
	m.wide_blit(regs, m_work.data(), PITCH);
}

void wide_view::draw_flat()
{
	m_floor_done = true;
}

// The game issues one command per object it draws, in list order, after the floor; objects it
// leaves out (off its screen) issue none. The command's control word, palette and constant colour
// are the object's, so the next object with the same three is the one being drawn. Commands that
// match no object (the game's extra queue: messages, arrows) stay on the game's 400 columns.
// A 3D object the game draws whole, with the same camera, keeps the game's own command (so the
// view is exactly the game's picture); one it cut at its screen edge, or any 3D object when the
// wide camera differs, is drawn whole by the wide view; ones it left out are drawn by draw_skipped
// at their place in the list order.
// Is the game's command at the object's own place? Where the game cut the image at its screen
// box, the command starts at that edge instead: left (x -32), right for a flipped image (x 431),
// top, or bottom for a vertically flipped one.
bool wide_view::place_matches(const obj_shift &k, const u16 *regs) const
{
	int const x = regs[R_XSTART] & 0x3ff, y = regs[R_YSTART] & 0x1ff;
	bool const fh = regs[R_COMMAND] & M_FLIPH, fv = regs[R_COMMAND] & M_FLIPV;
	bool const xok = std::abs(x - (k.gx & 0x3ff)) <= 1 || x == (fh ? LEFT + 431 : LEFT - 32);
	bool const yok = y == (k.gy & 0x1ff) || y == (fv ? m_page + m_scr_bot : m_page + m_scr_top);
	return xok && yok;
}

bool wide_view::draw_object(u16 *regs)
{
	for (size_t i = m_next; i < m_objs.size(); i++)
	{
		obj_shift &k = m_objs[i];
		u32 const at = u32(regs[R_OFFSETLO]) | u32(regs[R_OFFSETHI]) << 16;
		if ((k.ctrl | 0x8000) == regs[R_COMMAND] && k.pal == regs[R_PALETTE] && k.cons == regs[R_COLOR] &&
		    (regs[R_WIDTH] & 0x3ff) == k.w && at >= k.img && at < k.img_end &&
		    (!k.known || place_matches(k, regs)))
		{
			draw_skipped(i);
			m_next = i + 1;
			if (k.skip) return true;   // nothing of it is in the view
			bool const whole = at == k.sag && (regs[R_HEIGHT] & 0x3ff) == k.h;
			if (k.own && !(whole && k.exact))
			{
				m_own += !whole;
				blit(k.regs);
				return true;
			}
			regs[R_XSTART] = u16(regs[R_XSTART] + k.shift);
			blit(regs);
			return true;
		}
	}
	// the game's extra queue (messages, arrows): placed like the on-screen text
	int const x = int(regs[R_XSTART] & 0x3ff) - LEFT, w = regs[R_WIDTH] & 0x3ff;
	regs[R_XSTART] = u16(regs[R_XSTART] + hud_shift(regs[R_COMMAND] & M_FLIPH ? x - w / 2 : x + w / 2, int(regs[R_YSTART] & 0x1ff) - m_page));
	blit(regs);
	return false;
}

void wide_view::draw_skipped(size_t upto)
{
	for (size_t i = m_next; i < upto && i < m_objs.size(); i++)
		if (m_objs[i].own)
		{
			m_own++;
			blit(m_objs[i].regs);
		}
	m_next = std::max(m_next, std::min(upto, m_objs.size()));
}

// On-screen text and bars (W3.4). The game lays them out for four players across its 400
// columns: each player's name, turbo bar and messages sit in their own 100-column strip. Each
// strip moves as a whole so that its centre keeps its place across the wide view: player 1 and 4
// go towards the edges, 2 and 3 a little, and what is centred stays centred. Pieces of one
// player's display keep their spacing. Only the top rows (names and turbo bars, above row
// HUD_TOP) spread: lower down the game draws banners and messages from many pieces across the
// whole 400 columns (the end-of-quarter banner), which must stay together in the middle.
int wide_view::hud_shift(int centre, int top) const
{
	if (top >= HUD_TOP) return m_base;
	// the middle (150 to 249: the free-play sign, shot clock, centred messages) does not move
	int const x = std::clamp(centre, 0, 399);
	int const c = x < 100 ? 50 : x < 150 ? 150 : x < 250 ? 200 : x < 300 ? 250 : 350;
	return m_base + (c - 200) * (m_cols - 400) / 400;
}

void wide_view::on_dma(const u16 *in)
{
	if (!m_started) return;
	u16 regs[18];
	memcpy(regs, in, sizeof regs);
	if (m_wide && !m_floor_done && m_floor_line < g.gndi_h && (regs[R_WIDTH] & 0x3ff) == 400 && (regs[R_HEIGHT] & 0x3ff) == 1 &&
	    (regs[R_SCALE_X] == 0 || regs[R_SCALE_X] == 0x100))
	{
		// a floor line (they come first): the same line of COURT, as wide as the view
		// which court line: from its screen row (Hangtime leaves out the lines above the screen)
		m_floor_line++;
		int const line = FLOOR_FIRST + std::clamp(int(regs[R_YSTART] & 0x1ff) - m_page - (g.gndi_y - m_tly), 0, g.gndi_h - 1);
		int const bpp = bits_per_pixel(regs[R_COMMAND]);
		u32 const sag = u32(regs[R_OFFSETLO]) | u32(regs[R_OFFSETHI]) << 16;
		int const gx = g.gndi_w / 2 - 200 + gndpos(m_gndx, line);       // the game's first pixel
		int const sx = g.gndi_w / 2 - m_cols / 2 + gndpos(m_rgndx, line);   // the wide view's
		u32 const start = sag - u32(gx) * u32(bpp);                     // the line's pixel 0
		// like the game's own lines at the court's ends, a line may read a little past its
		// picture line into the next; the wide camera never goes further out than the game's
		if (sx >= 0 && sx + m_cols <= g.gndi_w) m_floor_full++;
		u32 const nsag = start + u32(s32(sx) * bpp);
		regs[R_OFFSETLO] = u16(nsag);
		regs[R_OFFSETHI] = u16(nsag >> 16);
		regs[R_XSTART] = u16(m_left);
		regs[R_WIDTH] = u16(m_cols);
		blit(regs);
		return;
	}
	if (!m_wide)
	{
		// fixed screen: the game's 400 columns, centred (W3.3 decides what the sides show)
		regs[R_XSTART] = u16(regs[R_XSTART] + m_base);
		blit(regs);
		return;
	}
	if (!m_floor_done) draw_flat();
	// everything else as the game drew it, moved to its place in the wide view (or drawn whole)
	draw_object(regs);
}
