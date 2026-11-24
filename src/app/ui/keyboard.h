// keyboard.h - the on-screen keyboard (roadmap U4.1): a letter grid in the style of the
// games' initials entry, drawn over a screen wherever text is needed, so a controller is
// enough to type a host's address, a port or a profile name. A physical keyboard keeps
// typing directly (the app passes its characters to type()), and paste still works.
//
//   Directions   move over the keys (they wrap round at the edges)
//   Confirm      press the key under the cursor
//   Context      delete the last character
//   Next tab     OK: take the text
//   Back         cancel
//
// The mouse and touch press a key directly.
#pragma once
#include "ui.h"
#include <string>
#include <vector>

namespace ui
{

enum class layout
{
	text,       // letters, digits, space and . - _ ; a shift key for lower case
	number,     // digits only (a port)
	address,    // digits and a dot (an IPv4 address); a physical keyboard can also type a name
};

class keyboard
{
public:
	enum outcome { none, typed, done, cancelled };
	enum code { k_del = -1, k_ok = -2, k_shift = -3 };
	struct key { std::string label; int ch = 0; rectf r; };   // ch: the character, or a code

	void open(layout l, const std::string &title, const std::string &text, size_t max_len);
	void close() { m_open = false; }
	bool is_open() const { return m_open; }

	outcome handle(const event &e);
	bool type(char c);                      // from a physical keyboard or paste: false if the layout does not take it
	void erase();
	void draw(canvas &cv, const theme &th, device dev, unsigned now_ms) const;

	std::string text;
	layout kind() const { return m_layout; }
	// the keys as drawn, by row: for the mouse and the tests
	const std::vector<std::vector<key>> &rows() const { return m_rows; }
	int row() const { return m_row; }
	int col() const { return m_col; }
	const key &current() const { return m_rows[size_t(m_row)][size_t(m_col)]; }
	bool lower() const { return m_lower; }

	static constexpr int kY = 70, kW = 308, kH = 170;   // the box, in canvas units, centred across
	static int box_x();

private:
	void build();
	outcome press(const key &k);
	void move(int dx, int dy);
	bool takes(char c) const;
	bool m_open = false, m_lower = false;
	layout m_layout = layout::text;
	std::string m_title;
	size_t m_max = 24;
	std::vector<std::vector<key>> m_rows;
	int m_row = 0, m_col = 0;
};

} // namespace ui
