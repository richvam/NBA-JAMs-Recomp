// netplay.h - online play for NBA JAM Recomp.
//
// Deterministic lockstep with input delay over UDP, star topology:
//   * the host is player 1, up to three clients take players 2..4;
//   * every peer runs the same emulation from power-on with the same NVRAM and DIPs;
//   * each peer samples its local input for frame f+delay while running frame f;
//   * clients send their inputs to the host, the host merges them and broadcasts the
//     merged input of every frame; nobody runs frame f before it has the merged input.
// Packets carry redundant history so lost datagrams are recovered by the next one.
// State hashes are exchanged every 30 frames to detect desyncs.
#pragma once
#include "machine.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

u16 net_pack_input(const cabinet_inputs::player &p);
void net_unpack_input(u16 v, cabinet_inputs::player &p);

// An IPv4 address (ip) or, when v6 is set, the IPv6 address in a6 (roadmap N2.2)
struct net_addr
{
	u32 ip = 0; u16 port = 0;
	bool v6 = false; u8 a6[16] = {};
	bool operator==(const net_addr &o) const;
	bool operator!=(const net_addr &o) const { return !(*this == o); }
};
std::string net_addr_str(const net_addr &a);       // "1.2.3.4:5" or "[2001:db8::1]:5"
std::vector<std::string> net_local_ipv4();
bool net_local_ipv6(net_addr &out);                  // this computer's public (global) IPv6 address, port 0

class udp_socket
{
public:
	~udp_socket() { close(); }
	// port 0 = any; bind_ip 0 = every interface, and then an IPv6 socket on the same port too
	// when the computer has IPv6 (packets to and from IPv6 addresses go through it)
	bool open(u16 port, std::string &err, u32 bind_ip = 0);
	bool has_ipv6() const { return m_fd6 >= 0; }
	void close();
	bool is_open() const { return m_fd >= 0; }
	void send(const net_addr &to, const void *data, size_t len);
	int recv(net_addr &from, void *data, size_t cap);   // -1 when nothing pending
	static bool resolve(const std::string &host, u16 port, net_addr &out, std::string &err);
	u16 local_port() const;
	double loss = 0.0;                             // test hook: drop this fraction of sends
	// test hook (the NAT simulator in nattest): when set, every packet goes to `via` with the
	// real destination in front (6 bytes), and packets from `via` carry the real source.
	// Packets to 127.0.0.1 go straight there: in the simulator that is the home network.
	net_addr via;
private:
	long long m_fd = -1, m_fd6 = -1;
	u32 m_lossrng = 12345;
};

class netplay
{
public:
	enum class mode { off, host, client };
	enum class phase { lobby, transfer, running, ended };
	static constexpr int MAX_HIST = 32;

	netplay();
	~netplay();

	bool host(u16 port, std::string &err);
	bool join(const std::string &host, u16 port, std::string &err);   // host: an address, a name or a connection code (port ignored then)
	void stop(const char *reason = nullptr);      // sends BYE

	void poll();                                  // receive + periodic sends; call every loop iteration

	mode role() const { return m_mode; }
	phase stage() const { return m_phase; }
	bool active() const { return m_mode != mode::off; }
	int local_slot() const { return m_slot; }
	int players() const { return m_nplayers; }
	std::vector<std::string> peer_list() const;   // host: one line per connected client
	std::string error() const { return m_error; }
	int ping_ms() const { return m_ping; }
	bool desynced() const { return m_desync; }

	// --- host: start the session (game must be loaded locally) ---
	// mods: the Lua mods the clients must run (mod_manager::net_description; roadmap 1.7)
	void host_start(const std::string &game_id, u32 fingerprint, const dip_settings &dips, const std::vector<u8> &nvram, int delay, const std::string &mods = "");
	void set_lobby_mods(const std::string &names) { m_lobby_mods = names; }   // host: shown in the players' lobby
	const std::string &lobby_mods() const { return m_lobby_mods; }
	bool all_ready() const;                       // host: every client confirmed

	// --- client: the start description, once fully received ---
	bool start_received() const { return m_phase == phase::transfer && m_got_start && m_chunks_ok == m_nchunks; }
	std::string start_game_id() const { return m_game_id; }
	u32 start_fingerprint() const { return m_fp; }
	const dip_settings &start_dips() const { return m_dips; }
	std::vector<u8> start_nvram() const { return std::vector<u8>(m_nv.begin(), m_nv.begin() + long(std::min<size_t>(m_nvlen, m_nv.size()))); }
	std::string start_mods() const { return m_nvlen < m_nv.size() ? std::string(m_nv.begin() + long(m_nvlen), m_nv.end()) : std::string(); }
	void client_ready(bool ok, const std::string &why);   // after loading the game

	// --- running ---
	int delay() const { return m_delay; }
	u32 frame() const { return m_frame; }
	bool can_run() const;                          // merged input for m_frame available
	cabinet_inputs frame_inputs(const cabinet_inputs::player &local_now); // records local input for frame+delay, returns merged input of frame
	void frame_done(u32 hash);                     // after running the frame
	u32 stall_ms() const;
	int buffered() const;                          // merged frames ready beyond the current one

	udp_socket &socket() { return m_sock; }

	// --- reaching players behind routers (roadmap N2.2, first part) ---
	// STUN through the game socket: the address and port the internet sees for it. Asked
	// again every 20 s while waiting, which also keeps the router's mapping open.
	void stun_begin(const net_addr &server);
	enum class stun_state { off, asking, found, failed };
	stun_state stun_status() const { return m_stun; }
	net_addr public_addr() const { return m_public; }
	// host: send to a player's address (from their code) for 60 s so the host's router lets
	// their packets in; the player's own packets to the host do the same on their side
	void invite(const net_addr &player);
	int invites() const { return int(m_invites.size()); }
	// Both behind one router: most routers do not send packets for their own public address
	// back inside. When the player's public address equals the host's, the player asks on the
	// home network (broadcast) for the host with that public address and switches to the
	// address that answers. The host answers for its STUN address and for also_public().
	void also_public(u32 ip) { m_also_public = ip; }
	bool found_on_lan() const { return m_lan_found; }
	std::vector<net_addr> lan_probe_to;           // test hook: where to ask instead of the broadcast

	// --- peer relay (roadmap N2.3) ---
	// A player who cannot reach the host directly goes through a member who can reach both.
	// The host names one in a longer code (its own code, then the member's); a player who
	// joined with it and has no answer after relay_after_ms sends through that member, who
	// forwards both ways (at most RELAY_BYTES_PER_S each way per player). When the host lets
	// a player in by code, every member sends to that player too, so their routers let it in.
	static constexpr u32 RELAY_BYTES_PER_S = 64 * 1024;
	net_addr relay_candidate() const;             // host: the member to name, or none (ip and port 0)
	bool relayed() const { return m_relay_on; }  // player: talking to the host through a member
	u32 relayed_bytes() const { return m_relayed; }   // member: bytes forwarded for others
	u32 relay_after_ms = 3000;

	// --- connection report (roadmap N2.4) ---
	// open: the port is forwarded (UPnP, PCP, NAT-PMP), or no router changes the address
	// (port_forwarded: the caller knows), or IPv6; moderate: a router that keeps one port for
	// every destination; strict: one that gives each destination its own port (another player
	// sees another port than STUN did), or relayed. Each side learns how the other sees it from
	// an 'o' packet every second; the host's also carries every player's ping and jitter.
	enum class conn_type { unknown, open, moderate, strict };
	conn_type connection(bool port_forwarded) const;
	static const char *conn_name(conn_type t);
	struct link_info
	{
		int slot = 0, ping = 0, jitter = 0;   // ms
		bool relayed = false, v6 = false;
		int through = -1;                     // host: the member relaying it
		bool via_host = false;                // player: another player, through the host
	};
	std::vector<link_info> links() const;     // host: one per player; player: the host, then the others
	net_addr seen_as() const { return m_seen_as; }

private:
	struct peer
	{
		net_addr addr;
		int slot = 0;
		bool ready = false;
		u32 last_seen = 0;
		u32 echo = 0;            // last timestamp received from it
		u32 echo_at = 0;         // local time when it arrived
		u32 acked_merged = 0;    // client: merged frames received below this (exclusive)
		u32 chunk_mask_lo = 0, chunk_mask_hi = 0;
		bool start_acked = false;
		int ping = 0, jitter = 0, last_rtt = 0;
	};
	void reset_state();
	void handle(const net_addr &from, const u8 *p, int n);
	void send_raw(const net_addr &to, const std::vector<u8> &b);
	void tick_host(u32 now);
	void tick_client(u32 now);
	void try_merge();
	u16 input_of(int slot, u32 f) const;
	bool has_input(int slot, u32 f) const;

	udp_socket m_sock;
	net_addr m_stun_server, m_public;
	stun_state m_stun = stun_state::off;
	u8 m_stun_tx[12] = {};
	u32 m_stun_t = 0, m_stun_tries = 0;
	struct invite_t { net_addr to; u32 until; };
	std::vector<invite_t> m_invites;
	void tick_stun(u32 now);
	net_addr m_target;             // client: the address joined (from the code)
	net_addr m_seen_as;            // our address as the other side sees it (host: the first player to say)
	std::vector<link_info> m_host_links;   // client: the host's links to every player
	u32 m_report_t = 0;
	int m_jitter = 0, m_last_rtt = 0;
	static void sample(int &ping, int &jitter, int &last, int rtt);
	net_addr m_relay;              // client: the member named in the code
	bool m_relay_on = false;
	u32 m_join_t = 0, m_relayed = 0;
	std::vector<std::pair<net_addr, net_addr>> m_routes;   // host: player -> the member relaying it
	struct flow { net_addr other; u32 t0, bytes; };
	std::vector<flow> m_flows;                     // member: bytes relayed this second, per player
	bool relay_packet(const net_addr &from, const u8 *p, int n);
	bool relay_budget(const net_addr &other, size_t bytes);
	net_addr m_target6;            // client: the host's IPv6 address from a code with both, tried first
	u32 m_target6_t = 0;
	u32 m_also_public = 0, m_lan_t = 0;
	bool m_lan_found = false;
	mode m_mode = mode::off;
	phase m_phase = phase::lobby;
	std::string m_error;
	int m_slot = 0, m_nplayers = 1;
	std::vector<peer> m_peers;               // host: clients; client: [0] = host
	u32 m_last_tick = 0, m_hello_t = 0, m_start_t = 0;
	int m_ping = 0;

	// session description
	bool m_got_start = false;
	std::string m_game_id;
	u32 m_fp = 0;
	dip_settings m_dips;
	std::vector<u8> m_nv;          // the NVRAM, then the mods description (m_nvlen bytes of NVRAM)
	size_t m_nvlen = 0;
	std::string m_lobby_mods;
	int m_nchunks = 0, m_chunks_ok = 0;
	std::vector<bool> m_chunk_have;
	bool m_client_ready_sent = false, m_client_ok = false;
	std::string m_client_why;
	int m_delay = 4;

	// lockstep
	u32 m_frame = 0;                         // next frame to run
	std::map<u32, u16> m_in[4];              // per-slot inputs by frame (host: all slots; client: own)
	std::map<u32, u64> m_merged;             // merged inputs (4 x 16 bits)
	u32 m_merged_upto = 0;                   // frames [0, m_merged_upto) merged (host)
	u32 m_stall_since = 0;
	std::map<u32, u32> m_hash;               // own hashes
	bool m_desync = false;
};

u32 net_now_ms();
