// menu_sound.h - menu music and the cursor, confirm and back sounds (roadmap U6), played
// by the game's own sound board from the user's ROMs: a second machine of the highlighted
// game runs with its main CPU held, and the menus send it the same command bytes the game
// itself sends on its title and team select screens. Nothing is extracted or written.
//
// The machine runs on a worker thread and hands its samples to a sink (the app queues them
// to the audio device). No SDL in here, so the headless tools can drive it too.
#pragma once
#include "machine.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// The command bytes for one game, found by logging what the game sends (tools/headless/sndlog)
struct menu_cues
{
	std::vector<u8> music, cursor, confirm, back;
	int warm_frames = 120;               // the sound board's start-up, before it takes commands
};
const menu_cues *menu_cues_for(const std::string &game_id);    // nullptr: no menu sound for it

class menu_sound
{
public:
	enum cue { cursor, confirm, back };
	using make_fn = std::function<std::unique_ptr<arcade_machine>(const std::string &game_id)>;
	using queued_fn = std::function<size_t()>;                     // samples waiting in the device
	using push_fn = std::function<void(const float *samples, size_t n)>;

	menu_sound(make_fn make, queued_fn queued, push_fn push);
	~menu_sound();

	// What should play: the game whose board plays ("" or no ROMs: silence), whether the
	// music and the sounds are on, and their volume. Called every menu frame.
	void want(const std::string &game_id, bool music, bool sounds, float volume);
	// Nothing at all, at once: the caller then clears the device (a game starts).
	void silence();
	void play(cue c);
	bool music_playing() const { return m_music_playing; }
	bool ready() const { return m_ready; }

private:
	struct board { std::unique_ptr<arcade_machine> m; bool music_sent = false; };
	void run();
	make_fn m_make;
	queued_fn m_queued;
	push_fn m_push;
	std::mutex m_mx;                     // the wishes below, held only for a moment
	std::mutex m_step;                   // one step of the worker: the boards
	std::condition_variable m_cv;
	std::string m_id, m_drop;
	bool m_music = true, m_sounds = true, m_quit = false;
	float m_volume = 0.6f;
	std::map<std::string, board> m_boards;   // a board per game, kept while the program runs
	std::vector<u8> m_pending;               // bytes still to send, one a frame
	std::string m_pending_for;
	std::atomic<bool> m_music_playing{ false }, m_ready{ false };
	std::thread m_worker;
};
