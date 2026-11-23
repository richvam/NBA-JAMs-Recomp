// wide_view.h - the wide gameplay picture (roadmap W3.2, the W2.6 choice).
//
// The game keeps drawing its own 400 columns; nothing in the game is changed. This class reads
// the game's state when its display routine starts and watches every blitter command it issues,
// then draws a second, wider picture of the same frame with the board's own blitter code:
//   * the court floor: each floor line the game draws 400 pixels wide is drawn again `columns`
//     wide from the same court picture in the ROM, with the floor's perspective worked out for
//     the wide view's camera;
//   * world objects drawn flat (the stands and banners): drawn whole from the object list, so
//     the parts the game cut off at its screen edges appear at the sides;
//   * every other command (players, ball, score) is drawn as the game drew it, moved so that it
//     lands where it belongs in the wide view.
// The wide view's camera follows the game's camera but stops where the arena art ends
// (docs/WIDESCREEN.md, W2.3); the game's own camera is never touched.
// Only frames in play (dtype > 0) are wide; on fixed screens is_wide() is false.
#pragma once
#include "machine.h"
#include <vector>

struct wide_symbols
{
	u32 display = 0, objlst = 0, worldtlx = 0, worldtly = 0, gndx = 0, gndpos_t = 0, dtype = 0, dpage = 0;
	bool complete() const { return display && objlst && worldtlx && worldtly && gndx && gndpos_t && dtype && dpage; }
};

class wide_view
{
public:
	static constexpr int PITCH = 1024, ROWS = 512, LEFT = 56 /* the board's pad */, HEIGHT = 254;
	// the art covers mid-court -512 .. +675 (W2.3)
	static constexpr int ART_LEFT = -512, ART_RIGHT = 675;
	// and the game itself never shows more than 510 from mid-court (its camera stops at ±310,
	// MAX_VIEW1): further out the floor lines would read past the court picture
	static constexpr int GAME_EDGE = 510;

	// columns: the wide view's width, 400 to 1000 (an even number)
	wide_view(arcade_machine &m, const wide_symbols &s, int columns);

	int columns() const { return m_cols; }
	// the last finished frame: 16-bit pixels (palette and colour, as the board's video RAM),
	// columns() wide and HEIGHT high, `pitch()` pixels a row.
	const u16 *picture() const { return &m_done[size_t(m_done_page) * PITCH + m_left]; }
	int pitch() const { return PITCH; }
	bool is_wide() const { return m_done_wide; }
	// the wide camera of the last frame, from mid-court, and the game's own
	int render_centre() const { return m_done_rc; }
	int game_centre() const { return m_done_gc; }
	long frames() const { return m_frames; }
	// floor lines in the last frame, and how many of them reach both edges of the wide view
	int floor_lines() const { return m_done_fl; }
	int floor_full() const { return m_done_ff; }

private:
	struct flat_obj { u16 regs[18]; };
	struct obj_shift { u16 ctrl, pal, cons; int shift; };   // per object on the list, in order
	void frame_start();
	void on_dma(const u16 *regs);
	void draw_flat();
	void blit(u16 *regs);
	int shift_for(const u16 *regs);
	static int gndpos(s32 gndx, int line);

	arcade_machine &m;
	wide_symbols s;
	int m_cols, m_left;            // the view is columns m_left .. m_left + m_cols - 1
	std::vector<u16> m_work, m_done;
	int m_page = 0, m_done_page = 0;
	bool m_wide = false, m_done_wide = false, m_started = false;
	int m_off = 0;                 // a flat world object: wide column = board column + m_off
	s32 m_gndx = 0, m_rgndx = 0;   // the game's and the wide view's floor offset, 16:16
	int m_floor_line = 0;          // floor lines seen this frame
	int m_floor_full = 0, m_done_fl = 0, m_done_ff = 0;
	bool m_floor_done = false;
	int m_rc = 0, m_gc = 0, m_done_rc = 0, m_done_gc = 0;
	std::vector<flat_obj> m_flat;
	std::vector<obj_shift> m_objs;
	size_t m_next = 0;
	int m_base = 0;               // the game's 400 columns in the middle of the view
	long m_frames = 0;
};
