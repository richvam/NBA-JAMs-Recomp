// meet_proto.h - the meeting point's messages (roadmap N3.1): the room list and introductions.
// See docs/MEETING.md for the protocol, the limits and the request budget.
//
// HTTPS with JSON bodies, five messages, nothing else: no chat and no game data go through it.
//   publish     POST /v1/rooms                 a host lists a room
//   wait        POST /v1/rooms/<id>/wait       the host refreshes it; held up to WAIT_S, and
//                                              answered at once when a player asks to come in
//   remove      POST /v1/rooms/<id>/remove     the host takes it down
//   list        GET  /v1/rooms?game=&region=   rooms for one game, nearest region first
//   introduce   POST /v1/rooms/<id>/introduce  a player gives its code and gets the host's;
//                                              the host gets the player's in its wait
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace meet {

// ---- timing (the client keeps to these; the service enforces the minimums) ----
constexpr int WAIT_S = 50;              // a wait is held at most this long
constexpr int ROOM_TTL_S = 120;         // a room without a wait for this long is gone
constexpr int LIST_EVERY_S = 30;        // the room list refreshes itself this often while open
constexpr int LIST_MIN_S = 5;           // and never more often than this (a manual refresh too)
constexpr int LIST_IDLE_S = 600;        // and stops refreshing itself after this long without a key or button

// ---- limits on every field (bytes; names in UTF-8) ----
constexpr size_t GAME_MAX = 16, REGION_MAX = 8, NAME_MAX = 32, NICK_MAX = 16, CODE_MAX = 47, VERSION_MAX = 32, ID_MAX = 16, KEY_MAX = 32;
constexpr int SEATS_MAX = 16;           // members of a room (N4.1)
constexpr size_t BODY_MAX = 1024;       // a request body
constexpr int LIST_MAX = 50;            // rooms in one list answer
constexpr int ROOMS_MAX = 1000;         // rooms the service holds

// ---- per address (the service's rate limits) ----
constexpr int PUBLISH_PER_H = 6, INTRODUCE_PER_H = 30, REQUESTS_PER_H = 300;

// ---- the free tier, as found on 2026-11-22 (docs/MEETING.md) ----
constexpr long FREE_WORKER_REQ_DAY = 100000;
constexpr long FREE_DO_REQ_DAY = 100000;
constexpr long FREE_DO_GBS_DAY = 13000;
constexpr double DO_GB = 0.128;          // a Durable Object is billed as 128 MB while in memory
constexpr long BUSY_AT = 90000;          // the service says "busy" past this many requests a day

struct room
{
	std::string id, game, region, name, version;
	int players = 1, seats = 4, age_s = 0;
};

struct intro { std::string code, nick; };

// field checks: the same rules as the service, so the program never sends what it refuses
bool valid_game(const std::string &s);       // [a-z0-9_], 1..GAME_MAX
bool valid_region(const std::string &s);     // [a-z-], 2..REGION_MAX
bool valid_text(const std::string &s, size_t max);   // UTF-8, printable, 1..max bytes
bool valid_code(const std::string &s);       // a connection code (nat.h), CODE_MAX at most

// request bodies (JSON); empty when a field is not valid
std::string publish_body(const room &r, const std::string &code);
std::string wait_body(const std::string &key, int players);
std::string remove_body(const std::string &key);
std::string introduce_body(const std::string &code, const std::string &nick);
std::string list_query(const std::string &game, const std::string &region);

// answers
struct answer
{
	bool ok = false;
	std::string error;                 // "busy", "limit", "gone", "bad" or the HTTP failure
	std::string id, key, code;         // publish: id and key; introduce: the host's code
	std::vector<room> rooms;           // list
	std::vector<intro> intros;         // wait
	bool more = false;                 // list: more rooms than LIST_MAX
};
answer parse_answer(const std::string &json);

// ---- the budget: requests per session from the schedule above ----
struct session
{
	int host_minutes = 0;              // hosting a listed room this long
	int intros = 0;                    // players let in (each ends a wait early)
	int browse_minutes = 0;            // the room list open this long
	int joins = 0;                     // rooms asked to join
};
long requests(const session &s);       // to the service (each is one Worker and one Durable Object request)

// a minimal JSON reader, enough for the answers (objects, arrays, strings, numbers, true/false/null)
struct json
{
	enum kind_t { null, boolean, number, string, array, object } kind = null;
	bool b = false;
	double n = 0;
	std::string s;
	std::vector<json> a;
	std::map<std::string, json> o;
	const json *get(const std::string &k) const { auto it = o.find(k); return it == o.end() ? nullptr : &it->second; }
	std::string str(const std::string &k) const { auto *j = get(k); return j && j->kind == string ? j->s : std::string(); }
	int num(const std::string &k, int def = 0) const { auto *j = get(k); return j && j->kind == number ? int(j->n) : def; }
};
bool parse_json(const std::string &text, json &out);
std::string quote(const std::string &s);   // a JSON string

} // namespace meet
