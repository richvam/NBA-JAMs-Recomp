// luadet.cpp - the Lua engine gives the same results on every build (roadmap R5.4).
//
//   luadet            prints one line per probe, then "luadet hash=<hash> lines=<n>"
//
// Runs a fixed script in a Lua state with the libraries a mod gets (base, table, string, math,
// utf8) and prints, exactly (floats as %a), the results that could differ between processors,
// compilers and C libraries: integer and float arithmetic, conversions, number formatting,
// every math function, math.random after math.randomseed, string.format, and the order of
// pairs() over string, integer and float keys. Builds that agree print the same lines: compare
// the output of two builds with diff. Needs no ROMs, so CI runs it on x86-64 and ARM64, and it
// fails unless the hash is the one every build gave (below).
#include <cstdio>
#include <cstring>
#include <string>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

static const char *kProbe = R"LUA(
local out = {}
local function p(...) local t = table.pack(...) for i = 1, t.n do t[i] = tostring(t[i]) end out[#out + 1] = table.concat(t, " ") end
local function f(x) return math.type(x) == "float" and string.format("%a", x) or tostring(x) end

-- integers: wrap-around, division, modulo, shifts, conversions
p("int", math.maxinteger + 1, math.mininteger - 1, math.mininteger // -1, 7 // -2, -7 // 2, 7 % -3, -7 % 3)
p("shift", 1 << 63, 1 << 64, -1 >> 1, -1 >> 63, 5 >> -1, 0x7fffffff * 0x7fffffff)
p("conv", math.tointeger(3.0), math.tointeger(2^53), 3 | 0, tostring(2^63), math.floor(-0.5), math.ceil(-0.5))
p("str2num", tonumber("0x10"), tonumber("1e2"), tonumber("  12  "), tonumber("0x1p4"), tonumber("10", 36), "10" + 1)
-- floats: formatting the way a mod would show or store them
for _, x in ipairs({ 0.1, 1/3, 2/3, 1e15, 1e16, 123456789.123, -0.0, 1e300 * 1e10, 2^-1074, 3.14159265358979 }) do
  p("fmt", f(x), tostring(x), string.format("%.3f %g %.17g %e", x, x, x, x))
end
p("float", f(0.1 + 0.2), f(1e308 * 10), f(-(0/0) ~= -(0/0)), f(7.5 // 2), f(-7.5 % 2), f(5.3 % -2), f(2^0.5))
-- every math function, over a spread of arguments
local xs = { 0.5, 1, 2, 3, 10, 100, 0.001, 1e10, -0.75, math.pi / 3, 12345.678 }
for _, x in ipairs(xs) do
  p("math", f(x), f(math.sin(x)), f(math.cos(x)), f(math.tan(x)), f(math.exp(x)), f(math.log(math.abs(x))),
    f(math.log(math.abs(x), 2)), f(math.log(math.abs(x), 10)), f(math.sqrt(math.abs(x))), f(math.abs(x) ^ 1.7),
    f(math.atan(x)), f(math.atan(x, 2)), f(math.fmod(x, 0.3)), f(math.asin(x / 1e11)), f(math.acos(x / 1e11)))
end
p("misc", math.floor(3.7), math.ceil(3.2), math.abs(math.mininteger), math.max(1, 2.5), math.min(-0.0, 0.0), math.ult(1, -1), f(math.huge))
-- math.random after a seed: the same numbers everywhere
math.randomseed(42)
local r = {} for i = 1, 20 do r[i] = math.random(1000) end p("random", table.concat(r, ","))
math.randomseed(7, 9) p("random2", math.random(0), math.random(-5, 5), f(math.random()), f(math.random()))
-- string.format and string functions
p("format", string.format("%5.2f|%-5d|%x|%X|%o|%q|%s|%10.4s|%c%c", 3.14159, 42, 255, 255, 8, "a\nb\0c", nil, "abcdefgh", 72, 105))
p("format2", string.format("%q", 0.1), string.format("%q", math.mininteger), string.format("%i", 3.0), string.format("%.0f", 0.5), string.format("%.0f", 1.5), string.format("%.0f", 2.5))
p("string", ("Hello"):upper(), ("x"):rep(3, ","), ("abc"):byte(1, -1), #("\u{10FFFF}"), utf8.len("h\u{E9}llo"), ("a1b22c333"):gsub("%d+", "#"))
p("pack", (string.pack("<i4d", -2, 0.1)):byte(1, -1))
-- pairs order: string keys, integer keys, float keys, and a table built, grown and emptied
local t = {} for i, k in ipairs({ "team", "score", "clock", "player", "shot", "pass", "turbo", "fire", "steal", "block", "dunk" }) do t[k] = i end
local o = {} for k in pairs(t) do o[#o + 1] = k end p("pairs-str", table.concat(o, ","))
local u = {} for i = 1, 40 do u[i * 7 % 23 + 0.5] = i; u[-i] = i end
o = {} for k in pairs(u) do o[#o + 1] = tostring(k) end p("pairs-num", table.concat(o, ","))
local w = {} for i = 1, 200 do w["k" .. i] = i end for i = 1, 200, 3 do w["k" .. i] = nil end
o = {} for k in pairs(w) do o[#o + 1] = k end p("pairs-grown", table.concat(o, ","))
local s = { 5, 3, 9, 1, 7, 2, 8, 6, 4, 0 } table.sort(s, function(a, b) return a > b end) p("sort", table.concat(s, ","))
for _, l in ipairs(out) do print(l) end
)LUA";

static unsigned fnv(const char *s, unsigned h) { while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; } return h; }

int main()
{
	lua_State *L = luaL_newstate();
	static const luaL_Reg libs[] = {
		{ LUA_GNAME, luaopen_base }, { LUA_TABLIBNAME, luaopen_table }, { LUA_STRLIBNAME, luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 },
	};
	for (const auto &l : libs) { luaL_requiref(L, l.name, l.func, 1); lua_pop(L, 1); }
	// collect print() output to hash it as well
	static std::string all;
	lua_pushcfunction(L, [](lua_State *L) -> int {
		const char *s = luaL_checkstring(L, 1);
		puts(s); all += s; all += '\n';
		return 0;
	});
	lua_setglobal(L, "print");
	if (luaL_loadstring(L, kProbe) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK)
	{
		printf("luadet error: %s\n", lua_tostring(L, -1));
		return 1;
	}
	int lines = 0;
	for (char c : all) lines += c == '\n';
	unsigned h = fnv(all.c_str(), 2166136261u);
	printf("luadet hash=%08x lines=%d\n", h, lines);
	lua_close(L);
	// the hash every build gave (GCC and Clang on x86-64 and ARM64, MinGW): a change in Lua,
	// its configuration or detmath that changes a result must update it on purpose
	const unsigned expected = 0x84b12b94u;
	if (h != expected) { printf("FAIL: expected hash=%08x; this build's Lua gives other results\n", expected); return 1; }
	printf("PASS\n");
	return 0;
}
