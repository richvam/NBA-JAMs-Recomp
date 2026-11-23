// machine.h - common interface for the emulated boards (T-unit, Wolf-unit)
#pragma once
#include "emu.h"
#include <functional>
#include <string>
#include <vector>

// Player/cabinet inputs, active-high booleans from the frontend
struct cabinet_inputs
{
	struct player { bool up, down, left, right, shoot, pass, turbo, start, coin; } p[4] = {};
	bool service = false;     // service credit
	bool test = false;        // test switch (dip)
	bool volume_up = false, volume_down = false;
};

struct dip_settings
{
	bool free_play = true;
	bool four_players = true;
	int country = 0;          // 0=USA 1=French 2=German
	bool video_clips = true;
	bool powerup_test = false;
	bool test_switch = false;
};

inline u32 fnv1a(const void *d, size_t n, u32 h = 2166136261u)
{
	const u8 *p = static_cast<const u8 *>(d);
	for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
	return h;
}

// Save states (roadmap N1.2): the program ROM as Lua scripts changed it, as the runs of
// bytes that differ from the ROM as loaded (orig). On load the ROM is put back to orig
// and the runs written again.
inline void io_rom_patches(state_io &s, std::vector<u8> &rom, const std::vector<u8> &orig)
{
	s.mark("rom patches");
	u32 runs = 0;
	if (s.saving())
	{
		std::vector<std::pair<u32, u32>> r;
		for (size_t i = 0; i < rom.size();)
		{
			// whole blocks that are the same are skipped with memcmp (roadmap N1.3: the ROM
			// is megabytes and a state may be saved every frame)
			if ((i & 4095) == 0 && i + 4096 <= rom.size() && !std::memcmp(&rom[i], &orig[i], 4096)) { i += 4096; continue; }
			if (rom[i] == orig[i]) { i++; continue; }
			size_t j = i;
			while (j < rom.size() && rom[j] != orig[j]) j++;
			r.push_back({ u32(i), u32(j - i) });
			i = j;
		}
		runs = u32(r.size());
		s(runs);
		for (auto &p : r) { s(p.first); s(p.second); s.raw(&rom[p.first], p.second); }
		return;
	}
	s(runs);
	if (!s.ok()) return;
	std::vector<u8> next = orig;
	for (u32 k = 0; k < runs && s.ok(); k++)
	{
		u32 at = 0, n = 0;
		s(at); s(n);
		if (!s.ok() || size_t(at) + n > next.size()) { s.fail("a ROM patch is out of range"); return; }
		s.raw(&next[at], n);
	}
	if (s.ok() && next.size() == rom.size()) std::copy(next.begin(), next.end(), rom.begin());   // same buffer: others keep pointers to it
	else if (s.ok()) s.fail("the program ROM size differs");
}

class arcade_machine
{
public:
	virtual ~arcade_machine() = default;
	virtual void reset() = 0;
	virtual void run_frame() = 0;                       // run one video frame
	virtual int width() const = 0;
	virtual int height() const = 0;
	virtual const u32 *frame() const = 0;               // XRGB8888, width*height
	virtual double refresh_hz() const = 0;
	virtual void set_inputs(const cabinet_inputs &in) = 0;
	virtual void set_dips(const dip_settings &d) = 0;
	virtual int audio_rate() const { return 48000; }
	virtual void take_audio(std::vector<float> &out) = 0;  // mono samples since last call
	virtual std::vector<u8> nvram() const = 0;
	virtual void set_nvram(const std::vector<u8> &d) = 0;
	virtual std::string status() const { return {}; }
	virtual u32 pc() const { return 0; }
	virtual cpu_device *maincpu() { return nullptr; }
	virtual cpu_device *soundcpu() { return nullptr; }
	virtual u32 state_hash() const { return 0; }        // main RAM hash (netplay desync check)

	// Save states (roadmap N1.1): the whole machine, every CPU and device, as bytes. The
	// ROMs are not in it; a state loads only into a machine of the same game and build.
	// save_state() gives an empty vector and load_state() false (the machine as it was
	// is not kept) on failure, with the reason in state_error().
	// save_state_to() writes into a buffer the caller keeps, so saving every frame does not
	// allocate (roadmap N1.3); it returns false (out empty) on failure.
	std::vector<u8> save_state() { std::vector<u8> v; save_state_to(v); return v; }
	virtual bool save_state_to(std::vector<u8> &out) { out.clear(); return false; }
	virtual bool load_state(const std::vector<u8> &) { return false; }
	const std::string &state_error() const { return m_state_error; }
protected:
	std::string m_state_error;
public:
	virtual u32 fingerprint() const { return 0; }       // ROM identity (netplay)

	// Lua mods: program ROM (interleaved little-endian words, mapped at bit address
	// 0xff800000) and the main CPU's view of memory (bit addresses, 16-bit words).
	virtual std::vector<u8> *program_rom() { return nullptr; }
	virtual u16 peek16(u32 bitaddr) { (void)bitaddr; return 0; }
	virtual void poke16(u32 bitaddr, u16 v) { (void)bitaddr; (void)v; }

	// Menu sound (roadmap U6.1): the board's own sound board on its own. With sound_only on
	// the main CPU is held, so only the bytes sent with sound_command() play.
	virtual void set_sound_only(bool on) { (void)on; }

	// Widescreen (roadmap W3.1): what the program needs to draw the picture wider itself.
	// None of it changes the game, its timing or its state hash.
	//   watch_pc     cb runs just before the main CPU executes the instruction at bit address
	//                pc (for reading the game's lists when its display routine starts); an
	//                empty cb removes the watch. One watch at a time.
	//   observe_dma  cb sees each blitter command the game issues (its 18 registers)
	//   wide_blit    draws one blitter command into a picture of the caller's (see
	//                midtunit_video_device::blit_to)
	//   pens         XRGB8888 for each of the 0x8000 palette entries
	virtual void watch_pc(u32 pc, std::function<void()> cb) { (void)pc; (void)cb; }
	virtual void observe_dma(std::function<void(const u16 *regs)> cb) { (void)cb; }
	virtual void wide_blit(const u16 regs[18], u16 *target, int pitch) { (void)regs; (void)target; (void)pitch; }
	virtual const u32 *pens() const { return nullptr; }
	virtual void sound_command(u8 byte) { (void)byte; }
	std::function<void(u8)> on_sound_command;           // each byte the main CPU sends the sound board
};
