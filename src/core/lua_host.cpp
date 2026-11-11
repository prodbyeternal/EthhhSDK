#include "lua_host.h"
#include "util.h"
#include "log.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <unordered_map>

namespace eth::lua {

namespace {

struct LuaMod {
    lua_State* L = nullptr;
    std::string id;
    std::string dir;
    std::string entry;
    int errors = 0;
    bool disabled = false;
    std::string lastError;
};

std::unordered_map<std::string, LuaMod> g_mods;

void PushTracebackHandler(lua_State* L)
{
    lua_getglobal(L, "debug");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "traceback");
        lua_remove(L, -2);
    } else {
        lua_pop(L, 1);
        lua_pushcfunction(L, [](lua_State* l) -> int {
            const char* msg = lua_tostring(l, 1);
            luaL_traceback(l, l, msg ? msg : "error", 1);
            return 1;
        });
    }
}

std::string ValueToString(lua_State* L, int idx)
{
    const char* s = lua_tostring(L, idx);
    return s ? s : "(non-string error)";
}

bool RunFile(LuaMod& m, const std::string& path)
{
    std::string src;
    if (!ReadFileToString(path, src)) {
        m.lastError = "cannot read " + path;
        return false;
    }

    lua_State* L = m.L;
    PushTracebackHandler(L);
    int handler = lua_gettop(L);

    if (luaL_loadbuffer(L, src.c_str(), src.size(), ("@" + path).c_str()) != LUA_OK) {
        m.lastError = ValueToString(L, -1);
        lua_pop(L, 2);
        return false;
    }
    if (lua_pcall(L, 0, 0, handler) != LUA_OK) {
        m.lastError = ValueToString(L, -1);
        lua_pop(L, 2);
        return false;
    }
    lua_pop(L, 1);
    return true;
}

int HookCount(LuaMod& m, const char* event)
{
    lua_State* L = m.L;
    lua_getfield(L, LUA_REGISTRYINDEX, "eth.hooks");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return 0; }
    lua_getfield(L, -1, event);
    int n = lua_istable(L, -1) ? (int)lua_rawlen(L, -1) : 0;
    lua_pop(L, 2);
    return n;
}

void CallHooks(LuaMod& m, const char* event, int nargs = 0, std::function<void(lua_State*)> pushArgs = nullptr)
{
    if (!m.L || m.disabled) return;
    lua_State* L = m.L;
    int n = HookCount(m, event);
    if (n == 0) return;

    PushTracebackHandler(L);
    int handler = lua_gettop(L);

    for (int i = 1; i <= n; ++i) {
        lua_getfield(L, LUA_REGISTRYINDEX, "eth.hooks");
        lua_getfield(L, -1, event);
        lua_rawgeti(L, -1, i);            // function
        lua_remove(L, -2);                // hooks table
        lua_remove(L, -2);                // eth.hooks
        if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }

        int base = lua_gettop(L) - 1;
        if (pushArgs) pushArgs(L);
        int argc = lua_gettop(L) - base - 1;
        if (lua_pcall(L, argc, 0, handler) != LUA_OK) {
            std::string err = ValueToString(L, -1);
            lua_pop(L, 1);
            m.errors++;
            m.lastError = err;
            ETH_ERROR("lua[%s]: %s", m.id.c_str(), err.c_str());
            if (m.errors >= 8) {
                m.disabled = true;
                ETH_ERROR("lua[%s]: too many errors, mod disabled", m.id.c_str());
                mods::ModManager::Get().SetStatus(m.id, "error: " + err);
                break;
            }
        }
    }
    lua_pop(L, 1); // traceback handler
}

} // namespace

bool LoadMod(mods::ModInfo& mod)
{
    UnloadMod(mod.id);

    LuaMod m;
    m.id = mod.id;
    m.dir = mod.dir;
    m.entry = mod.entry;
    m.L = luaL_newstate();
    if (!m.L) {
        mod.status = "error: luaL_newstate failed";
        return false;
    }
    luaL_openlibs(m.L);

    // Sandbox: drop dangerous entry points.
    const char* removeGlobals[] = { "dofile", "loadfile", "io", "package", nullptr };
    for (int i = 0; removeGlobals[i]; ++i) {
        lua_pushnil(m.L);
        lua_setglobal(m.L, removeGlobals[i]);
    }
    lua_getglobal(m.L, "os");
    if (lua_istable(m.L, -1)) {
        const char* removeOs[] = { "execute", "remove", "rename", "exit", "setlocale", nullptr };
        for (int i = 0; removeOs[i]; ++i) {
            lua_pushnil(m.L);
            lua_setfield(m.L, -2, removeOs[i]);
        }
    }
    lua_pop(m.L, 1);

    // print -> SDK log
    lua_pushcfunction(m.L, [](lua_State* L) -> int {
        int n = lua_gettop(L);
        std::string s;
        for (int i = 1; i <= n; ++i) {
            if (i > 1) s += "\t";
            size_t len = 0;
            const char* str = luaL_tolstring(L, i, &len);
            if (str) s.append(str, len);
            lua_pop(L, 1);
        }
        ETH_INFO("[lua] %s", s.c_str());
        return 0;
    });
    lua_setglobal(m.L, "print");

    // hook registries
    lua_newtable(m.L);
    lua_setfield(m.L, LUA_REGISTRYINDEX, "eth.hooks");

    // mod identity
    lua_pushstring(m.L, mod.id.c_str());
    lua_setfield(m.L, LUA_REGISTRYINDEX, "eth.modid");

    lua_newtable(m.L);
    lua_pushstring(m.L, mod.id.c_str());       lua_setfield(m.L, -2, "id");
    lua_pushstring(m.L, mod.name.c_str());     lua_setfield(m.L, -2, "name");
    lua_pushstring(m.L, mod.version.c_str());  lua_setfield(m.L, -2, "version");
    lua_pushstring(m.L, mod.author.c_str());   lua_setfield(m.L, -2, "author");
    lua_pushstring(m.L, mod.dir.c_str());      lua_setfield(m.L, -2, "dir");
    lua_setglobal(m.L, "mod");

    RegisterApi(m.L, mod);
    RegisterImGui(m.L);

    if (!RunFile(m, mod.entry)) {
        mod.status = "error: " + m.lastError;
        ETH_ERROR("lua[%s]: load failed: %s", mod.id.c_str(), m.lastError.c_str());
        lua_close(m.L);
        return false;
    }

    mod.loaded = true;
    mod.errored = false;
    mod.status = "loaded";
    g_mods[mod.id] = std::move(m);
    ETH_INFO("lua[%s]: loaded %s", mod.id.c_str(), mod.entry.c_str());
    return true;
}

void UnloadMod(const std::string& id)
{
    auto it = g_mods.find(id);
    if (it == g_mods.end()) return;
    if (it->second.L) lua_close(it->second.L);
    g_mods.erase(it);
    if (auto* mi = mods::ModManager::Get().Find(id)) {
        mi->loaded = false;
        if (mi->status == "loaded") mi->status = "not loaded";
    }
}

void UnloadAll()
{
    for (auto& kv : g_mods)
        if (kv.second.L) lua_close(kv.second.L);
    g_mods.clear();
}

bool IsLoaded(const std::string& id)
{
    auto it = g_mods.find(id);
    return it != g_mods.end() && it->second.L && !it->second.disabled;
}

void TickMod(const std::string& id, int tickIndex, float dt)
{
    auto it = g_mods.find(id);
    if (it == g_mods.end()) return;
    CallHooks(it->second, "tick", 2, [tickIndex, dt](lua_State* L) {
        lua_pushinteger(L, tickIndex);
        lua_pushnumber(L, dt);
    });
}

void DrawModGui(const std::string& id)
{
    auto it = g_mods.find(id);
    if (it == g_mods.end()) return;
    CallHooks(it->second, "gui");
}

void OnOverlayChanged(const std::string& id, bool open)
{
    auto it = g_mods.find(id);
    if (it == g_mods.end()) return;
    CallHooks(it->second, open ? "overlay_open" : "overlay_close", 1, [open](lua_State* L) {
        lua_pushboolean(L, open);
    });
}

void Reload(const std::string& id)
{
    auto* mi = mods::ModManager::Get().Find(id);
    if (!mi) return;
    if (!mi->enabled) { UnloadMod(id); return; }
    UnloadMod(id);
    LoadMod(*mi);
}

std::string LastError(const std::string& id)
{
    auto it = g_mods.find(id);
    return it == g_mods.end() ? std::string() : it->second.lastError;
}

void Shutdown()
{
    UnloadAll();
}

} // namespace eth::lua
