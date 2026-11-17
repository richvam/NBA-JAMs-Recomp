// nat.cpp - see nat.h
#include "nat.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
typedef SOCKET sock_t;
static const sock_t kBad = INVALID_SOCKET;
static void nat_init() { static bool done = false; if (!done) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); done = true; } }
static void sclose(sock_t s) { closesocket(s); }
static void nonblock(sock_t s, bool on) { u_long nb = on ? 1 : 0; ioctlsocket(s, FIONBIO, &nb); }
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
static const sock_t kBad = -1;
static void nat_init() {}
static void sclose(sock_t s) { ::close(s); }
static void nonblock(sock_t s, bool on) { int f = fcntl(s, F_GETFL); fcntl(s, F_SETFL, on ? (f | O_NONBLOCK) : (f & ~O_NONBLOCK)); }
#endif

// ---------------------------------------------------------------------------
// addresses and codes
// ---------------------------------------------------------------------------
static const char kB32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // Crockford: no I, L, O, U

std::string net_ip_str(u32 ip)
{
	char b[20];
	snprintf(b, sizeof b, "%u.%u.%u.%u", ip >> 24, ip >> 16 & 255, ip >> 8 & 255, ip & 255);
	return b;
}

bool net_ip_private(u32 ip)
{
	return (ip >> 24) == 10 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8 || (ip >> 22) == (100u << 2 | 1) ||
	       (ip >> 16) == 0xA9FE || (ip >> 24) == 127;
}

std::string net_code(const net_addr &a)
{
	u64 v = u64(a.ip) << 16 | a.port;          // 48 bits; ten characters hold 50
	std::string s(11, '-');
	for (int i = 9; i >= 0; i--) { s[size_t(i < 5 ? i : i + 1)] = kB32[v & 31]; v >>= 5; }
	return s;
}

bool net_code_parse(const std::string &s, net_addr &out)
{
	u64 v = 0;
	int n = 0;
	for (char c : s)
	{
		if (c == '-' || c == ' ') continue;
		c = char(toupper((unsigned char)c));
		if (c == 'O') c = '0';
		if (c == 'I' || c == 'L') c = '1';
		const char *p = strchr(kB32, c);
		if (!p || !c) return false;
		v = v << 5 | u64(p - kB32);
		if (++n > 10) return false;
	}
	if (n != 10 || (v >> 48) != 0) return false;
	out.ip = u32(v >> 16);
	out.port = u16(v);
	return out.ip != 0 && out.port != 0;
}

// ---------------------------------------------------------------------------
// STUN (RFC 5389): binding request, XOR-MAPPED-ADDRESS (or MAPPED-ADDRESS) in the answer
// ---------------------------------------------------------------------------
static const u32 kCookie = 0x2112A442;

void stun_request(u8 out[20], const u8 txid[12])
{
	out[0] = 0x00; out[1] = 0x01; out[2] = 0; out[3] = 0;
	out[4] = 0x21; out[5] = 0x12; out[6] = 0xA4; out[7] = 0x42;
	memcpy(out + 8, txid, 12);
}

bool stun_parse(const u8 *p, int n, const u8 txid[12], net_addr &mapped)
{
	if (n < 20 || p[0] != 0x01 || p[1] != 0x01) return false;
	if ((u32(p[4]) << 24 | u32(p[5]) << 16 | u32(p[6]) << 8 | p[7]) != kCookie || memcmp(p + 8, txid, 12) != 0) return false;
	int len = p[2] << 8 | p[3];
	if (20 + len > n) return false;
	bool got = false;
	for (int i = 20; i + 4 <= 20 + len;)
	{
		int t = p[i] << 8 | p[i + 1], l = p[i + 2] << 8 | p[i + 3];
		const u8 *v = p + i + 4;
		if (i + 4 + l > 20 + len) break;
		if ((t == 0x0020 || t == 0x0001) && l >= 8 && v[1] == 0x01)
		{
			u16 port = u16(v[2] << 8 | v[3]);
			u32 ip = u32(v[4]) << 24 | u32(v[5]) << 16 | u32(v[6]) << 8 | v[7];
			if (t == 0x0020) { port ^= u16(kCookie >> 16); ip ^= kCookie; }
			if (t == 0x0020 || !got) { mapped.ip = ip; mapped.port = port; got = true; }
		}
		i += 4 + ((l + 3) & ~3);
	}
	return got;
}

std::vector<u8> stun_response(const u8 *req, int n, const net_addr &seen)
{
	if (n < 20 || req[0] != 0x00 || req[1] != 0x01) return {};
	std::vector<u8> r(32);
	r[0] = 0x01; r[1] = 0x01; r[2] = 0; r[3] = 12;
	memcpy(&r[4], req + 4, 16);
	r[20] = 0x00; r[21] = 0x20; r[22] = 0; r[23] = 8;
	r[24] = 0; r[25] = 0x01;
	u16 xp = seen.port ^ u16(kCookie >> 16);
	u32 xi = seen.ip ^ kCookie;
	r[26] = u8(xp >> 8); r[27] = u8(xp);
	r[28] = u8(xi >> 24); r[29] = u8(xi >> 16); r[30] = u8(xi >> 8); r[31] = u8(xi);
	return r;
}

// ---------------------------------------------------------------------------
// a small blocking HTTP/1.1 client for the router (plain http on the local network)
// ---------------------------------------------------------------------------
namespace {
struct url { std::string host, path; u16 port = 80; };

bool parse_url(const std::string &u, url &out)
{
	if (u.compare(0, 7, "http://") != 0) return false;
	size_t a = 7, slash = u.find('/', a);
	std::string hp = u.substr(a, slash == std::string::npos ? std::string::npos : slash - a);
	out.path = slash == std::string::npos ? "/" : u.substr(slash);
	size_t colon = hp.find(':');
	out.host = hp.substr(0, colon);
	out.port = colon == std::string::npos ? 80 : u16(atoi(hp.c_str() + colon + 1));
	return !out.host.empty() && out.port != 0;
}

bool wait_sock(sock_t s, bool write, int ms)
{
	fd_set f;
	FD_ZERO(&f);
	FD_SET(s, &f);
	timeval tv{ ms / 1000, (ms % 1000) * 1000 };
	int r = select(int(s) + 1, write ? nullptr : &f, write ? &f : nullptr, nullptr, &tv);
	return r > 0;
}

std::string dechunk(const std::string &b)
{
	std::string out;
	size_t i = 0;
	while (i < b.size())
	{
		size_t e = b.find("\r\n", i);
		if (e == std::string::npos) break;
		size_t n = strtoul(b.substr(i, e - i).c_str(), nullptr, 16);
		if (n == 0) break;
		out += b.substr(e + 2, n);
		i = e + 2 + n + 2;
	}
	return out;
}

// returns the status code (0 on a network failure); body and the local address used
int http(const std::string &u, const std::string &method, const std::string &extra, const std::string &body,
         std::string &resp, std::string *local_ip, int timeout_ms)
{
	url p;
	if (!parse_url(u, p)) return 0;
	addrinfo hints{}, *res = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(p.host.c_str(), nullptr, &hints, &res) != 0 || !res) return 0;
	sockaddr_in a = *(sockaddr_in *)res->ai_addr;
	freeaddrinfo(res);
	a.sin_port = htons(p.port);
	sock_t s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == kBad) return 0;
	nonblock(s, true);
	connect(s, (sockaddr *)&a, sizeof a);
	int err = 0;
	socklen_t el = sizeof err;
	if (!wait_sock(s, true, timeout_ms) || getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &el) != 0 || err != 0) { sclose(s); return 0; }
	if (local_ip)
	{
		sockaddr_in me{};
		socklen_t ml = sizeof me;
		if (getsockname(s, (sockaddr *)&me, &ml) == 0) *local_ip = net_ip_str(ntohl(me.sin_addr.s_addr));
	}
	std::string req = method + " " + p.path + " HTTP/1.1\r\nHost: " + p.host + ":" + std::to_string(p.port) +
	                  "\r\nConnection: close\r\nUser-Agent: NBAJamRecomp UPnP/1.1\r\n" + extra;
	if (!body.empty() || method == "POST") req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
	req += "\r\n" + body;
	size_t sent = 0;
	while (sent < req.size())
	{
		if (!wait_sock(s, true, timeout_ms)) { sclose(s); return 0; }
		int k = int(send(s, req.data() + sent, int(req.size() - sent), 0));
		if (k <= 0) { sclose(s); return 0; }
		sent += size_t(k);
	}
	std::string all;
	char buf[4096];
	while (wait_sock(s, false, timeout_ms))
	{
		int k = int(recv(s, buf, sizeof buf, 0));
		if (k <= 0) break;
		all.append(buf, size_t(k));
		if (all.size() > 1 << 20) break;
	}
	sclose(s);
	size_t he = all.find("\r\n\r\n");
	if (all.compare(0, 5, "HTTP/") != 0 || he == std::string::npos) return 0;
	int status = atoi(all.c_str() + all.find(' ') + 1);
	std::string head = all.substr(0, he);
	for (auto &c : head) c = char(tolower((unsigned char)c));
	resp = all.substr(he + 4);
	if (head.find("transfer-encoding: chunked") != std::string::npos) resp = dechunk(resp);
	return status;
}

std::string tag(const std::string &x, const std::string &name, size_t from = 0)
{
	size_t a = x.find("<" + name + ">", from);
	if (a == std::string::npos) return "";
	a += name.size() + 2;
	size_t b = x.find("</" + name + ">", a);
	if (b == std::string::npos) return "";
	std::string v = x.substr(a, b - a);
	size_t s = v.find_first_not_of(" \t\r\n"), e = v.find_last_not_of(" \t\r\n");
	return s == std::string::npos ? "" : v.substr(s, e - s + 1);
}

struct igd { std::string control, service; };

// the WAN connection service of a router description
bool find_service(const std::string &loc, const std::string &xml, igd &out)
{
	static const char *kinds[] = { "urn:schemas-upnp-org:service:WANIPConnection:2", "urn:schemas-upnp-org:service:WANIPConnection:1",
	                               "urn:schemas-upnp-org:service:WANPPPConnection:1" };
	for (const char *k : kinds)
	{
		size_t at = xml.find(std::string(">") + k + "<");
		if (at == std::string::npos) continue;
		std::string c = tag(xml, "controlURL", at);
		if (c.empty()) continue;
		out.service = k;
		if (c.compare(0, 7, "http://") == 0) { out.control = c; return true; }
		std::string base = tag(xml, "URLBase");
		if (base.empty()) base = loc;
		size_t hs = base.find('/', 7);
		base = base.substr(0, hs);
		out.control = base + (c[0] == '/' ? c : "/" + c);
		return true;
	}
	return false;
}

int soap(const igd &g, const std::string &action, const std::string &args, std::string &resp, std::string *local_ip = nullptr)
{
	std::string body = "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
	                   "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" + action + " xmlns:u=\"" + g.service + "\">" +
	                   args + "</u:" + action + "></s:Body></s:Envelope>\r\n";
	return http(g.control, "POST", "Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"" + g.service + "#" + action + "\"\r\n", body, resp, local_ip, 2000);
}

std::string add_args(u16 port, const std::string &local, int lease)
{
	return "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(port) + "</NewExternalPort><NewProtocol>UDP</NewProtocol>"
	       "<NewInternalPort>" + std::to_string(port) + "</NewInternalPort><NewInternalClient>" + local + "</NewInternalClient>"
	       "<NewEnabled>1</NewEnabled><NewPortMappingDescription>NBA JAM Recomp</NewPortMappingDescription>"
	       "<NewLeaseDuration>" + std::to_string(lease) + "</NewLeaseDuration>";
}
}

// ---------------------------------------------------------------------------
// UPnP mapper
// ---------------------------------------------------------------------------
void upnp_mapper::start(u16 port)
{
	stop();
	m_quit = false;
	m_state = state::working;
	{ std::lock_guard<std::mutex> l(m_mx); m_ext.clear(); m_err.clear(); }
	m_thread = std::thread([this, port] { run(port); });
}

void upnp_mapper::stop()
{
	m_quit = true;
	if (m_thread.joinable()) m_thread.join();
	m_state = state::off;
}

std::string upnp_mapper::external_ip() const { std::lock_guard<std::mutex> l(m_mx); return m_ext; }
std::string upnp_mapper::error() const { std::lock_guard<std::mutex> l(m_mx); return m_err; }

void upnp_mapper::run(u16 port)
{
	nat_init();
	auto fail = [&](const std::string &e) { { std::lock_guard<std::mutex> l(m_mx); m_err = e; } m_state = state::failed; };
	// 1. discovery (SSDP)
	std::string target = ssdp_target;
	if (target.empty() && getenv("NBA_UPNP_SSDP")) target = getenv("NBA_UPNP_SSDP");
	if (target.empty()) target = "239.255.255.250:1900";
	sockaddr_in to{};
	to.sin_family = AF_INET;
	to.sin_port = htons(u16(atoi(target.c_str() + target.find(':') + 1)));
	{
		unsigned a = 0, b = 0, c = 0, d = 0;      // no inet_pton: older MinGW headers lack it
		sscanf(target.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
		to.sin_addr.s_addr = htonl(a << 24 | (b & 255) << 16 | (c & 255) << 8 | (d & 255));
	}
	sock_t u = socket(AF_INET, SOCK_DGRAM, 0);
	if (u == kBad) return fail("no socket");
	{
		// bound before sending: some network stacks (WSL's fallback mode) drop sends from an unbound socket
		sockaddr_in any{};
		any.sin_family = AF_INET;
		any.sin_addr.s_addr = htonl(INADDR_ANY);
		bind(u, (sockaddr *)&any, sizeof any);
	}
	std::vector<std::string> locs;
	auto t0 = std::chrono::steady_clock::now();
	auto ms = [&] { return int(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()); };
	for (int round = 0; round < 2 && locs.empty() && !m_quit; round++)
	{
		for (const char *st : { "urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:service:WANIPConnection:1" })
		{
			std::string m = std::string("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: ") + st + "\r\n\r\n";
			sendto(u, m.data(), int(m.size()), 0, (sockaddr *)&to, sizeof to);
		}
		t0 = std::chrono::steady_clock::now();
		while (ms() < 1500 && !m_quit)
		{
			if (!wait_sock(u, false, 100)) { if (!locs.empty() && ms() > 400) break; continue; }
			char b[2048];
			int n = int(recv(u, b, sizeof b - 1, 0));
			if (n <= 0) continue;
			std::string r(b, size_t(n)), low = r;
			for (auto &c : low) c = char(tolower((unsigned char)c));
			size_t at = low.find("\nlocation:");
			if (at == std::string::npos) continue;
			at += 10;
			size_t e = r.find_first_of("\r\n", at);
			std::string loc = r.substr(at, e - at);
			loc.erase(0, loc.find_first_not_of(" \t"));
			if (std::find(locs.begin(), locs.end(), loc) == locs.end()) locs.push_back(loc);
		}
	}
	sclose(u);
	if (m_quit) { m_state = state::off; return; }
	if (locs.empty()) return fail("no router answered (UPnP off?)");

	// 2. the router's description and its WAN connection service
	igd g;
	std::string local;
	for (const auto &loc : locs)
	{
		std::string xml;
		if (http(loc, "GET", "", "", xml, &local, 2000) == 200 && find_service(loc, xml, g)) break;
		g = igd();
	}
	if (g.control.empty()) return fail("the router does not offer port mapping");

	// 3. the mapping; some routers only take permanent ones (725), a stale one of ours is replaced (718)
	auto add = [&](std::string &why) {
		int lease = 3600;
		for (int tries = 0; tries < 3; tries++)
		{
			std::string r;
			int st = soap(g, "AddPortMapping", add_args(port, local, lease), r, &local);
			if (st == 200) return true;
			std::string code = tag(r, "errorCode");
			why = "the router refused the port mapping" + (code.empty() ? " (HTTP " + std::to_string(st) + ")" : " (error " + code + ")");
			if (code == "725") lease = 0;
			else if (code == "718")
			{
				std::string d;
				soap(g, "DeletePortMapping", "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(port) + "</NewExternalPort><NewProtocol>UDP</NewProtocol>", d);
			}
			else return false;
		}
		return false;
	};
	std::string why;
	if (!add(why)) return fail(why);
	std::string r, ext;
	if (soap(g, "GetExternalIPAddress", "", r) == 200) ext = tag(r, "NewExternalIPAddress");
	{ std::lock_guard<std::mutex> l(m_mx); m_ext = ext; }
	m_state = state::mapped;

	// 4. renewed every 20 minutes while hosting; removed at the end
	auto since = std::chrono::steady_clock::now();
	while (!m_quit)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (std::chrono::steady_clock::now() - since > std::chrono::minutes(20)) { add(why); since = std::chrono::steady_clock::now(); }
	}
	std::string d;
	soap(g, "DeletePortMapping", "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(port) + "</NewExternalPort><NewProtocol>UDP</NewProtocol>", d);
}
