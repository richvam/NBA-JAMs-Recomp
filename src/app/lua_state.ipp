// lua_state.ipp - a script's data in a save state (roadmap N1.2), included by mods.cpp.
//
// Lua 5.4 cannot save a whole interpreter, so this saves what a script keeps: every value
// reachable from its registry (its globals, the libraries it included, and through each
// function the locals it uses, its upvalues), as long as they are data: nil, booleans,
// numbers, strings, tables (with their metatables) and the bytes of a full userdata (such
// as math.random's state). Functions are not saved: each one is found again in the running
// script by the path it was first reached by (a key, an upvalue, a metatable), and its
// upvalues are set to the saved values, keeping which ones were shared. A table is filled
// in place when the running script has one at its path (so _G and the libraries keep
// their identity) and made new otherwise. Loading happens in two steps: prepare() finds
// everything and changes nothing, commit() writes; a script that cannot be matched (a
// function made during the game that the running script does not have, a coroutine) fails
// in prepare(), before the machine or any script is changed.
namespace lua_snap {

enum : uint8_t { V_NIL, V_FALSE, V_TRUE, V_INT, V_FLOAT, V_STR, V_REF };
enum : uint8_t { K_TABLE, K_LFUNC, K_CFUNC, K_UDATA, K_KEEP };
enum : uint8_t { S_KEY, S_UPV, S_META };

struct value { uint8_t tag = V_NIL; int64_t i = 0; double f = 0; std::string s; uint32_t ref = 0; };
struct step { uint8_t kind; value key; int upv = 0; };
struct upv { bool shared = false; value v; uint32_t fn = 0; int n = 0; };
struct object
{
	uint8_t kind;
	std::vector<step> path;
	std::vector<std::pair<value, value>> entries;   // K_TABLE
	value meta;                                     // K_TABLE: V_REF or V_NIL
	std::vector<upv> ups;                           // K_LFUNC, K_CFUNC
	std::string bytes;                              // K_UDATA
};

// ---- writing and reading the bytes -------------------------------------------------
static void put_value(state_io &s, value &v)
{
	s(v.tag);
	switch (v.tag)
	{
		case V_INT: s(v.i); break;
		case V_FLOAT: s(v.f); break;
		case V_STR: { std::vector<char> b(v.s.begin(), v.s.end()); s.vec(b); if (!s.saving()) v.s.assign(b.begin(), b.end()); break; }
		case V_REF: s(v.ref); break;
		default: break;
	}
}
static void io_objects(state_io &s, std::vector<object> &objs)
{
	uint32_t n = uint32_t(objs.size());
	s(n);
	if (!s.saving()) { if (!s.ok() || n > 1000000) { s.fail("script state: bad object count"); return; } objs.resize(n); }
	for (object &o : objs)
	{
		s(o.kind);
		uint32_t np = uint32_t(o.path.size()); s(np);
		if (!s.saving()) { if (!s.ok() || np > 10000) { s.fail("script state: bad path"); return; } o.path.resize(np); }
		for (step &p : o.path) { s(p.kind); if (p.kind == S_KEY) put_value(s, p.key); else if (p.kind == S_UPV) s(p.upv); }
		if (o.kind == K_TABLE)
		{
			uint32_t ne = uint32_t(o.entries.size()); s(ne);
			if (!s.saving()) { if (!s.ok() || ne > 10000000) { s.fail("script state: bad table"); return; } o.entries.resize(ne); }
			for (auto &e : o.entries) { put_value(s, e.first); put_value(s, e.second); }
			put_value(s, o.meta);
		}
		else if (o.kind == K_LFUNC || o.kind == K_CFUNC)
		{
			uint32_t nu = uint32_t(o.ups.size()); s(nu);
			if (!s.saving()) { if (!s.ok() || nu > 300) { s.fail("script state: bad function"); return; } o.ups.resize(nu); }
			for (upv &u : o.ups)
			{
				s(u.shared);
				if (u.shared) { s(u.fn); s(u.n); } else put_value(s, u.v);
			}
		}
		else if (o.kind == K_UDATA)
		{
			std::vector<char> b(o.bytes.begin(), o.bytes.end()); s.vec(b);
			if (!s.saving()) o.bytes.assign(b.begin(), b.end());
		}
		if (!s.ok()) return;
	}
}

// ---- saving -----------------------------------------------------------------------
struct saver
{
	lua_State *L;
	std::vector<object> objs;
	std::map<const void *, uint32_t> seen;          // object -> id
	std::map<void *, std::pair<uint32_t, int>> upids; // upvalue cell -> first (function, index)
	std::string err;

	static int key_rank(lua_State *L, int i) { int t = lua_type(L, i); return t == LUA_TBOOLEAN ? 0 : t == LUA_TNUMBER ? 1 : 2; }

	// the value at the top of the stack (popped)
	value take(const std::vector<step> &path)
	{
		value v;
		int t = lua_type(L, -1);
		switch (t)
		{
			case LUA_TNIL: break;
			case LUA_TBOOLEAN: v.tag = lua_toboolean(L, -1) ? V_TRUE : V_FALSE; break;
			case LUA_TNUMBER:
				if (lua_isinteger(L, -1)) { v.tag = V_INT; v.i = lua_tointeger(L, -1); }
				else { v.tag = V_FLOAT; v.f = lua_tonumber(L, -1); }
				break;
			case LUA_TSTRING: { size_t n; const char *p = lua_tolstring(L, -1, &n); v.tag = V_STR; v.s.assign(p, n); break; }
			default: v.tag = V_REF; v.ref = visit(path); return v;   // visit pops
		}
		lua_pop(L, 1);
		return v;
	}

	// an object at the top of the stack (popped): its id, walking it the first time
	uint32_t visit(const std::vector<step> &path)
	{
		const void *p = lua_topointer(L, -1);
		auto it = seen.find(p);
		if (it != seen.end()) { lua_pop(L, 1); return it->second; }
		uint32_t id = uint32_t(objs.size());
		seen[p] = id;
		objs.push_back({});
		objs[id].path = path;
		int t = lua_type(L, -1);
		if (t == LUA_TTABLE)
		{
			objs[id].kind = K_TABLE;
			int ti = lua_gettop(L);
			// keys in a fixed order: booleans, numbers, strings
			std::vector<value> keys;
			lua_pushnil(L);
			while (lua_next(L, ti))
			{
				lua_pop(L, 1);
				int kt = lua_type(L, -1);
				if (kt != LUA_TBOOLEAN && kt != LUA_TNUMBER && kt != LUA_TSTRING)
				{
					if (err.empty()) err = std::string("a ") + lua_typename(L, kt) + " is used as a table key";
					continue;
				}
				lua_pushvalue(L, -1);
				keys.push_back(take({}));
			}
			std::sort(keys.begin(), keys.end(), [](const value &a, const value &b) {
				auto rank = [](const value &v) { return v.tag == V_FALSE || v.tag == V_TRUE ? 0 : v.tag == V_STR ? 2 : 1; };
				int ra = rank(a), rb = rank(b);
				if (ra != rb) return ra < rb;
				if (ra == 0) return a.tag < b.tag;
				if (ra == 2) return a.s < b.s;
				double x = a.tag == V_INT ? double(a.i) : a.f, y = b.tag == V_INT ? double(b.i) : b.f;
				if (x != y) return x < y;
				return a.tag < b.tag;
			});
			for (value &k : keys)
			{
				if (path.empty() && k.tag == V_INT && k.i == LUA_RIDX_MAINTHREAD) continue;   // the registry's thread
				push(k);
				lua_rawget(L, ti);
				std::vector<step> sub = path;
				sub.push_back({ S_KEY, k, 0 });
				value v = take(sub);
				objs[id].entries.push_back({ k, v });
			}
			if (lua_getmetatable(L, ti))
			{
				std::vector<step> sub = path;
				sub.push_back({ S_META, {}, 0 });
				objs[id].meta = take(sub);
			}
		}
		else if (t == LUA_TFUNCTION)
		{
			bool c = lua_iscfunction(L, -1);
			objs[id].kind = c ? K_CFUNC : K_LFUNC;
			int fi = lua_gettop(L);
			for (int n = 1; lua_getupvalue(L, fi, n); n++)
			{
				upv u;
				void *cell = c ? nullptr : lua_upvalueid(L, fi, n);
				auto f = cell ? upids.find(cell) : upids.end();
				if (f != upids.end()) { u.shared = true; u.fn = f->second.first; u.n = f->second.second; lua_pop(L, 1); }
				else
				{
					if (cell) upids[cell] = { id, n };
					std::vector<step> sub = path;
					sub.push_back({ S_UPV, {}, n });
					u.v = take(sub);
				}
				objs[id].ups.push_back(u);
			}
		}
		else if (t == LUA_TUSERDATA)
		{
			objs[id].kind = K_UDATA;
			objs[id].bytes.assign(static_cast<const char *>(lua_touserdata(L, -1)), lua_rawlen(L, -1));
		}
		else if (t == LUA_TLIGHTUSERDATA) objs[id].kind = K_KEEP;   // the manager's own pointers
		else if (err.empty()) err = std::string("a ") + lua_typename(L, t) + " cannot be saved";
		lua_pop(L, 1);
		return id;
	}

	void push(const value &v)
	{
		switch (v.tag)
		{
			case V_FALSE: lua_pushboolean(L, 0); break;
			case V_TRUE: lua_pushboolean(L, 1); break;
			case V_INT: lua_pushinteger(L, v.i); break;
			case V_FLOAT: lua_pushnumber(L, v.f); break;
			case V_STR: lua_pushlstring(L, v.s.data(), v.s.size()); break;
			default: lua_pushnil(L); break;
		}
	}
};

static bool save(lua_State *L, state_io &s, std::string &err)
{
	saver sv{ L, {}, {}, {}, {} };
	lua_pushvalue(L, LUA_REGISTRYINDEX);
	sv.visit({});
	if (!sv.err.empty()) { err = sv.err; return false; }
	io_objects(s, sv.objs);
	return true;
}

// ---- loading ----------------------------------------------------------------------
struct loader
{
	lua_State *L;
	std::vector<object> objs;
	int live = 0;      // a table on the stack: id + 1 -> the live object
	std::string err;

	void push(const value &v)
	{
		switch (v.tag)
		{
			case V_FALSE: lua_pushboolean(L, 0); break;
			case V_TRUE: lua_pushboolean(L, 1); break;
			case V_INT: lua_pushinteger(L, v.i); break;
			case V_FLOAT: lua_pushnumber(L, v.f); break;
			case V_STR: lua_pushlstring(L, v.s.data(), v.s.size()); break;
			case V_REF: lua_rawgeti(L, live, lua_Integer(v.ref) + 1); break;
			default: lua_pushnil(L); break;
		}
	}

	// the value at a path in the running script (before anything is written), or nil
	void resolve(const std::vector<step> &path)
	{
		lua_pushvalue(L, LUA_REGISTRYINDEX);
		for (const step &p : path)
		{
			int t = lua_type(L, -1);
			if (p.kind == S_KEY && t == LUA_TTABLE)
			{
				if (p.key.tag == V_REF) { lua_pop(L, 1); lua_pushnil(L); return; }
				push(p.key);
				lua_rawget(L, -2);
			}
			else if (p.kind == S_UPV && t == LUA_TFUNCTION)
			{
				if (!lua_getupvalue(L, -1, p.upv)) lua_pushnil(L);
			}
			else if (p.kind == S_META && t == LUA_TTABLE)
			{
				if (!lua_getmetatable(L, -1)) lua_pushnil(L);
			}
			else lua_pushnil(L);
			lua_remove(L, -2);
		}
	}

	// step 1: find or make every object; nothing in the script is changed
	bool prepare()
	{
		lua_createtable(L, int(objs.size()), 0);
		live = lua_gettop(L);
		for (size_t id = 0; id < objs.size(); id++)
		{
			object &o = objs[id];
			resolve(o.path);
			int t = lua_type(L, -1);
			bool ok = true;
			switch (o.kind)
			{
				case K_TABLE: if (t != LUA_TTABLE) { lua_pop(L, 1); lua_newtable(L); } break;
				case K_LFUNC: ok = t == LUA_TFUNCTION && !lua_iscfunction(L, -1); break;
				case K_CFUNC: ok = t == LUA_TFUNCTION && lua_iscfunction(L, -1); break;
				case K_UDATA: ok = t == LUA_TUSERDATA && lua_rawlen(L, -1) == o.bytes.size(); break;
				default: break;
			}
			if (!ok)
			{
				err = "the running script has no matching ";
				err += o.kind == K_UDATA ? "userdata" : "function";
				err += " (one made during the game?)";
				lua_pop(L, 1);
				return false;
			}
			lua_rawseti(L, live, lua_Integer(id) + 1);
		}
		for (object &o : objs)
			if (o.kind == K_LFUNC || o.kind == K_CFUNC)
				for (upv &u : o.ups)
					if (u.shared && (u.fn >= objs.size() || objs[u.fn].kind != K_LFUNC)) { err = "bad shared upvalue"; return false; }
		return true;
	}

	// step 2: write the saved values
	void commit()
	{
		for (size_t id = 0; id < objs.size(); id++)
		{
			object &o = objs[id];
			lua_rawgeti(L, live, lua_Integer(id) + 1);
			int x = lua_gettop(L);
			if (o.kind == K_TABLE)
			{
				// the old keys are cleared first; not in the registry (id 0), whose thread and
				// other entries the save leaves out
				lua_newtable(L);
				int old = lua_gettop(L);
				int n = 0;
				lua_pushnil(L);
				while (lua_next(L, x)) { lua_pop(L, 1); lua_pushvalue(L, -1); lua_rawseti(L, old, ++n); }
				if (id != 0)
					for (int i = 1; i <= n; i++) { lua_rawgeti(L, old, i); lua_pushnil(L); lua_rawset(L, x); }
				lua_pop(L, 1);
				for (auto &e : o.entries)
				{
					if (e.first.tag == V_NIL) continue;
					push(e.first); push(e.second); lua_rawset(L, x);
				}
				if (o.meta.tag == V_REF) { push(o.meta); lua_setmetatable(L, x); }
				else if (id != 0) { lua_pushnil(L); lua_setmetatable(L, x); }
			}
			else if (o.kind == K_LFUNC || o.kind == K_CFUNC)
			{
				for (size_t n = 0; n < o.ups.size(); n++)
					if (!o.ups[n].shared) { push(o.ups[n].v); if (!lua_setupvalue(L, x, int(n) + 1)) lua_pop(L, 1); }
			}
			else if (o.kind == K_UDATA)
				std::memcpy(lua_touserdata(L, x), o.bytes.data(), o.bytes.size());
			lua_pop(L, 1);
		}
		// shared upvalues: the same cell as the function that had it first
		for (size_t id = 0; id < objs.size(); id++)
		{
			object &o = objs[id];
			if (o.kind != K_LFUNC) continue;
			for (size_t n = 0; n < o.ups.size(); n++)
			{
				if (!o.ups[n].shared) continue;
				lua_rawgeti(L, live, lua_Integer(id) + 1);
				lua_rawgeti(L, live, lua_Integer(o.ups[n].fn) + 1);
				lua_upvaluejoin(L, -2, int(n) + 1, -1, o.ups[n].n);
				lua_pop(L, 2);
			}
		}
		lua_remove(L, live);
		live = 0;
	}

	void abandon() { if (live) { lua_remove(L, live); live = 0; } }
};

} // namespace lua_snap
