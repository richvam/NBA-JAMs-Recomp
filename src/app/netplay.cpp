// netplay.cpp - see netplay.h
#include "netplay.h"
#include "nat.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600   // inet_pton and inet_ntop (Vista and later)
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
static void sock_init() { static bool done = false; if (!done) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); done = true; } }
static void sock_close(long long fd) { closesocket(SOCKET(fd)); }
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
static void sock_init() {}
static void sock_close(long long fd) { ::close(int(fd)); }
#endif

static constexpr u8 MAGIC = 0x4a;
static constexpr u16 DEFAULT_PORT = 7845;   // the home-network search also asks here
static constexpr u16 VERSION = 2;      // online protocol version, independent of the program version (version.h)
static constexpr u32 TIMEOUT_MS = 10000;
static constexpr int CHUNK = 1024;

u32 net_now_ms()
{
	using namespace std::chrono;
	static const auto t0 = steady_clock::now();
	return u32(duration_cast<milliseconds>(steady_clock::now() - t0).count()) + 1;
}

// ---------------------------------------------------------------------------
// input packing
// ---------------------------------------------------------------------------
u16 net_pack_input(const cabinet_inputs::player &p)
{
	return u16(p.up | p.down << 1 | p.left << 2 | p.right << 3 | p.shoot << 4 | p.pass << 5 | p.turbo << 6 | p.start << 7 | p.coin << 8);
}

void net_unpack_input(u16 v, cabinet_inputs::player &p)
{
	p.up = v & 1; p.down = v >> 1 & 1; p.left = v >> 2 & 1; p.right = v >> 3 & 1; p.shoot = v >> 4 & 1;
	p.pass = v >> 5 & 1; p.turbo = v >> 6 & 1; p.start = v >> 7 & 1; p.coin = v >> 8 & 1;
}

// ---------------------------------------------------------------------------
// sockets
// ---------------------------------------------------------------------------
bool net_addr::operator==(const net_addr &o) const
{
	if (v6 != o.v6 || port != o.port) return false;
	return v6 ? memcmp(a6, o.a6, 16) == 0 : ip == o.ip;
}

std::string net_addr_str(const net_addr &a)
{
	char b[64];
	if (a.v6)
	{
		char t[48] = {};
		inet_ntop(AF_INET6, (void *)a.a6, t, sizeof(t));
		snprintf(b, sizeof(b), "[%s]:%u", t, a.port);
		return b;
	}
	snprintf(b, sizeof(b), "%u.%u.%u.%u:%u", a.ip >> 24, a.ip >> 16 & 255, a.ip >> 8 & 255, a.ip & 255, a.port);
	return b;
}

typedef decltype(socket(0, 0, 0)) sock_fd;

static long long udp_new(int family)
{
#ifdef _WIN32
	SOCKET s = socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) return -1;
	u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
	return (long long)s;
#else
	int s = socket(family, SOCK_DGRAM, 0);
	if (s < 0) return -1;
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
	return s;
#endif
}

bool udp_socket::open(u16 port, std::string &err, u32 bind_ip)
{
	sock_init();
	close();
	m_fd = udp_new(AF_INET);
	if (m_fd < 0) { err = "could not create the socket"; return false; }
	int one = 1;
	setsockopt(sock_fd(m_fd), SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
	setsockopt(sock_fd(m_fd), SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one));   // the home-network search
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(bind_ip ? bind_ip : INADDR_ANY);
	a.sin_port = htons(port);
	if (bind(sock_fd(m_fd), (sockaddr *)&a, sizeof(a)) != 0)
	{
		err = "could not open UDP port " + std::to_string(port) + " (in use?)";
		close();
		return false;
	}
	// IPv6 beside it, on the same port; IPv6 only, so IPv4 keeps its own socket. A computer
	// without IPv6 simply has no second socket.
	if (bind_ip == 0)
	{
		m_fd6 = udp_new(AF_INET6);
		if (m_fd6 >= 0)
		{
			setsockopt(sock_fd(m_fd6), IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&one, sizeof(one));
			sockaddr_in6 a6{};
			a6.sin6_family = AF_INET6;
			a6.sin6_addr = in6addr_any;
			a6.sin6_port = htons(local_port());
			if (bind(sock_fd(m_fd6), (sockaddr *)&a6, sizeof(a6)) != 0) { sock_close(m_fd6); m_fd6 = -1; }
		}
	}
	return true;
}

void udp_socket::close()
{
	if (m_fd >= 0) sock_close(m_fd);
	if (m_fd6 >= 0) sock_close(m_fd6);
	m_fd = m_fd6 = -1;
}

void udp_socket::send(const net_addr &to, const void *data, size_t len)
{
	if (m_fd < 0) return;
	if (loss > 0)
	{
		m_lossrng = m_lossrng * 1103515245u + 12345u;
		if (double((m_lossrng >> 8) & 0xffff) / 65536.0 < loss) return;
	}
	if (to.v6)
	{
		if (m_fd6 < 0) return;
		sockaddr_in6 a6{};
		a6.sin6_family = AF_INET6;
		memcpy(&a6.sin6_addr, to.a6, 16);
		a6.sin6_port = htons(to.port);
		sendto(sock_fd(m_fd6), (const char *)data, int(len), 0, (sockaddr *)&a6, sizeof(a6));
		return;
	}
	sockaddr_in a{};
	a.sin_family = AF_INET;
	if (via.ip && to.ip != 0x7f000001)
	{
		std::vector<u8> w(6 + len);
		w[0] = u8(to.ip >> 24); w[1] = u8(to.ip >> 16); w[2] = u8(to.ip >> 8); w[3] = u8(to.ip); w[4] = u8(to.port >> 8); w[5] = u8(to.port);
		memcpy(w.data() + 6, data, len);
		a.sin_addr.s_addr = htonl(via.ip);
		a.sin_port = htons(via.port);
		sendto(sock_fd(m_fd), (const char *)w.data(), int(w.size()), 0, (sockaddr *)&a, sizeof(a));
		return;
	}
	a.sin_addr.s_addr = htonl(to.ip);
	a.sin_port = htons(to.port);
	sendto(sock_fd(m_fd), (const char *)data, int(len), 0, (sockaddr *)&a, sizeof(a));
}

int udp_socket::recv(net_addr &from, void *data, size_t cap)
{
	if (m_fd < 0) return -1;
	sockaddr_in a{};
	socklen_t al = sizeof(a);
	int n = int(recvfrom(sock_fd(m_fd), (char *)data, int(cap), 0, (sockaddr *)&a, &al));
	if (n < 0)
	{
		if (m_fd6 < 0) return -1;
		sockaddr_in6 a6{};
		socklen_t al6 = sizeof(a6);
		n = int(recvfrom(sock_fd(m_fd6), (char *)data, int(cap), 0, (sockaddr *)&a6, &al6));
		if (n < 0) return -1;
		from = {};
		from.v6 = true;
		memcpy(from.a6, &a6.sin6_addr, 16);
		from.port = ntohs(a6.sin6_port);
		return n;
	}
	from = {};
	from.ip = ntohl(a.sin_addr.s_addr);
	from.port = ntohs(a.sin_port);
	if (via.ip && from == via && n >= 6)
	{
		u8 *b = (u8 *)data;
		from.ip = u32(b[0]) << 24 | u32(b[1]) << 16 | u32(b[2]) << 8 | b[3];
		from.port = u16(b[4] << 8 | b[5]);
		memmove(b, b + 6, size_t(n - 6));
		n -= 6;
	}
	return n;
}

u16 udp_socket::local_port() const
{
	if (m_fd < 0) return 0;
	sockaddr_in a{};
	socklen_t al = sizeof(a);
	if (getsockname(sock_fd(m_fd), (sockaddr *)&a, &al) != 0) return 0;
	return ntohs(a.sin_port);
}

bool udp_socket::resolve(const std::string &host, u16 port, net_addr &out, std::string &err)
{
	sock_init();
	std::string h = host;
	if (h.size() > 2 && h.front() == '[' && h.back() == ']') h = h.substr(1, h.size() - 2);   // [2001:db8::1]
	out = {};
	out.port = port;
	in6_addr t6{};
	if (h.find(':') != std::string::npos && inet_pton(AF_INET6, h.c_str(), &t6) == 1)
	{
		out.v6 = true;
		memcpy(out.a6, &t6, 16);
		return true;
	}
	// a name: its IPv4 address, else its IPv6 address
	for (int fam : { AF_INET, AF_INET6 })
	{
		addrinfo hints{}, *res = nullptr;
		hints.ai_family = fam;
		hints.ai_socktype = SOCK_DGRAM;
		if (getaddrinfo(h.c_str(), nullptr, &hints, &res) != 0 || !res) continue;
		if (fam == AF_INET) out.ip = ntohl(((sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
		else { out.v6 = true; memcpy(out.a6, &((sockaddr_in6 *)res->ai_addr)->sin6_addr, 16); }
		freeaddrinfo(res);
		return true;
	}
	err = "address not found: '" + host + "'";
	return false;
}

bool net_local_ipv6(net_addr &out)
{
	sock_init();
	out = {};
	// the source address the system picks for the internet (no packet is sent by connect() on UDP)
	auto s = socket(AF_INET6, SOCK_DGRAM, 0);
	sockaddr_in6 a{};
	a.sin6_family = AF_INET6;
	a.sin6_port = htons(53);
	const u8 google[16] = { 0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88 };
	memcpy(&a.sin6_addr, google, 16);
	bool ok = false;
	if (connect(s, (sockaddr *)&a, sizeof(a)) == 0)
	{
		sockaddr_in6 me{};
		socklen_t ml = sizeof(me);
		if (getsockname(s, (sockaddr *)&me, &ml) == 0)
		{
			const u8 *b = (const u8 *)&me.sin6_addr;
			ok = (b[0] & 0xe0) == 0x20;          // global unicast (2000::/3), not link-local or unique-local
			if (ok) { out.v6 = true; memcpy(out.a6, b, 16); }
		}
	}
	sock_close((long long)s);
	return ok;
}

std::vector<std::string> net_local_ipv4()
{
	sock_init();
	std::vector<std::string> out;
	auto add = [&](u32 ip) {
		if (ip == 0 || (ip >> 24) == 127) return;
		net_addr a{ ip, 0 };
		std::string s = net_addr_str(a);
		s = s.substr(0, s.find(':'));
		if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
	};
	// primary outbound interface (no packet is sent by connect() on UDP)
	auto s = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(53);
	a.sin_addr.s_addr = htonl(0x08080808);
	if (connect(s, (sockaddr *)&a, sizeof(a)) == 0)
	{
		sockaddr_in me{};
		socklen_t ml = sizeof(me);
		if (getsockname(s, (sockaddr *)&me, &ml) == 0) add(ntohl(me.sin_addr.s_addr));
	}
	sock_close((long long)s);
	char name[256];
	if (gethostname(name, sizeof(name)) == 0)
	{
		addrinfo hints{}, *res = nullptr;
		hints.ai_family = AF_INET;
		if (getaddrinfo(name, nullptr, &hints, &res) == 0)
		{
			for (addrinfo *r = res; r; r = r->ai_next) add(ntohl(((sockaddr_in *)r->ai_addr)->sin_addr.s_addr));
			freeaddrinfo(res);
		}
	}
	return out;
}

// ---------------------------------------------------------------------------
// packet helpers
// ---------------------------------------------------------------------------
namespace {
struct wbuf
{
	std::vector<u8> b;
	void u8_(u8 v) { b.push_back(v); }
	void u16_(u16 v) { u8_(u8(v)); u8_(u8(v >> 8)); }
	void u32_(u32 v) { u16_(u16(v)); u16_(u16(v >> 16)); }
	void u64_(u64 v) { u32_(u32(v)); u32_(u32(v >> 32)); }
	void str(const std::string &s) { u8_(u8(std::min<size_t>(s.size(), 255))); b.insert(b.end(), s.begin(), s.begin() + long(std::min<size_t>(s.size(), 255))); }
	void bytes(const u8 *p, size_t n) { b.insert(b.end(), p, p + n); }
};
struct rbuf
{
	const u8 *p; int n, i = 0;
	bool ok = true;
	bool need(int k) { if (i + k > n) ok = false; return ok; }
	u8 u8_() { return need(1) ? p[i++] : 0; }
	u16 u16_() { u16 lo = u8_(); return u16(lo | (u8_() << 8)); }
	u32 u32_() { u32 lo = u16_(); return lo | (u32(u16_()) << 16); }
	u64 u64_() { u64 lo = u32_(); return lo | (u64(u32_()) << 32); }
	std::string str() { int l = u8_(); if (!need(l)) return {}; std::string s((const char *)p + i, size_t(l)); i += l; return s; }
};
}

// ---------------------------------------------------------------------------
// netplay
// ---------------------------------------------------------------------------
netplay::netplay() {}
netplay::~netplay() { stop(); }

bool netplay::host(u16 port, std::string &err)
{
	stop();
	reset_state();
	if (!m_sock.open(port, err)) return false;
	m_mode = mode::host;
	m_phase = phase::lobby;
	m_slot = 0;
	m_nplayers = 1;
	return true;
}

bool netplay::join(const std::string &host, u16 port, std::string &err)
{
	stop();
	net_addr a, a6, relay;
	if (!net_code_parse(host, a, &a6, &relay) && !udp_socket::resolve(host, port, a, err)) return false;
	reset_state();
	if (!m_sock.open(0, err)) return false;
	if (a.v6 && !m_sock.has_ipv6()) { m_sock.close(); err = "that is an IPv6 address, and this computer has no IPv6"; return false; }
	if (a6.v6 && m_sock.has_ipv6()) { m_target6 = a6; m_target6_t = net_now_ms(); }
	m_mode = mode::client;
	m_phase = phase::lobby;
	peer h;
	h.addr = a;
	m_target = a;
	h.last_seen = net_now_ms();
	m_peers.push_back(h);
	m_slot = -1;
	m_relay = relay;
	m_join_t = net_now_ms();
	return true;
}

void netplay::stop(const char *reason)
{
	if (m_mode != mode::off)
	{
		std::vector<u8> b{ u8('b'), MAGIC, 0, 0, 0, 0, 0, 0, 0, 0 };
		for (int k = 0; k < 3; k++)
			for (auto &p : m_peers) send_raw(p.addr, b);
	}
	m_sock.close();
	m_mode = mode::off;
	m_stun = stun_state::off;
	m_invites.clear();
	m_phase = phase::ended;
	if (reason) m_error = reason;
}

void netplay::reset_state()
{
	m_phase = phase::lobby;
	m_error.clear();
	m_slot = 0; m_nplayers = 1;
	m_peers.clear();
	m_last_tick = m_hello_t = m_start_t = 0;
	m_ping = 0;
	m_got_start = false; m_game_id.clear(); m_fp = 0; m_dips = dip_settings(); m_nv.clear(); m_nvlen = 0; m_lobby_mods.clear();
	m_nchunks = m_chunks_ok = 0; m_chunk_have.clear();
	m_client_ready_sent = m_client_ok = false; m_client_why.clear();
	m_delay = 4; m_frame = 0;
	for (auto &m : m_in) m.clear();
	m_merged.clear(); m_merged_upto = 0; m_stall_since = 0; m_hash.clear();
	m_desync = false;
	m_roster.clear(); m_roster_t = 0; m_mesh_frames = 0;
	m_seen_as = {}; m_host_links.clear(); m_report_t = 0; m_jitter = m_last_rtt = 0;
	m_target = {}; m_target6 = {}; m_relay = {}; m_relay_on = false; m_routes.clear(); m_flows.clear(); m_relayed = 0; m_also_public = 0; m_lan_t = 0; m_lan_found = false;
}

// ---------------------------------------------------------------------------
// peer relay (roadmap N2.3)
//   'X' [address] [packet]: to a relay, "send this packet to that address"
//   'Y' [address] [packet]: from a relay to its host, "this packet came from that address"
//   'I' [address]: host to its players, "send to this address for 60 s" (opens their routers)
// The address is 1 byte family (4 or 6), the IPv4 or IPv6 address, the port (big-endian).
// ---------------------------------------------------------------------------
static void put_addr(std::vector<u8> &w, const net_addr &a)
{
	w.push_back(a.v6 ? 6 : 4);
	if (a.v6) w.insert(w.end(), a.a6, a.a6 + 16);
	else for (int s = 24; s >= 0; s -= 8) w.push_back(u8(a.ip >> s));
	w.push_back(u8(a.port >> 8)); w.push_back(u8(a.port));
}

static int get_addr(const u8 *p, int n, net_addr &a)     // bytes used, 0 when malformed
{
	a = {};
	if (n < 1 || (p[0] != 4 && p[0] != 6)) return 0;
	int len = p[0] == 6 ? 16 : 4;
	if (n < 1 + len + 2) return 0;
	if (len == 16) { a.v6 = true; memcpy(a.a6, p + 1, 16); }
	else a.ip = u32(p[1]) << 24 | u32(p[2]) << 16 | u32(p[3]) << 8 | p[4];
	a.port = u16(p[1 + len] << 8 | p[2 + len]);
	return 1 + len + 2;
}

static std::vector<u8> wrap(char t, const net_addr &a, const u8 *p, size_t n)
{
	std::vector<u8> w{ u8(t), MAGIC };
	put_addr(w, a);
	w.insert(w.end(), p, p + n);
	return w;
}

bool netplay::relay_budget(const net_addr &other, size_t bytes)
{
	u32 now = net_now_ms();
	for (auto &f : m_flows)
		if (f.other == other)
		{
			if (now - f.t0 >= 1000) { f.t0 = now; f.bytes = 0; }
			if (f.bytes + bytes > RELAY_BYTES_PER_S) return false;
			f.bytes += u32(bytes);
			return true;
		}
	if (m_flows.size() >= 8) m_flows.erase(m_flows.begin());
	m_flows.push_back({ other, now, u32(bytes) });
	return true;
}

// a relay packet; true when it was one (handled here)
bool netplay::relay_packet(const net_addr &from, const u8 *p, int n)
{
	if (n < 2 || p[1] != MAGIC || (p[0] != 'X' && p[0] != 'Y' && p[0] != 'I')) return false;
	net_addr a;
	int k = get_addr(p + 2, n - 2, a);
	if (!k) return true;
	const u8 *in = p + 2 + k;
	int inn = n - 2 - k;
	const bool from_host = m_mode == mode::client && !m_peers.empty() && from == m_peers[0].addr && !m_relay_on;
	if (p[0] == 'I') { if (from_host) invite(a); return true; }
	if (p[0] == 'X' && m_mode == mode::client && m_slot > 0 && inn >= 10)
	{
		// a relay: from the host to a player, or from a player to the host
		if (from_host) { if (relay_budget(a, size_t(inn))) { m_sock.send(a, in, size_t(inn)); m_relayed += u32(inn); } }
		else if (relay_budget(from, size_t(inn)))   // only ever to our own host, whatever address it names
		{
			auto w = wrap('Y', from, in, size_t(inn));
			m_sock.send(m_peers[0].addr, w.data(), w.size());
			m_relayed += u32(inn);
		}
		return true;
	}
	if (p[0] == 'Y' && m_mode == mode::host && inn >= 10 && in[1] == MAGIC)
	{
		// a player that reaches us only through one of ours
		bool member = false;
		for (auto &q : m_peers) member = member || q.addr == from;
		if (!member) return true;
		bool known = false;
		for (auto &r : m_routes) if (r.first == a) { r.second = from; known = true; }
		if (!known) m_routes.push_back({ a, from });
		handle(a, in, inn);
		return true;
	}
	return true;
}

void netplay::send_raw(const net_addr &to, const std::vector<u8> &b)
{
	if (m_mode == mode::host)
		for (auto &r : m_routes)
			if (r.first == to) { auto w = wrap('X', to, b.data(), b.size()); m_sock.send(r.second, w.data(), w.size()); return; }
	if (m_mode == mode::client && m_relay_on && !m_peers.empty() && to == m_peers[0].addr)
	{
		auto w = wrap('X', to, b.data(), b.size());
		m_sock.send(m_relay, w.data(), w.size());
		return;
	}
	m_sock.send(to, b.data(), b.size());
}

// smoothed round trip and jitter (RFC 3550: the mean change between samples, 1/8 a step here)
void netplay::sample(int &ping, int &jitter, int &last, int rtt)
{
	if (rtt < 0 || rtt > 60000) return;
	ping = ping ? (ping * 3 + rtt) / 4 : rtt;
	if (last) jitter += (std::abs(rtt - last) - jitter) / 8;
	last = rtt;
}

std::vector<netplay::link_info> netplay::links() const
{
	std::vector<link_info> v;
	if (m_mode == mode::host)
		for (auto &p : m_peers)
		{
			link_info l;
			l.slot = p.slot; l.ping = p.ping; l.jitter = p.jitter; l.v6 = p.addr.v6;
			for (auto &r : m_routes)
				if (r.first == p.addr) { l.relayed = true; for (auto &q : m_peers) if (q.addr == r.second) l.through = q.slot; }
			v.push_back(l);
		}
	else if (m_mode == mode::client && m_slot >= 0)
	{
		// the host directly; every other player through the host, so their link adds to ours
		link_info h;
		h.slot = 0; h.ping = m_ping; h.jitter = m_jitter; h.relayed = m_relay_on; h.v6 = !m_peers.empty() && m_peers[0].addr.v6;
		v.push_back(h);
		for (auto &o : m_host_links)
		{
			if (o.slot == m_slot) continue;
			link_info l = o;
			l.ping += m_ping; l.jitter += m_jitter; l.via_host = true;
			v.push_back(l);
		}
	}
	return v;
}

netplay::conn_type netplay::connection(bool port_forwarded) const
{
	if (m_mode == mode::client && m_relay_on) return conn_type::strict;
	if (port_forwarded || m_seen_as.v6) return conn_type::open;
	if (m_stun != stun_state::found) return conn_type::unknown;
	// a router that gives each destination its own port: the other side sees another port
	// than the STUN server did, and only a relay helps against such a router on both ends
	if (m_seen_as.port && !m_seen_as.v6 && m_seen_as.ip == m_public.ip && m_seen_as.port != m_public.port) return conn_type::strict;
	return conn_type::moderate;
}

const char *netplay::conn_name(conn_type t)
{
	switch (t)
	{
		case conn_type::open: return "open";
		case conn_type::moderate: return "moderate";
		case conn_type::strict: return "strict";
		default: return "not known yet";
	}
}

// ---------------------------------------------------------------------------
// mesh (roadmap N2.5)
//   'D' MAGIC slot first count inputs...: a player's own inputs, sent straight to every other
//   player (and the host's to every player) besides the usual way through the host
// ---------------------------------------------------------------------------
void netplay::send_direct(const net_addr &to)
{
	auto &mine = m_in[m_slot];
	if (mine.empty()) return;
	u32 last = mine.rbegin()->first + 1;
	u32 first = last > 16 ? last - 16 : 0;
	if (first < m_frame) first = std::min(m_frame, last);
	std::vector<u8> w{ u8('D'), MAGIC, u8(m_slot) };
	for (int s = 0; s < 4; s++) w.push_back(u8(first >> (8 * s)));
	w.push_back(u8(last - first));
	for (u32 f = first; f < last; f++) { u16 v = input_of(m_slot, f); w.push_back(u8(v)); w.push_back(u8(v >> 8)); }
	send_raw(to, w);
}

bool netplay::mesh_packet(const net_addr &from, const u8 *p, int n)
{
	if (n < 2 || p[0] != 'D' || p[1] != MAGIC) return false;
	if (m_mode != mode::client || !mesh || m_phase != phase::running || n < 8) return true;
	int slot = p[2];
	bool known = slot == 0 ? (!m_peers.empty() && from == m_peers[0].addr) : false;
	for (auto &m : m_roster) known = known || (m.first == slot && m.second == from);
	if (!known || slot == m_slot || slot >= m_nplayers) return true;
	u32 first = u32(p[3]) | u32(p[4]) << 8 | u32(p[5]) << 16 | u32(p[6]) << 24;
	int cnt = p[7];
	if (n < 8 + 2 * cnt) return true;
	for (int k = 0; k < cnt; k++)
	{
		u32 f = first + u32(k);
		if (f >= u32(m_delay) && f >= m_frame) m_in[slot].emplace(f, u16(p[8 + 2 * k] | p[9 + 2 * k] << 8));
	}
	return true;
}

net_addr netplay::relay_candidate() const
{
	// the member with the lowest ping that is not relayed itself and is reached over IPv4
	const peer *best = nullptr;
	for (auto &p : m_peers)
	{
		bool routed = false;
		for (auto &r : m_routes) routed = routed || r.first == p.addr;
		if (routed || p.addr.v6 || p.addr.ip == 0x7f000001) continue;
		if (!best || p.ping < best->ping) best = &p;
	}
	return best ? best->addr : net_addr{};
}

std::vector<std::string> netplay::peer_list() const
{
	std::vector<std::string> v;
	for (auto &p : m_peers)
	{
		std::string via;
		for (auto &r : m_routes)
			if (r.first == p.addr)
				for (auto &q : m_peers) if (q.addr == r.second) via = "  through J" + std::to_string(q.slot + 1);
		v.push_back("J" + std::to_string(p.slot + 1) + "  " + net_addr_str(p.addr) + "  " + std::to_string(p.ping) + " ms" + via + (p.ready ? "  ready" : ""));
	}
	return v;
}

void netplay::host_start(const std::string &game_id, u32 fingerprint, const dip_settings &dips, const std::vector<u8> &nvram, int delay, const std::string &mods)
{
	m_game_id = game_id;
	m_fp = fingerprint;
	m_dips = dips;
	m_nv = nvram;
	m_nvlen = nvram.size();
	m_nv.insert(m_nv.end(), mods.begin(), mods.end());
	m_delay = std::clamp(delay, 1, 15);
	m_nchunks = int((m_nv.size() + CHUNK - 1) / CHUNK);
	m_phase = phase::transfer;
	m_frame = 0;
	for (int s = 0; s < 4; s++) { m_in[s].clear(); for (int f = 0; f < m_delay; f++) m_in[s][u32(f)] = 0; }
	m_merged.clear();
	for (int f = 0; f < m_delay; f++) m_merged[u32(f)] = 0;
	m_merged_upto = u32(m_delay);
	for (auto &p : m_peers) { p.ready = false; p.start_acked = false; p.chunk_mask_lo = p.chunk_mask_hi = 0; p.acked_merged = 0; }
	if (m_peers.empty()) m_phase = phase::running;
}

bool netplay::all_ready() const
{
	for (auto &p : m_peers) if (!p.ready) return false;
	return true;
}

void netplay::client_ready(bool ok, const std::string &why)
{
	m_client_ready_sent = true;
	m_client_ok = ok;
	m_client_why = why;
	if (!ok) m_error = why;
}

bool netplay::has_input(int slot, u32 f) const { return m_in[slot].count(f) != 0; }
u16 netplay::input_of(int slot, u32 f) const { auto it = m_in[slot].find(f); return it == m_in[slot].end() ? 0 : it->second; }

void netplay::try_merge()
{
	while (true)
	{
		u32 f = m_merged_upto;
		for (int s = 0; s < m_nplayers; s++) if (!has_input(s, f)) return;
		u64 m = 0;
		for (int s = 0; s < m_nplayers; s++) m |= u64(input_of(s, f)) << (16 * s);
		m_merged[f] = m;
		m_merged_upto = f + 1;
	}
}

bool netplay::can_run() const
{
	return m_phase == phase::running && (m_merged.count(m_frame) != 0 || local_merge_ok(m_frame));
}

// N2.5: a player with every player's input for frame f (sent straight to it, or its own)
// merges that frame itself instead of waiting for the host's merge, which is the same
bool netplay::local_merge_ok(u32 f) const
{
	if (m_mode != mode::client || !mesh || m_slot < 0) return false;
	for (int s = 0; s < m_nplayers; s++) if (!has_input(s, f)) return false;
	return true;
}

int netplay::buffered() const
{
	int n = 0;
	while (m_merged.count(m_frame + u32(n))) n++;
	return n;
}

u32 netplay::stall_ms() const
{
	return m_stall_since ? net_now_ms() - m_stall_since : 0;
}

cabinet_inputs netplay::frame_inputs(const cabinet_inputs::player &local_now)
{
	u32 tf = m_frame + u32(m_delay);
	if (!has_input(m_slot, tf)) m_in[m_slot][tf] = net_pack_input(local_now);
	if (m_mode == mode::host) try_merge();
	if (!m_merged.count(m_frame) && local_merge_ok(m_frame))
	{
		u64 m = 0;
		for (int s = 0; s < m_nplayers; s++) m |= u64(input_of(s, m_frame)) << (16 * s);
		m_merged[m_frame] = m;
		m_mesh_frames++;
	}
	cabinet_inputs in{};
	auto it = m_merged.find(m_frame);
	u64 m = it == m_merged.end() ? 0 : it->second;
	for (int s = 0; s < 4; s++) net_unpack_input(u16(m >> (16 * s)), in.p[s]);
	return in;
}

void netplay::frame_done(u32 hash)
{
	if (m_frame % 30 == 0) m_hash[m_frame] = hash;
	m_frame++;
	// forget old history
	u32 keep = m_frame > 900 ? m_frame - 900 : 0;
	auto trim = [&](auto &mp) { while (!mp.empty() && mp.begin()->first < keep) mp.erase(mp.begin()); };
	for (auto &m : m_in) trim(m);
	trim(m_merged);
	trim(m_hash);
	if (m_mode == mode::host) try_merge();
}

// ---------------------------------------------------------------------------
// receive
// ---------------------------------------------------------------------------
void netplay::poll()
{
	if (m_mode == mode::off) return;
	u8 buf[2048];
	net_addr from;
	int n;
	while ((n = m_sock.recv(from, buf, sizeof(buf))) >= 0)
	{
		// a relayed player hears its host from the relay
		if (m_relay_on && from == m_relay && n >= 2 && buf[1] == MAGIC && buf[0] != 'X' && buf[0] != 'Y') from = m_peers[0].addr;
		if (relay_packet(from, buf, n)) continue;
		if (mesh_packet(from, buf, n)) continue;
		if (n >= 10 && buf[1] == MAGIC) handle(from, buf, n);
		else if (m_stun != stun_state::off && from == m_stun_server)
		{
			net_addr pub;
			if (stun_parse(buf, n, m_stun_tx, pub)) { m_public = pub; m_stun = stun_state::found; m_stun_tries = 0; }
		}
	}
	if (m_mode == mode::off) return;
	u32 now = net_now_ms();
	tick_stun(now);
	for (size_t i = 0; i < m_invites.size();)
	{
		if (int(now - m_invites[i].until) > 0) { m_invites.erase(m_invites.begin() + long(i)); continue; }
		if (now - m_hello_t >= 100)
		{
			std::vector<u8> b{ u8('p'), MAGIC, 0, 0, 0, 0, 0, 0, 0, 0 };
			send_raw(m_invites[i].to, b);
		}
		i++;
	}
	if (m_phase == phase::running && !m_merged.count(m_frame)) { if (!m_stall_since) m_stall_since = now; }
	else m_stall_since = 0;
	if (m_mode == mode::host) tick_host(now); else tick_client(now);
}

void netplay::handle(const net_addr &from, const u8 *p, int n)
{
	rbuf r{ p, n };
	u8 type = r.u8_();
	r.u8_();
	u32 tsend = r.u32_(), techo = r.u32_();
	u32 now = net_now_ms();

	peer *pp = nullptr;
	for (auto &q : m_peers) if (q.addr == from) pp = &q;

	if (m_mode == mode::host)
	{
		if (type == 'q')
		{
			// a player on the home network looks for the host with this public address
			u32 ip = r.u32_();
			bool mine = r.ok && ip && ((m_stun == stun_state::found && ip == m_public.ip) || ip == m_also_public);
			if (mine && m_phase == phase::lobby)
			{
				wbuf w;
				w.u8_('Q'); w.u8_(MAGIC); w.u32_(now); w.u32_(tsend);
				send_raw(from, w.b);
			}
			return;
		}
		if (type == 'h')
		{
			u16 ver = r.u16_();
			if (!pp)
			{
				wbuf w;
				if (ver != VERSION || m_phase != phase::lobby || m_nplayers >= 4)
				{
					w.u8_('r'); w.u8_(MAGIC); w.u32_(now); w.u32_(tsend);
					w.str(ver != VERSION ? "different NBA JAM Recomp version" : m_phase != phase::lobby ? "the game has already started" : "the game is full (4)");
					send_raw(from, w.b);
					return;
				}
				peer np;
				np.addr = from;
				np.slot = m_nplayers++;
				m_peers.push_back(np);
				pp = &m_peers.back();
			}
			wbuf w;
			w.u8_('w'); w.u8_(MAGIC); w.u32_(now); w.u32_(tsend);
			w.u8_(u8(pp->slot)); w.u8_(u8(m_nplayers));
			send_raw(from, w.b);
		}
		if (!pp) return;
		pp->last_seen = now;
		pp->echo = tsend;
		pp->echo_at = now;
		if (techo) sample(pp->ping, pp->jitter, pp->last_rtt, int(now - techo));
		if (type == 'o')
		{
			// what the player sees as our address (N2.4)
			net_addr a;
			if (r.i < n && get_addr(p + r.i, n - r.i, a)) m_seen_as = a;
			return;
		}
		if (type == 'b')
		{
			std::string who = "P" + std::to_string(pp->slot + 1);
			if (m_phase == phase::lobby)
			{
				// renumber remaining clients
				m_peers.erase(m_peers.begin() + (pp - m_peers.data()));
				m_nplayers = 1;
				for (auto &q : m_peers) q.slot = m_nplayers++;
			}
			else stop(("player " + who + " has disconnected").c_str());
			return;
		}
		if (type == 'a')
		{
			pp->start_acked = r.u8_() != 0;
			pp->chunk_mask_lo = r.u32_();
			pp->chunk_mask_hi = r.u32_();
			u8 ready = r.u8_();
			std::string why = r.str();
			if (ready == 2) { stop(("J" + std::to_string(pp->slot + 1) + ": " + why).c_str()); return; }
			pp->ready = ready == 1;
			if (m_phase == phase::transfer && all_ready()) m_phase = phase::running;
		}
		if (type == 'i' && m_phase == phase::running)
		{
			int slot = r.u8_();
			u32 acked = r.u32_();
			u32 first = r.u32_();
			int cnt = r.u8_();
			if (slot != pp->slot) return;
			pp->acked_merged = std::max(pp->acked_merged, acked);
			for (int k = 0; k < cnt && r.ok; k++)
			{
				u16 v = r.u16_();
				if (r.ok && first + u32(k) >= u32(m_delay)) m_in[slot].emplace(first + u32(k), v);
			}
			u32 hf = r.u32_(), h = r.u32_();
			if (r.ok && hf)
			{
				auto it = m_hash.find(hf);
				if (it != m_hash.end() && it->second != h) m_desync = true;
			}
			try_merge();
		}
		return;
	}

	// ---- client ----
	if (type == 'q') return;
	if (type == 'Q')
	{
		// the host answered on the home network: talk to it there
		if (m_slot < 0 && !m_peers.empty() && !m_lan_found)
		{
			m_peers[0].addr = from;
			m_peers[0].last_seen = now;
			m_lan_found = true;
			m_start_t = 0;
		}
		return;
	}
	// Until the host has answered, a packet from the host's IP address on another port is the
	// host behind a router that gives each destination its own port: talk to that port.
	if (!pp && m_slot < 0 && !m_peers.empty() && !from.v6 && !m_peers[0].addr.v6 && from.ip == m_peers[0].addr.ip) { m_peers[0].addr = from; pp = &m_peers[0]; }
	// A code with both addresses: the host answered on IPv6.
	if (!pp && m_slot < 0 && !m_peers.empty() && m_target6.v6 && from == m_target6) { m_peers[0].addr = from; m_target6 = {}; pp = &m_peers[0]; }
	if (!pp) return;    // only talk to our host
	pp->last_seen = now;
	pp->echo = tsend;
	pp->echo_at = now;
	if (techo) sample(m_ping, m_jitter, m_last_rtt, int(now - techo));
	switch (type)
	{
		case 'M':
		{
			// the host: where the other players are, to send them our inputs directly (N2.5)
			int cnt = r.u8_();
			std::vector<std::pair<int, net_addr>> ro;
			for (int j = 0; j < cnt && r.ok && j < 4; j++)
			{
				int sl = r.u8_();
				net_addr a;
				int k = r.i < n ? get_addr(p + r.i, n - r.i, a) : 0;
				if (!k) break;
				r.i += k;
				if (sl != m_slot && sl > 0 && sl < 4) ro.push_back({ sl, a });
			}
			if (r.ok) m_roster = ro;
			break;
		}
		case 'o':
		{
			// the host: our address as it sees it, then every player's link to it (N2.4)
			int k = r.i < n ? get_addr(p + r.i, n - r.i, m_seen_as) : 0;
			if (!k) break;
			r.i += k;
			int cnt = r.u8_();
			m_host_links.clear();
			for (int j = 0; j < cnt && r.ok; j++)
			{
				link_info l;
				l.slot = r.u8_(); l.ping = r.u16_(); l.jitter = r.u16_();
				u8 fl = r.u8_();
				l.relayed = fl & 1; l.v6 = (fl & 2) != 0;
				if (r.ok) m_host_links.push_back(l);
			}
			break;
		}
		case 'r': stop(("rejected: " + r.str()).c_str()); return;
		case 'b': stop(m_phase == phase::running ? "the host has closed the game" : "the host has cancelled"); return;
		case 'w': m_slot = r.u8_(); m_nplayers = r.u8_(); break;
		case 'l': { m_nplayers = r.u8_(); m_slot = r.u8_(); std::string mods = r.str(); if (r.ok) m_lobby_mods = mods; break; }
		case 's':
		{
			if (m_got_start) break;
			m_game_id = r.str();
			m_fp = r.u32_();
			m_dips.free_play = r.u8_(); m_dips.four_players = r.u8_(); m_dips.country = r.u8_(); m_dips.video_clips = r.u8_();
			m_delay = r.u8_();
			u32 nvsz = r.u32_();
			m_nchunks = r.u16_();
			m_nplayers = r.u8_();
			m_slot = r.u8_();
			u32 nvlen = r.u32_();
			if (!r.ok || nvsz > 64 * CHUNK || nvlen > nvsz) break;
			m_nvlen = nvlen;
			m_nv.assign(nvsz, 0);
			m_chunk_have.assign(size_t(m_nchunks), false);
			m_chunks_ok = 0;
			m_got_start = true;
			m_phase = phase::transfer;
			m_frame = 0;
			for (int s = 0; s < 4; s++) { m_in[s].clear(); for (int f = 0; f < m_delay; f++) m_in[s][u32(f)] = 0; }
			m_merged.clear();
			for (int f = 0; f < m_delay; f++) m_merged[u32(f)] = 0;
			break;
		}
		case 'c':
		{
			if (!m_got_start) break;
			int idx = r.u16_(), len = r.u16_();
			if (!r.ok || idx >= m_nchunks || !r.need(len) || size_t(idx) * CHUNK + size_t(len) > m_nv.size()) break;
			if (!m_chunk_have[size_t(idx)])
			{
				std::memcpy(&m_nv[size_t(idx) * CHUNK], p + r.i, size_t(len));
				m_chunk_have[size_t(idx)] = true;
				m_chunks_ok++;
			}
			break;
		}
		case 'f':
		{
			if (!m_got_start || !m_client_ready_sent || !m_client_ok) break;
			if (m_phase == phase::transfer) m_phase = phase::running;
			u32 first = r.u32_();
			int cnt = r.u8_();
			for (int k = 0; k < cnt && r.ok; k++)
			{
				u64 v = r.u64_();
				if (r.ok) m_merged.emplace(first + u32(k), v);
			}
			u8 flags = r.u8_();
			if (r.ok && (flags & 1)) m_desync = true;
			u32 need = r.u32_();          // next input frame the host wants from us
			if (r.ok) pp->acked_merged = need;
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// periodic sends
// ---------------------------------------------------------------------------
void netplay::tick_host(u32 now)
{
	for (size_t i = 0; i < m_peers.size(); i++)
		if (now - m_peers[i].last_seen > TIMEOUT_MS)
		{
			if (m_phase == phase::lobby)
			{
				m_peers.erase(m_peers.begin() + long(i));
				m_nplayers = 1;
				for (auto &q : m_peers) q.slot = m_nplayers++;
				return;
			}
			stop(("lost the connection with P" + std::to_string(m_peers[i].slot + 1)).c_str());
			return;
		}

	bool fast = now - m_last_tick >= 8;
	bool slow = now - m_hello_t >= 100;
	if (!fast) return;
	m_last_tick = now;
	if (slow) m_hello_t = now;

	const bool report = now - m_report_t >= 1000;
	if (report) m_report_t = now;
	const bool roster_due = now - m_roster_t >= 500;
	if (roster_due) m_roster_t = now;
	const std::vector<link_info> mine = report ? links() : std::vector<link_info>();
	for (auto &p : m_peers)
	{
		auto hdr = [&](wbuf &w, char t) { w.u8_(u8(t)); w.u8_(MAGIC); w.u32_(now); w.u32_(p.echo ? p.echo + (now - p.echo_at) : 0); };
		if (report)
		{
			wbuf w; hdr(w, 'o');
			put_addr(w.b, p.addr);
			w.u8_(u8(mine.size()));
			for (auto &l : mine) { w.u8_(u8(l.slot)); w.u16_(u16(std::min(l.ping, 65535))); w.u16_(u16(std::min(l.jitter, 65535))); w.u8_(u8((l.relayed ? 1 : 0) | (l.v6 ? 2 : 0))); }
			send_raw(p.addr, w.b);
		}
		if (m_phase == phase::lobby && slow)
		{
			wbuf w; hdr(w, 'l'); w.u8_(u8(m_nplayers)); w.u8_(u8(p.slot)); w.str(m_lobby_mods); send_raw(p.addr, w.b);
		}
		if (m_phase == phase::transfer && slow)
		{
			if (!p.start_acked)
			{
				wbuf w; hdr(w, 's');
				w.str(m_game_id); w.u32_(m_fp);
				w.u8_(m_dips.free_play); w.u8_(m_dips.four_players); w.u8_(u8(m_dips.country)); w.u8_(m_dips.video_clips);
				w.u8_(u8(m_delay)); w.u32_(u32(m_nv.size())); w.u16_(u16(m_nchunks)); w.u8_(u8(m_nplayers)); w.u8_(u8(p.slot)); w.u32_(u32(m_nvlen));
				send_raw(p.addr, w.b);
			}
			else
				for (int c = 0; c < m_nchunks; c++)
				{
					bool have = c < 32 ? (p.chunk_mask_lo >> c & 1) : (p.chunk_mask_hi >> (c - 32) & 1);
					if (have) continue;
					wbuf w; hdr(w, 'c');
					size_t off = size_t(c) * CHUNK, len = std::min<size_t>(CHUNK, m_nv.size() - off);
					w.u16_(u16(c)); w.u16_(u16(len)); w.bytes(&m_nv[off], len);
					send_raw(p.addr, w.b);
				}
		}
		if (mesh && (m_phase == phase::transfer || m_phase == phase::running) && roster_due)
		{
			// the players reached directly (not through a relay), for the mesh (N2.5)
			wbuf w; hdr(w, 'M');
			std::vector<const peer *> direct;
			for (auto &q : m_peers)
			{
				bool routed = false;
				for (auto &r : m_routes) routed = routed || r.first == q.addr;
				if (!routed) direct.push_back(&q);
			}
			w.u8_(u8(direct.size()));
			for (const peer *q : direct) { w.u8_(u8(q->slot)); put_addr(w.b, q->addr); }
			send_raw(p.addr, w.b);
		}
		if (m_phase == phase::running && mesh) send_direct(p.addr);
		if (m_phase == phase::running)
		{
			wbuf w; hdr(w, 'f');
			u32 first = std::max(p.acked_merged, m_merged_upto > MAX_HIST ? m_merged_upto - MAX_HIST : 0u);
			u32 last = m_merged_upto;
			if (first > last) first = last;
			w.u32_(first);
			w.u8_(u8(last - first));
			for (u32 f = first; f < last; f++) { auto it = m_merged.find(f); w.u64_(it == m_merged.end() ? 0 : it->second); }
			w.u8_(m_desync ? 1 : 0);
			// next input frame we still need from this client
			u32 need = m_merged_upto;
			while (has_input(p.slot, need)) need++;
			w.u32_(need);
			send_raw(p.addr, w.b);
		}
	}
}

void netplay::tick_client(u32 now)
{
	peer &h = m_peers[0];
	// before the first answer the host may still have to let us in by our code: wait 2 minutes
	if (now - h.last_seen > (m_slot < 0 ? 120000u : TIMEOUT_MS))
	{
		stop(m_slot < 0 ? "no answer from the host (address, port, or a router that lets nothing in?)" : "lost the connection with the host");
		return;
	}
	bool fast = now - m_last_tick >= 8;
	bool slow = now - m_hello_t >= 100;
	if (!fast) return;
	m_last_tick = now;
	if (slow) m_hello_t = now;
	auto hdr = [&](wbuf &w, char t) { w.u8_(u8(t)); w.u8_(MAGIC); w.u32_(now); w.u32_(h.echo ? h.echo + (now - h.echo_at) : 0); };
	if (m_slot >= 0 && now - m_report_t >= 1000)
	{
		m_report_t = now;
		wbuf w; hdr(w, 'o');
		put_addr(w.b, h.addr);
		send_raw(h.addr, w.b);
	}
	// no answer through the code: go through the member the host's code names (N2.3)
	if (m_slot < 0 && !m_relay_on && m_relay.port && now - m_join_t >= relay_after_ms) { m_relay_on = true; m_start_t = 0; m_hello_t = 0; }

	if (!m_got_start)
	{
		// the same public address as the host: both are behind one router, which may not send
		// packets for its own address back inside, so ask on the home network too
		if (m_slot < 0 && !m_lan_found && m_stun == stun_state::found && !m_target.v6 && m_public.ip == m_target.ip && now - m_lan_t >= 500)
		{
			m_lan_t = now;
			wbuf w;
			w.u8_('q'); w.u8_(MAGIC); w.u32_(now); w.u32_(0);
			w.u32_(m_target.ip);
			if (!lan_probe_to.empty())
				for (auto &t : lan_probe_to) send_raw(t, w.b);
			else
			{
				send_raw({ 0xffffffffu, m_target.port }, w.b);
				if (m_target.port != DEFAULT_PORT) send_raw({ 0xffffffffu, DEFAULT_PORT }, w.b);
			}
		}
		if (slow && (now - m_start_t >= 400 || m_slot < 0))
		{
			m_start_t = now;
			wbuf w; hdr(w, 'h'); w.u16_(VERSION);
			// a code with both addresses: IPv6 first, for 1.5 s, then IPv4 (one at a time, so
			// the host never sees the player twice)
			if (m_target6.v6 && now - m_target6_t > 1500) m_target6 = {};
			send_raw(m_target6.v6 && m_slot < 0 ? m_target6 : h.addr, w.b);
		}
		return;
	}
	if (m_phase == phase::transfer && slow)
	{
		u32 lo = 0, hi = 0;
		for (int c = 0; c < m_nchunks; c++)
			if (m_chunk_have[size_t(c)]) { if (c < 32) lo |= 1u << c; else hi |= 1u << (c - 32); }
		wbuf w; hdr(w, 'a');
		w.u8_(1); w.u32_(lo); w.u32_(hi);
		w.u8_(m_client_ready_sent ? (m_client_ok ? 1 : 2) : 0);
		w.str(m_client_why);
		send_raw(h.addr, w.b);
		if (m_client_ready_sent && !m_client_ok) stop(m_client_why.c_str());
		return;
	}
	if (m_phase == phase::running && mesh)
		for (auto &m : m_roster) send_direct(m.second);
	if (m_phase == phase::running)
	{
		// next merged frame we are missing
		u32 have = m_frame;
		while (m_merged.count(have)) have++;
		wbuf w; hdr(w, 'i');
		w.u8_(u8(m_slot));
		w.u32_(have);
		u32 first = std::max(h.acked_merged, u32(m_delay));
		auto &mine = m_in[m_slot];
		u32 last = mine.empty() ? first : mine.rbegin()->first + 1;
		if (last > first + MAX_HIST) first = last - MAX_HIST;
		if (first > last) first = last;
		w.u32_(first);
		w.u8_(u8(last - first));
		for (u32 f = first; f < last; f++) w.u16_(input_of(m_slot, f));
		u32 hf = 0, hv = 0;
		if (!m_hash.empty()) { hf = m_hash.rbegin()->first; hv = m_hash.rbegin()->second; }
		w.u32_(hf); w.u32_(hv);
		send_raw(h.addr, w.b);
	}
}

// ---------------------------------------------------------------------------
// STUN and invitations (roadmap N2.2, first part)
// ---------------------------------------------------------------------------
void netplay::stun_begin(const net_addr &server)
{
	m_stun_server = server;
	m_stun = stun_state::asking;
	m_stun_t = 0;
	m_stun_tries = 0;
}

void netplay::tick_stun(u32 now)
{
	if (m_stun == stun_state::off || m_stun == stun_state::failed) return;
	// asking: every 300 ms, 10 times; found: once every 20 s, which keeps the mapping open
	u32 every = m_stun == stun_state::found ? 20000 : 300;
	if (m_stun_t && now - m_stun_t < every) return;
	if (m_stun == stun_state::asking && m_stun_tries >= 10) { m_stun = stun_state::failed; return; }
	m_stun_t = now;
	m_stun_tries++;
	static u32 seed = net_now_ms() * 2654435761u;
	for (auto &b : m_stun_tx) { seed = seed * 1103515245u + 12345u; b = u8(seed >> 16); }
	u8 req[20];
	stun_request(req, m_stun_tx);
	m_sock.send(m_stun_server, req, sizeof req);
}

void netplay::invite(const net_addr &player)
{
	// the players already in send to it too, so any of them can relay for it (N2.3)
	if (m_mode == mode::host)
	{
		std::vector<u8> w{ u8('I'), MAGIC };
		put_addr(w, player);
		for (auto &p : m_peers) send_raw(p.addr, w);
	}
	for (auto &i : m_invites) if (i.to == player) { i.until = net_now_ms() + 60000; return; }
	m_invites.push_back({ player, net_now_ms() + 60000 });
}
