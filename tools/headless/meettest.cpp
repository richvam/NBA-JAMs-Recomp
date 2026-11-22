// meettest - the meeting point's messages and request budget (roadmap N3.1), ROM-free.
// Prints the budget table that docs/MEETING.md shows.
#include "meet_proto.h"
#include <cstdio>
#include <string>

static int g_run = 0, g_fail = 0;
#define CHECK(c, what) do { g_run++; if (c) printf("ok   %s\n", what); else { g_fail++; printf("FAIL %s\n", what); } } while (0)

using namespace meet;

int main()
{
	// ---- fields ----
	CHECK(valid_game("nbajam") && valid_game("nbahangt") && valid_game("nbajamte_se") && !valid_game("") && !valid_game("NBAJAM") &&
	      !valid_game("nba jam") && !valid_game("a_very_long_game_name"),
	      "fields: a game id is 1 to 16 of a-z, 0-9 and _");
	CHECK(valid_region("eu") && valid_region("us-east") && !valid_region("e") && !valid_region("EU") && !valid_region("eu1") && !valid_region("north-ame"),
	      "fields: a region is 2 to 8 of a-z and -");
	CHECK(valid_text("Friday night \xc3\xa9t\xc3\xa9", NAME_MAX) && !valid_text("", NAME_MAX) && !valid_text(std::string(33, 'a'), NAME_MAX) &&
	      !valid_text("tab\there", NAME_MAX) && !valid_text("bad \xc3", NAME_MAX) && !valid_text("bad \x80x", NAME_MAX),
	      "fields: names are UTF-8, printable, and at most 32 bytes");
	CHECK(valid_code("6BR0E-7C2YD") && valid_code(std::string(47, 'A')) && !valid_code(std::string(48, 'A')) && !valid_code("6br0e-7c2yd") && !valid_code("1.2.3.4"),
	      "fields: a connection code is upper-case letters, digits and dashes, 47 at most");

	// ---- requests ----
	room r; r.game = "nbajam"; r.region = "eu"; r.name = "Friday \"jam\""; r.version = "2026.11.22"; r.players = 1; r.seats = 4;
	std::string pb = publish_body(r, "6BR0E-7C2YD");
	CHECK(pb == "{\"game\":\"nbajam\",\"region\":\"eu\",\"name\":\"Friday \\\"jam\\\"\",\"version\":\"2026.11.22\",\"code\":\"6BR0E-7C2YD\",\"players\":1,\"seats\":4}" && pb.size() <= BODY_MAX,
	      "publish: the body has the room's fields, quoted, and fits the body limit");
	room bad = r; bad.seats = 17;
	room bad2 = r; bad2.players = 5;
	CHECK(publish_body(bad, "6BR0E-7C2YD").empty() && publish_body(bad2, "6BR0E-7C2YD").empty() && publish_body(r, "x").empty(),
	      "publish: a field out of its limits gives no body, so it is never sent");
	CHECK(wait_body("k1", 2) == "{\"key\":\"k1\",\"players\":2}" && remove_body("k1") == "{\"key\":\"k1\"}" &&
	      introduce_body("6BR0E-7C2YD", "Ana") == "{\"code\":\"6BR0E-7C2YD\",\"nick\":\"Ana\"}" && list_query("nbajam", "eu") == "game=nbajam&region=eu" &&
	      list_query("nbajam", "") == "game=nbajam" && list_query("nba jam", "") .empty(),
	      "wait, remove, introduce and list: the bodies and the query");
	room big = r; big.name = std::string(NAME_MAX, 'x'); big.version = std::string(VERSION_MAX, 'v');
	CHECK(publish_body(big, std::string(CODE_MAX, 'A')).size() <= BODY_MAX && introduce_body(std::string(CODE_MAX, 'A'), std::string(NICK_MAX, 'n')).size() <= BODY_MAX,
	      "requests: every body is within 1 KB with every field at its longest");

	// ---- answers ----
	answer a = parse_answer("{\"rooms\":[{\"id\":\"r1\",\"game\":\"nbajam\",\"region\":\"eu\",\"name\":\"Caf\\u00e9\",\"version\":\"v\",\"players\":2,\"seats\":4,\"age_s\":30},"
	                        "{\"id\":\"r2\",\"game\":\"NBA JAM\",\"region\":\"eu\",\"name\":\"x\",\"players\":1,\"seats\":4},"
	                        "{\"id\":\"r3\",\"game\":\"nbajam\",\"region\":\"eu\",\"name\":\"x\",\"players\":9,\"seats\":4}],\"more\":true}");
	CHECK(a.ok && a.rooms.size() == 1 && a.rooms[0].name == "Caf\xc3\xa9" && a.rooms[0].players == 2 && a.rooms[0].age_s == 30 && a.more,
	      "list: rooms are read, and a room with a field out of its limits is dropped");
	answer w = parse_answer("{\"intros\":[{\"code\":\"6BR0E-7C2YD\",\"nick\":\"Ana\"},{\"code\":\"<script>\",\"nick\":\"x\"}]}");
	CHECK(w.ok && w.intros.size() == 1 && w.intros[0].code == "6BR0E-7C2YD", "wait: introductions are read; a code that is not one is dropped");
	answer e = parse_answer("{\"error\":\"busy\"}"), g = parse_answer("not json"), p = parse_answer("{\"id\":\"r9\",\"key\":\"k\"}");
	CHECK(!e.ok && e.error == "busy" && !g.ok && g.error == "bad answer" && p.ok && p.id == "r9" && p.key == "k",
	      "answers: an error, something that is not JSON, and a publish answer");
	json deep;
	CHECK(!parse_json(std::string(40, '[') + std::string(40, ']'), deep) && !parse_json("{\"a\":1,}", deep) && parse_json(" {\"a\":[1,2.5,true,null]} ", deep),
	      "JSON: nesting is limited and broken text is refused");

	// ---- budget ----
	struct row { const char *who; session s; double player_hours; } rows[] = {
		{ "hosts a room for 1 hour, lets 3 players in", { 60, 3, 0, 0 }, 1 },
		{ "looks at the list 2 minutes, joins 1 room", { 0, 0, 2, 1 }, 0 },
		{ "looks at the list 10 minutes, joins none", { 0, 0, 10, 0 }, 0 },
		{ "hosts 3 hours, players come and go (8 in)", { 180, 8, 0, 0 }, 3 },
	};
	printf("\n| Session | Requests |\n|---|---|\n");
	for (auto &r2 : rows) printf("| %s | %ld |\n", r2.who, requests(r2.s));
	// a four-player room for an hour: its host, and three players who each browse 2 minutes and join
	long room_hour = requests(rows[0].s) + 3 * requests(rows[1].s);
	long per_day = BUSY_AT / room_hour;
	double gbs = 86400.0 * DO_GB;        // one Durable Object in memory the whole day
	printf("\nfour-player room for an hour: %ld requests (%.0f per player-hour)\n", room_hour, room_hour / 4.0);
	printf("room-hours a day before \"busy\" (%ld requests): %ld, about %ld player-hours\n", BUSY_AT, per_day, per_day * 4);
	printf("Durable Object time: %.0f GB-s a day at most (one object, never out of memory), of %ld\n\n", gbs, FREE_DO_GBS_DAY);
	CHECK(requests(rows[0].s) == 77 && requests(rows[1].s) == 6, "budget: a host's hour is 77 requests (72 waits, publish, remove, 3 introductions); a player's join is 6");
	CHECK(per_day >= 900 && BUSY_AT < FREE_WORKER_REQ_DAY && BUSY_AT < FREE_DO_REQ_DAY,
	      "budget: the daily requests carry at least 900 four-player room-hours, and the service stops before the free limit");
	CHECK(gbs < FREE_DO_GBS_DAY, "budget: one Durable Object kept in memory all day stays within the daily time");

	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
