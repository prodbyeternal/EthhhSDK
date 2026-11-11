// ===========================================================================
//  EthhhSDK entry gate
//
//  Invoked by the dinput8.dll proxy from the patched PE entry point, i.e.
//  while the game's main thread is still initialising and before any game code
//  has run. This is the safe place to show the pre-game Mod Manager, load
//  mods, and install MinHook detours.
// ===========================================================================
#include "precomp.h"
#include "util.h"
#include "log.h"
#include "config.h"
#include "mods.h"
#include "hooks.h"
#include "launcher.h"
#include "overlay.h"
#include "game.h"
#include "input.h"

#include <shellapi.h>

using namespace eth;

namespace {

bool g_started = false;

} // namespace

// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_selfModule = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
ETH_CAPI(void, EthhhSDK_EntryGate, (void))
{
    if (g_started)
        return;
    g_started = true;

    LogInit();
    ETH_INFO("==========================================================");
    ETH_INFO("%s v%s (build %s) entry gate", kName, kVersion, ETH_BUILD_DATE);

    if (!ValidateGameImage()) {
        ETH_ERROR("game image validation failed; skipping SDK initialisation");
        return;
    }

    ETH_INFO("game directory: %s", GetGameDir().c_str());
    CreateDirectories(GetSdkDir());
    CreateDirectories(GetModsDir());

    Config& cfg = Config::Get();
    cfg.Load(JoinPath(GetConfigDir(), "EthhhSDK.ini"));

    overlay::SetToggleKey(cfg.GetInt("core", "overlay_key", VK_F9));

    const bool safeMode = cfg.GetBool("core", "safe_mode", false);
    if (safeMode)
        ETH_WARN("safe mode is enabled: hooks and mods will not be loaded");

    auto& mgr = mods::ModManager::Get();
    mgr.SetLauncherMode(true);
    mgr.Scan();
    mgr.LoadConfig();

    if (cfg.GetBool("core", "launcher", true)) {
        ETH_INFO("launcher: opening pre-game mod manager");
        const int start = launcher::Run();
        if (!start) {
            ETH_INFO("launcher: exit requested");
            LogShutdown();
            ExitProcess(0);
        }
    } else {
        ETH_INFO("launcher: disabled by config, starting the game directly");
    }

    if (!safeMode) {
        if (!hooks::Init()) {
            ETH_ERROR("MinHook initialization failed; game hooks are unavailable");
        } else {
            hooks::InstallTickHook();
        }
    }

    if (!safeMode)
        mgr.InitEnabledMods();

    if (!safeMode)
        hooks::StartGameWatcher();

    ETH_INFO("%s ready: %zu mod(s) enabled, %zu loaded",
             kName, mgr.CountEnabled(), mgr.CountLoaded());
}

// ---------------------------------------------------------------------------
ETH_CAPI(void, EthhhSDK_Shutdown, (void))
{
    if (!g_started)
        return;
    g_started = false;

    ETH_INFO("%s shutting down", kName);
    mods::ModManager::Get().Shutdown();
    hooks::Shutdown();
    overlay::Shutdown();
    input::Shutdown();
    LogShutdown();
}
