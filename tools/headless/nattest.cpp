// nattest - connecting through home routers (roadmap N2.1 and N2.2, first part), ROM-free.
//   * connection codes and STUN messages;
//   * UPnP port mapping against a test router (SSDP + HTTP + SOAP) run inside this program;
//   * hole punching through simulated routers (full cone, restricted, port restricted,
//     symmetric), every pair, host and player each behind their own; plus a host whose port
//     is forwarded (what UPnP gives), joined without the player's code.
// Linux only: the simulated routers use the addresses 127.0.1.x and 127.0.2.1 of the loopback.
#include "nat.h"
#include "netplay.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

static int g_run = 0, g_fail = 0;
#define CHECK(c, what) do { g_run++; if (c) printf("ok   %s\n", what); else { g_fail++; printf("FAIL %s\n", what); } } while (0)

#ifdef __APPLE__
// macOS (CI VMs especially) oversleeps short waits by several ms, which adds up at every
// simulated router hop and spoils the timing checks; wait by yielding instead (R6.4)
static void sleep_ms(int ms)
{
	auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
	while (std::chrono::steady_clock::now() < end) std::this_thread::yield();
}
#else
static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
#endif
static u32 ip4(int a, int b, int c, int d) { return u32(a) << 24 | u32(b) << 16 | u32(c) << 8 | u32(d); }

static int udp_bind(u32 ip, u16 port)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
	sockaddr_in a{};
	a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(ip); a.sin_port = htons(port);
	if (bind(s, (sockaddr *)&a, sizeof a) != 0) { perror("bind"); exit(2); }
	return s;
}
static net_addr local_of(int s) { sockaddr_in a{}; socklen_t l = sizeof a; getsockname(s, (sockaddr *)&a, &l); return { ntohl(a.sin_addr.s_addr), ntohs(a.sin_port) }; }
static void send_to(int s, const net_addr &to, const void *d, size_t n)
{
	sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(to.ip); a.sin_port = htons(to.port);
	sendto(s, d, n, 0, (sockaddr *)&a, sizeof a);
}
static int recv_from(int s, net_addr &from, u8 *d, size_t cap)
{
	sockaddr_in a{}; socklen_t l = sizeof a;
	int n = int(recvfrom(s, d, cap, 0, (sockaddr *)&a, &l));
	if (n >= 0) { from.ip = ntohl(a.sin_addr.s_addr); from.port = ntohs(a.sin_port); }
	return n;
}

// ---------------------------------------------------------------- a simulated home router
enum nat_kind { full_cone, restricted, port_restricted, symmetric };
static const char *kind_name[] = { "full cone", "restricted", "port restricted", "symmetric" };

struct nat_box
{
	nat_kind kind;
	u32 pub;
	int inside;                       // where the computers behind it send (with the destination in front)
	struct mapping { net_addr in, dest; int fd; std::set<u64> sent_to; bool open = false; };
	std::vector<mapping> maps;
	// N2.4: every packet leaving through this router waits delay_ms, plus 0..spread_ms at random
	int delay_ms = 0, spread_ms = 0;
	u32 rng = 99;
	struct held { u32 due; int fd; net_addr to; std::vector<u8> d; };
	std::vector<held> queue;
	nat_box(nat_kind k, u32 public_ip) : kind(k), pub(public_ip) { inside = udp_bind(ip4(127, 0, 0, 1), 0); }
	~nat_box() { close(inside); for (auto &m : maps) close(m.fd); }
	net_addr inside_addr() const { return local_of(inside); }
	// a port forward, as UPnP makes: public port -> computer, anyone may send
	net_addr forward(const net_addr &computer, u16 port)
	{
		mapping m; m.in = computer; m.fd = udp_bind(pub, port); m.open = true;
		maps.push_back(m);
		return local_of(m.fd);
	}
	static u64 key(const net_addr &a) { return u64(a.ip) << 16 | a.port; }
	void pump()
	{
		u8 b[4096];
		net_addr from;
		int n;
		while ((n = recv_from(inside, from, b, sizeof b)) >= 6)
		{
			net_addr to{ u32(b[0]) << 24 | u32(b[1]) << 16 | u32(b[2]) << 8 | b[3], u16(b[4] << 8 | b[5]) };
			mapping *m = nullptr;
			for (auto &x : maps) if (x.in == from && x.open) { m = &x; break; }       // a forward is used for replies too
			if (!m) for (auto &x : maps) if (x.in == from && (kind != symmetric || x.dest == to)) { m = &x; break; }
			if (!m) { mapping x; x.in = from; x.dest = to; x.fd = udp_bind(pub, 0); maps.push_back(x); m = &maps.back(); }
			m->sent_to.insert(key(to));
			if (!delay_ms && !spread_ms) { send_to(m->fd, to, b + 6, size_t(n - 6)); continue; }
			rng = rng * 1103515245u + 12345u;
			u32 extra = spread_ms ? (rng >> 8) % u32(spread_ms + 1) : 0;
			queue.push_back({ net_now_ms() + u32(delay_ms) + extra, m->fd, to, std::vector<u8>(b + 6, b + n) });
		}
		for (size_t i = 0; i < queue.size();)
			if (int(net_now_ms() - queue[i].due) >= 0) { send_to(queue[i].fd, queue[i].to, queue[i].d.data(), queue[i].d.size()); queue.erase(queue.begin() + long(i)); }
			else i++;
		for (auto &m : maps)
			while ((n = recv_from(m.fd, from, b + 6, sizeof b - 6)) >= 0)
			{
				bool pass = m.open || kind == full_cone;
				if (!pass && kind == restricted) for (u64 k : m.sent_to) pass = pass || u32(k >> 16) == from.ip;
				if (!pass) pass = m.sent_to.count(key(from)) != 0;
				if (!pass) continue;
				b[0] = u8(from.ip >> 24); b[1] = u8(from.ip >> 16); b[2] = u8(from.ip >> 8); b[3] = u8(from.ip); b[4] = u8(from.port >> 8); b[5] = u8(from.port);
				send_to(inside, m.in, b, size_t(n + 6));
			}
	}
};

// the public STUN server
struct stun_server
{
	int fd;
	stun_server() { fd = udp_bind(ip4(127, 0, 2, 1), 0); }
	~stun_server() { close(fd); }
	net_addr addr() const { return local_of(fd); }
	void pump()
	{
		u8 b[2048]; net_addr from; int n;
		while ((n = recv_from(fd, from, b, sizeof b)) >= 0)
		{
			auto r = stun_response(b, n, from);
			if (!r.empty()) send_to(fd, from, r.data(), r.size());
		}
	}
};

// host behind router h, player behind router c: the host shows its code (STUN), the player
// joins with it and shows theirs, the host lets them in by it
static bool punch(nat_kind hk, nat_kind ck, bool forwarded = false)
{
	stun_server st;
	nat_box hn(hk, ip4(127, 0, 1, 1)), cn(ck, ip4(127, 0, 1, 2));
	netplay host, cl;
	std::string err;
	if (!host.host(0, err)) { printf("host: %s\n", err.c_str()); return false; }
	host.socket().via = hn.inside_addr();
	net_addr host_code;
	auto pump = [&] { st.pump(); hn.pump(); cn.pump(); host.poll(); if (cl.active()) cl.poll(); };
	auto until = [&](auto done, int ms) { auto t0 = net_now_ms(); while (!done() && net_now_ms() - t0 < u32(ms)) { pump(); sleep_ms(2); } return done(); };
	if (forwarded)
	{
		// a port forward on the host's router; the host reads its own port to ask for it
		host.stun_begin(st.addr());
		until([&] { return host.stun_status() == netplay::stun_state::found; }, 3000);
		net_addr inside = hn.maps.empty() ? net_addr{} : hn.maps[0].in;
		host_code = hn.forward(inside, 0);
	}
	else
	{
		host.stun_begin(st.addr());
		if (!until([&] { return host.stun_status() == netplay::stun_state::found; }, 4000)) { printf("  host STUN failed\n"); return false; }
		host_code = host.public_addr();
	}
	if (!cl.join(net_code(host_code), 0, err)) { printf("join: %s\n", err.c_str()); return false; }
	cl.socket().via = cn.inside_addr();
	if (!forwarded)
	{
		cl.stun_begin(st.addr());
		if (!until([&] { return cl.stun_status() == netplay::stun_state::found; }, 4000)) { printf("  player STUN failed\n"); return false; }
		net_addr cc;
		if (!net_code_parse(net_code(cl.public_addr()), cc)) return false;
		host.invite(cc);
	}
	bool ok = until([&] { return cl.active() && cl.local_slot() == 1 && host.players() == 2; }, 6000);
	if (ok) { until([&] { return false; }, 1500); ok = cl.active() && host.players() == 2; }   // and it stays up
	host.stop(); cl.stop();
	return ok;
}

// host and player behind the SAME router, which does not send packets for its own public
// address back inside: the player joins with the host's code and finds it on the home network
// (127.0.0.1 is the home network in the simulator). lan = false: the search is pointed nowhere.
static bool same_router(bool lan, bool other_host = false)
{
	stun_server st;
	nat_box nb(port_restricted, ip4(127, 0, 1, 1));
	netplay host, cl;
	std::string err;
	if (!host.host(0, err)) return false;
	host.socket().via = nb.inside_addr();
	auto pump = [&] { st.pump(); nb.pump(); host.poll(); if (cl.active()) cl.poll(); };
	auto until = [&](auto done, int ms) { auto t0 = net_now_ms(); while (!done() && net_now_ms() - t0 < u32(ms)) { pump(); sleep_ms(2); } return done(); };
	host.stun_begin(st.addr());
	if (!until([&] { return host.stun_status() == netplay::stun_state::found; }, 4000)) return false;
	net_addr code = host.public_addr();
	if (other_host) code.ip = ip4(127, 0, 1, 9);   // a code of another home with the same router here
	if (!cl.join(net_code(code), 0, err)) return false;
	cl.socket().via = nb.inside_addr();
	cl.lan_probe_to = { net_addr{ ip4(127, 0, 0, 1), lan ? host.socket().local_port() : u16(1) } };
	cl.stun_begin(st.addr());
	bool ok = until([&] { return cl.active() && cl.local_slot() == 1 && host.players() == 2; }, 3000);
	if (ok) { until([&] { return false; }, 1000); ok = cl.active() && host.players() == 2 && cl.found_on_lan(); }
	host.stop(); cl.stop();
	return ok;
}

// Peer relay (N2.3): host and player both behind symmetric routers, which cannot connect
// directly; a member behind a full cone router relays. The three play 300 frames of lockstep
// (no game: the inputs and hashes only) and the relay's forwarding is capped.
struct relay_result { bool joined = false, relayed = false, shown = false, played = false, capped = false, report = false; };
static relay_result relay_play()
{
	relay_result res;
	stun_server st;
	nat_box hn(symmetric, ip4(127, 0, 1, 1)), rn(full_cone, ip4(127, 0, 1, 2)), cn(symmetric, ip4(127, 0, 1, 3));
	netplay host, rl, cl;
	std::string err;
	if (!host.host(0, err)) return res;
	host.socket().via = hn.inside_addr();
	auto pump = [&] { st.pump(); hn.pump(); rn.pump(); cn.pump(); host.poll(); if (rl.active()) rl.poll(); if (cl.active()) cl.poll(); };
	auto until = [&](auto done, int ms) { auto t0 = net_now_ms(); while (!done() && net_now_ms() - t0 < u32(ms)) { pump(); sleep_ms(2); } return done(); };
	host.stun_begin(st.addr());
	if (!until([&] { return host.stun_status() == netplay::stun_state::found; }, 4000)) return res;
	// the member: joins by the host's code, the host lets it in by its code
	if (!rl.join(net_code(host.public_addr()), 0, err)) return res;
	rl.socket().via = rn.inside_addr();
	rl.stun_begin(st.addr());
	if (!until([&] { return rl.stun_status() == netplay::stun_state::found; }, 4000)) return res;
	host.invite(rl.public_addr());
	if (!until([&] { return rl.local_slot() == 1 && host.players() == 2; }, 6000)) { printf("  member did not join\n"); return res; }
	until([&] { return false; }, 300);
	net_addr relay = host.relay_candidate();
	if (!relay.port) { printf("  no relay candidate\n"); return res; }
	// the player: the longer code (host, then member)
	std::string code = net_code(host.public_addr()) + "-" + net_code(relay);
	if (!cl.join(code, 0, err)) return res;
	cl.relay_after_ms = 1500;
	cl.socket().via = cn.inside_addr();
	cl.stun_begin(st.addr());
	if (!until([&] { return cl.stun_status() == netplay::stun_state::found; }, 4000)) return res;
	host.invite(cl.public_addr());
	res.joined = until([&] { return cl.active() && cl.local_slot() == 2 && host.players() == 3; }, 8000);
	res.relayed = cl.relayed() && rl.relayed_bytes() > 0;
	until([&] { return false; }, 2500);
	for (auto &l : host.links()) res.report = res.report || (l.slot == 2 && l.relayed && l.through == 1);
	for (auto &l : cl.links()) res.report = res.report && (l.slot != 1 || l.via_host);
	res.report = res.report && cl.connection(false) == netplay::conn_type::strict && cl.links().size() == 2;
	for (auto &l : host.peer_list()) res.shown = res.shown || l.find("through J2") != std::string::npos;
	if (!res.joined) { host.stop(); rl.stop(); cl.stop(); return res; }
	// lockstep through the relay
	host.host_start("relaytest", 1, dip_settings(), std::vector<u8>(64, 7), 4);
	bool ready = until([&] { return rl.start_received() && cl.start_received(); }, 8000);
	rl.client_ready(true, ""); cl.client_ready(true, "");
	ready = ready && until([&] { return host.all_ready(); }, 8000);
	if (ready)
	{
		netplay *all[3] = { &host, &rl, &cl };
		auto t0 = net_now_ms();
		cabinet_inputs::player in{};
		while (net_now_ms() - t0 < 20000)
		{
			pump();
			bool done = true;
			for (netplay *n : all)
			{
				if (n->frame() < 300 && n->can_run()) { n->frame_inputs(in); n->frame_done(n->frame() * 2654435761u); }
				done = done && n->frame() >= 300;
			}
			if (done) break;
			sleep_ms(1);
		}
		res.played = host.frame() >= 300 && rl.frame() >= 300 && cl.frame() >= 300 && !host.desynced() && !rl.desynced() && !cl.desynced() && cl.relayed();
	}
	// a flood through the member is capped at RELAY_BYTES_PER_S
	{
		int raw = udp_bind(ip4(127, 0, 0, 1), 0);
		u32 before = rl.relayed_bytes();
		std::vector<u8> pkt{ u8('X'), 0x4a, 4 };
		net_addr h = host.public_addr();
		for (int sft = 24; sft >= 0; sft -= 8) pkt.push_back(u8(h.ip >> sft));
		pkt.push_back(u8(h.port >> 8)); pkt.push_back(u8(h.port));
		pkt.push_back('z'); pkt.push_back(0x4a);
		pkt.resize(pkt.size() + 1000, 0);
		for (int i = 0; i < 200; i++) { send_to(raw, relay, pkt.data(), pkt.size()); if (i % 20 == 0) pump(); }
		until([&] { return false; }, 300);
		u32 sent = rl.relayed_bytes() - before;
		res.capped = sent > 0 && sent <= netplay::RELAY_BYTES_PER_S + 4096;
		printf("  flood: 200 KB sent to the member, %u bytes forwarded\n", sent);
		close(raw);
	}
	host.stop(); rl.stop(); cl.stop();
	return res;
}

// The connection report (N2.4): host behind hk, player behind ck, each router delaying what
// leaves it by its delay (and spread). Waits until the reports settled, then reads them.
struct report_result { netplay::conn_type host = netplay::conn_type::unknown, player = netplay::conn_type::unknown, host_fw = netplay::conn_type::unknown; int ping_h = 0, ping_c = 0, jit_c = 0; bool direct = false; };
static report_result report(nat_kind hk, nat_kind ck, int hdelay, int cdelay, int spread)
{
	report_result res;
	stun_server st;
	nat_box hn(hk, ip4(127, 0, 1, 1)), cn(ck, ip4(127, 0, 1, 2));
	hn.delay_ms = hdelay; cn.delay_ms = cdelay; cn.spread_ms = spread;
	netplay host, cl;
	std::string err;
	if (!host.host(0, err)) return res;
	host.socket().via = hn.inside_addr();
	auto pump = [&] { st.pump(); hn.pump(); cn.pump(); host.poll(); if (cl.active()) cl.poll(); };
	auto until = [&](auto done, int ms) { auto t0 = net_now_ms(); while (!done() && net_now_ms() - t0 < u32(ms)) { pump(); sleep_ms(1); } return done(); };
	host.stun_begin(st.addr());
	if (!until([&] { return host.stun_status() == netplay::stun_state::found; }, 4000)) return res;
	if (!cl.join(net_code(host.public_addr()), 0, err)) return res;
	cl.socket().via = cn.inside_addr();
	cl.stun_begin(st.addr());
	if (!until([&] { return cl.stun_status() == netplay::stun_state::found; }, 4000)) return res;
	host.invite(cl.public_addr());
	if (!until([&] { return cl.local_slot() == 1 && host.players() == 2; }, 6000)) { host.stop(); cl.stop(); return res; }
	until([&] { return false; }, 4000);     // reports every second, pings settle
	res.host = host.connection(false);
	res.host_fw = host.connection(true);
	res.player = cl.connection(false);
	auto hl = host.links(), cll = cl.links();
	if (!hl.empty()) res.ping_h = hl[0].ping;
	if (!cll.empty()) { res.ping_c = cll[0].ping; res.jit_c = cll[0].jitter; res.direct = !cll[0].relayed; }
	host.stop(); cl.stop();
	return res;
}

// Mesh (N2.5): four peers behind full cone routers that delay what leaves them: the host and
// player 3 by 40 ms, players 2 and 4 by 5 ms. Unpaced lockstep with an input delay of 4 frames
// runs as fast as the inputs arrive, so the time per 4 frames is the latency that limits it:
// with the mesh the worst direct pair (40 ms), through the host the sum (40 + 40 ms).
struct mesh_result { bool setup = false, ok = false; double ms_per4 = 0; u32 mesh_frames = 0; };
static mesh_result mesh_run(bool mesh)
{
	mesh_result res;
	stun_server st;
	const int delays[4] = { 40, 5, 40, 5 };
	std::vector<std::unique_ptr<nat_box>> box;
	for (int i = 0; i < 4; i++) { box.emplace_back(new nat_box(full_cone, ip4(127, 0, 1, u8(1 + i)))); box.back()->delay_ms = delays[i]; }
	netplay np[4];
	for (auto &n : np) n.mesh = mesh;
	std::string err;
	if (!np[0].host(0, err)) return res;
	np[0].socket().via = box[0]->inside_addr();
	auto pump = [&] { st.pump(); for (auto &b : box) b->pump(); for (auto &n : np) if (n.active()) n.poll(); };
	auto until = [&](auto done, int ms) { auto t0 = net_now_ms(); while (!done() && net_now_ms() - t0 < u32(ms)) { pump(); sleep_ms(1); } return done(); };
	np[0].stun_begin(st.addr());
	if (!until([&] { return np[0].stun_status() == netplay::stun_state::found; }, 4000)) return res;
	for (int i = 1; i < 4; i++)
	{
		if (!np[i].join(net_code(np[0].public_addr()), 0, err)) return res;
		np[i].socket().via = box[size_t(i)]->inside_addr();
		if (!until([&] { return np[i].local_slot() == i && np[0].players() == i + 1; }, 6000)) { printf("  player %d did not join\n", i + 1); return res; }
	}
	np[0].host_start("meshtest", 1, dip_settings(), std::vector<u8>(64, 7), 4);
	if (!until([&] { return np[1].start_received() && np[2].start_received() && np[3].start_received(); }, 8000)) return res;
	for (int i = 1; i < 4; i++) np[i].client_ready(true, "");
	if (!until([&] { return np[0].all_ready(); }, 8000)) return res;
	res.setup = true;
	cabinet_inputs::player in{};
	auto t0 = net_now_ms();
	u32 t100 = 0;
	const u32 N = 300;
	while (net_now_ms() - t0 < 30000)
	{
		pump();
		bool done = true;
		for (int i = 0; i < 4; i++)
		{
			netplay &n = np[i];
			if (n.frame() < N && n.can_run())
			{
				in.shoot = ((n.frame() + u32(i)) % 7) == 0;   // different inputs per player and frame
				cabinet_inputs ci = n.frame_inputs(in);
				u32 mix = 0;
				for (int q = 0; q < 4; q++) mix = mix * 31 + u32(ci.p[q].shoot);
				n.frame_done(n.frame() * 2654435761u + mix);   // a hash of the merged inputs: a wrong merge is a desync
			}
			done = done && n.frame() >= N;
		}
		if (!t100 && np[1].frame() >= 100) t100 = net_now_ms();
		if (done) break;
		sleep_ms(1);
	}
	bool desync = false;
	for (auto &n : np) desync = desync || n.desynced();
	res.ok = np[1].frame() >= N && np[3].frame() >= N && !desync;
	res.ms_per4 = t100 ? double(net_now_ms() - t100) / double(N - 100) * 4.0 : 0;   // after the start settled
	for (int i = 1; i < 4; i++) res.mesh_frames += np[i].mesh_frames();
	for (auto &n : np) n.stop();
	return res;
}

// IPv6 (N2.2): no router in the simulator, the loopback ::1. how: 0 = the address typed,
// 1 = a code with both parts (IPv6 right, IPv4 to a closed port), 2 = both (IPv6 to a closed
// port, IPv4 right). Returns whether it joined; v6 says over which.
static bool ipv6_join(int how, bool &v6)
{
	netplay host, cl;
	std::string err;
	v6 = false;
	if (!host.host(0, err) || !host.socket().has_ipv6()) return false;
	u16 port = host.socket().local_port();
	net_addr six{}, four{ ip4(127, 0, 0, 1), port }, closed4{ ip4(127, 0, 0, 1), 1 };
	six.v6 = true; six.a6[15] = 1; six.port = port;
	net_addr closed6 = six; closed6.port = 1;
	bool joined = how == 0 ? cl.join("[::1]", port, err) : cl.join(how == 1 ? net_code_pair(closed4, six) : net_code_pair(four, closed6), 0, err);
	if (!joined) { printf("  join: %s\n", err.c_str()); return false; }
	auto t0 = net_now_ms();
	while (!(cl.active() && cl.local_slot() == 1) && net_now_ms() - t0 < 4000) { host.poll(); cl.poll(); sleep_ms(2); }
	for (int i = 0; i < 300; i++) { host.poll(); if (cl.active()) cl.poll(); sleep_ms(2); }
	bool ok = cl.active() && cl.local_slot() == 1 && host.players() == 2;
	auto pl = host.peer_list();
	v6 = ok && !pl.empty() && pl[0].find('[') != std::string::npos;
	host.stop(); cl.stop();
	return ok;
}

// ---------------------------------------------------------------- a test router for UPnP
struct test_igd
{
	int ssdp, http;
	std::atomic<bool> quit{ false };
	std::thread th;
	std::vector<std::string> actions;   // "AddPortMapping lease=3600 port=7845 proto=UDP client=..."
	std::mutex mx;
	bool permanent_only = false, no_service = false;
	static std::string tagv(const std::string &x, const std::string &t)
	{
		size_t a = x.find("<" + t + ">"); if (a == std::string::npos) return "";
		a += t.size() + 2; return x.substr(a, x.find("</" + t + ">", a) - a);
	}
	test_igd()
	{
		ssdp = udp_bind(ip4(127, 0, 0, 1), 0);
		http = socket(AF_INET, SOCK_STREAM, 0);
		int one = 1; setsockopt(http, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
		sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(ip4(127, 0, 0, 1)); a.sin_port = 0;
		bind(http, (sockaddr *)&a, sizeof a); listen(http, 8);
		fcntl(http, F_SETFL, fcntl(http, F_GETFL) | O_NONBLOCK);
		th = std::thread([this] { run(); });
	}
	~test_igd() { quit = true; th.join(); close(ssdp); close(http); }
	std::string ssdp_target() const { auto a = local_of(ssdp); return "127.0.0.1:" + std::to_string(a.port); }
	void run()
	{
		u16 hp = local_of(http).port;
		while (!quit)
		{
			u8 b[2048]; net_addr from; int n;
			while ((n = recv_from(ssdp, from, b, sizeof b - 1)) > 0)
			{
				std::string r = "HTTP/1.1 200 OK\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nLOCATION: http://127.0.0.1:" + std::to_string(hp) + "/desc.xml\r\n\r\n";
				send_to(ssdp, from, r.data(), r.size());			}
			int c = accept(http, nullptr, nullptr);
			if (c < 0) { sleep_ms(2); continue; }
			fcntl(c, F_SETFL, fcntl(c, F_GETFL) & ~O_NONBLOCK);
			timeval tv{ 1, 0 }; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
			std::string req; char buf[4096];
			while (true)
			{
				int k = int(::recv(c, buf, sizeof buf, 0)); if (k <= 0) break; req.append(buf, size_t(k));
				size_t he = req.find("\r\n\r\n"); if (he == std::string::npos) continue;
				size_t cl = req.find("Content-Length: "); size_t len = cl == std::string::npos ? 0 : size_t(atoi(req.c_str() + cl + 16));
				if (req.size() >= he + 4 + len) break;
			}
			std::string body, status = "200 OK";
			if (req.compare(0, 4, "GET ") == 0)
				body = no_service ? "<root><device><serviceList></serviceList></device></root>"
				                  : "<?xml version=\"1.0\"?><root><device><deviceList><device><serviceList><service>"
				                    "<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
				                    "<controlURL>/ctl/IPConn</controlURL></service></serviceList></device></deviceList></device></root>";
			else
			{
				size_t sa = req.find("SOAPAction: \""); std::string act = req.substr(req.find('#', sa) + 1); act = act.substr(0, act.find('"'));
				std::string line = act;
				if (act == "AddPortMapping")
				{
					std::string lease = tagv(req, "NewLeaseDuration");
					line += " lease=" + lease + " port=" + tagv(req, "NewExternalPort") + " in=" + tagv(req, "NewInternalPort") + " proto=" + tagv(req, "NewProtocol") + " client=" + tagv(req, "NewInternalClient");
					if (permanent_only && lease != "0") { status = "500 Internal Server Error"; body = "<s:Envelope><s:Body><s:Fault><detail><UPnPError><errorCode>725</errorCode></UPnPError></detail></s:Fault></s:Body></s:Envelope>"; }
				}
				if (act == "DeletePortMapping") line += " port=" + tagv(req, "NewExternalPort");
				if (act == "GetExternalIPAddress") body = "<s:Envelope><s:Body><u:GetExternalIPAddressResponse><NewExternalIPAddress>203.0.113.7</NewExternalIPAddress></u:GetExternalIPAddressResponse></s:Body></s:Envelope>";
				std::lock_guard<std::mutex> l(mx); actions.push_back(line);
			}
			std::string resp = "HTTP/1.1 " + status + "\r\nContent-Type: text/xml\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
			::send(c, resp.data(), resp.size(), 0);
			close(c);
		}
	}
	std::vector<std::string> log() { std::lock_guard<std::mutex> l(mx); return actions; }
};

// ---------------------------------------------------------------- a test router for PCP / NAT-PMP
// Answers on a UDP port as a router's port 5351 does. pcp: answers PCP MAP; otherwise it
// answers PCP with NAT-PMP's "unsupported version" and takes NAT-PMP. Logs each request.
struct test_pmp
{
	int sock;
	bool pcp = true;
	std::atomic<bool> quit{ false };
	std::thread th;
	std::vector<std::string> reqs;      // "PCP MAP port=7845 life=3600", "PMP MAP port=7845 life=0", "PMP ADDR"
	std::mutex mx;
	test_pmp() { sock = udp_bind(ip4(127, 0, 0, 1), 0); th = std::thread([this] { run(); }); }
	~test_pmp() { quit = true; th.join(); close(sock); }
	std::string target() const { return "127.0.0.1:" + std::to_string(local_of(sock).port); }
	static u32 r32(const u8 *p) { return u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3]; }
	static void w32(u8 *p, u32 v) { p[0] = u8(v >> 24); p[1] = u8(v >> 16); p[2] = u8(v >> 8); p[3] = u8(v); }
	void run()
	{
		while (!quit)
		{
			u8 b[1100]; net_addr from;
			int n = recv_from(sock, from, b, sizeof b);
			if (n <= 0) { sleep_ms(2); continue; }
			std::string line;
			std::vector<u8> out;
			if (n >= 60 && b[0] == 2 && b[1] == 1)
			{
				u16 port = u16(b[40] << 8 | b[41]);
				u32 life = r32(b + 4);
				line = "PCP MAP port=" + std::to_string(port) + " life=" + std::to_string(life);
				if (pcp)
				{
					out.assign(60, 0);
					out[0] = 2; out[1] = 0x81; out[3] = 0;
					w32(&out[4], life ? 7200 : 0);
					std::memcpy(&out[24], b + 24, 12);   // the nonce
					out[36] = 17;
					out[40] = b[40]; out[41] = b[41]; out[42] = b[42]; out[43] = b[43];
					out[54] = out[55] = 0xff;
					out[56] = 203; out[57] = 0; out[58] = 113; out[59] = 9;
				}
				else { out.assign(8, 0); out[1] = 0x81; out[3] = 1; }   // version 0, unsupported version
			}
			else if (n >= 2 && b[0] == 0 && b[1] == 0 && !pcp)
			{
				line = "PMP ADDR";
				out.assign(12, 0); out[1] = 128; out[8] = 198; out[9] = 51; out[10] = 100; out[11] = 77;
			}
			else if (n >= 12 && b[0] == 0 && b[1] == 1 && !pcp)
			{
				u16 port = u16(b[4] << 8 | b[5]);
				u32 life = r32(b + 8);
				line = "PMP MAP port=" + std::to_string(port) + " life=" + std::to_string(life);
				out.assign(16, 0); out[1] = 129;
				out[8] = b[4]; out[9] = b[5]; out[10] = b[6]; out[11] = b[7];
				w32(&out[12], life ? 7200 : 0);
			}
			else line = "? " + std::to_string(n) + " bytes";
			{ std::lock_guard<std::mutex> l(mx); reqs.push_back(line); }
			if (!out.empty()) send_to(sock, from, out.data(), out.size());		}
	}
	std::vector<std::string> log() { std::lock_guard<std::mutex> l(mx); return reqs; }
};

static bool wait_state(upnp_mapper &m, int ms)
{
	for (int t = 0; t < ms && m.status() == upnp_mapper::state::working; t += 10) sleep_ms(10);
	return m.status() != upnp_mapper::state::working;
}

int main(int argc, char **argv)
{
	// nattest --stun host:port : ask a real STUN server through a netplay socket and print the code
	if (argc == 3 && std::string(argv[1]) == "--stun")
	{
		std::string sv = argv[2], err;
		size_t c = sv.rfind(':');
		net_addr a;
		if (!udp_socket::resolve(sv.substr(0, c), u16(atoi(sv.c_str() + c + 1)), a, err)) { printf("%s\n", err.c_str()); return 1; }
		netplay np;
		if (!np.host(0, err)) { printf("%s\n", err.c_str()); return 1; }
		np.stun_begin(a);
		for (int t = 0; t < 400 && np.stun_status() == netplay::stun_state::asking; t++) { np.poll(); sleep_ms(10); }
		if (np.stun_status() != netplay::stun_state::found) { printf("no answer from %s\n", sv.c_str()); return 1; }
		printf("public %s  code %s\n", net_addr_str(np.public_addr()).c_str(), net_code(np.public_addr()).c_str());
		return 0;
	}
	// ---- the router (R6.4: also on macOS, which has no /proc/net/route) ----
	if (getenv("NATTEST_EXPECT_GATEWAY"))
	{
		u32 g = default_gateway();
		printf("  default gateway: %u.%u.%u.%u\n", g >> 24, g >> 16 & 255, g >> 8 & 255, g & 255);
		CHECK(g != 0 && (g >> 24) != 127, "router: the default gateway is found (where NAT-PMP and PCP are asked)");
	}
	// ---- codes ----
	{
		bool rt = true;
		for (net_addr a : { net_addr{ ip4(203, 0, 113, 7), 7845 }, net_addr{ 0xffffffffu, 65535 }, net_addr{ ip4(1, 2, 3, 4), 1 } })
		{
			net_addr b;
			std::string c = net_code(a);
			rt = rt && c.size() == 11 && c[5] == '-' && net_code_parse(c, b) && b == a;
		}
		CHECK(rt, "codes: an address and port make an 11-character code (ABCDE-FGHJK) and back");
		net_addr a{ ip4(203, 0, 113, 7), 7845 }, b;
		std::string c = net_code(a), low = c;
		for (auto &ch : low) ch = char(tolower(ch));
		low.erase(5, 1);
		CHECK(net_code_parse(low, b) && b == a, "codes: lower case and no dash are accepted");
		CHECK(!net_code_parse("10.0.0.1", b) && !net_code_parse("ABCDE-FGHJ", b) && !net_code_parse("ABCDE-FGHJKM", b) && !net_code_parse("ABCDE-FGHJU", b) && !net_code_parse("", b),
		      "codes: an IP address, too short, too long, or a letter not used (U) is not a code");
		CHECK(net_ip_private(ip4(10, 207, 86, 55)) && net_ip_private(ip4(192, 168, 1, 2)) && net_ip_private(ip4(172, 20, 0, 1)) && net_ip_private(ip4(100, 70, 1, 1)) &&
		      !net_ip_private(ip4(203, 0, 113, 7)) && !net_ip_private(ip4(172, 32, 0, 1)) && !net_ip_private(ip4(100, 128, 0, 1)),
		      "addresses: private and carrier-NAT ranges are told from public ones");
	}
	// ---- IPv6 codes (N2.2) ----
	{
		net_addr six{}, back{}, four{ ip4(203, 0, 113, 7), 7845 }, a4{}, a6{};
		const u8 g[16] = { 0x20, 0x01, 0x0d, 0xb8, 0x12, 0x34, 0, 0, 0, 0, 0, 0, 0xab, 0xcd, 0x00, 0x01 };
		six.v6 = true; memcpy(six.a6, g, 16); six.port = 7845;
		std::string c = net_code(six);
		CHECK(c.size() == 34 && net_code_parse(c, back) && back == six && net_addr_str(back) == "[2001:db8:1234::abcd:1]:7845",
		      "IPv6 codes: an IPv6 address and port make a 29-character code and back");
		std::string pc = net_code_pair(four, six);
		CHECK(pc.size() == 46 && pc.substr(0, 11) == net_code(four) && net_code_parse(pc, a4, &a6) && a4 == four && a6 == six,
		      "IPv6 codes: a code with both starts with the IPv4 code; both parts come back");
		net_addr ll = six; ll.a6[0] = 0xfe; ll.a6[1] = 0x80;
		CHECK(!net_code_parse(net_code(ll), back) && net_code_pair(four, net_addr{}) == net_code(four),
		      "IPv6 codes: a link-local address is not a code; without IPv6 the code is the IPv4 one");
		net_addr r;
		CHECK(udp_socket::resolve("[2001:db8::1]", 9, r, c) && r.v6 && r.a6[0] == 0x20 && r.a6[15] == 1 && r.port == 9 && udp_socket::resolve("::1", 9, r, c) && r.v6,
		      "IPv6 addresses: typed with or without brackets");
	}
	// ---- STUN messages ----
	{
		u8 tx[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }, other[12] = {}, req[20];
		stun_request(req, tx);
		net_addr seen{ ip4(198, 51, 100, 23), 40001 }, got;
		auto r = stun_response(req, 20, seen);
		CHECK(stun_parse(r.data(), int(r.size()), tx, got) && got == seen, "STUN: the address in a binding response (XOR-MAPPED-ADDRESS) is read");
		CHECK(!stun_parse(r.data(), int(r.size()), other, got) && !stun_parse(r.data(), 19, tx, got), "STUN: an answer to another request, or a short one, is ignored");
		// a plain MAPPED-ADDRESS (old servers)
		std::vector<u8> m = r;
		m[21] = 0x01;
		m[26] = u8(seen.port >> 8); m[27] = u8(seen.port); m[28] = 198; m[29] = 51; m[30] = 100; m[31] = 23;
		CHECK(stun_parse(m.data(), int(m.size()), tx, got) && got == seen, "STUN: MAPPED-ADDRESS from older servers is read too");
	}
	// ---- UPnP ----
	{
		test_igd r;
		upnp_mapper m;
		m.ssdp_target = r.ssdp_target();
		m.start(7845);
		wait_state(m, 6000);
		auto lg = r.log();
		if (m.status() != upnp_mapper::state::mapped) { printf("  upnp: %s\n", m.error().c_str()); for (auto &l : lg) printf("  router: %s\n", l.c_str()); }
		bool add = false;
		for (auto &l : lg) add = add || (l.find("AddPortMapping lease=3600 port=7845 in=7845 proto=UDP client=127.0.0.1") == 0);
		CHECK(m.status() == upnp_mapper::state::mapped && m.external_ip() == "203.0.113.7" && add, "UPnP: the router is found, UDP 7845 is forwarded to this computer, and its public address read");
		m.stop();
		lg = r.log();
		CHECK(!lg.empty() && lg.back() == "DeletePortMapping port=7845", "UPnP: the forward is removed when hosting stops");
	}
	{
		test_igd r;
		r.permanent_only = true;
		upnp_mapper m;
		m.ssdp_target = r.ssdp_target();
		m.start(7845);
		wait_state(m, 6000);
		auto lg = r.log();
		CHECK(m.status() == upnp_mapper::state::mapped && lg.size() >= 2 && lg[1].find("lease=0") != std::string::npos, "UPnP: a router that only takes permanent forwards (error 725) gets one");
		m.stop();
	}
	{
		test_igd r;
		r.no_service = true;
		int dead = udp_bind(ip4(127, 0, 0, 1), 0);
		const std::string nobody = "127.0.0.1:" + std::to_string(local_of(dead).port);
		upnp_mapper m;
		m.ssdp_target = r.ssdp_target();
		m.pmp_target = nobody;               // and no PCP or NAT-PMP either
		m.start(7845);
		wait_state(m, 9000);
		CHECK(m.status() == upnp_mapper::state::failed && m.error().find("does not offer") != std::string::npos, "UPnP: a device without port mapping is reported, not used");
		upnp_mapper none;
		none.ssdp_target = nobody;
		none.pmp_target = nobody;
		auto t0 = net_now_ms();
		none.start(7845);
		wait_state(none, 10000);
		CHECK(none.status() == upnp_mapper::state::failed && none.error().find("no router") != std::string::npos && none.error().find("PCP or NAT-PMP") != std::string::npos && net_now_ms() - t0 < 6000,
		      "with no router answering UPnP, PCP or NAT-PMP it gives up within 6 seconds");
		close(dead);
	}
	// ---- PCP and NAT-PMP, when there is no UPnP ----
	{
		int dead = udp_bind(ip4(127, 0, 0, 1), 0);
		const std::string nobody = "127.0.0.1:" + std::to_string(local_of(dead).port);
		{
			test_pmp r;
			upnp_mapper m;
			m.ssdp_target = nobody; m.pmp_target = r.target();
			m.start(7845);
			wait_state(m, 9000);
			auto lg = r.log();
			if (m.status() != upnp_mapper::state::mapped) { printf("  pcp: %s\n", m.error().c_str()); for (auto &l : lg) printf("  router: %s\n", l.c_str()); }
			CHECK(m.status() == upnp_mapper::state::mapped && m.method() == "PCP" && m.external_ip() == "203.0.113.9" && !lg.empty() && lg[0] == "PCP MAP port=7845 life=3600",
			      "PCP: a router without UPnP forwards UDP 7845 by PCP, and its public address is read");
			m.stop();
			lg = r.log();
			CHECK(!lg.empty() && lg.back() == "PCP MAP port=7845 life=0", "PCP: the forward is removed (lifetime 0) when hosting stops");
		}
		{
			test_pmp r;
			r.pcp = false;
			upnp_mapper m;
			m.ssdp_target = nobody; m.pmp_target = r.target();
			m.start(7845);
			wait_state(m, 9000);
			auto lg = r.log();
			if (m.status() != upnp_mapper::state::mapped) { printf("  nat-pmp: %s\n", m.error().c_str()); for (auto &l : lg) printf("  router: %s\n", l.c_str()); }
			bool asked = false;
			for (auto &l : lg) asked = asked || l == "PMP MAP port=7845 life=3600";
			CHECK(m.status() == upnp_mapper::state::mapped && m.method() == "NAT-PMP" && m.external_ip() == "198.51.100.77" && asked,
			      "NAT-PMP: a router that answers PCP with \"unsupported version\" gets NAT-PMP, and its public address is read");
			m.stop();
			lg = r.log();
			CHECK(!lg.empty() && lg.back() == "PMP MAP port=7845 life=0", "NAT-PMP: the forward is removed (lifetime 0) when hosting stops");
		}
		{
			test_igd u;
			test_pmp r;
			upnp_mapper m;
			m.ssdp_target = u.ssdp_target(); m.pmp_target = r.target();
			m.start(7845);
			wait_state(m, 9000);
			CHECK(m.status() == upnp_mapper::state::mapped && m.method() == "UPnP" && r.log().empty(), "UPnP comes first: with a UPnP router PCP is not asked");
			m.stop();
		}
		close(dead);
	}
	// ---- hole punching, every pair of routers ----
	{
		printf("  host \\ player        full cone   restricted  port restr. symmetric\n");
		bool expected = true;
		for (int h = 0; h < 4; h++)
		{
			printf("  %-20s", kind_name[h]);
			for (int c = 0; c < 4; c++)
			{
				bool ok = punch(nat_kind(h), nat_kind(c));
				// a symmetric router gives each destination its own port, so the other side
				// can only reach it if its own router lets in any port of that address
				bool want = !((h == symmetric && c >= port_restricted) || (c == symmetric && h >= port_restricted));
				expected = expected && ok == want;
				printf(" %-11s", ok ? (want ? "connects" : "CONNECTS?") : (want ? "FAILS!" : "no (exp.)"));
				fflush(stdout);
			}
			printf("\n");
		}
		CHECK(expected, "punching: 12 of 16 router pairs connect by codes; the 4 that cannot (symmetric with port restricted or symmetric) do not");
		bool fw = true;
		for (int c = 0; c < 4; c++) fw = fw && punch(port_restricted, nat_kind(c), true);
		CHECK(fw, "forwarded port (as UPnP makes): a player behind any router joins with the host's code alone");
	}
	// ---- both behind one router ----
	CHECK(!same_router(false), "same router: without the home-network search the host's code does not connect (the router does not loop back)");
	CHECK(same_router(true), "same router: the player sees its public address equals the host's, finds the host on the home network, and joins");
	CHECK(!same_router(true, true), "same router: a code with another public address does not start the search");
	// ---- connection report (N2.4) ----
	{
		using ct = netplay::conn_type;
		report_result a = report(restricted, symmetric, 20, 30, 0);
		printf("  restricted host, symmetric player, 20 + 30 ms: host %s, player %s, ping %d / %d ms, jitter %d ms\n",
		       netplay::conn_name(a.host), netplay::conn_name(a.player), a.ping_h, a.ping_c, a.jit_c);
		CHECK(a.host == ct::moderate && a.player == ct::strict && a.host_fw == ct::open,
		      "report: a restricted router is moderate, a symmetric one strict, a forwarded port open");
		CHECK(a.ping_h >= 48 && a.ping_h <= 75 && a.ping_c >= 48 && a.ping_c <= 75 && a.direct,
		      "report: both see a ping of the two routers' delays (20 + 30 ms) and a direct path");
		report_result b = report(full_cone, restricted, 10, 10, 30);
		printf("  full cone host, restricted player, 10 + 10 ms, 0..30 ms spread: host %s, player %s, ping %d ms, jitter %d ms (and %d without spread)\n",
		       netplay::conn_name(b.host), netplay::conn_name(b.player), b.ping_c, b.jit_c, a.jit_c);
		CHECK(b.host == ct::moderate && b.player == ct::moderate && b.jit_c >= 4 && b.jit_c <= 30 && a.jit_c <= 4,
		      "report: cone routers are moderate; jitter follows the spread the router adds (about 10 ms for 0..30) and is near 0 without");
	}
	// ---- mesh (N2.5) ----
	{
		// joining four peers through the simulated routers can miss in a busy WSL; only a
		// setup that failed is tried again (up to three times), never a slow or wrong game
		auto run = [](bool m) { mesh_result r; for (int t = 0; t < 3 && !r.setup; t++) r = mesh_run(m); return r; };
		mesh_result hub = run(false), mesh = run(true);
		printf("  four peers, 40/5/40/5 ms: through the host %.0f ms per 4 frames, with the mesh %.0f ms (%u frames merged by players)\n", hub.ms_per4, mesh.ms_per4, mesh.mesh_frames);
		CHECK(hub.ok && mesh.ok, "mesh: four players play 300 frames of lockstep without a desync, with and without the mesh");
		CHECK(mesh.ms_per4 >= 30 && mesh.ms_per4 <= 60 && hub.ms_per4 >= 70 && mesh.mesh_frames > 0,
		      "mesh: the delay follows the worst direct pair (40 ms), not the sum through the host (80 ms)");
	}
	// ---- peer relay (N2.3) ----
	{
		relay_result r = relay_play();
		CHECK(r.joined && r.relayed, "relay: a player behind a symmetric router joins a host behind one through a member behind a full cone router");
		CHECK(r.shown, "relay: the host's player list says the player comes through J2");
		CHECK(r.report, "report: the host sees the player relayed by player 2; the relayed player is strict and sees player 2 through the host");
		CHECK(r.played, "relay: the three play 300 frames of lockstep without a desync");
		CHECK(r.capped, "relay: what a member forwards for one player is capped (64 KB a second)");
	}
	// ---- IPv6 (N2.2) ----
	{
		netplay probe;
		std::string err;
		if (probe.host(0, err) && probe.socket().has_ipv6())
		{
			probe.stop();
			bool v6 = false;
			CHECK(ipv6_join(0, v6) && v6, "IPv6: a player joins by the host's IPv6 address");
			CHECK(ipv6_join(1, v6) && v6, "IPv6: with a code with both, IPv6 is tried first and used");
			CHECK(ipv6_join(2, v6) && !v6, "IPv6: when IPv6 does not answer the player goes on to IPv4, and the host counts them once");
		}
		else printf("skip IPv6: this computer has no IPv6 (not even ::1)\n");
	}
	printf("\n%d checks, %d failed\n", g_run, g_fail);
	return g_fail ? 1 : 0;
}
