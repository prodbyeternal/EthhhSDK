// ===========================================================================
//  CLR host - runs EthhhSDK.Managed.dll (.NET Framework 4.x) inside the game.
//
//  mscoree.h / mscoree.lib are not part of every Windows SDK install, so the
//  host binds CorBindToRuntimeEx from mscoree.dll at runtime and declares the
//  minimum ICLRRuntimeHost interface it needs.
// ===========================================================================
#include "clr_host.h"
#include "util.h"
#include "log.h"
#include "mods.h"

#include <unknwn.h>

namespace eth::clr {

namespace {

// {90F1A06E-7712-4762-86B5-7A5EBA6BDB02}
const GUID kClsidClrRuntimeHost =
    { 0x90F1A06E, 0x7712, 0x4762, { 0x86, 0xB5, 0x7A, 0x5E, 0xBA, 0x6B, 0xDB, 0x02 } };
// {90F1A06C-7712-4762-86B5-7A5EBA6BDB02}
const GUID kIidIclrRuntimeHost =
    { 0x90F1A06C, 0x7712, 0x4762, { 0x86, 0xB5, 0x7A, 0x5E, 0xBA, 0x6B, 0xDB, 0x02 } };

struct ICLRRuntimeHostMin : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Start() = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetHostControl(IUnknown* pHostControl) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCLRControl(IUnknown** pCLRControl) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnloadAppDomain(DWORD dwAppDomainId, BOOL fWaitUntilDone) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteInAppDomain(DWORD dwAppDomainId, void* pManagedTarget, void* pArgument) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentAppDomainId(DWORD* pdwAppDomainId) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteApplication(LPCWSTR pwzAppFullName, DWORD dwManifestPaths,
                                                        LPCWSTR* ppwzManifestPaths, DWORD dwActivationData,
                                                        LPCWSTR* ppwzActivationData, int* pReturnValue) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteInDefaultAppDomain(LPCWSTR pwzAssemblyPath, LPCWSTR pwzTypeName,
                                                                LPCWSTR pwzMethodName, LPCWSTR pwzArgument,
                                                                DWORD* pReturnValue) = 0;
};

using CorBindToRuntimeExFn = HRESULT(STDAPICALLTYPE*)(LPCWSTR pwszVersion, LPCWSTR pwszBuildFlavor,
                                                     DWORD startupFlags, REFCLSID rclsid, REFIID riid, LPVOID* ppv);

using ManagedTickFn    = void(__cdecl*)(int);
using ManagedGuiFn     = void(__cdecl*)();
using ManagedOverlayFn = void(__cdecl*)(int);

ICLRRuntimeHostMin* g_runtime = nullptr;
HMODULE             g_mscoree = nullptr;
bool                g_started = false;
std::atomic<void*>  g_tickFn{ nullptr };
std::atomic<void*>  g_guiFn{ nullptr };
std::atomic<void*>  g_overlayFn{ nullptr };
std::string         g_lastError;

std::string HresultText(HRESULT hr)
{
    if (FAILED(hr)) {
        char buf[32];
        sprintf_s(buf, "0x%08lX", (unsigned long)hr);
        std::string sys = ErrorCodeString((DWORD)hr);
        return std::string(buf) + " (" + sys + ")";
    }
    return "ok";
}

// ICLRRuntimeHost expects a wide string argument.
std::wstring BuildArgument(const std::vector<mods::ModInfo>& csharpMods)
{
    std::wstring arg = Utf8ToWide(GetGameDir());
    arg += L"\n";
    for (const auto& m : csharpMods) {
        if (!m.enabled || !m.IsCSharp())
            continue;
        arg += Utf8ToWide(m.id);
        arg += L"|";
        arg += Utf8ToWide(m.entry);
        arg += L"|";
        arg += Utf8ToWide(m.dir);
        arg += L"\n";
    }
    return arg;
}

} // namespace

bool Start(const std::vector<mods::ModInfo>& csharpMods)
{
    if (g_started)
        return true;

    std::string assemblyPath = JoinPath(GetSdkDir(), "EthhhSDK.Managed.dll");
    if (!FileExists(assemblyPath)) {
        ETH_WARN("clr: %s not found; C# mods will not load", assemblyPath.c_str());
        return false;
    }

    g_mscoree = LoadLibraryW(L"mscoree.dll");
    if (!g_mscoree) {
        ETH_ERROR("clr: mscoree.dll missing (%s)", ErrorCodeString(GetLastError()).c_str());
        return false;
    }

    auto bind = (CorBindToRuntimeExFn)GetProcAddress(g_mscoree, "CorBindToRuntimeEx");
    if (!bind) {
        ETH_ERROR("clr: CorBindToRuntimeEx not exported by mscoree.dll");
        return false;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);   // RPC_E_CHANGED_MODE is fine

    HRESULT hr = bind(L"v4.0.30319", L"wks", 0, kClsidClrRuntimeHost, kIidIclrRuntimeHost,
                      (LPVOID*)&g_runtime);
    if (FAILED(hr) || !g_runtime) {
        ETH_ERROR("clr: CorBindToRuntimeEx failed: %s", HresultText(hr).c_str());
        return false;
    }

    hr = g_runtime->Start();
    if (FAILED(hr)) {
        ETH_ERROR("clr: ICLRRuntimeHost::Start failed: %s", HresultText(hr).c_str());
        return false;
    }

    std::wstring arg = BuildArgument(csharpMods);
    DWORD ret = 0;
    hr = g_runtime->ExecuteInDefaultAppDomain(Utf8ToWide(assemblyPath).c_str(),
                                              L"EthhhSDK.Managed.Bootstrap",
                                              L"Initialize",
                                              arg.c_str(),
                                              &ret);
    if (FAILED(hr)) {
        ETH_ERROR("clr: Bootstrap.Initialize failed: %s", HresultText(hr).c_str());
        return false;
    }

    g_started = true;
    ETH_INFO("clr: runtime started, Bootstrap.Initialize returned %lu (callbacks %s)",
             (unsigned long)ret, CallbacksRegistered() ? "registered" : "NOT registered");
    return true;
}

void Shutdown()
{
    g_tickFn.store(nullptr);
    g_guiFn.store(nullptr);
    g_overlayFn.store(nullptr);
    // Intentionally no runtime->Stop(): the game is exiting and the CLR
    // shutdown path can hang inside a hosted process.
    g_started = false;
}

bool Running() { return g_started; }

bool CallbacksRegistered()
{
    return g_tickFn.load() != nullptr || g_guiFn.load() != nullptr || g_overlayFn.load() != nullptr;
}

void SetCallbacks(void* tick, void* gui, void* overlay)
{
    g_tickFn.store(tick);
    g_guiFn.store(gui);
    g_overlayFn.store(overlay);
    ETH_INFO("clr: managed callbacks set (tick=%p gui=%p overlay=%p)", tick, gui, overlay);
}

void Tick(int tickIndex)
{
    auto fn = (ManagedTickFn)g_tickFn.load();
    if (!fn) return;
    __try { fn(tickIndex); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ETH_ERROR("clr: managed tick callback raised an exception; clearing callbacks");
        g_tickFn.store(nullptr);
    }
}

void DrawGui()
{
    auto fn = (ManagedGuiFn)g_guiFn.load();
    if (!fn) return;
    __try { fn(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ETH_ERROR("clr: managed gui callback raised an exception; clearing callbacks");
        g_guiFn.store(nullptr);
    }
}

void OnOverlayChanged(bool open)
{
    auto fn = (ManagedOverlayFn)g_overlayFn.load();
    if (!fn) return;
    __try { fn(open ? 1 : 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ETH_ERROR("clr: managed overlay callback raised an exception; clearing callbacks");
        g_overlayFn.store(nullptr);
    }
}

void ReportManagedError(const char* modId, const char* message)
{
    if (!modId || !message) return;
    mods::ModManager::Get().ReportError(modId, message);
}

} // namespace eth::clr
