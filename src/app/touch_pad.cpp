#include "touch_pad.h"
#include "ui/canvas.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

// Default places for a landscape phone: the stick low on the left, the three game buttons
// in an arc low on the right (Shoot under the thumb), Start and Coin small at the top
// corners, away from where the thumbs rest.
touch_layout touch_layout::defaults()
{
	touch_layout l;
	const float px[tp_count] = { 0.13f, 0.90f, 0.78f, 0.84f, 0.94f, 0.06f };
	const float py[tp_count] = { 0.70f, 0.62f, 0.80f, 0.38f, 0.10f, 0.10f };
	for (int i = 0; i < tp_count; i++) { l.x[i] = px[i]; l.y[i] = py[i]; }
	return l;
}

std::string touch_layout::save() const
{
	std::ostringstream o;
	char b[32];
	for (int i = 0; i < tp_count; i++) { snprintf(b, sizeof b, "%.3f,%.3f;", x[i], y[i]); o << b; }
	snprintf(b, sizeof b, "%.2f;%.2f", size, opacity);
	o << b;
	return o.str();
}

bool touch_layout::load(const std::string &s)
{
	touch_layout l = defaults();
	std::vector<std::string> parts;
	std::stringstream ss(s);
	for (std::string p; std::getline(ss, p, ';');) parts.push_back(p);
	if (parts.size() != size_t(tp_count) + 2) return false;
	for (int i = 0; i < tp_count; i++)
	{
		float a, b;
		if (sscanf(parts[size_t(i)].c_str(), "%f,%f", &a, &b) != 2 || !(a >= 0 && a <= 1 && b >= 0 && b <= 1)) return false;
		l.x[i] = a; l.y[i] = b;
	}
	float sz = float(atof(parts[size_t(tp_count)].c_str())), op = float(atof(parts[size_t(tp_count) + 1].c_str()));
	if (!(sz >= kMinSize && sz <= kMaxSize && op >= kMinOpacity && op <= kMaxOpacity)) return false;
	l.size = sz; l.opacity = op;
	*this = l;
	return true;
}

float touch_pad::radius(touch_part t, int win_w, int win_h) const
{
	float m = float(std::min(win_w, win_h)) * layout.size;
	switch (t)
	{
	case tp_stick: return 0.20f * m;
	case tp_start: case tp_coin: return 0.06f * m;
	default: return 0.095f * m;
	}
}

int touch_pad::hit(float x, float y, int win_w, int win_h, float slack) const
{
	int best = -1;
	float bd = 0;
	for (int i = 0; i < tp_count; i++)
	{
		float dx = (x - layout.x[i]) * float(win_w), dy = (y - layout.y[i]) * float(win_h);
		float d = std::sqrt(dx * dx + dy * dy), r = radius(touch_part(i), win_w, win_h) * slack;
		if (d <= r && (best < 0 || d / r < bd)) { best = i; bd = d / r; }
	}
	return best;
}

touch_pad::finger *touch_pad::find(long long id)
{
	for (auto &f : m_fingers) if (f.id == id) return &f;
	return nullptr;
}

void touch_pad::down(long long id, float x, float y, int win_w, int win_h)
{
	up(id);
	m_ww = win_w; m_wh = win_h;
	finger f{ id, -1, x, y };
	if (arranging)
	{
		f.part = hit(x, y, win_w, win_h, 1.2f);
		if (f.part >= 0) { f.gx = layout.x[f.part] - x; f.gy = layout.y[f.part] - y; }   // keep where it was grabbed
	}
	else
	{
		// the stick takes a thumb that lands anywhere near it (it is the part most often
		// missed); a button needs to be touched, with a little room around it
		f.part = hit(x, y, win_w, win_h, 1.25f);
		if (f.part < 0 && hit(x, y, win_w, win_h, 1.0f) < 0)
		{
			float dx = (x - layout.x[tp_stick]) * float(win_w), dy = (y - layout.y[tp_stick]) * float(win_h);
			if (std::sqrt(dx * dx + dy * dy) < radius(tp_stick, win_w, win_h) * 1.8f) f.part = tp_stick;
		}
	}
	m_fingers.push_back(f);
}

void touch_pad::move(long long id, float x, float y, int win_w, int win_h)
{
	finger *f = find(id);
	if (!f) return;
	m_ww = win_w; m_wh = win_h;
	f->x = x; f->y = y;
	if (arranging)
	{
		if (f->part >= 0)
		{
			layout.x[f->part] = std::clamp(x + f->gx, 0.02f, 0.98f);
			layout.y[f->part] = std::clamp(y + f->gy, 0.02f, 0.98f);
		}
		return;
	}
	// a finger on a button may slide to the next one (Shoot to Pass); the stick keeps its finger
	if (f->part != tp_stick)
	{
		int h = hit(x, y, win_w, win_h, 1.25f);
		f->part = h == tp_stick ? -1 : h;
	}
}

void touch_pad::up(long long id)
{
	m_fingers.erase(std::remove_if(m_fingers.begin(), m_fingers.end(), [&](const finger &f) { return f.id == id; }), m_fingers.end());
}

bool touch_pad::held(touch_part t) const
{
	if (arranging) return false;
	for (const auto &f : m_fingers) if (f.part == int(t)) return true;
	return false;
}

// eight ways: a direction is held when the thumb is more than a third of the stick's radius
// from its centre, and diagonals when the angle is within 22.5 degrees of one (as an arcade stick)
void touch_pad::stick_dirs(bool &l, bool &r, bool &u, bool &d) const
{
	l = r = u = d = false;
	if (arranging) return;
	for (const auto &f : m_fingers)
	{
		if (f.part != tp_stick) continue;
		// in pixels: on a wide phone a fraction of the width is much longer than one of the height
		float dx = (f.x - layout.x[tp_stick]) * float(m_ww), dy = (f.y - layout.y[tp_stick]) * float(m_wh);
		float len = std::sqrt(dx * dx + dy * dy);
		if (len < 0.3f * radius(tp_stick, m_ww, m_wh)) continue;   // dead zone: a third of the stick
		float a = std::atan2(-dy, dx) * 180.0f / 3.14159265f;    // 0 = right, 90 = up
		if (a < 0) a += 360;
		int sector = int((a + 22.5f) / 45.0f) % 8;               // 0 right, 1 up-right, 2 up, ...
		r = sector == 0 || sector == 1 || sector == 7;
		u = sector == 1 || sector == 2 || sector == 3;
		l = sector == 3 || sector == 4 || sector == 5;
		d = sector == 5 || sector == 6 || sector == 7;
	}
}

void touch_pad::apply(cabinet_inputs::player &p) const
{
	bool l, r, u, d;
	stick_dirs(l, r, u, d);
	p.left |= l; p.right |= r; p.up |= u; p.down |= d;
	p.shoot |= held(tp_shoot); p.pass |= held(tp_pass); p.turbo |= held(tp_turbo);
	p.start |= held(tp_start); p.coin |= held(tp_coin);
}

void touch_pad::render(std::vector<uint8_t> &rgba, int w, int h) const
{
	rgba.assign(size_t(w) * h * 4, 0);
	if (w <= 0 || h <= 0) return;
	const float op = layout.opacity;
	auto put = [&](int x, int y, uint32_t rgb, float a) {
		if (x < 0 || y < 0 || x >= w || y >= h || a <= 0) return;
		uint8_t *p = &rgba[(size_t(y) * w + x) * 4];
		float old = p[3] / 255.0f, na = a + old * (1 - a);
		for (int c = 0; c < 3; c++)
		{
			float s = float((rgb >> (16 - 8 * c)) & 0xff);
			p[c] = uint8_t(na > 0 ? (s * a + p[c] * old * (1 - a)) / na : 0);
		}
		p[3] = uint8_t(std::min(255.0f, na * 255.0f));
	};
	// a disc with a ring; a held part is filled brighter
	auto disc = [&](float cx, float cy, float r, uint32_t rgb, bool on, float fill) {
		int x0 = int(cx - r - 2), x1 = int(cx + r + 2), y0 = int(cy - r - 2), y1 = int(cy + r + 2);
		float ring = std::max(1.5f, r * 0.07f);
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
			{
				float dd = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
				float edge = std::clamp(r + 0.5f - dd, 0.0f, 1.0f);           // antialiased outer edge
				if (edge <= 0) continue;
				bool in_ring = dd > r - ring;
				float a = in_ring ? op : op * (on ? 0.75f : fill);
				put(x, y, in_ring || on ? rgb : 0x202020, a * edge);
			}
	};
	const float sx = float(w), sy = float(h);
	struct look { uint32_t rgb; const char *label; };
	const look looks[tp_count] = { { 0xd0d0d0, "" }, { 0xff6a2a, "SHOOT" }, { 0x3aa0ff, "PASS" }, { 0xffd23a, "TURBO" }, { 0xd0d0d0, "START" }, { 0xd0d0d0, "COIN" } };
	bool l, r, u, d;
	stick_dirs(l, r, u, d);
	for (int i = 0; i < tp_count; i++)
	{
		float cx = layout.x[i] * sx, cy = layout.y[i] * sy, rad = radius(touch_part(i), w, h);
		if (i == tp_stick)
		{
			disc(cx, cy, rad, looks[i].rgb, false, 0.25f);
			// the knob, pushed the way the stick is held
			float kx = cx + (r ? 0.45f : l ? -0.45f : 0) * rad, ky = cy + (d ? 0.45f : u ? -0.45f : 0) * rad;
			if (arranging) { kx = cx; ky = cy; }
			disc(kx, ky, rad * 0.42f, 0xffffff, l || r || u || d, 0.55f);
		}
		else disc(cx, cy, rad, looks[i].rgb, held(touch_part(i)), 0.30f);
	}
	// labels, in the project lettering, drawn white into a scratch canvas and copied over
	canvas cv(w, h);
	std::fill(cv.px.begin(), cv.px.end(), 0u);
	for (int i = 1; i < tp_count; i++)
	{
		std::string s = looks[i].label;
		float rad = radius(touch_part(i), w, h);
		int scale = std::max(1, int(rad / 22.0f));
		int tw = int(s.size()) * 8 * scale;
		ui::cell_text(cv, int(layout.x[i] * sx) - tw / 2, int(layout.y[i] * sy) - 4 * scale, s, 0xffffffffu, scale);
	}
	for (size_t i = 0; i < cv.px.size(); i++)
		if (cv.px[i] & 0x00ffffffu) put(int(i % size_t(w)), int(i / size_t(w)), cv.px[i] & 0xffffffu, std::min(1.0f, op + 0.3f));
}
