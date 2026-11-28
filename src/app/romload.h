// romload.h - find ROM files in folders or MAME-style .zip archives (by name or CRC32)
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

uint32_t crc32_of(const uint8_t *d, size_t n);
bool read_whole_file(const std::string &path, std::vector<uint8_t> &out);
bool write_whole_file(const std::string &path, const std::vector<uint8_t> &data);
bool file_exists(const std::string &path);
std::string path_join(const std::string &a, const std::string &b);

// A pool of candidate ROM files gathered from several folders and zips.
class rom_pool
{
public:
	void add_dir(const std::string &dir);          // loose files (non-recursive)
	// folder tree (sub-folders up to 'depth' levels, e.g. an unpacked GitHub repository);
	// loose files are only read when they have a ROM-like size or are .IRW files
	void add_tree(const std::string &dir, int depth = 4);
	void add_zip(const std::string &zippath);
	// lookup by exact (case-insensitive) name, else by CRC
	bool get(const std::string &name, uint32_t crc, std::vector<uint8_t> &out);
	bool has(const std::string &name, uint32_t crc);
	// a file next to the one found by 'crc' (same folder, or same folder inside the same zip),
	// e.g. the gfx.bin of a pre-built game whose program.bin matched
	bool sibling(uint32_t crc, const std::string &relname, std::vector<uint8_t> &out);
private:
	// lane 0/1: the even/odd bytes of an interleaved 1 MB program image (program.bin of a
	// pre-built game), so its halves are found by the CRCs of the two program ROMs
	struct entry { std::string path; std::string zipname; uint32_t crc; size_t size; int lane = -1; };
	std::map<std::string, entry> m_byname;
	std::multimap<uint32_t, entry> m_bycrc;
	void add(const entry &e, const std::string &name);
	void add_lanes(const entry &e, const std::vector<uint8_t> &d);
	bool load(const entry &e, std::vector<uint8_t> &out);
};
