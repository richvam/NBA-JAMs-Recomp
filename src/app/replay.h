// replay.h - a recorded game (roadmap N5.4): where it started and what every frame got.
// A replay is a save state (state_file: the machine and the Lua scripts) taken before its
// first frame, then the cabinet inputs of each frame. Played back on the same build with the
// same ROMs and Lua mods, the game runs exactly as it did, as an online game does on every
// peer; the end hash says whether it did. No SDL here, so it is tested without a display.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "machine.h"

struct replay
{
	static constexpr int kFrameBytes = 9;    // four players (16 bits each), then the cabinet's own switches

	std::string build;                       // the program version that recorded it (version.h)
	std::string game;                        // game id (games.h)
	std::string mods;                        // the Lua mods and options it ran with (mod_manager::net_description)
	int64_t saved_at = 0;                    // when it was written, seconds since 1970
	uint8_t players = 1;                     // computers in the game (1 offline)
	bool online = false;
	uint32_t start_hash = 0, end_hash = 0;   // the game state before the first frame and after the last
	std::vector<uint8_t> state;              // the state_file before the first frame
	std::vector<uint8_t> frames;             // kFrameBytes a frame

	uint32_t count() const { return uint32_t(frames.size() / kFrameBytes); }
	double seconds(double hz = 54.71) const { return count() / hz; }
	void add(const cabinet_inputs &in);
	cabinet_inputs at(uint32_t frame) const;
	void clear_frames() { frames.clear(); }

	// the file: "NJRP", a version, then everything else compressed (zlib)
	std::vector<uint8_t> encode() const;
	static bool decode(const std::vector<uint8_t> &file, replay &out, std::string &err);
};
