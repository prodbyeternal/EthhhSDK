// ===========================================================================
//  Stable C ABI (P/Invoke friendly) for EthhhSDK.Managed.dll and C# mods.
// ===========================================================================
#include "precomp.h"
#include "game.h"
#include "util.h"
#include "log.h"
#include "input.h"
#include "mods.h"
#include "clr_host.h"
#include "overlay.h"
#include "hooks.h"
#include "ethhhpatch_runtime.h"

using namespace eth;

// ---------------------------------------------------------------------------
//  SDK
// ---------------------------------------------------------------------------
ETH_CAPI(const char*, EthSDK_Version, (void))      { return kVersion; }
ETH_CAPI(const char*, EthSDK_BuildDate, (void))    { return ETH_BUILD_DATE; }
ETH_CAPI(const char*, EthSDK_GameDir, (void))      { static std::string s; s = GetGameDir(); return s.c_str(); }
ETH_CAPI(const char*, EthSDK_ModsDir, (void))      { static std::string s; s = GetModsDir(); return s.c_str(); }
ETH_CAPI(const char*, EthSDK_ConfigDir, (void))    { static std::string s; s = GetConfigDir(); return s.c_str(); }

ETH_CAPI(void, EthLog_Write, (int level, const char* message))
{
    LogMsg((LogLevel)std::clamp(level, 0, 3), "%s", message ? message : "");
}

// ---------------------------------------------------------------------------
//  Game
// ---------------------------------------------------------------------------
ETH_CAPI(int, EthGame_IsReady, (void))        { return game::IsReady() ? 1 : 0; }
ETH_CAPI(unsigned int, EthGame_TickCount, (void))  { return game::TickCount(); }
ETH_CAPI(unsigned int, EthGame_FrameCount, (void)) { return game::FrameCount(); }
ETH_CAPI(float, EthGame_Time, (void))         { return game::GameTime(); }
ETH_CAPI(float, EthGame_TimeStep, (void))     { return game::TimeStep(); }
ETH_CAPI(float, EthGame_TimeScale, (void))    { return game::TimeScale(); }
ETH_CAPI(void, EthGame_SetTimeScale, (float value)) { game::SetTimeScale(value); }
ETH_CAPI(int, EthGame_DebugFlags, (void))     { return (int)game::DebugFlags(); }
ETH_CAPI(void, EthGame_SetDebugFlags, (int flags)) { game::SetDebugFlags((uint16_t)flags); }
ETH_CAPI(void*, EthGame_Hwnd, (void))         { return game::GameHwnd(); }
ETH_CAPI(void*, EthGame_Device, (void))       { return game::Device(); }
ETH_CAPI(void*, EthGame_Renderer, (void))     { return game::Renderer(); }
ETH_CAPI(void, EthGame_UpdateView, (void))    { game::UpdateView(); }

ETH_CAPI(int, EthGame_StatInt, (int index, int* out))
{
    int v = 0;
    if (!out || !game::GetStatInt(index, v)) return 0;
    *out = v;
    return 1;
}
ETH_CAPI(int, EthGame_SetStatInt, (int index, int value)) { return game::SetStatInt(index, value) ? 1 : 0; }
ETH_CAPI(int, EthGame_StatBit, (int index))
{
    bool v = false;
    return game::GetStatBit(index, v) ? (v ? 1 : 0) : -1;
}
ETH_CAPI(int, EthGame_SetStatBit, (int index, int value)) { return game::SetStatBit(index, value != 0) ? 1 : 0; }
ETH_CAPI(unsigned int, EthGame_NativeAddr, (int index)) { return (unsigned int)game::NativeAddr(index); }
ETH_CAPI(int, EthGame_NativeCount, (void)) { return game::off::NativeCount; }

// ---------------------------------------------------------------------------
//  Camera
// ---------------------------------------------------------------------------
struct EthCamera {
    unsigned int addr;
    float x, y, z;
    float fov;
    float param104;
    float world[16];
    float view[16];
};

ETH_CAPI(int, EthGame_ReadCamera, (EthCamera* out))
{
    if (!out) return 0;
    game::CameraView c;
    if (!game::ReadCamera(c)) return 0;
    out->addr = (unsigned int)c.addr;
    out->x = c.x; out->y = c.y; out->z = c.z;
    out->fov = c.fov;
    out->param104 = c.param104;
    memcpy(out->world, c.world, sizeof(c.world));
    memcpy(out->view, c.view, sizeof(c.view));
    return 1;
}

ETH_CAPI(int, EthGame_SetCameraPos, (float x, float y, float z)) { return game::SetCameraPosition(x, y, z) ? 1 : 0; }
ETH_CAPI(int, EthGame_SetCameraWorld, (const float* world16))
{
    if (!world16) return 0;
    return game::SetCameraWorld(world16) ? 1 : 0;
}
ETH_CAPI(int, EthGame_SetCameraFovDegrees, (float degrees)) { return game::SetCameraFovDegrees(degrees) ? 1 : 0; }
ETH_CAPI(float, EthGame_CameraFovDegrees, (void))
{
    game::CameraView c;
    if (!game::ReadCamera(c)) return 0.0f;
    return c.fov * (180.0f / 3.14159265358979f);
}

// ---------------------------------------------------------------------------
//  Strats / player
// ---------------------------------------------------------------------------
struct EthStrat {
    unsigned int addr;
    float pos[4];
    float quat[4];
    unsigned int flags;
    unsigned int typeMask;
    unsigned int pClass;
    unsigned int next;
    int valid;
};

static void ToEthStrat(const game::StratView& s, EthStrat& o)
{
    memset(&o, 0, sizeof(o));
    o.addr = (unsigned int)s.addr;
    memcpy(o.pos, s.pos, sizeof(o.pos));
    memcpy(o.quat, s.quat, sizeof(o.quat));
    o.flags = s.flags;
    o.typeMask = s.typeMask;
    o.pClass = (unsigned int)(uintptr_t)s.pClass;
    o.next = (unsigned int)s.next;
    o.valid = 1;
}

ETH_CAPI(int, EthGame_StratCount, (void)) { return (int)game::CountStrats(); }

ETH_CAPI(int, EthGame_SnapshotStrats, (EthStrat* out, int maxCount))
{
    if (!out || maxCount <= 0) return 0;
    auto list = game::SnapshotStrats((size_t)maxCount);
    int n = 0;
    for (const auto& s : list) {
        if (n >= maxCount) break;
        ToEthStrat(s, out[n++]);
    }
    return n;
}

ETH_CAPI(int, EthGame_ReadStrat, (unsigned int addr, EthStrat* out))
{
    if (!out) return 0;
    game::StratView s;
    if (!game::ReadStrat(addr, s)) {
        memset(out, 0, sizeof(*out));
        return 0;
    }
    ToEthStrat(s, *out);
    return 1;
}

ETH_CAPI(int, EthGame_WriteStratPos, (unsigned int addr, float x, float y, float z))
{
    return game::WriteStratPos(addr, x, y, z) ? 1 : 0;
}

// Additive ABI; never append fields to EthStrat/managed StratInfo.
ETH_CAPI(int, EthGame_StratClassName, (unsigned int addr, char* out, int outSize))
{
    if (!out || outSize <= 0) return 0;
    out[0] = 0;
    game::StratIdentity identity;
    game::StratView s;
    if (!game::ReadStratIdentity(addr, identity) || !game::ReadStrat(addr, s) ||
        (uint32_t)(uintptr_t)s.pClass != identity.pClass || s.selfHandle != identity.selfHandle ||
        !s.className[0] || strlen(s.className) + 1 > (size_t)outSize) return 0;
    memcpy(out, s.className, strlen(s.className) + 1);
    return 1;
}

ETH_CAPI(int, EthGame_ReadStratIdentity, (unsigned int addr, game::StratIdentity* out))
{
    if (!out) return 0;
    return game::ReadStratIdentity(addr, *out) ? 1 : 0;
}

ETH_CAPI(int, EthGame_TraceSegments, (unsigned int exactNinja, const game::TraceSegment* segments, int count, game::TraceHit* hits))
{
    return game::TraceWorldSegments(exactNinja, segments, count, hits) ? 1 : 0;
}
ETH_CAPI(void, EthPatch_SetExternalCameraOwned, (int owned)) { patch::runtime::SetExternalCameraOwned(owned != 0); }
ETH_CAPI(int, EthGame_SetNinjaFacing, (unsigned int exactNinja, float yawRadians))
{
    return game::WriteNinjaFacing(exactNinja, yawRadians) ? 1 : 0;
}

ETH_CAPI(int, EthGame_PlayerDetectionInfo, (char* out, int outSize))
{
    if (!out || outSize <= 0) return 0;
    strncpy_s(out, (size_t)outSize, game::PlayerTracker::Get().DetectionInfo().c_str(), _TRUNCATE);
    return 1;
}

ETH_CAPI(unsigned int, EthGame_PlayerAddr, (void)) { return (unsigned int)game::PlayerTracker::Get().Addr(); }
ETH_CAPI(int, EthGame_PlayerMode, (void))
{
    auto m = game::PlayerTracker::Get().Mode();
    return m == game::PlayerMode::Auto ? 0 : m == game::PlayerMode::Manual ? 1 : 2;
}
ETH_CAPI(void, EthGame_PlayerSetMode, (int mode))
{
    auto& pt = game::PlayerTracker::Get();
    if (mode == 0) pt.SetMode(game::PlayerMode::Auto);
    else if (mode == 1) pt.SetMode(game::PlayerMode::Manual);
    else pt.BeginLearning();
}
ETH_CAPI(void, EthGame_PlayerSelect, (unsigned int addr)) { game::PlayerTracker::Get().Select(addr); }
ETH_CAPI(void, EthGame_PlayerLearn, (void)) { game::PlayerTracker::Get().BeginLearning(); }
ETH_CAPI(int, EthGame_PlayerReacquireExactNinja, (void)) { return game::PlayerTracker::Get().ReacquireExactNinja() ? 1 : 0; }
ETH_CAPI(int, EthGame_PlayerValid, (void)) { return game::PlayerTracker::Get().Valid() ? 1 : 0; }
ETH_CAPI(int, EthGame_PlayerCandidateCount, (void))
{
    return (int)game::PlayerTracker::Get().Candidates().size();
}

// ---------------------------------------------------------------------------
//  Memory
// ---------------------------------------------------------------------------
ETH_CAPI(int, EthMem_Read, (unsigned int addr, void* out, int size))
{
    if (!out || size <= 0) return 0;
    return SafeReadBytes(addr, out, (size_t)size) ? 1 : 0;
}
ETH_CAPI(int, EthMem_Write, (unsigned int addr, const void* data, int size))
{
    if (!data || size <= 0) return 0;
    return SafeWriteBytes(addr, data, (size_t)size) ? 1 : 0;
}
ETH_CAPI(int, EthMem_IsReadable, (unsigned int addr, int size))
{
    return IsReadable((const void*)(uintptr_t)addr, (size_t)std::max(1, size)) ? 1 : 0;
}
ETH_CAPI(unsigned int, EthMem_ModuleBase, (void)) { return (unsigned int)ModuleBase(); }
ETH_CAPI(unsigned int, EthMem_GameAddr, (unsigned int va)) { return (unsigned int)GameAddr(va); }

// ---------------------------------------------------------------------------
//  Input / overlay
// ---------------------------------------------------------------------------
ETH_CAPI(int, EthInput_KeyDown, (int vk)) { return input::KeyDown(vk) ? 1 : 0; }
ETH_CAPI(int, EthInput_KeyPressed, (int vk)) { return input::KeyPressed(vk) ? 1 : 0; }
ETH_CAPI(int, EthInput_MouseDown, (int button)) { return input::MouseDown(button) ? 1 : 0; }
ETH_CAPI(void, EthInput_GetMouseDelta, (float* dx, float* dy))
{
    float x = 0, y = 0;
    input::GetMouseDelta(x, y);
    if (dx) *dx = x;
    if (dy) *dy = y;
}
ETH_CAPI(void, EthInput_ConsumeMouseDelta, (void)) { input::ConsumeMouseDelta(); }
ETH_CAPI(int, EthInput_SetCapture, (int on)) { return input::SetCapture(on != 0) ? 1 : 0; }
ETH_CAPI(int, EthInput_GetCapture, (void)) { return input::CaptureActive() ? 1 : 0; }

ETH_CAPI(int, EthOverlay_IsOpen, (void)) { return overlay::IsVisible() ? 1 : 0; }
ETH_CAPI(void, EthOverlay_Toggle, (void)) { overlay::Toggle(); }
ETH_CAPI(void, EthOverlay_SetOpen, (int open)) { overlay::SetVisible(open != 0); }
ETH_CAPI(int, EthOverlay_ToggleKey, (void)) { return overlay::ToggleKey(); }
ETH_CAPI(void, EthOverlay_NotifyRenderException, (void)) { overlay::NotifyRenderException(); }

// ---------------------------------------------------------------------------
//  Hooks / managed callbacks
// ---------------------------------------------------------------------------
ETH_CAPI(void, EthHooks_SetManagedCallbacks, (void* tick, void* gui, void* overlayChanged))
{
    clr::SetCallbacks(tick, gui, overlayChanged);
}
ETH_CAPI(int, EthHooks_GameHooked, (void))
{
    return hooks::TickHooked() && hooks::D3DHooked() && hooks::WndProcHooked() ? 1 : 0;
}

// ---------------------------------------------------------------------------
//  Mods
// ---------------------------------------------------------------------------
ETH_CAPI(int, EthMod_IsEnabled, (const char* id))
{
    return id && mods::ModManager::Get().IsEnabled(id) ? 1 : 0;
}
ETH_CAPI(int, EthMod_Count, (void)) { return (int)mods::ModManager::Get().List().size(); }

ETH_CAPI(int, EthMod_GetId, (int index, char* out, int outSize))
{
    auto& list = mods::ModManager::Get().List();
    if (index < 0 || index >= (int)list.size() || !out || outSize <= 0) return 0;
    strncpy_s(out, (size_t)outSize, list[index].id.c_str(), _TRUNCATE);
    return 1;
}

ETH_CAPI(int, EthMod_GetName, (int index, char* out, int outSize))
{
    auto& list = mods::ModManager::Get().List();
    if (index < 0 || index >= (int)list.size() || !out || outSize <= 0) return 0;
    strncpy_s(out, (size_t)outSize, list[index].name.c_str(), _TRUNCATE);
    return 1;
}

ETH_CAPI(int, EthMod_GetSetting, (const char* id, const char* key, char* out, int outSize))
{
    if (!id || !key || !out || outSize <= 0) return 0;
    std::string value = mods::ModManager::Get().GetSetting(id, key);
    strncpy_s(out, (size_t)outSize, value.c_str(), _TRUNCATE);
    return 1;
}
ETH_CAPI(void, EthMod_SetSetting, (const char* id, const char* key, const char* value))
{
    if (!id || !key) return;
    mods::ModManager::Get().SetSetting(id, key, value ? value : "");
}
ETH_CAPI(float, EthMod_GetSettingFloat, (const char* id, const char* key, float def))
{
    if (!id || !key) return def;
    return mods::ModManager::Get().GetSettingFloat(id, key, def);
}
ETH_CAPI(void, EthMod_SetSettingFloat, (const char* id, const char* key, float value))
{
    if (!id || !key) return;
    mods::ModManager::Get().SetSettingFloat(id, key, value);
}
ETH_CAPI(int, EthMod_GetSettingBool, (const char* id, const char* key, int def))
{
    if (!id || !key) return def;
    return mods::ModManager::Get().GetSettingBool(id, key, def != 0) ? 1 : 0;
}
ETH_CAPI(void, EthMod_SetSettingBool, (const char* id, const char* key, int value))
{
    if (!id || !key) return;
    mods::ModManager::Get().SetSettingBool(id, key, value != 0);
}
ETH_CAPI(void, EthMod_SaveSettings, (void)) { mods::ModManager::Get().SaveSettings(); }
ETH_CAPI(void, EthMod_ReportError, (const char* id, const char* message))
{
    if (!id) return;
    mods::ModManager::Get().ReportError(id, message ? message : "unknown error");
}
ETH_CAPI(void, EthMod_SetStatus, (const char* id, const char* status))
{
    if (!id) return;
    mods::ModManager::Get().SetStatus(id, status ? status : "");
}
ETH_CAPI(void, EthMod_OpenFolder, (void)) { mods::ModManager::Get().OpenModsFolder(); }
