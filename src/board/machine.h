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
	virtual std::vector<u8> save_state() { return {}; }
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
	virtual void sound_command(u8 byte) { (void)byte; }
	std::function<void(u8)> on_sound_command;           // each byte the main CPU sends the sound board
};
