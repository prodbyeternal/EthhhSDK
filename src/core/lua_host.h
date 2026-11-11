#pragma once
#include "precomp.h"
#include "mods.h"

struct lua_State;

namespace eth::lua {

// Creates an isolated Lua 5.4 state per mod, registers the EthhhSDK API and
// runs the mod entry script.
bool LoadMod(mods::ModInfo& mod);
void UnloadMod(const std::string& id);
void UnloadAll();
bool IsLoaded(const std::string& id);

void TickMod(const std::string& id, int tickIndex, float dt);
void DrawModGui(const std::string& id);
void OnOverlayChanged(const std::string& id, bool open);

void Reload(const std::string& id);
std::string LastError(const std::string& id);
void Shutdown();

// Registers the shared `imgui`, `game`, ... tables. Implemented in
// lua_bindings.cpp / lua_imgui.cpp.
void RegisterApi(lua_State* L, const mods::ModInfo& mod);
void RegisterImGui(lua_State* L);

} // namespace eth::lua
