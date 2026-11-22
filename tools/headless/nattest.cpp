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
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

static int g_run = 0, g_fail = 0;
#define CHECK(c, what) do { g_run++; if (c) printf("ok   %s\n", what); else { g_fail++; printf("FAIL %s\n", what); } } while (0)

static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
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
			send_to(m->fd, to, b + 6, size_t(n - 6));
		}
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
