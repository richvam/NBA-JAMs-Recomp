// meet_proto.cpp - see meet_proto.h
#include "meet_proto.h"
#include <cstdio>
#include <cstdlib>

namespace meet {

// ---------------------------------------------------------------------------
// fields
// ---------------------------------------------------------------------------
bool valid_game(const std::string &s)
{
	if (s.empty() || s.size() > GAME_MAX) return false;
	for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
	return true;
}

bool valid_region(const std::string &s)
{
	if (s.size() < 2 || s.size() > REGION_MAX) return false;
	for (char c : s) if (!((c >= 'a' && c <= 'z') || c == '-')) return false;
	return true;
}

bool valid_text(const std::string &s, size_t max)
{
	if (s.empty() || s.size() > max) return false;
	for (size_t i = 0; i < s.size();)
	{
		unsigned char c = (unsigned char)s[i];
		int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
		if (!len || i + size_t(len) > s.size()) return false;
		if (len == 1 && (c < 0x20 || c == 0x7f)) return false;           // no control characters
		for (int k = 1; k < len; k++) if (((unsigned char)s[i + size_t(k)] >> 6) != 2) return false;
		i += size_t(len);
	}
	return true;
}

bool valid_code(const std::string &s)
{
	if (s.empty() || s.size() > CODE_MAX) return false;
	for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '-')) return false;
	return true;
}

std::string quote(const std::string &s)
{
	std::string o = "\"";
	for (unsigned char c : s)
	{
		if (c == '"' || c == '\\') { o += '\\'; o += char(c); }
		else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
		else o += char(c);
	}
	return o + "\"";
}

// ---------------------------------------------------------------------------
// requests
// ---------------------------------------------------------------------------
std::string publish_body(const room &r, const std::string &code)
{
	if (!valid_game(r.game) || !valid_region(r.region) || !valid_text(r.name, NAME_MAX) || !valid_text(r.version, VERSION_MAX) || !valid_code(code) ||
	    r.seats < 1 || r.seats > SEATS_MAX || r.players < 1 || r.players > r.seats) return {};
	return "{\"game\":" + quote(r.game) + ",\"region\":" + quote(r.region) + ",\"name\":" + quote(r.name) + ",\"version\":" + quote(r.version) +
	       ",\"code\":" + quote(code) + ",\"players\":" + std::to_string(r.players) + ",\"seats\":" + std::to_string(r.seats) + "}";
}

std::string wait_body(const std::string &key, int players)
{
	if (!valid_text(key, KEY_MAX) || players < 1 || players > SEATS_MAX) return {};
	return "{\"key\":" + quote(key) + ",\"players\":" + std::to_string(players) + "}";
}

std::string remove_body(const std::string &key)
{
	if (!valid_text(key, KEY_MAX)) return {};
	return "{\"key\":" + quote(key) + "}";
}

std::string introduce_body(const std::string &code, const std::string &nick)
{
	if (!valid_code(code) || !valid_text(nick, NICK_MAX)) return {};
	return "{\"code\":" + quote(code) + ",\"nick\":" + quote(nick) + "}";
}

std::string list_query(const std::string &game, const std::string &region)
{
	if (!valid_game(game) || (!region.empty() && !valid_region(region))) return {};
	return "game=" + game + (region.empty() ? "" : "&region=" + region);   // both are URL-safe as checked
}

// ---------------------------------------------------------------------------
// answers
// ---------------------------------------------------------------------------
answer parse_answer(const std::string &text)
{
	answer a;
	json j;
	if (text.size() > 64 * 1024 || !parse_json(text, j) || j.kind != json::object) { a.error = "bad answer"; return a; }
	a.error = j.str("error");
	a.ok = a.error.empty();
	a.id = j.str("id"); a.key = j.str("key"); a.code = j.str("code");
	if (const json *m = j.get("more")) a.more = m->kind == json::boolean && m->b;
	if (const json *rs = j.get("rooms"))
		for (const json &r : rs->a)
		{
			if (r.kind != json::object || int(a.rooms.size()) >= LIST_MAX) continue;
			room x;
			x.id = r.str("id"); x.game = r.str("game"); x.region = r.str("region"); x.name = r.str("name"); x.version = r.str("version");
			x.players = r.num("players", 1); x.seats = r.num("seats", 4); x.age_s = r.num("age_s");
			// what the service sends is checked as strictly as what we send
			if (valid_game(x.game) && valid_region(x.region) && valid_text(x.name, NAME_MAX) && valid_text(x.id, ID_MAX) &&
			    x.seats >= 1 && x.seats <= SEATS_MAX && x.players >= 0 && x.players <= x.seats)
				a.rooms.push_back(x);
		}
	if (const json *is = j.get("intros"))
		for (const json &i : is->a)
		{
			intro x{ i.str("code"), i.str("nick") };
			if (i.kind == json::object && valid_code(x.code) && valid_text(x.nick, NICK_MAX) && a.intros.size() < 16) a.intros.push_back(x);
		}
	return a;
}

// ---------------------------------------------------------------------------
// budget
// ---------------------------------------------------------------------------
long requests(const session &s)
{
	long n = 0;
	if (s.host_minutes > 0)
	{
		n += 2;                                                         // publish, remove
		n += (long(s.host_minutes) * 60 + WAIT_S - 1) / WAIT_S;         // waits held to the end
		n += s.intros;                                                  // a wait ended early by each player
	}
	if (s.browse_minutes > 0) n += 1 + long(s.browse_minutes) * 60 / LIST_EVERY_S;   // on opening, then every LIST_EVERY_S
	n += s.joins;                                                       // introduce
	return n;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------
namespace {
struct reader
{
	const std::string &t;
	size_t i = 0;
	int depth = 0;
	void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++; }
	bool lit(const char *w) { size_t n = strlen_(w); if (t.compare(i, n, w) != 0) return false; i += n; return true; }
	static size_t strlen_(const char *w) { size_t n = 0; while (w[n]) n++; return n; }
	bool str(std::string &o)
	{
		if (i >= t.size() || t[i] != '"') return false;
		i++;
		while (i < t.size() && t[i] != '"')
		{
			char c = t[i++];
			if (c != '\\') { o += c; continue; }
			if (i >= t.size()) return false;
			char e = t[i++];
			switch (e)
			{
				case '"': case '\\': case '/': o += e; break;
				case 'n': o += '\n'; break; case 't': o += '\t'; break; case 'r': o += '\r'; break; case 'b': o += '\b'; break; case 'f': o += '\f'; break;
				case 'u':
				{
					if (i + 4 > t.size()) return false;
					unsigned v = unsigned(strtoul(t.substr(i, 4).c_str(), nullptr, 16));
					i += 4;
					// UTF-8 (surrogate pairs are not needed for the fields here and come out as '?')
					if (v >= 0xd800 && v < 0xe000) o += '?';
					else if (v < 0x80) o += char(v);
					else if (v < 0x800) { o += char(0xc0 | v >> 6); o += char(0x80 | (v & 63)); }
					else { o += char(0xe0 | v >> 12); o += char(0x80 | (v >> 6 & 63)); o += char(0x80 | (v & 63)); }
					break;
				}
				default: return false;
			}
		}
		if (i >= t.size()) return false;
		i++;
		return true;
	}
	bool value(json &v)
	{
		if (++depth > 16) return false;
		ws();
		if (i >= t.size()) return false;
		char c = t[i];
		bool ok = true;
		if (c == '{')
		{
			v.kind = json::object; i++; ws();
			if (i < t.size() && t[i] == '}') i++;
			else
				while (ok)
				{
					ws();
					std::string k;
					json x;
					ok = str(k); ws();
					ok = ok && i < t.size() && t[i++] == ':' && value(x);
					if (ok) v.o[k] = x;
					ws();
					if (ok && i < t.size() && t[i] == ',') { i++; continue; }
					ok = ok && i < t.size() && t[i++] == '}';
					break;
				}
		}
		else if (c == '[')
		{
			v.kind = json::array; i++; ws();
			if (i < t.size() && t[i] == ']') i++;
			else
				while (ok)
				{
					json x;
					ok = value(x);
					if (ok) v.a.push_back(x);
					ws();
					if (ok && i < t.size() && t[i] == ',') { i++; continue; }
					ok = ok && i < t.size() && t[i++] == ']';
					break;
				}
		}
		else if (c == '"') { v.kind = json::string; ok = str(v.s); }
		else if (lit("true")) { v.kind = json::boolean; v.b = true; }
		else if (lit("false")) { v.kind = json::boolean; }
		else if (lit("null")) v.kind = json::null;
		else
		{
			char *end = nullptr;
			v.n = strtod(t.c_str() + i, &end);
			if (end == t.c_str() + i) ok = false;
			else { v.kind = json::number; i = size_t(end - t.c_str()); }
		}
		depth--;
		return ok;
	}
};
}

bool parse_json(const std::string &text, json &out)
{
	reader r{ text };
	out = json();
	if (!r.value(out)) return false;
	r.ws();
	return r.i == text.size();
}

} // namespace meet
