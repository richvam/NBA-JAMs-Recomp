// main.cpp - NBA JAM Recomp: one application for the whole NBA JAM saga.
// Game picker at start-up, SDL2 video/audio/input, per-game NVRAM.
#include "version.h"
#include "paths.h"
#include "news.h"
#include "overlay.h"
#include "games.h"
#include "ui/canvas.h"
#include "ui/gallery.h"
#include "ui/art_view.h"
#include "ui/themes.h"
#include "ui/screens.h"
#include "ui/keyboard.h"
#include "menu_sound.h"
#include "pad_script.h"
#include "ui/ui_sdl.h"
#include "miniz.h"
#include "gl_present.h"
#include "netplay.h"
#include "nat.h"
#include "mods.h"
#include "state_file.h"
#include "audio_join.h"
#include "mod_draw.h"
#include "controls.h"
#include "shaders.h"
#include "wide_view.h"
#include "frame_guard.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <array>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <ctime>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#endif

// ---------------------------------------------------------------------------
// configuration (nbajam_recomp.ini next to the executable)
// ---------------------------------------------------------------------------
struct key_map
{
	SDL_Scancode up, down, left, right, shoot, pass, turbo, start, coin;
};


#ifdef _WIN32
// A crash on Windows writes crashes/NBAJamRecomp-<time>.dmp (a minidump: the threads' stacks
// and the modules) and crashes/NBAJamRecomp-<time>.txt (the exception, where it happened, the
// version) into the data folder, so a crash that cannot be repeated can still be looked at.
// dbghelp.dll is loaded only then; nothing else is done in a crashed process than writing.
static char g_crash_dir[MAX_PATH * 2];
static LONG WINAPI write_crash_dump(EXCEPTION_POINTERS *ep)
{
	if (!g_crash_dir[0]) return EXCEPTION_CONTINUE_SEARCH;
	CreateDirectoryA(g_crash_dir, nullptr);
	SYSTEMTIME t;
	GetLocalTime(&t);
	char base[MAX_PATH * 2 + 64];
	snprintf(base, sizeof base, "%s\\NBAJamRecomp-%04u%02u%02u-%02u%02u%02u", g_crash_dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	// where: the module the faulting address is in, and the offset into it
	void *at = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
	char mod[MAX_PATH] = "?";
	HMODULE hm = nullptr;
	if (at && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)at, &hm))
		GetModuleFileNameA(hm, mod, sizeof mod);
	char txt[sizeof base + 8];
	snprintf(txt, sizeof txt, "%s.txt", base);
	if (FILE *f = fopen(txt, "w"))
	{
		fprintf(f, "NBA JAM Recomp %s crashed\nexception 0x%08lX at %p\nmodule %s + 0x%llx\nthread %lu\n", app_version(),
			ep && ep->ExceptionRecord ? (unsigned long)ep->ExceptionRecord->ExceptionCode : 0ul, at, mod,
			(unsigned long long)((const char *)at - (const char *)hm), (unsigned long)GetCurrentThreadId());
		fclose(f);
	}
	if (HMODULE dh = LoadLibraryA("dbghelp.dll"))
	{
		using write_fn = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
		auto fn = reinterpret_cast<write_fn>(reinterpret_cast<void *>(GetProcAddress(dh, "MiniDumpWriteDump")));
		char dmp[sizeof base + 8];
		snprintf(dmp, sizeof dmp, "%s.dmp", base);
		HANDLE h = CreateFileA(dmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (fn && h != INVALID_HANDLE_VALUE)
		{
			MINIDUMP_EXCEPTION_INFORMATION mi{ GetCurrentThreadId(), ep, FALSE };
			fn(GetCurrentProcess(), GetCurrentProcessId(), h, MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules), ep ? &mi : nullptr, nullptr, nullptr);
		}
		if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
	}
	return EXCEPTION_CONTINUE_SEARCH;   // Windows still reports it as before
}
static void install_crash_dumps(const std::string &data_dir)
{
	snprintf(g_crash_dir, sizeof g_crash_dir, "%s\\crashes", data_dir.c_str());
	SetUnhandledExceptionFilter(write_crash_dump);
}
#else
static void install_crash_dumps(const std::string &) {}
#endif

struct config
{
	int scale = 0;                // window size in multiples of 400x300; 0 = from the screen size
	bool fullscreen = false;
	bool tv_margins = false;      // the menus inside a TV's safe area: 5 % in from each edge (U7.2)
	bool smooth = false;          // legacy (SDL renderer fallback)
	bool scanlines = false;       // legacy
	int shader = 3;               // 0 sharp, 1 smooth, 2 scanlines, 3 CRT, 4 CRT curved
	view_scaling scaling = view_scaling::fit;     // F5: integer, fit, smooth (view.h)
	float scan_strength = 0.6f;
	bool frame_log = false;       // frame_time_log=1: one line per 120 game frames in frame_times.log (W5.3)
	int max_wide = 0;             // the widest true widescreen view (W5.2): 0 as wide as the window, 1 16:9, 2 21:9
	int side_fill = 0;            // F8: around a 4:3 picture on a wider screen: 0 black, 1 blurred picture, 2 an overlay
	std::string overlay;          // name of the chosen overlay (overlays/ in the data folder)
	float overlay_opacity = 1.0f; // of overlays with a transparent opening
	view_shape shape = view_shape::original;      // F7: original 4:3, widescreen, stretch (view.h)
	bool use_opengl = true;
	float volume = 0.8f;
	bool news = true;             // ask the Releases feed for the latest version at start (R2.3)
	bool save_states = false;     // N1.4: Save and Load in the pause menu, offline only
	int net_port = 7845;
	int net_delay = 4;
	std::string net_host;
	std::vector<std::string> net_recent;   // hosts joined before, the last one first (U4.2)
	bool net_upnp = true;         // N2.1: ask the router to forward the port when hosting
	bool net_stun = true;         // N2.2: ask a STUN server for this computer's public address (its code)
	std::string net_stun_server = "stun.l.google.com:19302";
	// which cabinet player each controller plays as, by the controller's GUID (missing: its
	// place in the connection order, as before). Lets two controllers sit on players 1 and 3
	// for co-op on one side, or 1 and 2 for head to head.
	std::map<std::string, int> pad_slot;
	int pause_buttons = 0;        // held on a controller to open the pause menu: 0 Start (1 s), 1 Start + Back, 2 L3 + R3 (U5.1)
	bool menu_music = true, menu_sounds = true;   // U6: from the highlighted game's sound board
	float menu_volume = 0.6f;
	std::string pad_profile[4] = { "Default", "Default", "Default", "Default" };   // controller profile of each player
	std::map<std::string, std::array<std::string, 4>> game_profiles;   // per-game override ("" = use pad_profile)
	dip_settings dips;
	key_map keys[4] = {
		{ SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_1, SDL_SCANCODE_5 },
		{ SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_V, SDL_SCANCODE_2, SDL_SCANCODE_6 },
		{ SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_3, SDL_SCANCODE_7 },
		{ SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L, SDL_SCANCODE_H, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_U, SDL_SCANCODE_4, SDL_SCANCODE_8 },
	};
};

static std::string trim(std::string s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static const char *kNames[9] = { "up", "down", "left", "right", "shoot", "pass", "turbo", "start", "coin" };

static SDL_Scancode *key_slot(key_map &k, int i)
{
	SDL_Scancode *s[9] = { &k.up, &k.down, &k.left, &k.right, &k.shoot, &k.pass, &k.turbo, &k.start, &k.coin };
	return s[i];
}

static void save_config(const std::string &path, config &c)
{
	std::ofstream f(path);
	f << "# NBA JAM Recomp - configuration\n[general]\n# news=1 asks github.com once at start for the latest release (only to tell you; nothing is downloaded)\nnews=" << c.news << "\n# pause_button: held on a controller for the pause menu (Esc and Guide always work): start (one second; a quick press still goes to the game), start+back or l3+r3 (half a second)\npause_button=" << (c.pause_buttons == 2 ? "l3+r3" : c.pause_buttons == 1 ? "start+back" : "start") << "\n# save_states=1 adds Save and Load (three slots per game) to the pause menu of an offline game\nsave_states=" << c.save_states << "\n# the menus play the highlighted game's own music and sounds, from its ROMs\nmenu_music=" << c.menu_music << "\nmenu_sounds=" << c.menu_sounds << "\nmenu_volume=" << c.menu_volume << "\n\n[video]\n# scale: size of the window in multiples of 400x300; 0 = chosen from the screen size\nscale=" << c.scale << "\nfullscreen=" << c.fullscreen << "\n# tv_margins=1 keeps the menus 5% in from each edge, for TVs that cut the edges off\ntv_margins=" << c.tv_margins
	  << "\n# shader: 0 sharp, 1 smooth, 2 scanlines, 3 CRT, 4 curved CRT\nshader=" << c.shader
	  << "\n# view: original (4:3), widescreen (more of the court in play; all five games) or stretch (fills the window, distorted).\nview=" << view_shape_id(c.shape)
	  << "\n# scaling: integer (whole multiples of the game's lines), fit (fills the height, crisp pixels), smooth (fills the height, filtered)\nscaling=" << view_scaling_id(c.scaling) << "\n# frame_time_log=1 writes how long each 120 game frames took to draw to frame_times.log in the data folder\nframe_time_log=" << c.frame_log << "\n# max_view_width: the widest the Widescreen view gets: widest (as wide as the window, up to 1000 columns), 16:9 or 21:9; a wider window gets sides\nmax_view_width=" << (c.max_wide == 1 ? "16:9" : c.max_wide == 2 ? "21:9" : "widest") << "\n# side_fill: around a 4:3 picture on a wider screen: black, blur (the picture, blurred and dimmed), overlay (an image from overlays/ in the data folder)\nside_fill=" << (c.side_fill == 1 ? "blur" : c.side_fill == 2 ? "overlay" : "black") << "\noverlay=" << c.overlay << "\n# overlay_opacity: 0.1 to 1, for overlays with a transparent opening\noverlay_opacity=" << c.overlay_opacity << "\nscanline_strength=" << c.scan_strength
	  << "\n# opengl=0 uses the basic SDL renderer (no shaders)\nopengl=" << c.use_opengl
	  << "\n\n[audio]\nvolume=" << c.volume
	  << "\n\n[network]\nport=" << c.net_port << "\ninput_delay=" << c.net_delay << "\nupnp=" << c.net_upnp << "\nstun=" << c.net_stun << "\nstun_server=" << c.net_stun_server << "\nlast_host=" << c.net_host << "\nrecent_hosts=" << [&] { std::string r; for (auto &h : c.net_recent) r += (r.empty() ? "" : ",") + h; return r; }()
	  << "\n\n[dips]\nfree_play=" << c.dips.free_play << "\nfour_players=" << c.dips.four_players
	  << "\ncountry=" << c.dips.country << "\nvideo_clips=" << c.dips.video_clips
	  << "\n\n[controllers]\n# profile of each player's pad (files in controller_profiles/, edit them in the Controls menu)\n";
	for (int p = 0; p < 4; p++) f << "p" << (p + 1) << "_profile=" << c.pad_profile[p] << "\n";
	f << "# plays_as=<controller GUID>:<1-4>: the cabinet player a controller plays as (Controls, \"Plays as\")\n";
	for (auto &ps : c.pad_slot) f << "plays_as=" << ps.first << ":" << (ps.second + 1) << "\n";
	f << "# per-game profiles: <game>.p1_profile=... (games: nbajam nbajamte nbajamse nbahangt nbamht)\n";
	for (auto &g : c.game_profiles)
		for (int p = 0; p < 4; p++)
			if (!g.second[size_t(p)].empty()) f << g.first << ".p" << (p + 1) << "_profile=" << g.second[size_t(p)] << "\n";
	f << "\n[keys]\n"
	  << "# SDL key names (https://wiki.libsdl.org/SDL2/SDL_Scancode)\n";
	for (int p = 0; p < 4; p++)
		for (int i = 0; i < 9; i++)
			f << "p" << (p + 1) << "_" << kNames[i] << "=" << SDL_GetScancodeName(*key_slot(c.keys[p], i)) << "\n";
}

static void load_config(const std::string &path, config &c)
{
	std::ifstream f(path);
	if (!f) { save_config(path, c); return; }
	std::string line;
	bool have_view = false;
	int old_aspect = -1;
	while (std::getline(f, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == '[') continue;
		auto eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
		if (k == "language") continue;       // from older versions: the program is English only for now
		else if (k == "news") c.news = atoi(v.c_str()) != 0;
		else if (k == "save_states") c.save_states = atoi(v.c_str()) != 0;
		else if (k == "pause_button") c.pause_buttons = v == "l3+r3" ? 2 : v == "start+back" ? 1 : 0;
		else if (k == "pause_buttons") c.pause_buttons = v == "l3+r3" ? 2 : 0;   // before: start+back (the default then, which opens a Windows menu) becomes Start
		else if (k == "menu_music") c.menu_music = atoi(v.c_str()) != 0;
		else if (k == "menu_sounds") c.menu_sounds = atoi(v.c_str()) != 0;
		else if (k == "menu_volume") c.menu_volume = std::clamp(float(atof(v.c_str())), 0.0f, 1.0f);
		else if (k == "scale") c.scale = std::clamp(atoi(v.c_str()), 0, 8);
		else if (k == "fullscreen") c.fullscreen = atoi(v.c_str()) != 0;
		else if (k == "tv_margins") c.tv_margins = atoi(v.c_str()) != 0;
		else if (k == "frame_time_log") c.frame_log = atoi(v.c_str()) != 0;
		else if (k == "max_view_width") c.max_wide = v == "16:9" ? 1 : v == "21:9" ? 2 : 0;
		else if (k == "smooth") c.smooth = atoi(v.c_str()) != 0;
		else if (k == "scanlines") c.scanlines = atoi(v.c_str()) != 0;
		else if (k == "shader") c.shader = std::clamp(atoi(v.c_str()), 0, kNumShaders - 1);
		else if (k == "view") { view_shape_from_id(v.c_str(), c.shape); have_view = true; }
		else if (k == "scaling") view_scaling_from_id(v.c_str(), c.scaling);
		else if (k == "side_fill") { c.side_fill = v == "blur" ? 1 : (v == "overlay" || v == "bezel") ? 2 : 0; if (v == "bezel" && c.overlay.empty()) c.overlay = "bezel"; }
		else if (k == "overlay") c.overlay = v;
		else if (k == "overlay_opacity") c.overlay_opacity = std::clamp(float(atof(v.c_str())), 0.1f, 1.0f);
		// settings of versions before W1. aspect= becomes the nearest new shape unless view= is there.
		// integer_scale= is not carried over: it was the old default, and Fit is the new one.
		else if (k == "aspect") old_aspect = std::clamp(atoi(v.c_str()), 0, 3);
		else if (k == "integer_scale") continue;
		else if (k == "scanline_strength") c.scan_strength = std::clamp(float(atof(v.c_str())), 0.0f, 1.0f);
		else if (k == "opengl") c.use_opengl = atoi(v.c_str()) != 0;
		else if (k == "port") c.net_port = std::clamp(atoi(v.c_str()), 1024, 65535);
		else if (k == "input_delay") c.net_delay = std::clamp(atoi(v.c_str()), 1, 10);
		else if (k == "last_host") c.net_host = v;
		else if (k == "upnp") c.net_upnp = atoi(v.c_str()) != 0;
		else if (k == "stun") c.net_stun = atoi(v.c_str()) != 0;
		else if (k == "stun_server") c.net_stun_server = v;
		else if (k == "recent_hosts")
		{
			c.net_recent.clear();
			size_t a = 0;
			while (a <= v.size() && c.net_recent.size() < 5)
			{
				size_t b = v.find(',', a);
				std::string h = trim(v.substr(a, b == std::string::npos ? std::string::npos : b - a));
				if (!h.empty()) c.net_recent.push_back(h);
				if (b == std::string::npos) break;
				a = b + 1;
			}
		}
		else if (k == "plays_as")
		{
			size_t const colon = v.rfind(':');
			if (colon != std::string::npos && colon > 0)
			{
				int const n = atoi(v.c_str() + colon + 1);
				if (n >= 1 && n <= 4) c.pad_slot[v.substr(0, colon)] = n - 1;
			}
		}
		else if (k == "volume") c.volume = std::clamp(float(atof(v.c_str())), 0.0f, 1.0f);
		else if (k == "free_play") c.dips.free_play = atoi(v.c_str()) != 0;
		else if (k == "four_players") c.dips.four_players = atoi(v.c_str()) != 0;
		else if (k == "country") c.dips.country = std::clamp(atoi(v.c_str()), 0, 2);
		else if (k == "video_clips") c.dips.video_clips = atoi(v.c_str()) != 0;
		else if (k.size() > 11 && k.substr(k.size() - 11, 2) == ".p" && k.substr(k.size() - 8) == "_profile" && k[k.size() - 9] >= '1' && k[k.size() - 9] <= '4')
		{
			if (valid_profile_name(v)) c.game_profiles[k.substr(0, k.size() - 11)][size_t(k[k.size() - 9] - '1')] = v;
		}
		else if (k.size() == 10 && k[0] == 'p' && k.substr(2) == "_profile" && k[1] >= '1' && k[1] <= '4') { if (valid_profile_name(v)) c.pad_profile[k[1] - '1'] = v; }
		else if (k.size() > 3 && k[0] == 'p' && k[2] == '_')
		{
			int p = k[1] - '1';
			if (p < 0 || p > 3) continue;
			for (int i = 0; i < 9; i++)
				if (k.substr(3) == kNames[i])
				{
					SDL_Scancode sc = SDL_GetScancodeFromName(v.c_str());
					if (sc != SDL_SCANCODE_UNKNOWN) *key_slot(c.keys[p], i) = sc;
				}
		}
	}
	if (!have_view && old_aspect >= 0) c.shape = view_shape_from_old_aspect(old_aspect);
}

// ---------------------------------------------------------------------------
// small software text renderer
// ---------------------------------------------------------------------------
static const char *shader_name(int i)
{
	static const char *en[] = { "Sharp (pixel perfect)", "Smooth", "Scanlines", "CRT", "Curved CRT" };
	i = std::clamp(i, 0, kNumShaders - 1);
	return en[i];
}

// ---------------------------------------------------------------------------
// application
// ---------------------------------------------------------------------------
class app
{
public:
	int run();
	void show_gallery() { m_gallery = true; }      // --ui-gallery: the widget gallery instead of the menu
	void show_rom_art() { m_rom_art = true; }      // --rom-art: what the art reader finds in the ROMs
private:
	bool m_gallery = false, m_rom_art = false;
public:
	// --pad-script <file>: the controller-only audit (U7.1). A virtual controller follows the
	// script and the keyboard is ignored.
	void use_pad_script(const std::string &path) { m_script_path = path; }
private:
	std::string m_script_path;
	pad_script m_script;
	bool m_scripted = false;
	SDL_Joystick *m_vpad = nullptr;
	unsigned m_script_t0 = 0;
	int m_script_pass = 0, m_script_fail = 0;
	bool start_pad_script();
	void script_tick();
	const char *screen_name() const;
	void gallery_loop();
	void rom_art_loop();
	enum class screen { menu, settings, net_menu, net_lobby, game, about, mods, mod_detail, controls, news, pause };

	void open_window();
	void present(const u32 *pixels, int w, int h, bool game);
	void menu_frame();
	void settings_frame();
	// the screens rebuilt on the menu toolkit (roadmap U3): the game picker and Settings
	ui::input m_ui_in;
	ui::screen m_picker, m_settings, m_mods_scr, m_ctl_scr, m_online_scr, m_lobby_scr, m_about_scr, m_news_scr;
	// menu music and sounds (U6)
	std::unique_ptr<menu_sound> m_msound;
	bool ui_next(ui::event &ev);       // the next menu event, with its sound
	// the pause menu (U5)
	ui::screen m_pause_scr;
	screen m_back_to = screen::menu;   // where Settings, Controls, Lua mods and About return: the picker, or the pause menu
	int m_coin_pulse[4] = { 0, 0, 0, 0 }, m_service_pulse = 0;   // coins from the menu: 8 frames each, the coin held for 4 of them
	unsigned m_combo_since = 0;
	static constexpr unsigned kStartHoldMs = 1000;   // Start held this long opens the pause menu
	unsigned m_start_since[4] = {};    // when Start went down on each controller (0: up)
	bool m_start_used[4] = {};         // that press opened the pause menu: nothing for the game
	int m_start_pulse[4] = {};         // frames of Start still to give the game after a quick press
	bool m_combo_wait = false;         // the shortcut acted: wait for its buttons to be let go
	bool m_menu_guide = false;
	bool pause_held(SDL_GameController *c);
	bool pause_shortcut();
	void open_pause();
	void resume_game();
	bool pause_frame();
	bool step_game(bool menu_open);
	cabinet_inputs menu_inputs();
	void add_pulses(cabinet_inputs &in);
	int m_edit_what = 0;               // online: 1 the address is being typed, 2 the port
	bool m_edit_join = false;          // join as soon as the address is typed
	int m_about_player = 0;
	bool m_news_shown = false;         // the News screen was built with a release to show
	void ui_show(ui::screen &s);
	ui::about_state about_now();
	ui::mods_state mods_now();
	view_rect m_ui_shown;                 // where the menu picture is in the window, for the mouse
	int m_ui_ww = 1, m_ui_wh = 1;
	void ui_frame_begin();
	ui::picker_state picker_now();
	ui::settings_state settings_now();
	void apply_settings(const ui::settings_state &want);
	void open_online();
	// the theme follows the highlighted game, with a cross-fade (U2.3)
	std::map<std::string, rom_art::art_set> m_arts;
	ui::theme m_theme, m_theme_from, m_theme_to;
	int m_theme_w = 0;
	std::string m_theme_game;
	unsigned m_theme_start = 0;
	bool m_theme_fading = false;
	const rom_art::art_set *art_of(const std::string &game_id);
	void follow_theme(const std::string &game_id, unsigned now);
	void net_menu_frame();
	void net_lobby_frame();
	void about_frame();
	void mods_frame();
	void controls_frame();
	void apply_setup(const import_result &r);
	bool game_frame();                 // returns true when an emulated frame was run
	bool start_game(int index, bool net_client = false);
	void stop_game();
	void poll_events();
	cabinet_inputs gather_inputs();
	void draw_overlay(canvas &cv);
	void draw_background(canvas &cv);
	// where the picture goes: the game uses the player's settings, menus are always 4:3 and fitted
	view_rect view_for(int win_w, int win_h, int w, int h, bool game) const
	{
		if (game) return compute_view(win_w, win_h, w, h, m_cfg.shape, m_cfg.scaling, m_presenting_wide);
		return menu_view(win_w, win_h, w, h, m_cfg.tv_margins);
	}
	void say_view(bool scaling_changed);
	// overlays / borders (overlay.h)
	std::vector<overlay_entry> m_overlays;
	bool use_overlay(const std::string &name);   // loads it into the presenter; false if it is not there or unreadable
	void next_side_fill();
	void save_screenshot(const u32 *px, int w, int h);
	std::string net_mod_names(const std::string &game_id);
	void refresh_availability();
	// R3.2: ROMs dropped on the window are copied into the ROM folder, then looked for again
	bool rescan_roms(unsigned now);                 // false in a game (the catalog is in use)
	int copy_into_roms(const std::string &from, std::string &err);   // files copied; never overwrites
	int m_dropped = 0;                              // files copied by the drop in progress
	std::string m_drop_err;
	void open_rom_folder();
	void message(const std::string &m, u32 ms = 2500) { m_message = m; m_message_until = SDL_GetTicks() + ms; }
	std::string nv_path(const game_info &g) const { return path_join(path_join(m_base, "saves"), std::string(g.id) + ".nv"); }

	std::string m_base, m_cfgpath;      // m_base = the user's data folder (paths.h)
	app_paths m_paths;
	// release news (R2.3)
	news_feed m_news;
	news_item m_news_item;          // latest published release, once known
	bool m_news_new = false;        // ...and it is newer than this program
	void poll_news();
	void news_frame();
	void choose_data_folder();
	config m_cfg;
	SDL_Window *m_win = nullptr;
	SDL_Renderer *m_ren = nullptr;
	SDL_Texture *m_tex = nullptr;
	gl_presenter m_gl;
	int m_texw = 0, m_texh = 0;
	SDL_AudioDeviceID m_audio = 0;
	std::vector<SDL_GameController *> m_pads;
	profile_store m_profiles;
	int m_ctl_player = 0, m_ctl_sel = 0, m_ctl_game = -1, m_ctl_scroll = 0;   // m_ctl_game: -1 = all games
	// profile a player's pad uses in a game (-1 = the all-games choice)
	std::string &profile_slot(int game, int player)
	{
		if (game < 0) return m_cfg.pad_profile[player];
		return m_cfg.game_profiles[game_list()[size_t(game)].id][size_t(player)];
	}
	static std::string pad_guid(SDL_GameController *c)
	{
		char g[64] = {};
		SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(c)), g, sizeof g);
		return g;
	}
	// the cabinet player controller i plays as (Controls, "Plays as"); online the host gives
	// each computer its seat, and the first controller plays it
	int pad_player(size_t i)
	{
		if (m_net_game || i >= m_pads.size()) return int(i);
		auto it = m_cfg.pad_slot.find(pad_guid(m_pads[i]));
		return it == m_cfg.pad_slot.end() ? int(i) : std::clamp(it->second, 0, 3);
	}
	const pad_profile &active_profile(int player)
	{
		const std::string *n = &m_cfg.pad_profile[player];
		if (m_cur_game >= 0)
		{
			auto it = m_cfg.game_profiles.find(game_list()[size_t(m_cur_game)].id);
			if (it != m_cfg.game_profiles.end() && !it->second[size_t(player)].empty() && m_profiles.find(it->second[size_t(player)]) >= 0) n = &it->second[size_t(player)];
		}
		return m_profiles.get(*n);
	}
	int m_capture = -1;                // action waiting for a pad press, -1 = none
	u32 m_capture_until = 0;
	bool m_captured = false;
	pad_input m_capture_in;
	bool m_menu_clear = false;
	std::unique_ptr<game_catalog> m_cat;
	std::unique_ptr<arcade_machine> m_mach;
	// true widescreen (roadmap W3): the wide gameplay picture of the running game, when it has
	// one (Tournament Edition 4.0 and Special Edition) and the shape is Widescreen
	std::unique_ptr<wide_view> m_wide;
	frame_guard m_guard;               // steps the shader down when drawing is too slow (W5.3)
	void guard_frame(double ms, int ww, int wh, int used);
	bool m_wide_possible = false;      // the running game can draw it (its symbols are there)
	bool m_presenting_wide = false;    // the picture being presented is the wide one
	int wide_columns() const;          // the view width that fills the window's shape
	void update_wide();                // makes, remakes or drops m_wide to match shape and window
	bool wide_symbols_of(wide_symbols &ws);
	screen m_screen = screen::menu;
	int m_cur_game = -1;
	int m_sel = 0;
	bool m_quit = false, m_paused = false, m_show_help = false, m_test = false, m_shot = false;
	std::string m_message;
	u32 m_message_until = 0;
	std::vector<bool> m_avail;
	std::vector<std::string> m_why;
	bool m_menu_up = false, m_menu_down = false, m_menu_left = false, m_menu_right = false, m_menu_ok = false, m_menu_back = false, m_menu_net = false;
	std::vector<float> m_abuf;
	audio_join m_join;                 // no click where a loaded state's sound starts (N1.2)
	// save states (roadmap N1.4): offline, three slots per game in states/<game>/
	int m_state_slot = 0;
	std::string m_slot_info[3];        // what is in each slot of this game ("" empty)
	std::string slot_path(int n) const;
	void read_slots();
	void save_slot(int n);
	bool load_slot(int n);

	// Lua mods
	mod_manager m_mods;
	int m_mod_sel = 0, m_mod_row = 0;

	// netplay
	netplay m_net;
	upnp_mapper m_upnp;                // N2.1: the router's port forward while hosting
	u32 m_join_t = 0;                  // when joining started (the hint after a few seconds)
	void net_traversal_start();
	std::string net_code_note(std::string &code);
	std::string note_v4(std::string &code, bool host);
	net_addr m_code4;                         // the IPv4 part of the code last shown
	bool m_editing = false;            // text is being typed: on the on-screen keyboard or a physical one (U4.1)
	ui::keyboard m_kbd;
	std::string &m_edit = m_kbd.text;
	void kbd_open(ui::layout l, const std::string &title, const std::string &start, size_t max_len);
	int kbd_step();                    // 0 still typing, 1 OK, 2 cancelled
	bool m_net_game = false;           // current game runs under netplay
	std::vector<std::string> m_local_ips;
	net_addr m_local6;                        // this computer's public IPv6 address, if it has one (N2.2)
};

bool app::rescan_roms(unsigned)
{
	if (m_mach) return false;
	if (m_msound) m_msound->silence();       // not while its worker reads a game
	m_cat->rescan();
	refresh_availability();
	m_arts.clear();
	m_theme_game.clear();
	return true;
}

// Copies a dropped file, or every file under a dropped folder, into the ROM folder. A file
// already there with the same name and size is skipped; another with the same name is kept
// and the new one gets a number. ROMs are recognised by their contents, so names do not matter.
int app::copy_into_roms(const std::string &from, std::string &err)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	auto utf8 = [](const fs::path &p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); };
	const fs::path dst = fs::u8path(path_join(m_base, "roms"));
	fs::create_directories(dst, ec);
	const fs::path src = fs::u8path(from);
	if (fs::equivalent(src.parent_path(), dst, ec)) return 0;   // already there
	std::vector<std::pair<fs::path, fs::path>> todo;           // source, name inside roms/
	if (fs::is_directory(src, ec))
	{
		for (auto it = fs::recursive_directory_iterator(src, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
			if (it->is_regular_file(ec)) todo.push_back({ it->path(), fs::relative(it->path(), src.parent_path(), ec) });
	}
	else if (fs::is_regular_file(src, ec)) todo.push_back({ src, src.filename() });
	else { err = "cannot read " + utf8(src.filename()); return 0; }
	int n = 0;
	for (auto &[s, rel] : todo)
	{
		fs::path d = dst / rel;
		fs::create_directories(d.parent_path(), ec);
		if (fs::exists(d, ec))
		{
			if (fs::file_size(d, ec) == fs::file_size(s, ec)) continue;
			for (int k = 2; fs::exists(d, ec); k++) d = dst / rel.parent_path() / fs::u8path(utf8(rel.stem()) + " (" + std::to_string(k) + ")" + utf8(rel.extension()));
		}
		if (fs::copy_file(s, d, fs::copy_options::none, ec)) n++;
		else err = "could not copy " + utf8(s.filename());
	}
	SDL_Log("dropped %s: %d copied into %s", from.c_str(), n, utf8(dst).c_str());
	return n;
}

void app::open_rom_folder()
{
	const std::string dir = path_join(m_base, "roms");
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
	if (m_scripted) { SDL_Log("open folder: %s", dir.c_str()); message("Opened " + dir, 3000); return; }   // no file manager in a scripted run
	std::string url = "file:///";
	for (char c : dir) url += c == '\\' ? '/' : c;
	if (SDL_OpenURL(url.c_str()) == 0) message("Opened " + dir, 3000);
	else message("Could not open " + dir + ": put your ROMs there", 6000);
}

void app::refresh_availability()
{
	m_avail.clear();
	m_why.clear();
	for (const auto &g : game_list())
	{
		std::string why;
		m_avail.push_back(m_cat->available(g, why));
		m_why.push_back(why);
	}
}

void app::open_window()
{
	// window size: the saved multiple, or (0) about three quarters of the screen height, so the
	// window is not tiny on a 4K screen (W1.4)
	int scale = m_cfg.scale;
	if (scale <= 0)
	{
		SDL_DisplayMode dm{};
		scale = SDL_GetDesktopDisplayMode(0, &dm) == 0 ? std::clamp(dm.h * 3 / 4 / 300, 2, 8) : 3;
	}
	int w = 400 * scale, h = 300 * scale;
	Uint32 fs = m_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
	if (m_cfg.use_opengl)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		m_win = SDL_CreateWindow("NBA JAM Recomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | fs);
		std::string err;
		if (m_win && m_gl.init(m_win, err)) return;
		if (m_win) SDL_DestroyWindow(m_win);
		m_win = nullptr;
		message("OpenGL not available: no shaders (" + err.substr(0, 60) + ")", 8000);
		SDL_Log("OpenGL: %s", err.c_str());
	}
	m_win = SDL_CreateWindow("NBA JAM Recomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE | fs);
	m_ren = SDL_CreateRenderer(m_win, -1, SDL_RENDERER_ACCELERATED);
	if (!m_ren) m_ren = SDL_CreateRenderer(m_win, -1, SDL_RENDERER_SOFTWARE);
}

// Message after F5 / F7: the two settings, and what they mean on this window. Integer
// scaling never leaves a large part of the screen empty without saying so.
void app::say_view(bool scaling_changed)
{
	int ww = 0, wh = 0;
	if (m_gl.ok()) m_gl.drawable_size(ww, wh);
	else if (m_ren) SDL_GetRendererOutputSize(m_ren, &ww, &wh);
	view_rect r = compute_view(ww, wh, 400, 254, m_cfg.shape, m_cfg.scaling);
	// one line of at most 49 characters fits over the game picture
	std::string s;
	if (scaling_changed)
	{
		s = std::string("Scaling: ") + view_scaling_name(m_cfg.scaling);
		if (m_cfg.scaling == view_scaling::integer && r.unused > kIntegerUnusedWarning)
			s += " (" + std::to_string(int(r.unused * 100 + 0.5f)) + "% of the screen unused)";
		else if (m_cfg.scaling == view_scaling::integer) s += " (whole-number size)";
		else if (m_cfg.scaling == view_scaling::fit) s += " (fills the height, crisp)";
		else s += " (fills the height, filtered)";
	}
	else
	{
		s = std::string("Shape: ") + view_shape_name(m_cfg.shape);
		if (m_cfg.shape == view_shape::stretch) s += " (picture is distorted)";
	}
	message(s, 4000);
}

bool app::use_overlay(const std::string &name)
{
	if (!m_gl.ok()) return false;
	for (const auto &e : m_overlays)
		if (e.name == name)
		{
			std::vector<u8> rgba;
			int w = 0, h = 0;
			std::string err;
			if (!load_overlay(e, rgba, w, h, err)) { SDL_Log("overlay %s: %s", e.name.c_str(), err.c_str()); return false; }
			m_gl.set_overlay(rgba.data(), w, h);
			return true;
		}
	return false;
}

// F8: black -> blurred picture -> each overlay in overlays/ -> black. An overlay that cannot
// be read is skipped with a message in the log.
void app::next_side_fill()
{
	if (!m_gl.ok()) { m_cfg.side_fill = 0; message("Sides: need OpenGL (black)", 4000); return; }
	m_overlays = list_overlays(m_base);          // pick up files added while the program runs
	size_t at = m_overlays.size();
	if (m_cfg.side_fill == 2)
		for (size_t i = 0; i < m_overlays.size(); i++) if (m_overlays[i].name == m_cfg.overlay) at = i;
	if (m_cfg.side_fill == 0) { m_cfg.side_fill = 1; message("Sides: blurred picture", 3000); return; }
	for (size_t i = m_cfg.side_fill == 1 ? 0 : at + 1; i < m_overlays.size(); i++)
		if (use_overlay(m_overlays[i].name))
		{
			m_cfg.side_fill = 2;
			m_cfg.overlay = m_overlays[i].name;
			message("Overlay: " + m_cfg.overlay.substr(0, 38), 3000);
			return;
		}
	bool none = m_overlays.empty();
	m_cfg.side_fill = 0;
	m_gl.set_overlay(nullptr, 0, 0);
	message(none ? "Sides: black (no images in overlays/)" : "Sides: black", none ? 5000 : 3000);
}

void app::guard_frame(double ms, int ww, int wh, int used)
{
	bool stepped = false;
	if (!m_guard.add(ms, ww, wh, used, stepped)) return;
	if (stepped)
		message(std::string("Video: ") + shader_name(used) + " is too slow at " + std::to_string(ww) + "x" + std::to_string(wh) +
		        " (" + std::to_string(int(m_guard.last_avg + 0.5)) + " ms a frame): using " + shader_name(m_guard.cap), 6000);
	if (m_cfg.frame_log)
		if (FILE *f = fopen(path_join(m_base, "frame_times.log").c_str(), "a"))
		{
			fprintf(f, "%dx%d %s: GPU %.2f ms average, %.2f ms longest, over %d frames%s\n", ww, wh, shader_name(used), m_guard.last_avg, m_guard.last_max,
			        frame_guard::kWindow, stepped ? (std::string(" -> stepped down to ") + shader_name(m_guard.cap)).c_str() : "");
			fclose(f);
		}
}

void app::present(const u32 *pixels, int w, int h, bool game)
{
	int shader = game ? m_cfg.shader : 0;
	if (m_shot && !game) save_screenshot(pixels, w, h);
	if (m_gl.ok())
	{
		int ww, wh;
		m_gl.drawable_size(ww, wh);
		if (!game)
		{
			m_gl.present(pixels, w, h, shader, view_for(ww, wh, w, h, game), m_cfg.scan_strength, true, m_cfg.side_fill);
			return;
		}
		// a game frame: timed, and drawn with the shader the guard allows (W5.3)
		int const used = m_guard.shader(shader);
		m_gl.present(pixels, w, h, used, view_for(ww, wh, w, h, game), m_cfg.scan_strength, true, m_cfg.side_fill);
		if (m_gl.gpu_timing() && m_gl.gpu_ms() >= 0) guard_frame(m_gl.gpu_ms(), ww, wh, used);
		return;
	}
	bool smooth = shader == 1 || (game && m_cfg.scaling == view_scaling::smooth);
	if (!m_tex || w != m_texw || h != m_texh || m_cfg.smooth != smooth)
	{
		if (m_tex) SDL_DestroyTexture(m_tex);
		m_cfg.smooth = smooth;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "1" : "0");
		m_tex = SDL_CreateTexture(m_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		m_texw = w; m_texh = h;
	}
	SDL_UpdateTexture(m_tex, nullptr, pixels, w * 4);
	int ww, wh;
	SDL_GetRendererOutputSize(m_ren, &ww, &wh);
	view_rect vr = view_for(ww, wh, w, h, game);
	SDL_Rect dst = { vr.x, vr.y, vr.w, vr.h };
	SDL_SetRenderDrawColor(m_ren, 0, 0, 0, 255);
	SDL_RenderClear(m_ren);
	SDL_RenderCopy(m_ren, m_tex, nullptr, &dst);
	if (shader >= 2 && dst.h >= h * 2)
	{
		SDL_SetRenderDrawBlendMode(m_ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(m_ren, 0, 0, 0, Uint8(140 * m_cfg.scan_strength));
		for (int y = 0; y < h; y++)
		{
			int yy = dst.y + (y * dst.h) / h + (dst.h / h) - 1;
			SDL_RenderDrawLine(m_ren, dst.x, yy, dst.x + dst.w, yy);
		}
	}
	SDL_RenderPresent(m_ren);
}

void app::poll_events()
{
	m_menu_up = m_menu_down = m_menu_left = m_menu_right = m_menu_ok = m_menu_back = m_menu_net = m_menu_clear = m_menu_guide = false;
	if (m_scripted) script_tick();
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		if (m_scripted && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP || e.type == SDL_TEXTINPUT)) continue;   // the keyboard is unplugged
		// the rebuilt screens take their input through the toolkit's one layer (U1.3); releases
		// are always passed on, so nothing stays "held" after a screen change
		// not while a button is being bound; while text is typed, keys type and only the
		// controller, mouse and touch drive the on-screen keyboard
		const bool toolkit = m_screen != screen::game && m_capture < 0 && !(m_editing && (e.type == SDL_KEYDOWN || e.type == SDL_TEXTINPUT));
		if (toolkit || e.type == SDL_KEYUP || e.type == SDL_CONTROLLERBUTTONUP || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_FINGERUP)
		{
			ui::raw_event raw;
			if (ui::from_sdl(e, m_ui_shown, m_ui_ww, m_ui_wh, raw)) m_ui_in.feed(raw, SDL_GetTicks());
		}
		switch (e.type)
		{
			case SDL_QUIT: m_quit = true; break;
			case SDL_CONTROLLERDEVICEADDED:
				if (SDL_GameController *c = SDL_GameControllerOpen(e.cdevice.which)) m_pads.push_back(c);
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				for (size_t i = 0; i < m_pads.size(); i++)
					if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_pads[i])) == e.cdevice.which)
					{
						SDL_GameControllerClose(m_pads[i]);
						m_pads.erase(m_pads.begin() + long(i));
						break;
					}
				break;
			case SDL_DROPBEGIN: m_dropped = 0; m_drop_err.clear(); break;
			case SDL_DROPFILE:
				if (e.drop.file)
				{
					std::string const f = e.drop.file;
					SDL_free(e.drop.file);
					std::string ext = f.size() > 4 ? f.substr(f.size() - 4) : "";
					for (char &ch : ext) ch = char(tolower(ch));
					if (ext == ".ini")
					{
						// a controller profile or setup (U3)
						std::string m;
						import_result r;
						bool ok = m_profiles.import_file(f, m, r);
						if (ok && r.setup) apply_setup(r);
						message(ok ? "Imported: " + m + (r.setup ? " (setup applied)" : "") : "Import: " + m, 5000);
					}
					else if (m_mach) message("Drop ROMs on the game list, not during a game", 4000);
					else
					{
						// ROMs: a zip, any file, or a folder of them
						std::string err;
						m_dropped += copy_into_roms(f, err);
						if (!err.empty()) m_drop_err = err;
					}
				}
				break;
			case SDL_DROPCOMPLETE:
				if (!m_mach && (m_dropped > 0 || !m_drop_err.empty()))
				{
					rescan_roms(SDL_GetTicks());
					int ready = 0;
					for (bool a : m_avail) ready += a;
					std::string m = "Copied " + std::to_string(m_dropped) + (m_dropped == 1 ? " file" : " files") + " to the ROM folder: " +
					                std::to_string(ready) + " of " + std::to_string(int(game_list().size())) + " games ready";
					if (!m_drop_err.empty()) m += " (" + m_drop_err + ")";
					message(m, 6000);
					SDL_Log("%s", m.c_str());
				}
				m_dropped = 0;
				m_drop_err.clear();
				break;
			case SDL_CONTROLLERAXISMOTION:
				if (m_capture >= 0 && !m_captured && std::abs(int(e.caxis.value)) > 24000)
				{
					m_capture_in = { true, int(e.caxis.axis), e.caxis.value < 0 ? -1 : 1 };
					m_captured = true;
				}
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				if (m_capture >= 0)
				{
					if (!m_captured) { m_capture_in = { false, int(e.cbutton.button), 1 }; m_captured = true; }
					break;
				}
				if (m_editing) break;          // the on-screen keyboard has the controller
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_X) m_menu_clear = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) m_menu_up = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) m_menu_down = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) m_menu_left = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) m_menu_right = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A || e.cbutton.button == SDL_CONTROLLER_BUTTON_START) m_menu_ok = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) m_menu_guide = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE || (m_screen != screen::game && e.cbutton.button == SDL_CONTROLLER_BUTTON_B)) m_menu_back = true;
				if (e.cbutton.button == SDL_CONTROLLER_BUTTON_Y) m_menu_net = true;
				break;
			case SDL_WINDOWEVENT:
				// fullscreen chosen with the window's own control (the green button on a Mac,
				// which gives the game its own Space): the setting follows
				if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && m_win)
					m_cfg.fullscreen = (SDL_GetWindowFlags(m_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
				break;
			case SDL_TEXTINPUT:
				if (m_editing)
					for (const char *c = e.text.text; *c; c++) m_kbd.type(*c);       // the keyboard's layout says which characters it takes
				break;
			case SDL_KEYDOWN:
			{
				SDL_Keycode k = e.key.keysym.sym;
				bool alt = (e.key.keysym.mod & KMOD_ALT) != 0;
				// Mac (R6.2): Command is KMOD_GUI. Ctrl+Command+F and Command+Enter go fullscreen,
				// Command+V pastes; Command+Q quits through SDL's own app menu
				bool cmd = (e.key.keysym.mod & KMOD_GUI) != 0;
				bool fs_key = k == SDLK_F11 || (k == SDLK_RETURN && (alt || cmd)) || (k == SDLK_f && cmd && (e.key.keysym.mod & KMOD_CTRL));
				if (m_editing)
				{
					if (k == SDLK_F12 || k == SDLK_PRINTSCREEN) m_shot = true;     // a screenshot works while typing too
					if (k == SDLK_BACKSPACE) m_kbd.erase();
					if (k == SDLK_v && (e.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)))
						if (char *t = SDL_GetClipboardText()) { for (char *c = t; *c; c++) if (!isspace((unsigned char)*c)) m_kbd.type(*c); SDL_free(t); }
					if (k == SDLK_RETURN || k == SDLK_KP_ENTER) m_menu_ok = true;
					if (k == SDLK_ESCAPE) m_menu_back = true;
					break;
				}
				if (k == SDLK_UP) m_menu_up = true;
				if (k == SDLK_DOWN) m_menu_down = true;
				if (k == SDLK_LEFT) m_menu_left = true;
				if (k == SDLK_RIGHT) m_menu_right = true;
				if (k == SDLK_RETURN && !alt && !cmd) m_menu_ok = true;
				if (k == SDLK_ESCAPE) m_menu_back = true;
				if (k == SDLK_n) m_menu_net = true;
				if (k == SDLK_DELETE || k == SDLK_BACKSPACE) m_menu_clear = true;
				if (fs_key)
				{
					m_cfg.fullscreen = !m_cfg.fullscreen;
					SDL_SetWindowFullscreen(m_win, m_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
				}
				if (k == SDLK_F1) m_show_help = !m_show_help;
				if (k == SDLK_F12 || k == SDLK_PRINTSCREEN) m_shot = true;
				if (k == SDLK_F4)
				{
					m_cfg.shader = (m_cfg.shader + 1) % kNumShaders;
					m_guard.reset();    // the player's choice: measured again from it
					message(std::string("Video: ") + shader_name(m_cfg.shader) + (m_gl.ok() ? "" : " (no OpenGL: approximate)"));
				}
				if (k == SDLK_F5)
				{
					m_cfg.scaling = view_scaling((int(m_cfg.scaling) + 1) % 3);
					say_view(true);
				}
				if (k == SDLK_F8)
				{
					next_side_fill();
				}
				if (k == SDLK_F7)
				{
					// Widescreen is only offered on a game that really draws a wider picture (W3):
					// Tournament Edition 4.0 and Special Edition. On the others F7 goes between
					// Original 4:3 and Stretch; calling a 4:3 picture "Widescreen" would be a lie.
					const bool wide_ok = m_wide_possible;
					if (m_cfg.shape == view_shape::original) m_cfg.shape = wide_ok ? view_shape::widescreen : view_shape::stretch;
					else if (m_cfg.shape == view_shape::widescreen) m_cfg.shape = view_shape::stretch;
					else m_cfg.shape = view_shape::original;
					say_view(false);
				}
				if (k == SDLK_F6)
				{
					m_cfg.scan_strength = m_cfg.scan_strength >= 0.95f ? 0.2f : m_cfg.scan_strength + 0.2f;
					message("Scanline strength: " + std::to_string(int(m_cfg.scan_strength * 100 + 0.5f)) + "%");
				}
				if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { m_cfg.volume = std::max(0.0f, m_cfg.volume - 0.1f); message("Volume " + std::to_string(int(m_cfg.volume * 100 + 0.5f)) + "%", 1200); }
				if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { m_cfg.volume = std::min(1.0f, m_cfg.volume + 0.1f); message("Volume " + std::to_string(int(m_cfg.volume * 100 + 0.5f)) + "%", 1200); }
				if (m_mach && !m_net_game)
				{
					if (k == SDLK_F2) { m_test = !m_test; message(m_test ? "TEST switch: ON (service menu)" : "TEST switch: OFF", 2000); }
					if (k == SDLK_F3) { m_mach->reset(); message("Reset", 1500); }
					if (k == SDLK_p || k == SDLK_PAUSE) m_paused = !m_paused;
				}
				break;
			}
		}
	}
}

// ---------------------------------------------------------------- the controller-only audit (U7.1)
const char *app::screen_name() const
{
	if (m_editing) return "typing";
	switch (m_screen)
	{
	case screen::menu: return "menu";
	case screen::settings: return "settings";
	case screen::net_menu: return "online";
	case screen::net_lobby: return "lobby";
	case screen::game: return "game";
	case screen::about: return "about";
	case screen::mods: case screen::mod_detail: return "mods";
	case screen::controls: return "controls";
	case screen::news: return "news";
	case screen::pause: return "pause";
	}
	return "?";
}

bool app::start_pad_script()
{
	std::vector<u8> text;
	std::string err;
	if (!read_whole_file(m_script_path, text)) { SDL_Log("pad script: cannot read %s", m_script_path.c_str()); return false; }
	if (!m_script.load(std::string(text.begin(), text.end()), err)) { SDL_Log("pad script: %s", err.c_str()); return false; }
	SDL_VirtualJoystickDesc d;
	SDL_zero(d);
	d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	d.naxes = SDL_CONTROLLER_AXIS_MAX;
	d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
	d.name = "Scripted controller";
	int idx = SDL_JoystickAttachVirtualEx(&d);
	if (idx < 0 || !(m_vpad = SDL_JoystickOpen(idx))) { SDL_Log("pad script: no virtual controller: %s", SDL_GetError()); return false; }
	m_scripted = true;
	m_script_t0 = SDL_GetTicks();
	SDL_Log("pad script: %s, keyboard ignored", m_script_path.c_str());
	return true;
}

void app::script_tick()
{
	for (const pad_script::action &a : m_script.step(SDL_GetTicks() - m_script_t0))
		switch (a.kind)
		{
		case pad_script::action::press:
		case pad_script::action::release:
			for (int b : a.buttons)
			{
				if (b >= 100) SDL_JoystickSetVirtualAxis(m_vpad, b == 100 ? SDL_CONTROLLER_AXIS_TRIGGERLEFT : SDL_CONTROLLER_AXIS_TRIGGERRIGHT, a.kind == pad_script::action::press ? 32767 : 0);
				else SDL_JoystickSetVirtualButton(m_vpad, b, a.kind == pad_script::action::press ? 1 : 0);
			}
			break;
		case pad_script::action::shot: m_shot = true; break;
		case pad_script::action::expect:
		{
			bool ok = a.text == screen_name();
			(ok ? m_script_pass : m_script_fail)++;
			SDL_Log("pad script line %d: %s expect %s (on %s)", a.line, ok ? "PASS" : "FAIL", a.text.c_str(), screen_name());
			break;
		}
		case pad_script::action::log: SDL_Log("pad script line %d: %s", a.line, a.text.c_str()); break;
		case pad_script::action::quit: m_quit = true; break;
		case pad_script::action::drop:
		{
			// the same events a drop from the desktop gives
			SDL_Event e;
			SDL_zero(e);
			e.type = SDL_DROPBEGIN; SDL_PushEvent(&e);
			e.type = SDL_DROPFILE; e.drop.file = SDL_strdup(a.text.c_str()); SDL_PushEvent(&e);
			SDL_zero(e);
			e.type = SDL_DROPCOMPLETE; SDL_PushEvent(&e);
			SDL_Log("pad script line %d: drop %s", a.line, a.text.c_str());
			break;
		}
		case pad_script::action::expect_ready:
		{
			int ready = 0;
			for (bool r : m_avail) ready += r;
			bool ok = ready == a.n;
			(ok ? m_script_pass : m_script_fail)++;
			SDL_Log("pad script line %d: %s expect_ready %d (%d ready)", a.line, ok ? "PASS" : "FAIL", a.n, ready);
			break;
		}
		default: break;
		}
	if (m_quit || m_script.done())
	{
		SDL_Log("pad script: done, %d passed, %d failed", m_script_pass, m_script_fail);
		m_scripted = false;
		m_quit = true;
	}
}

cabinet_inputs app::gather_inputs()
{
	cabinet_inputs in{};
	const Uint8 *ks = SDL_GetKeyboardState(nullptr);
	for (int p = 0; p < 4; p++)
	{
		const key_map &k = m_cfg.keys[p];
		auto &q = in.p[p];
		q.up = ks[k.up]; q.down = ks[k.down]; q.left = ks[k.left]; q.right = ks[k.right];
		q.shoot = ks[k.shoot]; q.pass = ks[k.pass]; q.turbo = ks[k.turbo]; q.start = ks[k.start]; q.coin = ks[k.coin];
	}
	for (size_t i = 0; i < m_pads.size() && i < 4; i++)
	{
		SDL_GameController *c = m_pads[i];
		unsigned m = active_profile(int(i)).buttons(c);      // shortcuts already expanded to cabinet buttons
		// while Start + Back is held for the pause menu, the game sees neither
		if (m_cfg.pause_buttons == 1 && pause_held(c)) m &= ~((1u << A_START) | (1u << A_COIN));
		// Start alone opens the pause menu when held a second: the game gets Start only when it
		// is let go sooner, as a short press of its own (a held Start joins nothing by itself)
		if (m_cfg.pause_buttons == 0)
		{
			bool const down = (m >> A_START) & 1u;
			m &= ~(1u << A_START);
			unsigned const now = SDL_GetTicks();
			if (down && !m_start_since[i]) m_start_since[i] = now ? now : 1;
			if (!down && m_start_since[i])
			{
				if (!m_start_used[i] && now - m_start_since[i] < kStartHoldMs) m_start_pulse[i] = 6;
				m_start_since[i] = 0;
				m_start_used[i] = false;
			}
			if (m_start_pulse[i] > 0) { m |= 1u << A_START; m_start_pulse[i]--; }
		}
		auto &q = in.p[size_t(pad_player(i))];
		auto on = [&](int a) { return ((m >> a) & 1u) != 0; };
		q.up |= on(A_UP); q.down |= on(A_DOWN); q.left |= on(A_LEFT); q.right |= on(A_RIGHT);
		q.shoot |= on(A_SHOOT); q.pass |= on(A_PASS); q.turbo |= on(A_TURBO); q.start |= on(A_START); q.coin |= on(A_COIN);
	}
	in.test = m_test;
	in.service = ks[SDL_SCANCODE_9] != 0;
	add_pulses(in);
	return in;
}

// Coins and service credits from the pause menu go in as short presses, one after the
// other, while the game runs.
void app::add_pulses(cabinet_inputs &in)
{
	for (int p = 0; p < 4; p++) if (m_coin_pulse[p] % 8 >= 4) in.p[p].coin = true;
	if (m_service_pulse % 8 >= 4) in.service = true;
}

// What this computer sends while the pause menu is open online: nothing pressed, except a
// coin asked for in the menu. The others' games go on with that.
cabinet_inputs app::menu_inputs()
{
	cabinet_inputs in{};
	in.test = m_test;
	add_pulses(in);
	return in;
}

bool app::wide_symbols_of(wide_symbols &ws)
{
	const auto &sy = m_mods.symbols();
	auto get = [&](const char *n) { auto i = sy.find(n); return i == sy.end() ? 0u : i->second; };
	ws.display = get("DISPLAY"); ws.objlst = get("OBJLST"); ws.worldtlx = get("WORLDTLX"); ws.worldtly = get("WORLDTLY");
	ws.gndx = get("gndx"); ws.gndpos_t = get("gndpos_t"); ws.dtype = get("dtype"); ws.dpage = get("dpage"); ws.pcnt = get("PCNT"); ws.scrntl = get("SCRNTL"); ws.scrnlr = get("SCRNLR");
	return ws.complete();
}

int app::wide_columns() const
{
	int ww = 0, wh = 0;
	if (m_gl.ok()) m_gl.drawable_size(ww, wh);
	else if (m_ren) SDL_GetRendererOutputSize(m_ren, &ww, &wh);
	if (ww <= 0 || wh <= 0) return 400;
	// 400 columns fill a 4:3 shape; the window's shape needs this many
	int cols = int(std::lround(400.0 * (double(ww) / wh) / kOriginalAspect / 2.0)) * 2;
	// the player's limit (W5.2): 16:9 is 533 columns, 21:9 (64:27) is 711
	int const cap = m_cfg.max_wide == 1 ? 534 : m_cfg.max_wide == 2 ? 712 : 1000;
	return std::clamp(cols, 400, cap);
}

void app::update_wide()
{
	if (!m_mach || !m_wide_possible || m_cfg.shape != view_shape::widescreen) { m_wide.reset(); return; }
	int const cols = wide_columns();
	if (m_wide && m_wide->columns() == cols) return;
	wide_symbols ws;
	if (!wide_symbols_of(ws)) { m_wide_possible = false; m_wide.reset(); return; }
	m_wide.reset();
	const wide_game *wg = m_cur_game >= 0 ? wide_game_for(game_list()[size_t(m_cur_game)].id) : nullptr;
	if (!wg) { m_wide_possible = false; return; }
	m_wide = std::make_unique<wide_view>(*m_mach, ws, cols, *wg);
}

bool app::start_game(int index, bool net_client)
{
	const game_info &g = game_list()[size_t(index)];
	std::string err;
	SDL_SetWindowTitle(m_win, (std::string("NBA JAM Recomp - loading ") + g.title + "...").c_str());
	m_wide.reset();          // it watches the machine being replaced
	m_mach = m_cat->create(g, err);
	if (!m_mach)
	{
		message("Error: " + err, 5000);
		SDL_SetWindowTitle(m_win, "NBA JAM Recomp");
		return false;
	}
	if (!net_client)
	{
		std::vector<u8> nv;
		if (read_whole_file(nv_path(g), nv)) m_mach->set_nvram(nv);
		m_mach->set_dips(m_cfg.dips);
	}
	// Lua mods patch the ROM / hook frames; they also count in the netplay fingerprint
	m_mods.set_netplay(m_net_game);
	m_mods.start(g.id, *m_mach);
	{
		// true widescreen: the T.E. and Hangtime games (wide_game_for), with their symbol files
		m_wide.reset();
		wide_symbols ws;
		std::string const id = g.id;
		m_wide_possible = wide_game_for(id.c_str()) && wide_symbols_of(ws);
	}
	bool const wide_missing = m_cfg.shape == view_shape::widescreen && !m_wide_possible;
	m_join = audio_join();
	m_mach->reset();
	m_cur_game = index;
	m_paused = false;
	m_test = false;
	m_screen = screen::game;
	if (m_msound) m_msound->silence();
	SDL_SetWindowTitle(m_win, (std::string("NBA JAM Recomp - ") + g.title + (m_net_game ? "   [ONLINE]" : "   [Esc: pause menu  F1: help]")).c_str());
	SDL_ClearQueuedAudio(m_audio);
	if (wide_missing) message("Widescreen is not available for this game: 4:3", 4000);
	else if (!m_net_game) message("Press F1 to see the controls", 3000);
	if (m_mods.active_count() > 0)
	{
		std::string log;
		for (const auto &l : m_mods.take_log()) log += (log.empty() ? "" : "  ") + l;
		message("Lua mods active: " + std::to_string(m_mods.active_count()) + (log.empty() ? "" : "  " + log), 5000);
	}
	else
	{
		auto log = m_mods.take_log();       // errors of mods that failed to load
		if (!log.empty()) message(log.front(), 6000);
	}
	return true;
}

void app::stop_game()
{
	m_wide.reset();
	m_wide_possible = false;
	m_presenting_wide = false;
	// a netplay client never overwrites its own NVRAM with the host's
	bool save = !(m_net_game && m_net.role() == netplay::mode::client);
	if (m_mach && m_cur_game >= 0 && save)
		write_whole_file(nv_path(game_list()[size_t(m_cur_game)]), m_mach->nvram());
	m_mods.stop();
	m_mods.net_restore();   // a player's own Lua mod settings, after the host's set (1.7)
	m_mach.reset();
	m_cur_game = -1;
	if (m_net_game)
	{
		std::string err = m_net.error();
		m_net.stop();
		m_net_game = false;
		if (!err.empty()) message("Network: " + err, 6000);
	}
	m_screen = screen::menu;
	SDL_ClearQueuedAudio(m_audio);
	SDL_SetWindowTitle(m_win, "NBA JAM Recomp");
}

void app::draw_overlay(canvas &cv)
{
	if (m_show_help)
	{
		cv.blend(0, 0, cv.w, cv.h, 0xff000000, 200);
		int y = 6;
		auto line = [&](const std::string &s, u32 c = 0xffffffff) { cv.text(8, y, s, c); y += 11; };
		line("CONTROLS (keyboard / pad)", 0xffffd040);
		y += 2;
		line("            shoot/pass/turbo start coin", 0xffb0b0b0);
		line("P1 arrows   Z / X / C         1     5");
		line("P2 WASD     F / G / V         2     6");
		line("P3 num8462  n0 / n. / nEnter 3     7");
		line("P4 IJKL     H / ; / U         4     8");
		{
			const pad_profile &pp = active_profile(0);
			line("Pad P1 (" + pp.name + "): " + pp.action_text(4) + " shoot " + pp.action_text(5) + " pass");
		}
		y += 3;
		line("Esc  pause menu            P  pause", 0xffa0e0ff);
		line("F2   TEST switch           F3 reset", 0xffa0e0ff);
		line("F4   video: sharp/smooth/scanl./CRT", 0xffa0e0ff);
		line("F5   scaling: integer/fit/smooth", 0xffa0e0ff);
		line("F6   scanline strength", 0xffa0e0ff);
		line(m_wide_possible ? "F7   shape: 4:3 / widescreen / stretch" : "F7   shape: original 4:3 / stretch", 0xffa0e0ff);
		line("F8   sides: black / blur / overlays", 0xffa0e0ff);
		line("F11  fullscreen            - / +  volume", 0xffa0e0ff);
		line("F12  screenshot            9  service credit", 0xffa0e0ff);
		y += 3;
		line(std::string("Video: ") + shader_name(m_cfg.shader) + ", " + view_scaling_name(m_cfg.scaling) + ", " + view_shape_name(m_cfg.shape), 0xffb0b0b0);
		line("Pads: Controls menu. Keys: nbajam_recomp.ini", 0xffb0b0b0);
	}
	if (m_net_game)
	{
		std::string tag = "NET P" + std::to_string(m_net.local_slot() + 1) + " " + std::to_string(m_net.ping_ms()) + "ms";
		cv.blend(cv.w - cv.text_w(tag) - 6, 0, cv.text_w(tag) + 6, 10, 0xff000000, 140);
		cv.text(cv.w - cv.text_w(tag) - 3, 1, tag, 0xff80ff80, 1, false);
		if (m_net.desynced())
		{
			cv.blend(0, 12, cv.w, 12, 0xff800000, 200);
			cv.text_wrap(4, 14, "DESYNC: restart the online game", 0xffffffff);
		}
		if (m_net.stall_ms() > 400)
			{
				std::string wt = "Waiting for the network...";
				cv.text(cv.w / 2 - cv.text_w(wt) / 2, cv.h / 2 - 4, wt, 0xffffff40);
			}
	}
	if (m_paused) { std::string pt = "PAUSED"; cv.text(cv.w / 2 - cv.text_w(pt, 2) / 2, cv.h / 2 - 8, pt, 0xffffff40, 2); }
	if (!m_message.empty() && SDL_GetTicks() < m_message_until)
	{
		cv.blend(0, cv.h - 14, cv.w, 14, 0xff000000, 170);
		cv.text(4, cv.h - 11, m_message.substr(0, size_t(cv.w / 8)), 0xffffffff);
	}
}

void app::draw_background(canvas &cv)
{
	for (int y = 0; y < cv.h; y++)
	{
		u32 r = 20 + y / 10, g = 10 + y / 14, b = 40 + y / 5;
		cv.fill(0, y, cv.w, 1, 0xff000000 | (r << 16) | (g << 8) | b);
	}
	u32 t = SDL_GetTicks();
	for (int i = 0; i < 6; i++)
	{
		int x = int((t / 30 + i * 80) % 520) - 60;
		cv.blend(x, 0, 18, cv.h, 0xffff6020, 18);
	}
}

void app::save_screenshot(const u32 *px, int w, int h)
{
	m_shot = false;
	std::vector<u8> rgb(size_t(w) * h * 3);
	for (size_t i = 0; i < size_t(w) * h; i++)
	{
		rgb[i * 3] = u8(px[i] >> 16); rgb[i * 3 + 1] = u8(px[i] >> 8); rgb[i * 3 + 2] = u8(px[i]);
	}
	size_t len = 0;
	void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &len);
	if (!png) { message("Could not create the screenshot", 3000); return; }
	std::time_t t = std::time(nullptr);
	char stamp[32];
	std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", std::localtime(&t));
	std::string name = std::string(m_cur_game >= 0 ? game_list()[size_t(m_cur_game)].id : "menu") + "_" + stamp;
	std::string dir = path_join(m_base, "screenshots"), path;
	for (int n = 0; n < 100; n++)
	{
		path = path_join(dir, name + (n ? "_" + std::to_string(n) : std::string()) + ".png");
		if (!file_exists(path)) break;
	}
	bool ok = write_whole_file(path, std::vector<u8>(static_cast<u8 *>(png), static_cast<u8 *>(png) + len));
	mz_free(png);
	message(ok ? std::string("Screenshot saved: screenshots/") + path.substr(dir.size() + 1)
	           : std::string("Could not save ") + path, 3000);
}

// message bar of the menu screens (two lines, word-wrapped)
static void menu_message(canvas &cv, const std::string &m)
{
	cv.blend(0, cv.h - 46, cv.w, 24, 0xff800000, 200);
	cv.text_wrap(4, cv.h - 43, m.substr(0, 96), 0xffffffff, 10, true);
}

// ---------------------------------------------------------------------------
// The game picker and Settings, on the menu toolkit (roadmap U3.1, U3.2; ui/screens.h)
// ---------------------------------------------------------------------------
bool app::ui_next(ui::event &ev)
{
	if (!m_ui_in.next(ev)) return false;
	if (m_msound)
	{
		using ui::act;
		if (ev.a == act::back) m_msound->play(menu_sound::back);
		else if (ev.a == act::confirm || ev.ptr == ui::event::click) m_msound->play(menu_sound::confirm);
		else if (ev.a == act::up || ev.a == act::down || ev.a == act::left || ev.a == act::right || ev.a == act::tab_prev || ev.a == act::tab_next ||
		         ev.a == act::page_up || ev.a == act::page_down)
			m_msound->play(menu_sound::cursor);
	}
	return true;
}

void app::ui_frame_begin()
{
	// the menus' music and sounds: the highlighted game's, and none while a game is loaded
	// (the pause menu, and the screens opened from it)
	if (m_msound)
	{
		const std::string id = m_mach ? std::string() : game_list()[size_t(std::clamp(m_sel, 0, int(game_list().size()) - 1))].id;
		const bool have = !id.empty() && size_t(m_sel) < m_avail.size() && m_avail[size_t(m_sel)];
		m_msound->want(have ? id : std::string(), m_cfg.menu_music, m_cfg.menu_sounds, m_cfg.menu_volume * m_cfg.volume);
	}
	int dw = 0, dh = 0;
	if (m_gl.ok()) m_gl.drawable_size(dw, dh); else if (m_ren) SDL_GetRendererOutputSize(m_ren, &dw, &dh);
	SDL_GetWindowSize(m_win, &m_ui_ww, &m_ui_wh);
	// the menu picture is as wide as the window's shape (4:3 to 21:9), 300 rows high
	if (dw > 0 && dh > 0)
	{
		int const hint = int(std::lround(300.0 * dw / dh)) & ~1;
		ui::screen::kW = std::clamp(hint, ui::screen::kMinW, ui::screen::kMaxW);
	}
	// the menu picture's place, in the units the mouse reports (window points, not pixels)
	m_ui_shown = view_for(dw, dh, ui::screen::kW, ui::screen::kH, false);
	if (dw > 0 && dh > 0)
	{
		m_ui_shown.x = m_ui_shown.x * m_ui_ww / dw; m_ui_shown.w = m_ui_shown.w * m_ui_ww / dw;
		m_ui_shown.y = m_ui_shown.y * m_ui_wh / dh; m_ui_shown.h = m_ui_shown.h * m_ui_wh / dh;
	}
	m_ui_in.tick(SDL_GetTicks());
}

const rom_art::art_set *app::art_of(const std::string &game_id)
{
	auto it = m_arts.find(game_id);
	if (it == m_arts.end())
	{
		// read from the ROMs the first time the game is highlighted; into memory only
		rom_art::roms roms;
		rom_art::art_set set;
		for (const game_info &g : game_list()) if (game_id == g.id && m_cat->art_roms(g, roms)) set = rom_art::read(roms);
		it = m_arts.emplace(game_id, std::move(set)).first;
	}
	return it->second.empty() ? nullptr : &it->second;
}

void app::follow_theme(const std::string &game_id, unsigned now)
{
	if (m_theme_w != ui::screen::kW) { m_theme_w = ui::screen::kW; m_theme_game.clear(); }   // the backdrop is made for one width
	if (game_id != m_theme_game)
	{
		m_theme_from = m_theme;
		m_theme_to = ui::make_theme(ui::load_theme_spec(ui::theme_id_for_game(game_id), m_base), art_of(game_id));
		m_theme_fading = !m_theme_game.empty();          // the first theme is simply there
		if (!m_theme_fading) m_theme = m_theme_to;
		m_theme_game = game_id;
		m_theme_start = SDL_GetTicks();                  // after the read, so the fade is seen whole
		now = m_theme_start;
	}
	if (m_theme_fading)
	{
		float t = float(now - m_theme_start) / float(ui::kThemeFadeMs);
		m_theme = t >= 1 ? m_theme_to : ui::mix_themes(m_theme_from, m_theme_to, t);
		m_theme_fading = t < 1;
	}
	m_theme.note = m_msound && m_msound->music_playing() ? "Music on" : "";
}

ui::picker_state app::picker_now()
{
	// the name in short lines for the large lettering
	static const std::map<std::string, std::vector<std::string>> big = {
		{ "nbajam", { "NBA JAM" } }, { "nbajamte", { "NBA JAM", "T.E." } }, { "nbajamse", { "NBA JAM", "T.E. SE" } },
		{ "nbahangt", { "NBA", "HANGTIME" } }, { "nbamht", { "MAXIMUM", "HANGTIME" } } };
	ui::picker_state st;
	const auto &list = game_list();
	for (size_t i = 0; i < list.size(); i++)
	{
		ui::picker_game g;
		g.title = list[i].title; g.revision = list[i].subtitle; g.board = list[i].hardware;
		g.available = m_avail[i]; g.why = m_why[i]; g.from_source = list[i].from_source;
		auto it = big.find(list[i].id);
		g.big = it != big.end() ? it->second : std::vector<std::string>{ list[i].title };
		st.games.push_back(g);
	}
	for (const auto &md : m_mods.mods()) { st.mods_total++; st.mods_on += md.enabled && md.error.empty(); }
	st.news_on = m_cfg.news; st.news_new = m_cfg.news && m_news_new; st.news_tag = m_news_item.tag;
	st.version = app_version();
	st.roms_dir = path_join(m_base, "roms");
	return st;
}

void app::open_online()
{
	m_screen = screen::net_menu;
	m_online_scr = ui::screen();
	m_local_ips = net_local_ipv4();
	net_local_ipv6(m_local6);
}

void app::menu_frame()
{
	m_back_to = screen::menu;
	const auto &list = game_list();
	const int ng = int(list.size());
	m_sel = std::clamp(m_sel, 0, ng - 1);
	poll_news();
	ui_frame_begin();
	const unsigned now = SDL_GetTicks();
	ui::picker_state st = picker_now();
	if (m_picker.tabs.empty())
	{
		m_picker = ui::make_picker(st);
		m_picker.tabs[0].panels[0].focus = m_sel;        // the first game that can be played
	}
	ui::event ev;
	while (ui_next(ev))
	{
		// Online game: left and right choose its game, so a controller can reach every game from
		// this row (down from the game list always arrived with the last game chosen)
		if ((ev.a == ui::act::left || ev.a == ui::act::right) && m_picker.focused() && m_picker.focused()->id == ui::pk_online)
		{
			for (int n = 0; n < ng; n++)
			{
				m_sel = (m_sel + (ev.a == ui::act::right ? 1 : ng - 1)) % ng;
				if (m_avail[size_t(m_sel)]) break;
			}
			m_picker.tabs[0].panels[0].focus = m_sel;
			continue;
		}
		ui::result r = m_picker.handle(ev);
		m_sel = ui::picker_game_index(m_picker, m_sel);
		if (r.what == ui::result::back) m_picker.ask("Quit NBA JAM Recomp?", ui::pk_quit);
		else if (r.what == ui::result::yes && r.id == ui::pk_quit) { m_quit = true; return; }
		else if (r.what == ui::result::context && m_avail[size_t(m_sel)]) { open_online(); return; }
		else if (r.what == ui::result::activated)
		{
			if (r.id >= ui::pk_game0 && r.id < ui::pk_game0 + ng)
			{
				int i = r.id - ui::pk_game0;
				if (m_avail[size_t(i)]) { if (start_game(i)) return; }
				else m_picker.toast("No ROMs for this game: see the panel on the right", now, 4000);
			}
			else if (r.id == ui::pk_online) { open_online(); return; }
			else if (r.id == ui::pk_settings)
			{
				m_overlays = list_overlays(m_base);          // pick up files added while the program runs
				m_settings = ui::make_settings(settings_now());
				m_screen = screen::settings;
				return;
			}
			else if (r.id == ui::pk_mods)
			{
				m_mods.scan(path_join(m_base, "mods"), path_join(m_base, "mods.ini"));   // pick up new scripts
				m_mod_sel = 0;
				m_mods_scr = ui::screen();
				m_screen = screen::mods;
				return;
			}
			else if (r.id == ui::pk_controls) { m_ctl_scr = ui::screen(); m_screen = screen::controls; return; }
			else if (r.id == ui::pk_news) { m_news_scr = ui::screen(); m_screen = screen::news; return; }
			else if (r.id == ui::pk_about) { m_about_scr = ui::screen(); m_screen = screen::about; return; }
			else if (r.id == ui::pk_romdir) open_rom_folder();
			else if (r.id == ui::pk_rescan)
			{
				rescan_roms(now);
				int ready = 0;
				for (bool a : m_avail) ready += a;
				m_picker.toast(std::to_string(ready) + " of " + std::to_string(ng) + " games ready to play", now, 3000);
				st = picker_now();
			}
			else if (r.id == ui::pk_quit) m_picker.ask("Quit NBA JAM Recomp?", ui::pk_quit);
		}
	}
	// N on the keyboard still opens the online menu for the highlighted game
	if (m_menu_net && !m_picker.asking() && m_avail[size_t(m_sel)]) { open_online(); return; }

	ui::update_picker(m_picker, st, m_sel);
	follow_theme(list[size_t(m_sel)].id, now);
	canvas cv(ui::screen::kW, ui::screen::kH);
	m_picker.draw(cv, m_theme, m_ui_in.last_device(), now);
	if (!m_picker.asking()) ui::draw_picker_name(cv, m_picker, m_theme, st.games[size_t(m_sel)], art_of(list[size_t(m_sel)].id));
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(8);
}

ui::settings_state app::settings_now()
{
	ui::settings_state st;
	st.shape = m_cfg.shape == view_shape::stretch ? 2 : m_cfg.shape == view_shape::widescreen ? 1 : 0;
	st.max_wide = m_cfg.max_wide;
	st.scaling = int(m_cfg.scaling);
	st.shader = m_cfg.shader;
	for (int i = 0; i < kNumShaders; i++) st.shaders.push_back(shader_name(i));
	st.scanlines = m_cfg.scan_strength;
	st.side_names = { "Black", "Blurred picture" };
	for (const overlay_entry &o : m_overlays) st.side_names.push_back(o.name);
	st.sides = m_cfg.side_fill == 1 ? 1 : 0;
	if (m_cfg.side_fill == 2)
		for (size_t i = 0; i < m_overlays.size(); i++) if (m_overlays[i].name == m_cfg.overlay) st.sides = int(i) + 2;
	st.fullscreen = m_cfg.fullscreen;
	st.tv_margins = m_cfg.tv_margins;
	st.volume = m_cfg.volume;
	st.free_play = m_cfg.dips.free_play; st.four_players = m_cfg.dips.four_players; st.video_clips = m_cfg.dips.video_clips; st.country = m_cfg.dips.country;
	st.news = m_cfg.news;
	st.save_states = m_cfg.save_states;
	st.pause_buttons = m_cfg.pause_buttons;
	st.menu_music = m_cfg.menu_music; st.menu_sounds = m_cfg.menu_sounds; st.menu_volume = m_cfg.menu_volume;
	return st;
}

void app::apply_settings(const ui::settings_state &want)
{
	const ui::settings_state had = settings_now();
	m_cfg.shape = want.shape == 2 ? view_shape::stretch : want.shape == 1 ? view_shape::widescreen : view_shape::original;
	m_cfg.max_wide = std::clamp(want.max_wide, 0, 2);
	m_cfg.scaling = view_scaling(std::clamp(want.scaling, 0, 2));
	if (want.shader != had.shader) m_guard.reset();
	m_cfg.shader = std::clamp(want.shader, 0, kNumShaders - 1);
	m_cfg.scan_strength = std::clamp(want.scanlines, 0.2f, 1.0f);
	m_cfg.volume = std::clamp(want.volume, 0.0f, 1.0f);
	m_cfg.dips.free_play = want.free_play; m_cfg.dips.four_players = want.four_players; m_cfg.dips.video_clips = want.video_clips;
	m_cfg.dips.country = std::clamp(want.country, 0, 2);
	m_cfg.pause_buttons = std::clamp(want.pause_buttons, 0, 2);
	m_cfg.save_states = want.save_states;
	m_cfg.menu_music = want.menu_music; m_cfg.menu_sounds = want.menu_sounds; m_cfg.menu_volume = std::clamp(want.menu_volume, 0.0f, 1.0f);
	m_cfg.tv_margins = want.tv_margins;
	if (want.fullscreen != had.fullscreen)
	{
		m_cfg.fullscreen = want.fullscreen;
		SDL_SetWindowFullscreen(m_win, m_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	}
	if (want.news != had.news)
	{
		m_cfg.news = want.news;
		if (m_cfg.news) m_news.start();          // asks once per run; does nothing if it already did
	}
	if (want.sides != had.sides)
	{
		if (!m_gl.ok() && want.sides != 0) { m_cfg.side_fill = 0; message("Sides: need OpenGL (black)", 4000); }
		else if (want.sides <= 1) { m_cfg.side_fill = want.sides; if (want.sides == 0) m_gl.set_overlay(nullptr, 0, 0); }
		else if (size_t(want.sides - 2) < m_overlays.size() && use_overlay(m_overlays[size_t(want.sides - 2)].name))
		{
			m_cfg.side_fill = 2;
			m_cfg.overlay = m_overlays[size_t(want.sides - 2)].name;
		}
		else message("That image could not be read", 4000);
	}
	save_config(m_cfgpath, m_cfg);
}

void app::settings_frame()
{
	ui_frame_begin();
	const unsigned now = SDL_GetTicks();
	if (m_settings.tabs.empty()) m_settings = ui::make_settings(settings_now());
	ui::show_settings(m_settings, settings_now());       // a key (F4 to F8, F11, - and +) may have changed one
	ui::event ev;
	while (ui_next(ev))
	{
		ui::result r = m_settings.handle(ev);
		if (r.what == ui::result::back) { save_config(m_cfgpath, m_cfg); m_screen = m_back_to; return; }
		if (r.what == ui::result::changed)
		{
			ui::settings_state want = settings_now();
			ui::read_settings(m_settings, want);
			apply_settings(want);
			ui::show_settings(m_settings, settings_now());   // what really took effect
		}
	}
	follow_theme(game_list()[size_t(std::clamp(m_sel, 0, int(game_list().size()) - 1))].id, now);
	canvas cv(ui::screen::kW, ui::screen::kH);
	m_settings.draw(cv, m_theme, m_ui_in.last_device(), now);
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(8);
}

// ---------------------------------------------------------------------------
// Lua mods menu: list of scripts in mods/, then one page per mod with
// on/off, "apply to: all games / selected games", a checkbox per game and the options
// ---------------------------------------------------------------------------
ui::mods_state app::mods_now()
{
	ui::mods_state st;
	st.folder = path_join(m_base, "mods");
	st.in_game = m_back_to == screen::pause && m_mach;
	for (const mod_script &m : m_mods.mods())
	{
		ui::mod_view v;
		v.name = m.name; v.description = m.description; v.error = m.error;
		v.enabled = m.enabled && m.error.empty(); v.all_games = m.all_games;
		for (const mod_option &o : m.options)
		{
			ui::mod_view::option vo;
			vo.label = o.label;
			vo.group = o.group;
			vo.kind = o.type == mod_option::toggle ? 1 : o.type == mod_option::players ? 2 : 0;
			for (size_t k = 0; k < o.values.size(); k++) vo.values.push_back(o.value_text(int(k)));
			vo.value = m.option_index(o);
			vo.greyed = m.option_greyed(o);
			vo.live = o.live;
			v.options.push_back(vo);
		}
		for (const game_info &g : game_list())
		{
			bool sup = m.supports(g.id);
			v.games.push_back({ g.title, sup, sup && (m.all_games || m.game_checked(g.id)) });
		}
		st.mods.push_back(v);
	}
	return st;
}

// Lua mods (U3.4): the scripts in mods/ in a list, and beside it the highlighted script's
// page: on or off, all games or selected ones, its options, a switch per game.
void app::mods_frame()
{
	ui_frame_begin();
	const unsigned now = SDL_GetTicks();
	auto &mods = m_mods.mods();
	const auto &games = game_list();
	if (m_mods_scr.tabs.empty()) m_mods_scr = ui::make_mods(mods_now());
	ui::event ev;
	while (ui_next(ev))
	{
		ui::result r = m_mods_scr.handle(ev);
		m_mod_sel = ui::mods_index(m_mods_scr, m_mod_sel);
		if (r.what == ui::result::back)
		{
			// from a script's page back to the list; from the list back to the menu
			if (m_mods_scr.cur_panel == 1) { m_mods_scr.cur_panel = 0; continue; }
			m_mods.save();
			m_screen = m_back_to;
			return;
		}
		if (m_mod_sel < 0 || m_mod_sel >= int(mods.size())) continue;
		mod_script &m = mods[size_t(m_mod_sel)];
		const bool in_list = r.id >= ui::md_mod0 && r.id < ui::md_enabled;
		if (r.what == ui::result::activated && in_list) ui::mods_open_page(m_mods_scr);
		else if (r.what == ui::result::context && in_list && m.error.empty()) { m.enabled = !m.enabled; m_mods.save(); }
		else if (r.what == ui::result::changed && m.error.empty())
		{
			const ui::item *it = m_mods_scr.find(r.id);
			if (!it) continue;
			const auto before = mod_manager::used_values(m);
			auto supported = [&]() { m.games.clear(); for (const auto &g : games) if (m.supports(g.id)) m.games.push_back(g.id); };
			if (r.id == ui::md_enabled) m.enabled = it->on;
			else if (r.id == ui::md_apply)
			{
				m.all_games = it->choice == 0;
				if (!m.all_games && m.games.empty()) supported();          // start from every supported game
			}
			else if (r.id >= ui::md_opt0 && r.id < ui::md_opt0 + int(m.options.size()))
			{
				const mod_option &o = m.options[size_t(r.id - ui::md_opt0)];
				m.opt[o.id] = o.type == mod_option::toggle ? (it->on ? 1 : 0) : it->choice;   // a toggle's values are { 0, 1 }
			}
			else if (r.id >= ui::md_ply0 && r.id < ui::md_ply0 + 4 * int(m.options.size()))
			{
				const mod_option &o = m.options[size_t((r.id - ui::md_ply0) / 4)];
				const int bit = 1 << ((r.id - ui::md_ply0) % 4);
				const int mask = m.option_index(o);                                     // a players option's values are 0 to 15
				m.opt[o.id] = it->on ? (mask | bit) : (mask & ~bit);
			}
			if (m_mach && m_back_to == screen::pause) m_mods.options_changed(m, before);   // live options act now
			else if (r.id >= ui::md_game0 && r.id < ui::md_game0 + int(games.size()))
			{
				const game_info &g = games[size_t(r.id - ui::md_game0)];
				// switching a game turns "apply to" to the selected games
				if (m.all_games) { m.all_games = false; supported(); }
				m.set_game(g.id, it->on);
			}
			m_mods.save();
		}
		ui::update_mods(m_mods_scr, mods_now(), m_mod_sel);
	}
	ui::update_mods(m_mods_scr, mods_now(), m_mod_sel);
	follow_theme(games[size_t(std::clamp(m_sel, 0, int(games.size()) - 1))].id, now);
	canvas cv(ui::screen::kW, ui::screen::kH);
	m_mods_scr.draw(cv, m_theme, m_ui_in.last_device(), now);
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(8);
}

static const char *const act_en[kNumActions] = { "Up", "Down", "Left", "Right", "Shoot", "Pass", "Turbo", "Start", "Coin",
                                                 "Shove", "Turbo shot", "Shortcut 1", "Shortcut 2" };

// The on-screen keyboard (U4.1) over the current screen. A physical keyboard types into
// it as well; Enter and Esc take or drop the text.
void app::kbd_open(ui::layout l, const std::string &title, const std::string &start, size_t max_len)
{
	m_kbd.open(l, title, start, max_len);
	m_editing = true;
	SDL_StartTextInput();
}

int app::kbd_step()
{
	int r = m_menu_ok ? 1 : m_menu_back ? 2 : 0;
	ui::event ev;
	while (!r && ui_next(ev))
	{
		ui::keyboard::outcome o = m_kbd.handle(ev);
		if (o == ui::keyboard::done) r = 1;
		if (o == ui::keyboard::cancelled) r = 2;
	}
	if (r)
	{
		while (m_ui_in.next(ev)) {}     // nothing typed this frame reaches the screen under it
		m_editing = false;
		m_kbd.close();
		SDL_StopTextInput();
	}
	return r;
}

// The end of every rebuilt screen's frame: the theme of the highlighted game, the screen,
// a program message over it if there is one.
void app::ui_show(ui::screen &s)
{
	const unsigned now = SDL_GetTicks();
	follow_theme(game_list()[size_t(std::clamp(m_sel, 0, int(game_list().size()) - 1))].id, now);
	canvas cv(ui::screen::kW, ui::screen::kH);
	s.draw(cv, m_theme, m_ui_in.last_device(), now);
	m_kbd.draw(cv, m_theme, m_ui_in.last_device(), now);
	if (!m_message.empty() && now < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(8);
}

void app::poll_news()
{
	news_item n;
	if (m_news.poll(n))
	{
		m_news_item = n;
		m_news_new = version_is_newer(n.tag, app_version());
		SDL_Log("latest release: %s%s", n.tag.c_str(), m_news_new ? " (newer than this program)" : "");
	}
}

// News: the notes of the latest published release, and whether it is newer than this
// program. It only informs; nothing is downloaded.
void app::news_frame()
{
	poll_news();
	ui_frame_begin();
	// built when the screen opens, and again if the answer arrives while it is open
	const bool have = m_cfg.news && m_news_item.ok;
	if (m_news_scr.tabs.empty() || have != m_news_shown)
	{
		ui::news_state st;
		st.on = m_cfg.news;
		st.ok = m_news_item.ok;
		st.is_new = m_news_new;
		st.name = m_news_item.name;
		st.date = m_news_item.date;
		st.version = app_version();
		if (have)
		{
			st.lines = news_plain_lines(m_news_item.body);
			// the Releases page, without "https://" and the tag part, so that it fits on one line
			st.page = m_news_item.url;
			if (st.page.find("://") != std::string::npos) st.page = st.page.substr(st.page.find("://") + 3);
			if (st.page.find("/tag/") != std::string::npos) st.page = st.page.substr(0, st.page.find("/tag/"));
		}
		m_news_scr = ui::make_news(st);
		m_news_shown = have;
	}
	ui::event ev;
	while (ui_next(ev))
		if (ev.a == ui::act::confirm || m_news_scr.handle(ev).what == ui::result::back) { m_screen = screen::menu; return; }
	ui_show(m_news_scr);
}

// About and help (U3.6). The Controls tab is made from what is set now: the keys of the
// settings file and the controller profile each player has for the highlighted game.
ui::about_state app::about_now()
{
	ui::about_state st;
	st.version = app_version();
	st.build = std::string(app_commit()) + "  " + app_build_date();
	st.data_dir = m_base;
	st.portable = m_paths.portable;
	if (m_cfg.news && m_news_item.ok) { st.latest = m_news_item.tag; st.latest_new = m_news_new; }
	const int p = std::clamp(m_about_player, 0, 3);
	st.player = p;
	const game_info &g = game_list()[size_t(std::clamp(m_sel, 0, int(game_list().size()) - 1))];
	st.game = g.title;
	const std::string *name = &m_cfg.pad_profile[p];
	auto it = m_cfg.game_profiles.find(g.id);
	if (it != m_cfg.game_profiles.end() && !it->second[size_t(p)].empty() && m_profiles.find(it->second[size_t(p)]) >= 0) name = &it->second[size_t(p)];
	const pad_profile &prof = m_profiles.get(*name);
	st.profile = prof.name;
	if (size_t(p) < m_pads.size() && m_pads[size_t(p)]) { const char *pn = SDL_GameControllerName(m_pads[size_t(p)]); st.pad = pn ? pn : "?"; }
	for (int a = 0; a < kNumActions; a++)
	{
		ui::about_state::row r;
		r.action = act_en[a];
		if (a < kNumButtons) r.key = SDL_GetScancodeName(*key_slot(m_cfg.keys[p], a));
		r.binding = prof.action_text(a);
		// one of the two free shortcuts: which cabinet buttons it presses
		if (a >= A_CUSTOM1 && prof.combo[a - kNumButtons]) r.action += " = " + combo_mask_text(prof.combo[a - kNumButtons]);
		st.controls.push_back(r);
	}
	return st;
}

void app::about_frame()
{
	poll_news();
	ui_frame_begin();
	if (m_about_scr.tabs.empty()) m_about_scr = ui::make_about(about_now());
	ui::event ev;
	while (ui_next(ev))
	{
		ui::result r = m_about_scr.handle(ev);
		if (r.what == ui::result::back) { m_screen = m_back_to; return; }
		if (r.what == ui::result::changed && r.id == ui::ab_player)
			if (const ui::item *it = m_about_scr.find(r.id)) m_about_player = it->choice;
	}
	ui::update_about(m_about_scr, about_now());
	ui_show(m_about_scr);
}

// applies a game setup bundle (from an import): the profile of each player for one game or all games
void app::apply_setup(const import_result &r)
{
	int gi = -1;
	if (!r.game.empty())
		for (size_t i = 0; i < game_list().size(); i++) if (r.game == game_list()[i].id) gi = int(i);
	if (!r.game.empty() && gi < 0) return;            // unknown game: keep just the profiles
	for (int p = 0; p < 4; p++)
		if (gi >= 0 || !r.players[p].empty()) profile_slot(gi, p) = r.players[p];
	save_config(m_cfgpath, m_cfg);
}

// Controls screen: choose all games or one game, pick a profile for each player,
// remap its pad inputs and shortcuts, and create / delete / export / import profiles.
// Controls (U3.3): who (game, player, profile), the buttons of that profile, and the
// profile itself. A button is bound by pressing it; waiting five seconds, or Esc, cancels,
// so a controller alone is enough.
void app::controls_frame()
{
	// button sets a custom shortcut can be given; any set can be written in the file
	static const unsigned kMasks[] = { 0, 1u << A_SHOOT, 1u << A_PASS, 1u << A_TURBO, (1u << A_SHOOT) | (1u << A_PASS),
	                                   (1u << A_TURBO) | (1u << A_SHOOT), (1u << A_TURBO) | (1u << A_PASS),
	                                   (1u << A_TURBO) | (1u << A_SHOOT) | (1u << A_PASS), (1u << A_START) | (1u << A_COIN) };
	const int nmask = int(sizeof(kMasks) / sizeof(kMasks[0]));
	ui_frame_begin();
	const unsigned now = SDL_GetTicks();
	auto &list = m_profiles.list();
	const int ng = int(game_list().size());
	if (m_ctl_game >= ng) m_ctl_game = -1;
	std::string &slot = profile_slot(m_ctl_game, m_ctl_player);
	if (!slot.empty() && m_profiles.find(slot) < 0) slot = m_ctl_game < 0 ? "Default" : "";
	const bool inherit = slot.empty();                 // per-game slot that follows the all-games choice
	const std::string &shown = inherit ? m_cfg.pad_profile[m_ctl_player] : slot;
	const int pi = std::max(0, m_profiles.find(shown));
	pad_profile &prof = list[size_t(pi)];
	const int extra = m_ctl_game >= 0 ? 1 : 0;         // "as all games" is a choice when one game is selected

	// what the screen shows
	ui::controls_state st;
	st.games.push_back("All games");
	for (const game_info &g : game_list()) st.games.push_back(g.title);
	st.game = m_ctl_game + 1;
	st.player = m_ctl_player;
	if (extra) st.profiles.push_back("As all games: " + m_cfg.pad_profile[m_ctl_player]);
	for (const pad_profile &p : list) st.profiles.push_back(p.name);
	st.profile = inherit ? 0 : m_profiles.find(slot) + extra;
	SDL_GameController *pad = size_t(m_ctl_player) < m_pads.size() ? m_pads[size_t(m_ctl_player)] : nullptr;
	if (pad) { const char *pn = SDL_GameControllerName(pad); st.pad = pn ? pn : "?"; }
	st.has_pad = pad != nullptr;
	st.plays_as = pad ? pad_player(size_t(m_ctl_player)) : m_ctl_player;
	const unsigned held = pad ? prof.buttons(pad) : 0;
	for (int a = 0; a < kNumActions; a++)
	{
		bool live = (pad && prof.pressed(pad, a)) || (a < kNumButtons && ((held >> a) & 1u));
		st.actions.push_back({ act_en[a], prof.action_text(a), live });
	}
	st.capturing = m_capture;
	st.seconds = m_capture >= 0 ? int(m_capture_until - now) / 1000 + 1 : 0;
	for (int k = 0; k < nmask; k++) st.shortcut_sets.push_back(combo_mask_text(kMasks[k]));
	for (int c = 0; c < 2; c++)
	{
		const unsigned cm = prof.combo[int(A_CUSTOM1) - int(kNumButtons) + c];
		int k = 0;
		while (k < nmask && kMasks[k] != cm) k++;
		if (k >= nmask) { st.shortcut_sets.push_back(combo_mask_text(cm)); k = int(st.shortcut_sets.size()) - 1; }   // a set written by hand in the file
		st.shortcut[c] = k;
	}
	st.deadzone = std::clamp((prof.deadzone * 100 / 32767 + 2) / 5 * 5, 5, 95);
	st.naming = m_editing;
	st.name = m_edit;
	if (m_ctl_scr.tabs.empty()) m_ctl_scr = ui::make_controls(st);

	if (m_capture >= 0)
	{
		// the next press on the controller is the binding; pressing a bound input removes it
		if (m_captured) { prof.add(m_capture, m_capture_in); m_profiles.save(prof); m_capture = -1; }
		else if (m_menu_back || now > m_capture_until) m_capture = -1;
	}
	else if (m_editing)
	{
		// the name of a new profile, on the on-screen keyboard or typed
		if (kbd_step() == 1)
		{
			std::string nm = m_edit;
			while (!nm.empty() && nm.back() == ' ') nm.pop_back();
			if (!valid_profile_name(nm)) message("Invalid name");
			else if (m_profiles.find(nm) >= 0) message("A profile with that name already exists");
			else
			{
				pad_profile np = prof;          // the new profile starts as a copy of the current one
				np.name = nm;
				if (m_profiles.save(np)) { profile_slot(m_ctl_game, m_ctl_player) = nm; save_config(m_cfgpath, m_cfg); message("Profile created: " + nm); }
			}
			return;
		}
	}
	else
	{
		ui::event ev;
		while (ui_next(ev))
		{
			ui::result r = m_ctl_scr.handle(ev);
			const ui::item *it = m_ctl_scr.find(r.id);
			const bool on_action = r.id >= ui::ct_act0 && r.id < ui::ct_act0 + int(kNumActions);
			if (r.what == ui::result::back) { save_config(m_cfgpath, m_cfg); m_screen = m_back_to; return; }
			if (r.what == ui::result::changed && it)
			{
				if (r.id == ui::ct_game) { m_ctl_game = it->choice - 1; return; }
				if (r.id == ui::ct_player) { m_ctl_player = std::clamp(it->choice, 0, 3); return; }
				if (r.id == ui::ct_slot)
				{
					if (pad)
					{
						int const want = std::clamp(it->choice, 0, 3);
						if (want == m_ctl_player) m_cfg.pad_slot.erase(pad_guid(pad));   // its own place: nothing to keep
						else m_cfg.pad_slot[pad_guid(pad)] = want;
						save_config(m_cfgpath, m_cfg);
						message(st.pad + " plays as player " + std::to_string(want + 1), 2500);
					}
					else message("Connect this controller first", 2500);
					return;
				}
				if (r.id == ui::ct_profile)
				{
					int c = it->choice - extra;
					profile_slot(m_ctl_game, m_ctl_player) = c < 0 || c >= int(list.size()) ? std::string() : list[size_t(c)].name;
					save_config(m_cfgpath, m_cfg);
					return;
				}
				if (r.id == ui::ct_dead) { prof.deadzone = std::clamp(int(it->value + 0.5f) * 32767 / 100, 1600, 32000); m_profiles.save(prof); }
				if (r.id == ui::ct_short0 || r.id == ui::ct_short0 + 1)
				{
					const int which = r.id == ui::ct_short0 ? 0 : 1;
					if (it->choice >= 0 && it->choice < nmask) prof.combo[int(A_CUSTOM1) - int(kNumButtons) + which] = kMasks[it->choice];
					m_profiles.save(prof);
					return;
				}
			}
			if (r.what == ui::result::context && on_action) { prof.bind[r.id - ui::ct_act0].clear(); m_profiles.save(prof); return; }
			if (r.what != ui::result::activated) continue;
			if (on_action) { m_capture = r.id - ui::ct_act0; m_captured = false; m_capture_until = now + 5000; return; }
			std::string m, path;
			switch (r.id)
			{
				case ui::ct_new: kbd_open(ui::layout::text, "Name of the new profile", prof.name.substr(0, 18) + " copy", 24); return;
				case ui::ct_reset:
				{
					pad_profile d0 = pad_profile::defaults();
					d0.name = prof.name;
					prof = d0;
					m_profiles.save(prof);
					message("Profile reset: " + prof.name);
					return;
				}
				case ui::ct_delete:
				{
					std::string nm = prof.name;
					if (!m_profiles.remove(nm)) { message("The Default profile cannot be deleted"); return; }
					for (auto &p : m_cfg.pad_profile) if (p == nm) p = "Default";
					for (auto &g : m_cfg.game_profiles) for (auto &p : g.second) if (p == nm) p.clear();
					save_config(m_cfgpath, m_cfg);
					message("Profile deleted: " + nm);
					return;
				}
				case ui::ct_export:
				case ui::ct_export_setup:
				{
					bool ok;
					if (r.id == ui::ct_export) ok = m_profiles.export_profile(prof, path);
					else
					{
						std::string pl[4];
						for (int p = 0; p < 4; p++) pl[p] = profile_slot(m_ctl_game, p);
						ok = m_profiles.export_setup(m_ctl_game < 0 ? std::string() : game_list()[size_t(m_ctl_game)].id, pl, path);
					}
					if (ok)
					{
						SDL_SetClipboardText(path.c_str());
						message("Exported (path copied): " + path.substr(m_base.size()), 6000);
					}
					else message("Could not export " + path, 5000);
					return;
				}
				case ui::ct_import:
				{
					std::vector<import_result> setups;
					int n = m_profiles.import_folder(m, setups);
					for (auto &ir : setups) apply_setup(ir);
					if (m.empty()) message("Copy .ini profiles to controller_profiles/import or drop them on the window", 6000);
					else message(std::to_string(n) + " file(s) imported: " + m, 6000);
					return;
				}
			}
		}
	}

	ui::update_controls(m_ctl_scr, st);
	follow_theme(game_list()[size_t(std::clamp(m_sel, 0, ng - 1))].id, now);
	canvas cv(ui::screen::kW, ui::screen::kH);
	m_ctl_scr.draw(cv, m_theme, m_ui_in.last_device(), now);
	m_kbd.draw(cv, m_theme, m_ui_in.last_device(), now);
	if (m_capture >= 0) m_ctl_scr.toast("Press it now. Wait or Esc: cancel", now, 200);
	if (!m_message.empty() && SDL_GetTicks() < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	SDL_Delay(8);
}

// Online (U3.5): host or join, the host's address, the port and the input delay.
void app::net_menu_frame()
{
	ui_frame_begin();
	const unsigned now = SDL_GetTicks();
	const game_info &g = game_list()[size_t(m_sel)];
	auto state = [&]()
	{
		ui::online_state st;
		st.game = g.title;
		st.available = m_avail[size_t(m_sel)];
		st.address = m_cfg.net_host;
		st.port = m_cfg.net_port;
		st.delay = m_cfg.net_delay;
		st.local_ips = m_local_ips;
		st.typing = m_editing ? m_edit_what : 0;
		st.typed = m_edit;
		st.recent = m_cfg.net_recent;
		st.upnp = m_cfg.net_upnp;
		st.stun = m_cfg.net_stun;
		st.stun_server = m_cfg.net_stun_server;
		return st;
	};
	auto type = [&](int what, const std::string &start, bool join)
	{
		m_edit_what = what; m_edit_join = join;
		if (what == 1) kbd_open(ui::layout::text, "The host's code or address", start, 60);
		else kbd_open(ui::layout::number, "UDP port (1024 to 65535)", start, 5);
	};
	if (m_online_scr.tabs.empty()) m_online_scr = ui::make_online(state());
	bool join_now = false;
	if (m_editing)
	{
		// the address or the port, on the on-screen keyboard or typed
		if (kbd_step() == 1)
		{
			if (m_edit_what == 1) { m_cfg.net_host = m_edit; join_now = m_edit_join && !m_edit.empty(); }
			else
			{
				int port = 0;
				bool digits = !m_edit.empty() && m_edit.size() <= 5;
				for (char c : m_edit) digits = digits && c >= '0' && c <= '9';
				if (digits) port = atoi(m_edit.c_str());
				if (port >= 1024 && port <= 65535) m_cfg.net_port = port;
				else message("The port is a number from 1024 to 65535", 4000);
			}
		}
	}
	else
	{
		ui::event ev;
		while (ui_next(ev))
		{
			ui::result r = m_online_scr.handle(ev);
			if (r.what == ui::result::back) { m_screen = screen::menu; return; }
			if (r.what == ui::result::changed && r.id == ui::on_delay)
			{
				if (const ui::item *it = m_online_scr.find(r.id)) m_cfg.net_delay = std::clamp(int(it->value + 0.5f), 1, 10);
			}
			if (r.what == ui::result::changed && (r.id == ui::on_upnp || r.id == ui::on_stun))
				if (const ui::item *it = m_online_scr.find(r.id)) { (r.id == ui::on_upnp ? m_cfg.net_upnp : m_cfg.net_stun) = it->on; save_config(m_cfgpath, m_cfg); }
			if (r.what != ui::result::activated) continue;
			if (r.id == ui::on_host)
			{
				std::string err;
				if (!m_avail[size_t(m_sel)]) message(m_why[size_t(m_sel)], 6000);
				else if (!m_net.host(u16(m_cfg.net_port), err)) message("Network: " + err, 6000);
				else { net_traversal_start(); m_lobby_scr = ui::screen(); m_screen = screen::net_lobby; return; }
			}
			else if (r.id == ui::on_join)
			{
				if (m_cfg.net_host.empty()) { type(1, "", true); break; }
				join_now = true;
			}
			else if (r.id == ui::on_address) { type(1, m_cfg.net_host, false); break; }
			else if (r.id == ui::on_port) { type(2, std::to_string(m_cfg.net_port), false); break; }
			else if (r.id >= ui::on_recent0 && r.id < ui::on_recent0 + int(m_cfg.net_recent.size()))
			{
				m_cfg.net_host = m_cfg.net_recent[size_t(r.id - ui::on_recent0)];
				join_now = true;
			}
		}
	}
	if (join_now)
	{
		std::string err;
		if (!m_net.join(m_cfg.net_host, u16(m_cfg.net_port), err)) message("Network: " + err, 6000);
		else
		{
			// remembered, the last one first, so it is typed once (U4.2)
			auto &rc = m_cfg.net_recent;
			rc.erase(std::remove(rc.begin(), rc.end(), m_cfg.net_host), rc.end());
			rc.insert(rc.begin(), m_cfg.net_host);
			if (rc.size() > 5) rc.resize(5);
			save_config(m_cfgpath, m_cfg);
			net_traversal_start();
			m_join_t = SDL_GetTicks();
			m_lobby_scr = ui::screen();
			m_screen = screen::net_lobby;
			return;
		}
	}
	ui::update_online(m_online_scr, state());
	ui_show(m_online_scr);
}

// N2: when hosting, the router is asked to forward the port (UPnP, else PCP or NAT-PMP); host and player both ask
// a STUN server for the address and port the internet sees, which is their code.
void app::net_traversal_start()
{
	if (m_cfg.net_upnp && m_net.role() == netplay::mode::host) m_upnp.start(u16(m_cfg.net_port));
	if (!m_cfg.net_stun || m_cfg.net_stun_server.empty()) return;
	const std::string &sv = m_cfg.net_stun_server;
	size_t c = sv.rfind(':');
	net_addr a;
	std::string err;
	if (udp_socket::resolve(sv.substr(0, c), c == std::string::npos ? 3478 : u16(atoi(sv.c_str() + c + 1)), a, err)) m_net.stun_begin(a);
}

// this computer's code and a line on how it was found
std::string app::net_code_note(std::string &code)
{
	code.clear();
	const bool host = m_net.role() == netplay::mode::host;
	// IPv6 has no router in the way of the address: this computer's own address and port
	net_addr six{};
	if (m_local6.v6 && m_net.socket().has_ipv6()) { six = m_local6; six.port = m_net.socket().local_port(); }
	std::string note = note_v4(code, host);
	if (six.v6)
	{
		const bool had4 = !code.empty();
		code = net_code_pair(had4 ? m_code4 : net_addr{}, six);
		note = had4 ? note + " The code ends with your IPv6 address: a player with IPv6 tries that first."
		            : "Your code holds only your IPv6 address: players with IPv6 can join with it.";
	}
	// N2.3: a player who cannot reach the host goes through a player already in
	net_addr relay = host ? m_net.relay_candidate() : net_addr{};
	if (relay.port && m_code4.ip)
		note += " A player who still cannot connect can join through a player already in with this code: " + net_code(m_code4) + "-" + net_code(relay) + ".";
	return note;
}

std::string app::note_v4(std::string &code, bool host)
{
	m_code4 = {};
	net_addr ext{};
	if (host && m_upnp.status() == upnp_mapper::state::mapped)
	{
		std::string ip = m_upnp.external_ip();
		int a, b, c, d;
		if (sscanf(ip.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4) ext = { u32(a) << 24 | u32(b) << 16 | u32(c) << 8 | u32(d), u16(m_cfg.net_port) };
		if (ext.ip) m_net.also_public(ext.ip);
		if (ext.ip && !net_ip_private(ext.ip)) { m_code4 = ext; code = net_code(ext); return "Your router's port is open (" + m_upnp.method() + "): anyone can join with this code."; }
	}
	if (m_net.stun_status() == netplay::stun_state::found)
	{
		m_code4 = m_net.public_addr();
		code = net_code(m_code4);
		if (host) return ext.ip ? "Your router is behind another one, so its open port does not reach the internet. Let each player in by their code."
		                        : "Players join with this code. If one cannot, let them in by their code.";
		return "If the host's code alone does not connect, send them this one.";
	}
	if ((host && m_upnp.status() == upnp_mapper::state::working) || m_net.stun_status() == netplay::stun_state::asking) return "Finding your public address...";
	if (!m_cfg.net_stun && !(host && m_cfg.net_upnp)) return "Codes are off (Online menu). Use an address.";
	return "No public address found. Forward the UDP port on your router, or use a virtual LAN.";
}

// the Lua mods the host runs for this game, for the lobby (roadmap 1.7)
std::string app::net_mod_names(const std::string &game_id)
{
	std::string names;
	for (const auto &n : m_mods.active_names(game_id)) names += (names.empty() ? "" : ", ") + n;
	return names.empty() ? "none" : names;
}

void app::net_lobby_frame()
{
	m_net.poll();
	if (!m_net.active())
	{
		message("Network: " + m_net.error(), 6000);
		m_screen = screen::net_menu;
		return;
	}
	ui_frame_begin();
	const game_info &g = game_list()[size_t(m_sel)];
	bool host = m_net.role() == netplay::mode::host;
	auto state = [&]()
	{
		ui::lobby_state st;
		st.host = host;
		st.game = g.title;
		st.local_ips = m_local_ips;
		st.port = m_cfg.net_port;
		st.delay = m_cfg.net_delay;
		st.address = m_cfg.net_host;
		st.slot = m_net.local_slot();
		st.ping = m_net.ping_ms();
		st.count = m_net.players();
		st.receiving = m_net.stage() == netplay::phase::transfer;
		st.code_note = net_code_note(st.code);
		st.inviting = m_net.invites();
		st.no_answer = !host && st.slot < 0 && SDL_GetTicks() - m_join_t > 5000;
		if (host) m_net.set_lobby_mods(net_mod_names(g.id));
		st.mods = m_net.lobby_mods();
		// N2.4: the connection type and every link
		bool forwarded = host && m_upnp.status() == upnp_mapper::state::mapped && !st.code.empty() && st.code_note.find("port is open") != std::string::npos;
		if (m_net.stun_status() == netplay::stun_state::found)
			for (const std::string &ip : m_local_ips) forwarded = forwarded || ip == net_ip_str(m_net.public_addr().ip);   // no router in between
		st.connection = netplay::conn_name(m_net.connection(forwarded));
		auto path = [](const netplay::link_info &l)
		{
			// short enough for one line of the players' panel
			std::string s = std::to_string(l.ping) + " ms, jitter " + std::to_string(l.jitter) + ", ";
			if (l.via_host) return s + "via host";
			if (l.relayed) return s + (l.through >= 0 ? "via P" + std::to_string(l.through + 1) : std::string("relayed"));
			return s + (l.v6 ? "direct IPv6" : "direct");
		};
		const auto links = m_net.links();
		if (!host)
			for (auto &l : links) st.links.push_back({ l.slot == 0 ? std::string("Host") : "Player " + std::to_string(l.slot + 1), path(l) });
		if (host)
		{
			st.players.push_back({ "Player 1", "you (host)" });
			// a peer's line is its slot, then its address, ping and whether it is ready
			for (const std::string &p : m_net.peer_list())
			{
				size_t sp = p.find("  ");
				std::string slot = p.substr(0, sp), rest = sp == std::string::npos ? "" : p.substr(sp + 2);
				int n = slot.size() > 1 ? atoi(slot.c_str() + 1) - 1 : -1;
				for (auto &l : links)
					if (l.slot == n) rest = path(l) + (rest.find("ready") != std::string::npos ? ", ready" : "");
				st.players.push_back({ "Player " + (slot.size() > 1 ? slot.substr(1) : slot), rest });
			}
		}
		return st;
	};
	if (m_lobby_scr.tabs.empty()) m_lobby_scr = ui::make_lobby(state());
	bool start = false;
	if (m_editing)
	{
		// a player's code, to let them in
		if (kbd_step() == 1)
		{
			net_addr a, a6;
			if (net_code_parse(m_edit, a, &a6))
			{
				m_net.invite(a);
				if (a6.v6) m_net.invite(a6);
				message("Letting " + net_code(a) + " in for 60 seconds", 3000);
			}
			else message("That is not a code: letters and digits, like ABCDE-FGHJK", 5000);
		}
		ui::update_lobby(m_lobby_scr, state());
		ui_show(m_lobby_scr);
		return;
	}
	ui::event ev;
	while (ui_next(ev))
	{
		ui::result r = m_lobby_scr.handle(ev);
		if (r.what == ui::result::activated && r.id == ui::lb_invite) { kbd_open(ui::layout::text, "The player's code", "", 48); break; }
		if (r.what == ui::result::activated && r.id == ui::lb_copy)
		{
			std::string code;
			net_code_note(code);
			if (code.empty()) message("No code yet", 3000);
			else { SDL_SetClipboardText(code.c_str()); message("Copied: " + code, 3000); }
			continue;
		}
		if (r.what == ui::result::back || (r.what == ui::result::activated && r.id == ui::lb_leave)) { m_net.stop(); m_screen = screen::net_menu; return; }
		if (r.what == ui::result::activated && r.id == ui::lb_start) start = true;
		if (r.what == ui::result::changed && r.id == ui::lb_delay)
			if (const ui::item *it = m_lobby_scr.find(r.id)) m_cfg.net_delay = std::clamp(int(it->value + 0.5f), 1, 10);
	}

	if (host && start)
	{
		m_net_game = true;
		if (!start_game(m_sel)) { m_net_game = false; m_net.stop(); m_screen = screen::net_menu; return; }
		m_net.host_start(g.id, m_mach->fingerprint(), m_cfg.dips, m_mach->nvram(), m_cfg.net_delay, m_mods.net_description(g.id));
		message("Online game: " + std::to_string(m_net.players()) + " players", 3000);
		return;
	}
	if (!host && m_net.start_received())
	{
		int idx = -1;
		for (size_t i = 0; i < game_list().size(); i++) if (m_net.start_game_id() == game_list()[i].id) idx = int(i);
		std::string why;
		if (idx < 0) why = "unknown game: " + m_net.start_game_id();
		else if (!m_avail[size_t(idx)]) why = std::string("you don't have ") + game_list()[size_t(idx)].title;
		// exactly the host's Lua mods, with its options (roadmap 1.7); this computer's own
		// settings come back when the game ends
		if (why.empty()) m_mods.net_apply(game_list()[size_t(idx)].id, m_net.start_mods(), why);
		if (why.empty())
		{
			m_net_game = true;
			if (!start_game(idx, true)) why = "the game could not be loaded";
			else if (m_mach->fingerprint() != m_net.start_fingerprint()) why = "your ROMs (or Lua mods) are not the same as the host's";
		}
		if (!why.empty())
		{
			m_net.client_ready(false, why);
			m_mods.net_restore();
			m_net.poll();
			m_wide.reset();
			m_wide_possible = false;
			m_mach.reset();
			m_net_game = false;
			m_net.stop(why.c_str());
			message("Network: " + why, 6000);
			m_screen = screen::net_menu;
			return;
		}
		m_mach->set_nvram(m_net.start_nvram());
		m_mach->set_dips(m_net.start_dips());
		m_mach->reset();
		m_net.client_ready(true, "");
		return;
	}

	ui::update_lobby(m_lobby_scr, state());
	ui_show(m_lobby_scr);
}

// One frame of the game, when it is due. menu_open: the pause menu is up, so offline nothing
// runs and online this computer sends neutral inputs.
bool app::step_game(bool menu_open)
{
	bool ran = false;
	if (m_net_game)
	{
		m_net.poll();
		if (!m_net.active()) { stop_game(); return false; }
		if (m_net.can_run())
		{
			cabinet_inputs local = menu_open ? menu_inputs() : gather_inputs();
			if (!menu_open) m_mods.apply_presses(local, m_net.local_slot());   // nba.press (1.6), sent like the pad
			cabinet_inputs in = m_net.frame_inputs(local.p[0]);
			m_mods.take_inputs(in);                                    // nba.input / nba.hold (6.F2)
			m_mach->set_inputs(in);
			m_mach->run_frame();
			m_mods.frame();
			m_net.frame_done(m_mach->state_hash());
			ran = true;
		}
	}
	else if (!m_paused && !menu_open)
	{
		cabinet_inputs in = gather_inputs();
		m_mods.apply_presses(in);                                          // nba.press (1.6)
		m_mods.take_inputs(in);                                            // nba.input / nba.hold (6.F2)
		m_mach->set_inputs(in);
		m_mach->run_frame();
		m_mods.frame();
		ran = true;
	}
	for (const auto &l : m_mods.take_log()) message(l, 4000);
	if (ran)
	{
		for (int &c : m_coin_pulse) c = std::max(0, c - 1);
		m_service_pulse = std::max(0, m_service_pulse - 1);
		m_abuf.clear();
		m_mach->take_audio(m_abuf);
		m_join.apply(m_abuf);
		if (m_audio && !m_abuf.empty())
		{
			for (float &v : m_abuf) v = std::clamp(v * m_cfg.volume * 1.6f, -1.0f, 1.0f);
			// keep latency bounded (about 120 ms)
			if (SDL_GetQueuedAudioSize(m_audio) < 48000 * 4 / 8)
				SDL_QueueAudio(m_audio, m_abuf.data(), Uint32(m_abuf.size() * sizeof(float)));
		}
	}
	return ran;
}

bool app::game_frame()
{
	// Esc, the Guide button, or the held shortcut: the pause menu (U5.1)
	if (m_menu_back || pause_shortcut()) { open_pause(); return false; }
	bool ran = step_game(false);
	if (m_screen != screen::game) return ran;
	update_wide();
	int w = m_mach->width(), h = m_mach->height();
	// in play, with Widescreen: the wide picture; on fixed screens the game's own 4:3 picture
	m_presenting_wide = m_wide && m_wide->show_wide() && m_mach->pens();
	if (m_presenting_wide) w = m_wide->columns();
	canvas cv(w, h);
	if (m_presenting_wide)
	{
		const u32 *pens = m_mach->pens();
		for (int y = 0; y < h && y < wide_view::HEIGHT; y++)
		{
			const u16 *src = m_wide->picture() + size_t(y) * m_wide->pitch();
			u32 *dst = &cv.px[size_t(y) * w];
			for (int x = 0; x < w; x++) dst[x] = pens[src[x] & 0x7fff] | 0xff000000u;
		}
	}
	else
		std::memcpy(cv.px.data(), m_mach->frame(), size_t(w) * h * 4);
	if (m_shot) save_screenshot(cv.px.data(), w, h);      // the game picture, without the overlay
	draw_mod_overlay(cv, m_mods.draws());                  // nba.text / rect / line (1.6)
	draw_overlay(cv);
	present(cv.px.data(), w, h, true);
	return ran;
}

// ---------------------------------------------------------------- save states (N1.4)
// Offline only: the pause menu shows Save and Load when save_states is on and the game is
// not online, and both check that again here. A slot holds the machine and the Lua
// scripts' state (state_file.h); it loads only into the same game and the same build.
std::string app::slot_path(int n) const
{
	return path_join(path_join(path_join(m_base, "states"), game_list()[size_t(m_cur_game)].id), "slot" + std::to_string(n + 1) + ".state");
}

void app::read_slots()
{
	for (int n = 0; n < 3; n++)
	{
		m_slot_info[n].clear();
		std::vector<u8> f;
		state_file::header h;
		if (!read_whole_file(slot_path(n), f)) continue;
		if (!state_file::read_header(f, h)) { m_slot_info[n] = "damaged"; continue; }
		if (h.build != app_version()) { m_slot_info[n] = "other version"; continue; }
		// the time when it was saved today, else the day (the menu's value column is narrow)
		std::time_t t = std::time_t(h.saved_at), now = std::time(nullptr);
		std::tm a = *std::localtime(&t), b = *std::localtime(&now);
		char when[32];
		std::strftime(when, sizeof(when), a.tm_yday == b.tm_yday && a.tm_year == b.tm_year ? "%H:%M" : "%d %b", &a);
		m_slot_info[n] = when;
	}
}

void app::save_slot(int n)
{
	std::vector<u8> mach, scripts, file;
	std::string err;
	if (!m_mach->save_state_to(mach)) { message("Could not save: " + m_mach->state_error(), 4000); return; }
	if (!m_mods.save_state(scripts, err)) { message("Could not save: " + err, 4000); return; }
	state_file::header h;
	h.build = app_version();
	h.game = game_list()[size_t(m_cur_game)].id;
	h.saved_at = int64_t(std::time(nullptr));
	h.hash = m_mach->state_hash();
	if (!state_file::pack(h, mach, scripts, file) || !write_whole_file(slot_path(n), file)) { message("Could not write " + slot_path(n), 4000); return; }
	SDL_Log("save state: slot %d saved, game state %08x, %zu bytes", n + 1, h.hash, file.size());
	message("Saved in slot " + std::to_string(n + 1), 2000);
	read_slots();
}

bool app::load_slot(int n)
{
	std::vector<u8> file, mach, scripts, before;
	std::string err;
	state_file::header h;
	if (!read_whole_file(slot_path(n), file)) { message("Slot " + std::to_string(n + 1) + " is empty", 2000); return false; }
	if (!state_file::unpack(file, h, mach, scripts, err)) { message("Could not load: " + err, 4000); return false; }
	if (h.game != game_list()[size_t(m_cur_game)].id) { message("Could not load: a state of another game", 4000); return false; }
	if (h.build != app_version()) { message("Could not load: saved by another version (" + h.build + ")", 4000); return false; }
	// a machine load that fails part way leaves the machine changed: keep it as it is now
	if (!m_mach->save_state_to(before)) { message("Could not load: " + m_mach->state_error(), 4000); return false; }
	bool mach_ok = true;
	if (!m_mods.load_state(scripts, err, [&] { mach_ok = m_mach->load_state(mach); if (!mach_ok) err = m_mach->state_error(); return mach_ok; }))
	{
		if (!mach_ok) m_mach->load_state(before);
		message("Could not load: " + err, 4000);
		return false;
	}
	if (m_audio) SDL_ClearQueuedAudio(m_audio);
	m_join.jump();
	const u32 now = m_mach->state_hash();
	SDL_Log("save state: slot %d loaded, game state %08x%s", n + 1, now, now == h.hash ? ", as saved" : ", NOT as saved");
	message("Loaded slot " + std::to_string(n + 1), 2000);
	return true;
}

// ---------------------------------------------------------------- pause menu (U5)
bool app::pause_held(SDL_GameController *c)
{
	auto b = [&](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(c, x) != 0; };
	if (m_cfg.pause_buttons == 2) return b(SDL_CONTROLLER_BUTTON_LEFTSTICK) && b(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
	if (m_cfg.pause_buttons == 1) return b(SDL_CONTROLLER_BUTTON_START) && b(SDL_CONTROLLER_BUTTON_BACK);
	return b(SDL_CONTROLLER_BUTTON_START);
}

// The held shortcut, on any controller: true once it has been held half a second (Start alone:
// one second), then not again until its buttons are let go.
bool app::pause_shortcut()
{
	bool held = false;
	for (size_t i = 0; i < m_pads.size(); i++)
		if (pause_held(m_pads[i]))
		{
			held = true;
			if (i < 4 && m_combo_since && SDL_GetTicks() - m_combo_since >= (m_cfg.pause_buttons == 0 ? kStartHoldMs : 500u)) m_start_used[i] = true;
		}
	const unsigned now = SDL_GetTicks();
	if (!held) { m_combo_since = 0; m_combo_wait = false; return false; }
	if (m_combo_wait) return false;
	if (!m_combo_since) m_combo_since = now ? now : 1;
	if (now - m_combo_since < (m_cfg.pause_buttons == 0 ? kStartHoldMs : 500u)) return false;
	m_combo_wait = true;
	return true;
}

void app::open_pause()
{
	ui::pause_state st;
	st.game = game_list()[size_t(m_cur_game)].title;
	st.online = m_net_game;
	st.states = m_cfg.save_states && !m_net_game;
	if (st.states) read_slots();
	st.slot = m_state_slot;
	for (int n = 0; n < 3; n++) st.slots[n] = m_slot_info[n];
	m_pause_scr = ui::make_pause(st);
	m_screen = screen::pause;
	m_show_help = false;
	if (!m_net_game) SDL_ClearQueuedAudio(m_audio);     // offline the game stops: so does its sound
	SDL_Log("pause menu opened, game state %08x", m_mach->state_hash());
}

void app::resume_game()
{
	save_config(m_cfgpath, m_cfg);                        // the volume may have changed
	SDL_Log("pause menu closed, game state %08x", m_mach->state_hash());
	m_screen = screen::game;
}

bool app::pause_frame()
{
	ui_frame_begin();
	const unsigned now = SDL_GetTicks();
	// online the game goes on for everybody, with nothing pressed here
	bool ran = m_net_game ? step_game(true) : false;
	if (m_screen != screen::pause || !m_mach) return ran;     // the online game ended
	auto state = [&]()
	{
		ui::pause_state st;
		st.game = game_list()[size_t(m_cur_game)].title;
		st.online = m_net_game;
		st.volume = m_cfg.volume;
		st.test = m_test;
		for (int p = 0; p < 4; p++) st.coins[p] = (m_coin_pulse[p] + 7) / 8;
		st.service = (m_service_pulse + 7) / 8;
		st.states = m_cfg.save_states && !m_net_game;
		st.slot = m_state_slot;
		for (int n = 0; n < 3; n++) st.slots[n] = m_slot_info[n];
		return st;
	};
	if (m_menu_guide || pause_shortcut()) { resume_game(); return ran; }
	ui::event ev;
	while (ui_next(ev))
	{
		ui::result r = m_pause_scr.handle(ev);
		const ui::item *it = m_pause_scr.find(r.id);
		if (r.what == ui::result::back) { resume_game(); return ran; }
		if (r.what == ui::result::yes && r.id == ui::ps_quit) { stop_game(); return ran; }
		if (r.what == ui::result::changed && it)
		{
			if (r.id == ui::ps_volume) m_cfg.volume = std::clamp(std::round(it->value * 10.0f) / 10.0f, 0.0f, 1.0f);
			if (r.id == ui::ps_test && !m_net_game) { m_test = it->on; message(m_test ? "TEST switch: ON (service menu)" : "TEST switch: OFF", 2000); }
			if (r.id == ui::ps_slot) m_state_slot = std::clamp(it->choice, 0, 2);
		}
		if (r.what != ui::result::activated) continue;
		switch (r.id)
		{
		case ui::ps_resume: resume_game(); return ran;
		case ui::ps_shot:
		{
			int w = m_mach->width(), h = m_mach->height();
			canvas g(w, h);
			std::memcpy(g.px.data(), m_mach->frame(), size_t(w) * h * 4);
			save_screenshot(g.px.data(), w, h);           // the game picture, without the menu
			break;
		}
		case ui::ps_quit:
			if (m_net_game) m_pause_scr.ask("Leave the online game? It ends for you.", ui::ps_quit);
			else { stop_game(); return ran; }
			break;
		case ui::ps_service: if (!m_net_game) m_service_pulse += 8; break;
		case ui::ps_state_save: if (!m_net_game && m_cfg.save_states) save_slot(m_state_slot); break;
		case ui::ps_state_load: if (!m_net_game && m_cfg.save_states && load_slot(m_state_slot)) { resume_game(); return ran; } break;
		case ui::ps_reset: if (!m_net_game) { m_mach->reset(); message("Reset", 1500); resume_game(); return ran; } break;
		// the full screens, offline only; Back there returns here
		case ui::ps_video:
			m_overlays = list_overlays(m_base);
			m_settings = ui::make_settings(settings_now());
			m_back_to = screen::pause; m_screen = screen::settings; return ran;
		case ui::ps_controls: m_ctl_scr = ui::screen(); m_back_to = screen::pause; m_screen = screen::controls; return ran;
		case ui::ps_mods: m_mod_sel = 0; m_mods_scr = ui::screen(); m_back_to = screen::pause; m_screen = screen::mods; return ran;
		case ui::ps_help:
			m_about_scr = ui::make_about(about_now());
			m_about_scr.cur_tab = 1;
			m_about_scr.fix_focus();
			m_back_to = screen::pause; m_screen = screen::about; return ran;
		default:
			if (r.id >= ui::ps_coin0 && r.id < ui::ps_coin0 + 4 && (!m_net_game || r.id == ui::ps_coin0)) m_coin_pulse[r.id - ui::ps_coin0] += 8;
			break;
		}
	}
	ui::update_pause(m_pause_scr, state());

	// drawn over the game picture, darkened
	follow_theme(game_list()[size_t(m_cur_game)].id, now);
	ui::theme th = m_theme;
	const int w = m_mach->width(), h = m_mach->height();
	const u32 *src = m_mach->frame();
	// the game's 4:3 picture in the middle, black at the sides of a wider menu
	const int gw = 400, gx = (ui::screen::kW - gw) / 2;
	th.backdrop.assign(size_t(ui::screen::kW) * ui::screen::kH, 0xff000000);
	for (int y = 0; y < ui::screen::kH; y++)
		for (int x = gx; x < gx + gw; x++)
		{
			u32 c = src[size_t(y * h / ui::screen::kH) * size_t(w) + size_t((x - gx) * w / gw)];
			th.backdrop[size_t(y) * ui::screen::kW + size_t(x)] = 0xff000000 | ((c >> 1) & 0x7f7f7f);
		}
	th.panel_alpha = 215;
	canvas cv(ui::screen::kW, ui::screen::kH);
	m_pause_scr.draw(cv, th, m_ui_in.last_device(), now);
	if (m_net_game && m_net.desynced()) m_pause_scr.toast("DESYNC: restart the online game", now, 500);
	if (m_shot) save_screenshot(cv.px.data(), cv.w, cv.h);
	if (!m_message.empty() && now < m_message_until) menu_message(cv, m_message);
	present(cv.px.data(), cv.w, cv.h, false);
	return ran;
}

// Where the user's files live (R1.4): next to the executable for a portable copy, the
// per-user folder otherwise. An installed copy that finds an older setup next to the
// executable offers to move it, once.
void app::choose_data_folder()
{
	char *bp = SDL_GetBasePath();
	std::string exe = bp ? bp : ".";
	SDL_free(bp);
	const char *env = getenv("NBAJAM_DATA_DIR");
	std::string user;
	// SDL_GetPrefPath creates the folder, so only ask for it when it will be used
	if (!(env && *env) && !file_exists(path_join(exe, "portable.txt")))
		if (char *pp = SDL_GetPrefPath("", "NBAJamRecomp")) { user = pp; SDL_free(pp); }
	m_paths = resolve_paths(exe, user, env);
	if (legacy_setup_present(m_paths))
	{
		std::string text =
			"Your settings, saves, mods and ROMs are next to the program. This version keeps them in your user folder:\n\n"
			+ m_paths.data_dir + "\n\n"
			"Move: takes them there (recommended).\n"
			"Keep here: everything stays in this folder (portable mode).\n"
			"Later: nothing changes and you are asked again.";
		const SDL_MessageBoxButtonData buttons[] = {
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Move" },
			{ 0, 2, "Keep here" },
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Later" },
		};
		const SDL_MessageBoxData box = { SDL_MESSAGEBOX_INFORMATION, nullptr, "NBA JAM Recomp", text.c_str(), 3, buttons, nullptr };
		int choice = 0;
		if (SDL_ShowMessageBox(&box, &choice) != 0) choice = 0;
		if (choice == 1)
		{
			auto failed = migrate_legacy_setup(m_paths);
			if (!failed.empty())
			{
				std::string names;
				for (const auto &f : failed) names += (names.empty() ? "" : ", ") + f;
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "NBA JAM Recomp",
					("Could not be moved: " + names + "\nThey are still in: " + m_paths.exe_dir).c_str(), nullptr);
			}
		}
		else if (choice == 2)
		{
			if (make_portable(m_paths)) m_paths = resolve_paths(exe, user, env);
			else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "NBA JAM Recomp",
				"The program folder cannot be written; the user folder is used.", nullptr);
		}
		else
		{
			// asked again next time; until then keep using the old setup where it is, if it can be written
			app_paths old = m_paths;
			old.data_dir = old.exe_dir;
			std::string probe = path_join(exe, ".write_test");
			if (write_whole_file(probe, {})) { remove(probe.c_str()); m_paths = old; m_paths.portable = true; }
		}
	}
	seed_data_dir(m_paths);
	m_base = m_paths.data_dir;
	install_crash_dumps(m_base);
	// NBAJAM_CRASH_TEST=1: crash here on purpose, to check that a dump is written
	if (const char *ct = getenv("NBAJAM_CRASH_TEST")) if (*ct == '1') *(volatile int *)nullptr = 1;
	SDL_Log("data folder: %s%s", m_base.c_str(), m_paths.portable ? " (portable)" : "");
}

// The widget gallery of the menu toolkit (roadmap U1), driven by the real devices through
// the unified input layer: controller, keyboard, mouse and touch. Back leaves it.
void app::gallery_loop()
{
	// the last tab tries the theme of each game family (U2.3), with the art of the user's ROMs
	std::vector<ui::theme_spec> specs;
	std::vector<std::string> names;
	for (const std::string &id : ui::theme_ids()) { specs.push_back(ui::load_theme_spec(id, m_base)); names.push_back(specs.back().name); }
	std::map<std::string, rom_art::art_set> arts;          // read when a theme first asks for a game
	ui::screen scr = ui::make_gallery(names);
	ui::input in;
	ui::theme th, from, to;                                 // th is what is drawn; a change fades from `from` to `to`
	unsigned fade_start = 0;
	bool fading = false;
	while (true)
	{
		int dw = 0, dh = 0, ww = 0, wh = 0;
		if (m_gl.ok()) m_gl.drawable_size(dw, dh); else if (m_ren) SDL_GetRendererOutputSize(m_ren, &dw, &dh);
		SDL_GetWindowSize(m_win, &ww, &wh);
		// the menu picture's place, in the units the mouse reports (window points, not pixels)
		view_rect shown = menu_view(dw, dh, ui::screen::kW, ui::screen::kH, m_cfg.tv_margins);
		if (dw > 0 && dh > 0)
		{
			shown.x = shown.x * ww / dw; shown.w = shown.w * ww / dw;
			shown.y = shown.y * wh / dh; shown.h = shown.h * wh / dh;
		}
		unsigned now = SDL_GetTicks();
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT) return;
			if (e.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(e.cdevice.which);
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11)
			{
				m_cfg.fullscreen = !m_cfg.fullscreen;
				SDL_SetWindowFullscreen(m_win, m_cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
				continue;
			}
			if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_F12 || e.key.keysym.sym == SDLK_PRINTSCREEN)) { m_shot = true; continue; }
			ui::raw_event raw;
			if (ui::from_sdl(e, shown, ww, wh, raw)) in.feed(raw, now);
		}
		in.tick(now);
		ui::event ev;
		while (in.next(ev))
		{
			ui::result r = scr.handle(ev);
			if (r.what == ui::result::activated && r.id >= ui::g_theme0 && r.id < ui::g_theme0 + int(specs.size()))
			{
				const ui::theme_spec &sp = specs[size_t(r.id - ui::g_theme0)];
				const rom_art::art_set *art = nullptr;
				for (const std::string &gid : sp.games)
				{
					if (!arts.count(gid))
					{
						rom_art::roms roms;
						arts[gid] = rom_art::art_set();
						for (const game_info &g : game_list()) if (gid == g.id && m_cat->art_roms(g, roms)) arts[gid] = rom_art::read(roms);
					}
					if (!arts[gid].empty()) { art = &arts[gid]; break; }
				}
				from = th; to = ui::make_theme(sp, art);
				fade_start = now; fading = true;
				scr.toast(art || sp.games.empty() ? sp.name : sp.name + ": no ROMs, so the neutral theme", now);
			}
			if (ui::gallery_react(scr, r, now)) return;
		}
		if (fading)
		{
			float t = float(now - fade_start) / float(ui::kThemeFadeMs);
			th = ui::mix_themes(from, to, t);
			fading = t < 1;
		}
		canvas cv(ui::screen::kW, ui::screen::kH);
		scr.draw(cv, th, in.last_device(), now);
		present(cv.px.data(), cv.w, cv.h, false);
		SDL_Delay(8);
	}
}

// The test screen of the ROM art reader (roadmap U2.2): lettering, team logos and
// backgrounds read from each game's ROMs, or the project lettering where there are none.
// Left and right change game, up and down change page, Back leaves.
void app::rom_art_loop()
{
	ui::input in;
	ui::theme th;
	std::map<int, rom_art::art_set> sets;       // read when a game is first shown
	int game = m_sel, page = 0;
	const int games = int(game_list().size());
	while (true)
	{
		unsigned now = SDL_GetTicks();
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT) return;
			if (e.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(e.cdevice.which);
			if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_F12 || e.key.keysym.sym == SDLK_PRINTSCREEN)) { m_shot = true; continue; }
			ui::raw_event raw;
			if (ui::from_sdl(e, view_rect(), 1, 1, raw)) in.feed(raw, now);
		}
		in.tick(now);
		if (!sets.count(game))
		{
			rom_art::roms roms;
			sets[game] = m_cat->art_roms(game_list()[size_t(game)], roms) ? rom_art::read(roms) : rom_art::art_set();
		}
		const rom_art::art_set &set = sets[game];
		ui::event ev;
		while (in.next(ev))
		{
			if (ev.a == ui::act::back) return;
			if (ev.a == ui::act::left) { game = (game + games - 1) % games; page = 0; }
			if (ev.a == ui::act::right) { game = (game + 1) % games; page = 0; }
			if (ev.a == ui::act::up) page = std::max(0, page - 1);
			if (ev.a == ui::act::down) page = std::min(ui::art_pages(set) - 1, page + 1);
		}
		if (!sets.count(game)) continue;         // the game changed: read it first
		canvas cv(ui::screen::kW, ui::screen::kH);
		ui::draw_art_page(cv, th, game_list()[size_t(game)].title, sets[game], page, in.last_device());
		present(cv.px.data(), cv.w, cv.h, false);
		SDL_Delay(8);
	}
}

int app::run()
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	// Windows: draw at the real resolution of each monitor instead of being enlarged by the
	// system at 150% or 200% display scaling (W1.4)
	SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
	SDL_Log("%s", app_version_line().c_str());
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "NBA JAM Recomp", SDL_GetError(), nullptr);
		return 1;
	}
	choose_data_folder();
	m_cfgpath = path_join(m_base, "nbajam_recomp.ini");
	load_config(m_cfgpath, m_cfg);
	if (m_cfg.news) m_news.start();          // background; the menu never waits for it
	m_cat = std::make_unique<game_catalog>(m_base);
	m_mods.scan(path_join(m_base, "mods"), path_join(m_base, "mods.ini"));
	m_profiles.init(m_base);

	open_window();
	// overlays / borders: what is in overlays/ in the data folder, and the one chosen last time
	m_overlays = list_overlays(m_base);
	m_gl.set_overlay_opacity(m_cfg.overlay_opacity);
	if (m_cfg.side_fill == 2 && !use_overlay(m_cfg.overlay)) m_cfg.side_fill = 0;
	SDL_StopTextInput();
	SDL_AudioSpec want{}, have{};
	want.freq = 48000;
	want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = 1024;
	m_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	if (m_audio) SDL_PauseAudioDevice(m_audio, 0);
	if (m_audio)
		m_msound = std::make_unique<menu_sound>(
			[this](const std::string &id) -> std::unique_ptr<arcade_machine>
			{
				for (const game_info &g : game_list())
					if (g.id == id) { std::string err; return m_cat->create(g, err); }
				return nullptr;
			},
			[this] { return size_t(SDL_GetQueuedAudioSize(m_audio) / sizeof(float)); },
			[this](const float *p, size_t n) { SDL_QueueAudio(m_audio, p, Uint32(n * sizeof(float))); });

	refresh_availability();
	// start on the first available game
	for (size_t i = 0; i < m_avail.size(); i++) if (m_avail[i]) { m_sel = int(i); break; }

	if (!m_script_path.empty() && !start_pad_script()) return 2;
	if (m_gallery) { gallery_loop(); m_quit = true; }
	if (m_rom_art) { rom_art_loop(); m_quit = true; }

	using clock = std::chrono::steady_clock;
	auto next = clock::now();
	while (!m_quit)
	{
		poll_events();
		if (m_quit) break;
		if (!m_net.active() && m_upnp.status() != upnp_mapper::state::off) m_upnp.stop();   // N2.1: the forward goes with the session
		switch (m_screen)
		{
			case screen::menu: menu_frame(); next = clock::now(); continue;
			case screen::settings: settings_frame(); next = clock::now(); continue;
			case screen::net_menu: net_menu_frame(); next = clock::now(); continue;
			case screen::net_lobby: net_lobby_frame(); next = clock::now(); continue;
			case screen::about: about_frame(); next = clock::now(); continue;
			case screen::news: news_frame(); next = clock::now(); continue;
			case screen::mods: mods_frame(); next = clock::now(); continue;
			case screen::mod_detail: m_screen = screen::mods; continue;
			case screen::controls: controls_frame(); next = clock::now(); continue;
			case screen::game: case screen::pause: break;
		}
		bool ran = m_screen == screen::pause ? pause_frame() : game_frame();
		if (m_screen != screen::game && m_screen != screen::pause) continue;
		if (m_net_game && !ran)
		{
			// waiting for the network: don't advance the clock, poll again shortly
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		// pace to the game's own refresh rate (~54.7 Hz on these boards)
		double hz = m_mach->refresh_hz();
		if (hz < 30 || hz > 120) hz = 60;
		next += std::chrono::nanoseconds(static_cast<long long>(1e9 / hz));
		auto now = clock::now();
		// netplay: if frames are piling up (we are behind the others), skip the wait
		bool behind = m_net_game && m_net.buffered() > m_net.delay() + 1;
		if (next > now && !behind) std::this_thread::sleep_for(next - now);
		else if (now - next > std::chrono::milliseconds(100)) next = now;
	}
	if (m_mach) stop_game();
	m_net.stop();
	save_config(m_cfgpath, m_cfg);
	for (auto *c : m_pads) SDL_GameControllerClose(c);
	m_msound.reset();
	if (m_audio) SDL_CloseAudioDevice(m_audio);
	if (m_tex) SDL_DestroyTexture(m_tex);
	m_gl.shutdown();
	if (m_ren) SDL_DestroyRenderer(m_ren);
	SDL_DestroyWindow(m_win);
	SDL_Quit();
	return 0;
}

int main(int argc, char **argv)
{
	// --version: print what this build is and leave (CI uses it to check a tagged build)
	if (argc > 1 && std::string(argv[1]) == "--version")
	{
		printf("%s\n", app_version());
		fprintf(stderr, "%s\n", app_version_line().c_str());
		return 0;
	}
	SDL_SetMainReady();
	try
	{
		app a;
		if (argc > 1 && std::string(argv[1]) == "--ui-gallery") a.show_gallery();
		if (argc > 1 && std::string(argv[1]) == "--rom-art") a.show_rom_art();
		if (argc > 2 && std::string(argv[1]) == "--pad-script") a.use_pad_script(argv[2]);
		return a.run();
	}
	catch (const std::exception &e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "NBA JAM Recomp", e.what(), nullptr);
		return 1;
	}
}
