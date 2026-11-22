# The meeting point: protocol and budget

Roadmap N3.1. The meeting point is the one shared service online play needs: a list of open
rooms, and introductions between a player and a room's host. Everything else (seats, chat,
the game itself) stays between the players. The messages and their limits are in
`src/app/meet_proto.h`; `meettest` checks them and prints the budget table below.

The service itself (N3.2) does not exist yet. Nothing in the program talks to it so far.

## Free tier limits, as found on 2026-11-22

From Cloudflare's own pages (Workers limits, Durable Objects pricing, KV limits):

| Product | Free plan, per day | Resets |
|---|---|---|
| Workers | 100,000 requests; 10 ms CPU per request | 00:00 UTC |
| Durable Objects (SQLite only on the free plan) | 100,000 requests; 13,000 GB-s of time; 5 million rows read; 100,000 rows written; 5 GB stored in all | 00:00 UTC |
| Workers KV | 100,000 reads; 1,000 writes | 00:00 UTC |

What this means for the design:

* **No KV.** 1,000 writes a day is not enough to list rooms.
* **One Durable Object holds every room, in memory.** It writes no storage at all, so rows written and storage never count. If it is evicted, the rooms reappear within one wait (50 s), because every host's next wait publishes again.
* **One object, not one per region.** Time is billed while an object is in memory, at 128 MB. One object in memory all day is 86,400 s × 0.128 GB = 11,059 GB-s, under 13,000. Two would not fit.
* **Every request counts twice,** once as a Worker request and once as a Durable Object request. Each kind has its own 100,000 a day, so the lower of the two is the limit: 100,000.

These limits change. Check them again before deploying, and change the numbers in `meet_proto.h` if they did.

## Messages

All are HTTPS to the service's address, which is a setting (N3.2). Bodies are JSON and at
most 1 KB. Answers are JSON; an error is `{"error": "busy" | "limit" | "gone" | "bad"}`.

| Message | Request | Answer |
|---|---|---|
| **publish** | `POST /v1/rooms` `{game, region, name, version, code, players, seats}` | `{id, key}` |
| **wait** | `POST /v1/rooms/<id>/wait` `{key, players}` | `{intros: [{code, nick}]}` |
| **remove** | `POST /v1/rooms/<id>/remove` `{key}` | `{}` |
| **list** | `GET /v1/rooms?game=<id>&region=<r>` | `{rooms: [{id, game, region, name, version, players, seats, age_s}], more}` |
| **introduce** | `POST /v1/rooms/<id>/introduce` `{code, nick}` | `{code}`: the host's code |

* **wait** does two jobs, so a host needs one request a minute and nothing more:
  * It refreshes the room. A room with no wait for 120 s is gone.
  * It receives introductions. The service holds a wait open for up to 50 s and answers it as soon as a player introduces themselves. The host then lets that player in by their code, as with "Let a player in" (N2.2), and opens its next wait.
* **introduce** gives the player the host's code straight away, so both sides punch at once.
* **key** is the room's secret from publish. Only its holder can refresh or remove the room.
* **list** returns at most 50 rooms for one game, nearest region first. The program shows it and refreshes it every 30 s while it is open, never more often than every 5 s.
* **Never through the service:** chat, game data, IP addresses other than the connection code the host chose to publish, and anything about a player beyond a 16-byte nickname.

## Limits on every field

| Field | Rule |
|---|---|
| game | 1 to 16 of `a-z 0-9 _` |
| region | 2 to 8 of `a-z -` |
| name | 1 to 32 bytes of printable UTF-8 |
| nick | 1 to 16 bytes of printable UTF-8 |
| version | 1 to 32 bytes of printable UTF-8 |
| code | a connection code: `0-9 A-Z -`, at most 47 |
| players, seats | 1 ≤ players ≤ seats ≤ 16 |
| id, key | at most 16 and 32 bytes |
| a request body | 1 KB |
| rooms held | 1,000 |

The program checks every field by the same rules before sending (`meet_proto.cpp`). It also
checks what comes back, and drops a room or introduction whose fields break them.

## Rate limits per address

Enforced by the service (N3.2):

| What | At most |
|---|---|
| publish | 6 an hour |
| introduce | 30 an hour |
| list | 1 every 5 s |
| any request | 300 an hour |
| open waits per room | 1 |

At 90,000 requests in a day (UTC), the service answers `busy` to publish and list. This keeps
it below the free limit. The program then uses the fallback (N3.3), and rooms already open
keep working, because their players are already connected to each other.

## Budget

From `meettest` (the schedule above):

| Session | Requests |
|---|---|
| hosts a room for 1 hour, lets 3 players in | 77 |
| looks at the list 2 minutes, joins 1 room | 6 |
| looks at the list 10 minutes, joins none | 21 |
| hosts 3 hours, players come and go (8 in) | 226 |

* **A four-player room for an hour:** 95 requests, about 24 per player-hour. That is the host's 77, plus three players who each look at the list for 2 minutes and join.
* **A day before "busy":** 947 such room-hours, about 3,800 player-hours.
* **Durable Object time:** at most 11,059 GB-s a day (one object, always in memory), of 13,000.

This is the estimate the roadmap asked for ("hundreds of players a day"), measured against
the limits of 2026-11-22. A player who leaves the list open all day costs 2 requests a
minute, which is why the list stops refreshing itself after 10 minutes without a key or
button press.
