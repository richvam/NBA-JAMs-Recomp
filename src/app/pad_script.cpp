// pad_script.cpp - see pad_script.h
#include "pad_script.h"
#include <cstdlib>
#include <sstream>

int pad_script::button(const std::string &n)
{
	// SDL_GameControllerButton values
	static const char *names[] = { "a", "b", "x", "y", "back", "guide", "start", "ls", "rs", "lb", "rb", "up", "down", "left", "right" };
	for (int i = 0; i < int(sizeof(names) / sizeof(names[0])); i++) if (n == names[i]) return i;
	if (n == "lt") return 100;
	if (n == "rt") return 101;
	return -1;
}

bool pad_script::load(const std::string &text, std::string &err)
{
	std::istringstream in(text);
	std::string l;
	int ln = 0;
	while (std::getline(in, l))
	{
		ln++;
		if (size_t h = l.find('#'); h != std::string::npos) l = l.substr(0, h);
		std::istringstream w(l);
		cmd c;
		c.line = ln;
		if (!(w >> c.op)) continue;
		auto buttons = [&](const std::string &spec)
		{
			size_t a = 0;
			while (a <= spec.size())
			{
				size_t b = spec.find('+', a);
				std::string one = spec.substr(a, b == std::string::npos ? std::string::npos : b - a);
				int k = button(one);
				if (k < 0) { err = "line " + std::to_string(ln) + ": unknown button " + one; return false; }
				c.buttons.push_back(k);
				if (b == std::string::npos) break;
				a = b + 1;
			}
			return true;
		};
		std::string a;
		if (c.op == "wait") { w >> c.ms; }
		else if (c.op == "tap") { w >> a; if (!buttons(a)) return false; if (!(w >> c.count)) c.count = 1; }
		else if (c.op == "hold") { w >> a; if (!buttons(a)) return false; w >> c.ms; }
		else if (c.op == "expect" || c.op == "log") { std::getline(w >> std::ws, c.text); }
		else if (c.op != "shot" && c.op != "quit") { err = "line " + std::to_string(ln) + ": unknown command " + c.op; return false; }
		m_cmds.push_back(c);
	}
	return true;
}

std::vector<pad_script::action> pad_script::step(unsigned now)
{
	std::vector<action> out;
	if (m_wait_until && now < m_wait_until) return out;
	m_wait_until = 0;
	while (m_pc < m_cmds.size())
	{
		const cmd &c = m_cmds[m_pc];
		action a;
		a.line = c.line;
		if (c.op == "tap" || c.op == "hold")
		{
			if (m_phase == 0) { m_phase = 1; m_left = c.op == "tap" ? c.count : 1; }
			if (m_phase == 1)
			{
				a.kind = action::press; a.buttons = c.buttons; out.push_back(a);
				m_phase = 2;
				m_wait_until = now + unsigned(c.op == "tap" ? 80 : c.ms);
				return out;
			}
			a.kind = action::release; a.buttons = c.buttons; out.push_back(a);
			m_wait_until = now + 250;
			if (--m_left > 0) { m_phase = 1; return out; }
			m_phase = 0;
			m_pc++;
			return out;
		}
		m_pc++;
		if (c.op == "wait") { m_wait_until = now + unsigned(c.ms); return out; }
		if (c.op == "shot") a.kind = action::shot;
		else if (c.op == "expect") { a.kind = action::expect; a.text = c.text; }
		else if (c.op == "log") { a.kind = action::log; a.text = c.text; }
		else if (c.op == "quit") a.kind = action::quit;
		out.push_back(a);
		if (a.kind == action::shot) { m_wait_until = now + 100; return out; }
	}
	return out;
}
