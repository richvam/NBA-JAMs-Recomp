// menu_sound.cpp - see menu_sound.h
#include "menu_sound.h"
#include <algorithm>
#include <chrono>

// Logged with sndlog from the games themselves:
//   T-Unit (NBA JAM, T.E., SE; Williams ADPCM board): 01 is the tune of the record-keeping
//   and team select screens, 80 the sound of a move on the YES / NO question, 84 the one
//   when a team is chosen, a7 the one of a player joining in. One byte a sound.
//   Wolf Unit (Hangtime, Maximum Hangtime; DCS board): two bytes a sound. 00 0d is the tune
//   of the same screens, 01 bd a move on the initials grid, 03 97 a choice on team select,
//   02 60 the pass button there. The DCS board takes about seven seconds to start.
const menu_cues *menu_cues_for(const std::string &id)
{
	static const menu_cues tunit = { { 0x01 }, { 0x80 }, { 0x84 }, { 0xa7 }, 240 };
	static const menu_cues wolf = { { 0x00, 0x0d }, { 0x01, 0xbd }, { 0x03, 0x97 }, { 0x02, 0x60 }, 400 };
	if (id == "nbajam" || id == "nbajamte" || id == "nbajamse") return &tunit;
	if (id == "nbahangt" || id == "nbamht") return &wolf;
	return nullptr;
}

menu_sound::menu_sound(make_fn make, queued_fn queued, push_fn push)
	: m_make(std::move(make)), m_queued(std::move(queued)), m_push(std::move(push))
{
	m_worker = std::thread([this] { run(); });
}

menu_sound::~menu_sound()
{
	{
		std::lock_guard<std::mutex> l(m_mx);
		m_quit = true;
	}
	m_cv.notify_all();
	if (m_worker.joinable()) m_worker.join();
}

void menu_sound::want(const std::string &id, bool music, bool sounds, float volume)
{
	std::lock_guard<std::mutex> l(m_mx);
	const std::string use = (music || sounds) && menu_cues_for(id) ? id : std::string();
	if (use != m_id) { m_pending.clear(); m_ready = false; m_music_playing = false; }
	// music switched off: that board is dropped, so the tune starts from the beginning when
	// it is switched back on
	if (m_music && !music && !m_id.empty()) { m_drop = m_id; m_ready = false; m_music_playing = false; }
	m_id = use;
	m_music = music;
	m_sounds = sounds;
	m_volume = std::clamp(volume, 0.0f, 1.0f);
	m_cv.notify_all();
}

void menu_sound::silence()
{
	{
		std::lock_guard<std::mutex> l(m_mx);
		m_id.clear();
		m_pending.clear();
		m_ready = false;
		m_music_playing = false;
	}
	// wait for a step in progress: it sees the change before it pushes anything
	std::lock_guard<std::mutex> s(m_step);
}

void menu_sound::play(cue c)
{
	std::lock_guard<std::mutex> l(m_mx);
	const menu_cues *q = menu_cues_for(m_id);
	if (!q || !m_sounds || !m_ready) return;
	const std::vector<u8> &b = c == cursor ? q->cursor : c == confirm ? q->confirm : q->back;
	if (m_pending.size() < 8) m_pending.insert(m_pending.end(), b.begin(), b.end());
}

// The boards are only touched here. m_mx guards the wishes above and is held only for a
// moment; m_step is held for a whole step, so silence() can wait for one to end.
void menu_sound::run()
{
	std::vector<float> a;
	while (true)
	{
		std::string id, drop;
		bool music = false;
		float volume = 1;
		{
			std::unique_lock<std::mutex> l(m_mx);
			if (m_quit) return;
			drop.swap(m_drop);
			id = m_id;
			music = m_music;
			volume = m_volume;
			if (id.empty() && drop.empty()) { m_cv.wait_for(l, std::chrono::milliseconds(50)); continue; }
		}
		bool full = false;
		{
			std::lock_guard<std::mutex> step(m_step);
			if (!drop.empty()) m_boards.erase(drop);
			if (id.empty()) continue;
			const menu_cues *q = menu_cues_for(id);
			board &b = m_boards[id];
			if (!b.m)
			{
				std::unique_ptr<arcade_machine> m = m_make(id);
				if (!m)
				{
					std::lock_guard<std::mutex> l(m_mx);
					if (m_id == id) m_id.clear();       // no ROMs after all: silence
					m_boards.erase(id);
					continue;
				}
				m->set_sound_only(true);
				m->reset();
				std::vector<float> skip;
				for (int f = 0; f < q->warm_frames; f++) { m->run_frame(); m->take_audio(skip); skip.clear(); }
				b.m = std::move(m);
			}
			u8 send = 0;
			bool have = false;
			{
				std::lock_guard<std::mutex> l(m_mx);
				if (m_id != id) continue;                // changed while the board was made
				if (!m_ready)
				{
					m_ready = true;
					if (music && !b.music_sent) { m_pending.insert(m_pending.begin(), q->music.begin(), q->music.end()); b.music_sent = true; }
				}
				// keep about a tenth of a second queued
				full = m_queued() >= size_t(b.m->audio_rate() / 10);
				if (!full && !m_pending.empty()) { send = m_pending.front(); m_pending.erase(m_pending.begin()); have = true; }
			}
			if (!full)
			{
				if (have) b.m->sound_command(send);
				b.m->run_frame();
				a.clear();
				b.m->take_audio(a);
				for (float &v : a) v = std::clamp(v * volume * 1.6f, -1.0f, 1.0f);
				std::lock_guard<std::mutex> l(m_mx);
				if (m_id == id)                          // not silenced or changed during the frame
				{
					m_music_playing = music && b.music_sent;
					if (!a.empty()) m_push(a.data(), a.size());
				}
			}
		}
		if (full) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}
