// touch_pad.h - on-screen game controls for touch screens (roadmap R7.3): a stick, Shoot,
// Pass and Turbo, plus Start and Coin, for player 1. Everything is in window fractions, so
// the layout fits any phone; nothing here knows SDL, so tools/headless/touchtest drives it.
#pragma once
#include "machine.h"
#include <cstdint>
#include <string>
#include <vector>

enum touch_part { tp_stick, tp_shoot, tp_pass, tp_turbo, tp_start, tp_coin, tp_count };

struct touch_layout
{
	// centre of each part, as fractions of the window's width and height
	float x[tp_count], y[tp_count];
	float size = 1.0f;        // 0.6 .. 1.6, all parts together
	float opacity = 0.5f;     // 0.2 .. 1.0
	static touch_layout defaults();
	// "x,y;x,y;...;size;opacity" for the configuration file; load keeps the defaults for
	// anything missing or out of range
	std::string save() const;
	bool load(const std::string &s);
	static constexpr float kMinSize = 0.6f, kMaxSize = 1.6f, kMinOpacity = 0.2f, kMaxOpacity = 1.0f;
};

class touch_pad
{
public:
	touch_layout layout = touch_layout::defaults();

	// fingers, with SDL's finger id and position as window fractions (0..1)
	void down(long long id, float x, float y, int win_w, int win_h);
	void move(long long id, float x, float y, int win_w, int win_h);
	void up(long long id);
	void clear() { m_fingers.clear(); }
	bool any_finger() const { return !m_fingers.empty(); }

	// what the fingers hold now, added to a player's buttons
	void apply(cabinet_inputs::player &p) const;
	bool held(touch_part t) const;

	// arranging (Settings): a finger drags the part it went down on; nothing is pressed
	bool arranging = false;

	// radius of a part in pixels of a window of that size
	float radius(touch_part t, int win_w, int win_h) const;
	// which part a point (window fractions) is on, or -1; `slack` widens every part
	int hit(float x, float y, int win_w, int win_h, float slack = 1.0f) const;

	// The controls as an RGBA picture (top row first) of w x h, the window's shape, to be
	// drawn over the whole window with its transparency.
	void render(std::vector<uint8_t> &rgba, int w, int h) const;

private:
	struct finger { long long id; int part; float x, y; float gx = 0, gy = 0; };
	std::vector<finger> m_fingers;
	int m_ww = 1600, m_wh = 720;   // the window the fingers were last seen in
	void stick_dirs(bool &l, bool &r, bool &u, bool &d) const;
	finger *find(long long id);
};
