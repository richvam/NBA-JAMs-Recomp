// replay.cpp - see replay.h
#include "replay.h"
#include "miniz.h"

namespace
{
const uint8_t kMagic[4] = { 'N', 'J', 'R', 'P' };
const uint16_t kVersion = 1;

uint16_t pack(const cabinet_inputs::player &p)
{
	return uint16_t(p.up | p.down << 1 | p.left << 2 | p.right << 3 | p.shoot << 4 | p.pass << 5 | p.turbo << 6 | p.start << 7 | p.coin << 8);
}

void unpack(uint16_t v, cabinet_inputs::player &p)
{
	p.up = v & 1; p.down = v >> 1 & 1; p.left = v >> 2 & 1; p.right = v >> 3 & 1; p.shoot = v >> 4 & 1;
	p.pass = v >> 5 & 1; p.turbo = v >> 6 & 1; p.start = v >> 7 & 1; p.coin = v >> 8 & 1;
}

struct writer
{
	std::vector<uint8_t> b;
	void u8(uint8_t v) { b.push_back(v); }
	void u32(uint32_t v) { for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i))); }
	void i64(int64_t v) { for (int i = 0; i < 8; i++) b.push_back(uint8_t(uint64_t(v) >> (8 * i))); }
	void bytes(const std::vector<uint8_t> &v) { u32(uint32_t(v.size())); b.insert(b.end(), v.begin(), v.end()); }
	void str(const std::string &s) { bytes(std::vector<uint8_t>(s.begin(), s.end())); }
};

struct reader
{
	const std::vector<uint8_t> &b;
	size_t i = 0;
	bool ok = true;
	bool need(size_t n) { if (b.size() - i < n || i > b.size()) ok = false; return ok; }
	uint8_t u8() { return need(1) ? b[i++] : 0; }
	uint32_t u32() { uint32_t v = 0; if (need(4)) for (int k = 0; k < 4; k++) v |= uint32_t(b[i++]) << (8 * k); return v; }
	int64_t i64() { uint64_t v = 0; if (need(8)) for (int k = 0; k < 8; k++) v |= uint64_t(b[i++]) << (8 * k); return int64_t(v); }
	std::vector<uint8_t> bytes() { uint32_t n = u32(); if (!need(n)) return {}; std::vector<uint8_t> v(b.begin() + long(i), b.begin() + long(i + n)); i += n; return v; }
	std::string str() { auto v = bytes(); return std::string(v.begin(), v.end()); }
};
} // namespace

void replay::add(const cabinet_inputs &in)
{
	for (const auto &p : in.p) { uint16_t v = pack(p); frames.push_back(uint8_t(v)); frames.push_back(uint8_t(v >> 8)); }
	frames.push_back(uint8_t(in.service | in.test << 1 | in.volume_up << 2 | in.volume_down << 3));
}

cabinet_inputs replay::at(uint32_t frame) const
{
	cabinet_inputs in{};
	if (frame >= count()) return in;
	const uint8_t *f = &frames[size_t(frame) * kFrameBytes];
	for (int s = 0; s < 4; s++) unpack(uint16_t(f[2 * s] | f[2 * s + 1] << 8), in.p[s]);
	in.service = f[8] & 1; in.test = f[8] >> 1 & 1; in.volume_up = f[8] >> 2 & 1; in.volume_down = f[8] >> 3 & 1;
	return in;
}

std::vector<uint8_t> replay::encode() const
{
	writer w;
	w.str(build); w.str(game); w.str(mods);
	w.i64(saved_at); w.u8(players); w.u8(online ? 1 : 0);
	w.u32(start_hash); w.u32(end_hash);
	w.bytes(state); w.bytes(frames);
	mz_ulong n = mz_compressBound(mz_ulong(w.b.size()));
	std::vector<uint8_t> out(4 + 2 + 4 + n);
	for (int i = 0; i < 4; i++) out[size_t(i)] = kMagic[i];
	out[4] = uint8_t(kVersion); out[5] = uint8_t(kVersion >> 8);
	for (int i = 0; i < 4; i++) out[size_t(6 + i)] = uint8_t(w.b.size() >> (8 * i));
	if (mz_compress2(out.data() + 10, &n, w.b.data(), mz_ulong(w.b.size()), 6) != MZ_OK) return {};
	out.resize(10 + n);
	return out;
}

bool replay::decode(const std::vector<uint8_t> &file, replay &out, std::string &err)
{
	if (file.size() < 10 || file[0] != kMagic[0] || file[1] != kMagic[1] || file[2] != kMagic[2] || file[3] != kMagic[3]) { err = "not a replay"; return false; }
	if ((file[4] | file[5] << 8) != kVersion) { err = "a replay of another format"; return false; }
	mz_ulong n = mz_ulong(file[6]) | mz_ulong(file[7]) << 8 | mz_ulong(file[8]) << 16 | mz_ulong(file[9]) << 24;
	if (n > 256u * 1024 * 1024) { err = "damaged"; return false; }
	std::vector<uint8_t> raw(n);
	mz_ulong got = n;
	if (mz_uncompress(raw.data(), &got, file.data() + 10, mz_ulong(file.size() - 10)) != MZ_OK || got != n) { err = "damaged"; return false; }
	reader r{ raw };
	replay rp;
	rp.build = r.str(); rp.game = r.str(); rp.mods = r.str();
	rp.saved_at = r.i64(); rp.players = r.u8(); rp.online = r.u8() != 0;
	rp.start_hash = r.u32(); rp.end_hash = r.u32();
	rp.state = r.bytes(); rp.frames = r.bytes();
	if (!r.ok || r.i != raw.size() || rp.frames.size() % kFrameBytes || rp.state.empty()) { err = "damaged"; return false; }
	out = std::move(rp);
	return true;
}
