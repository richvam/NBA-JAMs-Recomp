// audio_join.h - no click where the sound jumps (roadmap N1.2).
//
// After a save state is loaded the machine's sound goes on from the moment of the state,
// so the first new sample need not be near the last one played: a step the ear hears as a
// click. join() adds a short ramp that starts at the last sample played and fades into the
// new sound. It changes only what is played, never the machine (whose sound stays exactly
// what it was when the state was saved).
#pragma once
#include <cstddef>
#include <vector>

class audio_join
{
public:
	// every sample played (the last one is remembered)
	void played(const std::vector<float> &s) { if (!s.empty()) m_last = s.back(); }
	// the sound jumps here: the next samples passed to apply() fade in from the last played
	void jump(int ramp_samples = 96) { m_ramp = ramp_samples; m_pos = 0; m_from = m_last; m_have_from = false; }
	void apply(std::vector<float> &s)
	{
		for (size_t i = 0; i < s.size() && m_pos < m_ramp; i++, m_pos++)
		{
			if (!m_have_from) { m_offset = m_from - s[i]; m_have_from = true; }
			float k = 1.0f - float(m_pos) / float(m_ramp);
			s[i] += m_offset * k;
		}
		played(s);
	}
private:
	float m_last = 0, m_from = 0, m_offset = 0;
	int m_ramp = 0, m_pos = 0;
	bool m_have_from = false;
};
