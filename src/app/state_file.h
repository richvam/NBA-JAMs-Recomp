// state_file.h - a save slot's file (roadmap N1.4): the machine's state and the Lua
// scripts' state, compressed, with what is needed to refuse a file that does not fit.
//
//   "NBJSTATE"  8 bytes
//   u32         format (1)
//   string      the program's version (a state loads only into the same build)
//   string      the game's id
//   s64         when it was saved (seconds since 1970)
//   u32         the game state's hash when it was saved (checked after a load)
//   u32 u32     the machine's and the scripts' state sizes, raw
//   u32         the compressed size, then the two states compressed together (miniz)
// A string is a u32 length and its bytes. Numbers are little-endian.
#pragma once
#include "miniz.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace state_file
{
struct header
{
	std::string build, game;
	int64_t saved_at = 0;
	uint32_t hash = 0;
};

namespace detail
{
inline void put32(std::vector<uint8_t> &o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back(uint8_t(v >> (8 * i))); }
inline void put64(std::vector<uint8_t> &o, uint64_t v) { for (int i = 0; i < 8; i++) o.push_back(uint8_t(v >> (8 * i))); }
inline void put_str(std::vector<uint8_t> &o, const std::string &s) { put32(o, uint32_t(s.size())); o.insert(o.end(), s.begin(), s.end()); }
struct reader
{
	const std::vector<uint8_t> &d;
	size_t at = 0;
	bool ok = true;
	uint64_t num(int n)
	{
		if (at + size_t(n) > d.size()) { ok = false; return 0; }
		uint64_t v = 0;
		for (int i = 0; i < n; i++) v |= uint64_t(d[at + size_t(i)]) << (8 * i);
		at += size_t(n);
		return v;
	}
	std::string str()
	{
		uint32_t n = uint32_t(num(4));
		if (!ok || n > d.size() - at) { ok = false; return {}; }
		std::string s(d.begin() + long(at), d.begin() + long(at + n));
		at += n;
		return s;
	}
};
static const char kMagic[8] = { 'N', 'B', 'J', 'S', 'T', 'A', 'T', 'E' };
}

inline bool pack(const header &h, const std::vector<uint8_t> &machine, const std::vector<uint8_t> &scripts, std::vector<uint8_t> &out)
{
	using namespace detail;
	std::vector<uint8_t> raw(machine);
	raw.insert(raw.end(), scripts.begin(), scripts.end());
	mz_ulong n = mz_compressBound(mz_ulong(raw.size()));
	std::vector<uint8_t> z(n);
	if (mz_compress2(z.data(), &n, raw.data(), mz_ulong(raw.size()), 1) != MZ_OK) return false;
	out.assign(kMagic, kMagic + 8);
	put32(out, 1);
	put_str(out, h.build); put_str(out, h.game);
	put64(out, uint64_t(h.saved_at));
	put32(out, h.hash);
	put32(out, uint32_t(machine.size())); put32(out, uint32_t(scripts.size()));
	put32(out, uint32_t(n));
	out.insert(out.end(), z.begin(), z.begin() + long(n));
	return true;
}

// the header only (for the slot list); false when it is not a state file
inline bool read_header(const std::vector<uint8_t> &file, header &h)
{
	using namespace detail;
	if (file.size() < 12 || std::memcmp(file.data(), kMagic, 8) != 0) return false;
	reader r{ file, 8 };
	if (r.num(4) != 1) return false;
	h.build = r.str(); h.game = r.str();
	h.saved_at = int64_t(r.num(8));
	h.hash = uint32_t(r.num(4));
	return r.ok;
}

inline bool unpack(const std::vector<uint8_t> &file, header &h, std::vector<uint8_t> &machine, std::vector<uint8_t> &scripts, std::string &err)
{
	using namespace detail;
	if (!read_header(file, h)) { err = "not a save state file"; return false; }
	reader r{ file, 8 + 4 + 4 + h.build.size() + 4 + h.game.size() + 8 + 4 };
	uint32_t nm = uint32_t(r.num(4)), ns = uint32_t(r.num(4)), nz = uint32_t(r.num(4));
	if (!r.ok || nz > file.size() - r.at || uint64_t(nm) + ns > (64u << 20)) { err = "the file is damaged"; return false; }
	std::vector<uint8_t> raw(size_t(nm) + ns);
	mz_ulong got = mz_ulong(raw.size());
	if (mz_uncompress(raw.data(), &got, file.data() + r.at, nz) != MZ_OK || got != raw.size()) { err = "the file is damaged"; return false; }
	machine.assign(raw.begin(), raw.begin() + long(nm));
	scripts.assign(raw.begin() + long(nm), raw.end());
	return true;
}
}
