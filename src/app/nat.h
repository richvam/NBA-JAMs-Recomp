// nat.h - reaching a player behind a home router without changing the router by hand
// (roadmap N2.1 and the first part of N2.2, pulled forward).
//
//   * Connection codes: a public address and UDP port written as ten letters and digits,
//     "ABCDE-FGHJK", so it can be read out or pasted into a chat.
//   * STUN (RFC 5389 binding request): the address and port the internet sees for the game's
//     own UDP socket. The packets go through that socket; see netplay::stun_begin.
//   * UPnP IGD: asks the router to forward the host's UDP port and for its public address.
//     Blocking HTTP on a worker thread; the mapping is removed when hosting stops.
// NAT-PMP, PCP and IPv6 are not done.
#pragma once
#include "netplay.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

std::string net_code(const net_addr &a);                       // "ABCDE-FGHJK"
bool net_code_parse(const std::string &s, net_addr &out);      // a code, dashes and case ignored
bool net_ip_private(u32 ip);                                    // 10/8, 172.16/12, 192.168/16, 100.64/10 (carrier NAT), 169.254/16, 127/8
std::string net_ip_str(u32 ip);

// STUN messages
void stun_request(u8 out[20], const u8 txid[12]);
bool stun_parse(const u8 *p, int n, const u8 txid[12], net_addr &mapped);   // a success response to txid
std::vector<u8> stun_response(const u8 *req, int n, const net_addr &seen); // for tests: what a server answers

// UPnP Internet Gateway Device port mapping
class upnp_mapper
{
public:
	enum class state { off, working, mapped, failed };
	~upnp_mapper() { stop(); }
	void start(u16 port);            // discovery and mapping on a thread
	void stop();                     // removes the mapping (waits up to ~2 s), joins the thread
	state status() const { return m_state; }
	std::string external_ip() const; // the router's public address, when mapped
	std::string error() const;
	// For tests and unusual networks: where discovery is sent (default 239.255.255.250:1900).
	// The environment variable NBA_UPNP_SSDP=<ip>:<port> sets it too.
	std::string ssdp_target;
private:
	void run(u16 port);
	std::thread m_thread;
	std::atomic<state> m_state{ state::off };
	std::atomic<bool> m_quit{ false };
	mutable std::mutex m_mx;
	std::string m_ext, m_err;
};
