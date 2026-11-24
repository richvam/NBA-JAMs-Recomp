// pad_script.h - the controller-only audit (roadmap U7.1): a script of controller presses
// for a virtual controller, so a scripted run can go through every flow of the program
// with the keyboard ignored. `NBAJamRecomp --pad-script <file>`.
//
// One command a line; # starts a comment:
//   wait <ms>                    nothing for that long
//   tap <button> [count]         press and let go (80 ms down, 250 ms after), count times
//   hold <button>[+<button>] <ms> hold buttons together
//   shot                         a screenshot of what is shown
//   expect <screen>              log PASS or FAIL: is the program on that screen now
//   log <text>                   a line in the log
//   drop <path>                  a file dropped on the window (${NAME} is the environment variable)
//   expect_ready <n>             log PASS or FAIL: are n games ready to play now
//   quit                         end the program (as closing the window does)
// Buttons: a b x y back guide start ls rs lb rb up down left right lt rt.
// No SDL in here: the app turns the actions into presses on the virtual controller.
#pragma once
#include <string>
#include <vector>

class pad_script
{
public:
	struct action
	{
		enum kind_t { none, press, release, shot, expect, log, quit, drop, expect_ready } kind = none;
		std::vector<int> buttons;           // press / release: SDL game controller buttons (lt, rt: 100, 101)
		std::string text;                   // expect: the screen's name; log: the text; drop: the path
		int n = 0;                          // expect_ready: the number of games
		int line = 0;
	};

	bool load(const std::string &text, std::string &err);
	// what to do at this time (ms since the script started); nothing while waiting
	std::vector<action> step(unsigned now_ms);
	bool done() const { return m_pc >= m_cmds.size() && m_wait_until == 0; }

	static int button(const std::string &name);   // -1 when unknown

private:
	struct cmd { std::string op; std::vector<int> buttons; int count = 1, ms = 0; std::string text; int line = 0; };
	std::vector<cmd> m_cmds;
	size_t m_pc = 0;
	unsigned m_wait_until = 0;
	int m_phase = 0, m_left = 0;            // a tap in progress: 1 down, 2 up; taps still to do
};
