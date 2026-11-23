// frame_guard.h - keeps the picture shader fast enough for full speed (roadmap W5.3).
// Pure arithmetic: no SDL and no OpenGL, so it is tested without a display.
//
// The app gives it the GPU time of each game frame's picture (timer queries: the drawing only,
// so a screen refresh the driver waits for does not count). The game runs at about 54.7 frames
// a second, 18.3 ms a frame; the picture may take at most kBudgetMs of GPU time on average. Over each window of
// kWindow frames the guard takes the average; when it is over the budget twice in a row and a
// cheaper shader exists, it lowers the shader cap by one step (Curved CRT -> CRT -> Scanlines ->
// Smooth -> Sharp) and says so. The cap is for this window size only: a resize, or the player
// choosing a shader (F4 or Settings), starts again from their choice.
#pragma once
#include <algorithm>

struct frame_guard
{
	static constexpr double kBudgetMs = 12.0;
	static constexpr int kWindow = 120;

	int cap = 99;               // the most expensive shader index allowed now (99: no limit)
	int win_w = 0, win_h = 0;   // the size the cap was found at
	double sum = 0, max_ms = 0;
	int frames = 0, slow_windows = 0;
	double last_avg = 0, last_max = 0;   // the last finished window, for the log

	void reset() { cap = 99; sum = 0; max_ms = 0; frames = 0; slow_windows = 0; }

	// the shader to draw with: the player's choice, at most the cap
	int shader(int chosen) const { return std::min(chosen, cap); }

	// one game frame took `ms` at window size w x h with shader `used` (already capped).
	// Returns true when a window finished (last_avg / last_max are new); `stepped` is set when
	// the cap went down.
	bool add(double ms, int w, int h, int used, bool &stepped)
	{
		stepped = false;
		if (w != win_w || h != win_h) { reset(); win_w = w; win_h = h; }
		sum += ms;
		max_ms = std::max(max_ms, ms);
		if (++frames < kWindow) return false;
		last_avg = sum / frames;
		last_max = max_ms;
		sum = 0; max_ms = 0; frames = 0;
		if (last_avg > kBudgetMs && used > 0)
		{
			if (++slow_windows >= 2)
			{
				cap = used - 1;
				slow_windows = 0;
				stepped = true;
			}
		}
		else
			slow_windows = 0;
		return true;
	}
};
