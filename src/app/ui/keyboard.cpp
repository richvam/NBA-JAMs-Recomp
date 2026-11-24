// keyboard.cpp - see keyboard.h
#include "keyboard.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ui
{

int keyboard::box_x() { return (screen::kW - kW) / 2; }


static const int kKeyH = 16, kGap = 3;

bool keyboard::takes(char c) const
{
	const bool digit = c >= '0' && c <= '9', letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	switch (m_layout)
	{
	case layout::number: return digit;
	case layout::address: return digit || letter || c == '.' || c == '-' || c == ':';
	default: return digit || letter || c == ' ' || c == '.' || c == '-' || c == '_';
	}
}

void keyboard::open(layout l, const std::string &title, const std::string &start, size_t max_len)
{
	m_layout = l;
	m_title = title;
	m_max = max_len;
	m_lower = false;
	text.clear();
	for (char c : start) if (takes(c) && text.size() < m_max) text += c;
	m_open = true;
	build();
	m_row = m_col = 0;
}

void keyboard::build()
{
	std::vector<std::string> chars;
	std::vector<key> special;
	switch (m_layout)
	{
	case layout::text:
		chars = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ.-_ ", "1234567890" };
		special = { { m_lower ? "CAPITALS" : "SMALL", k_shift, {} }, { "DELETE", k_del, {} }, { "OK", k_ok, {} } };
		break;
	case layout::number:
		chars = { "123", "456", "789", "0" };
		special = { { "DELETE", k_del, {} }, { "OK", k_ok, {} } };
		break;
	case layout::address:
		chars = { "123", "456", "789", ".0" };
		special = { { "DELETE", k_del, {} }, { "OK", k_ok, {} } };
		break;
	}
	size_t cols = 0;
	for (auto &r : chars) cols = std::max(cols, r.size());
	const int cw = cols > 5 ? 26 : 44, total = int(cols) * (cw + kGap) - kGap;
	const float top = float(kY + 44);
	m_rows.clear();
	for (size_t r = 0; r < chars.size(); r++)
	{
		std::vector<key> row;
		const int n = int(chars[r].size()), w = n * (cw + kGap) - kGap;
		const float x0 = float(box_x() + kW / 2 - w / 2);
		for (int c = 0; c < n; c++)
		{
			char ch = chars[r][size_t(c)];
			if (m_lower && ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
			key k;
			k.ch = ch;
			k.label = ch == ' ' ? "SP" : std::string(1, ch);
			k.r = { x0 + float(c * (cw + kGap)), top + float(r) * (kKeyH + kGap), float(cw), float(kKeyH) };
			row.push_back(k);
		}
		m_rows.push_back(row);
	}
	// the last row: the special keys share the width of the grid
	const int n = int(special.size()), w = (total - (n - 1) * kGap) / n;
	const float x0 = float(box_x() + kW / 2 - total / 2), y = top + float(chars.size()) * (kKeyH + kGap) + 2;
	for (int i = 0; i < n; i++) special[size_t(i)].r = { x0 + float(i * (w + kGap)), y, float(w), float(kKeyH) };
	m_rows.push_back(special);
}

void keyboard::move(int dx, int dy)
{
	const int nr = int(m_rows.size());
	if (dx)
	{
		const int n = int(m_rows[size_t(m_row)].size());
		m_col = ((m_col + dx) % n + n) % n;
		return;
	}
	// up or down: the key in that row nearest under the cursor
	const rectf &from = current().r;
	const float cx = from.x + from.w / 2;
	m_row = ((m_row + dy) % nr + nr) % nr;
	int best = 0;
	float bd = 1e9f;
	for (int c = 0; c < int(m_rows[size_t(m_row)].size()); c++)
	{
		const rectf &r = m_rows[size_t(m_row)][size_t(c)].r;
		float d = std::fabs(r.x + r.w / 2 - cx);
		if (d < bd) { bd = d; best = c; }
	}
	m_col = best;
}

bool keyboard::type(char c)
{
	if (!m_open || !takes(c) || text.size() >= m_max) return false;
	text += c;
	return true;
}

void keyboard::erase()
{
	if (!text.empty()) text.pop_back();
}

keyboard::outcome keyboard::press(const key &k)
{
	switch (k.ch)
	{
	case k_ok: return done;
	case k_del: erase(); return typed;
	case k_shift:
	{
		const int r = m_row, c = m_col;
		m_lower = !m_lower;
		build();
		m_row = r; m_col = c;
		return none;
	}
	default: return type(char(k.ch)) ? typed : none;
	}
}

keyboard::outcome keyboard::handle(const event &e)
{
	if (!m_open) return none;
	if (e.ptr == event::hover || e.ptr == event::click)
	{
		for (int r = 0; r < int(m_rows.size()); r++)
			for (int c = 0; c < int(m_rows[size_t(r)].size()); c++)
				if (m_rows[size_t(r)][size_t(c)].r.has(e.x, e.y))
				{
					m_row = r; m_col = c;
					if (e.ptr == event::click) return press(current());
					return none;
				}
		return none;
	}
	switch (e.a)
	{
	case act::up: move(0, -1); break;
	case act::down: move(0, 1); break;
	case act::left: move(-1, 0); break;
	case act::right: move(1, 0); break;
	case act::confirm: return press(current());
	case act::context: case act::tab_prev: erase(); return typed;
	case act::tab_next: return done;
	case act::back: return cancelled;
	default: break;
	}
	return none;
}

void keyboard::draw(canvas &cv, const theme &th, device dev, unsigned now_ms) const
{
	if (!m_open) return;
	auto put = [&](int x, int y, const std::string &s, uint32_t c, face f = face::list) { draw_text(cv, x, y, s, f, th.ink_for(c)); };
	cv.blend(0, 0, cv.w, cv.h, 0xff000000, 150);
	cv.fill(box_x(), kY, kW, kH, th.dialog);
	cv.frame(box_x(), kY, kW, kH, th.bar);
	cv.fill(box_x() + 1, kY + 1, kW - 2, 11, th.strip);
	put(box_x() + 4, kY + 3, fit_px(m_title, kW - 8, face::note), th.strip_text, face::note);

	// the text so far, in the plain font so that small letters show as small letters
	const int fx = box_x() + 10, fy = kY + 18, fw = kW - 20;
	cv.fill(fx, fy, fw, 16, th.panel);
	cv.frame(fx, fy, fw, 16, th.panel_edge);
	const int room = (fw - 12) / 8 - 1;
	std::string shown = int(text.size()) > room ? text.substr(text.size() - size_t(room)) : text;
	cv.text_plain(fx + 5, fy + 4, shown, th.text_focus);
	if ((now_ms / 400) % 2 == 0) cv.fill(fx + 5 + int(shown.size()) * 8, fy + 3, 2, 10, th.bar);

	for (int r = 0; r < int(m_rows.size()); r++)
		for (int c = 0; c < int(m_rows[size_t(r)].size()); c++)
		{
			const key &k = m_rows[size_t(r)][size_t(c)];
			const bool on = r == m_row && c == m_col;
			const int x = int(k.r.x), y = int(k.r.y), w = int(k.r.w), h = int(k.r.h);
			cv.fill(x, y, w, h, on ? th.bar : th.panel);
			cv.frame(x, y, w, h, on ? th.tab_sel : th.panel_edge);
			if (k.ch >= 'a' && k.ch <= 'z')
				cv.text_plain(x + (w - 8) / 2, y + 4, k.label, on ? th.text_focus : th.text);
			else
			{
				std::string t = fit_px(k.label, w - 4, face::list);
				put(x + (w - text_w(t, face::list)) / 2, y + 4, t, on ? th.text_focus : th.text);
			}
		}

	std::string help = "[" + glyph(dev, act::confirm) + "] Type  [" + glyph(dev, act::context) + "] Del  [" + glyph(dev, act::tab_next) + "] OK  [" +
	                   glyph(dev, act::back) + "] Back";
	if (dev == device::keyboard) help = "Keys type. Enter: OK  Esc: cancel  Ctrl+V: paste";
	put(box_x() + 6, kY + kH - 12, fit_px(help, kW - 12, face::note), th.status, face::note);
}

} // namespace ui
