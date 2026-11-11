#include "hooks.h"
#include "game.h"
#include "util.h"
#include "log.h"
#include "input.h"
#include "overlay.h"
#include "mods.h"
#include "ethhhpatch.h"
#include "ethhhpatch_runtime.h"

#include <MinHook.h>

namespace eth::hooks {

namespace {

bool g_mhInit = false;
bool g_tickHooked = false;
bool g_d3dHooked = false;
bool g_wndHooked = false;
bool g_watcherStarted = false;

HWND    g_hookedHwnd = nullptr;
WNDPROC g_oldWndProc = nullptr;

using TickFn = void(__thiscall*)(void*, int);
TickFn g_tickOrig = nullptr;
using KeyboardTickFn = void(__thiscall*)(void*);
KeyboardTickFn g_keyboardTickOrig = nullptr;
bool g_keyboardTickHooked = false;

using EndSceneFn = HRESULT(WINAPI*)(IDirect3DDevice9*);
using ResetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using SetTransformFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
EndSceneFn g_endSceneOrig = nullptr;
ResetFn g_resetOrig = nullptr;
PresentFn g_presentOrig = nullptr;
SetTransformFn g_setTransformOrig = nullptr;

std::mutex g_tickMutex;
std::vector<TickCallback> g_tickCallbacks;

// __try must live in a function without C++ objects that need unwinding;
// the callback vector is owned by RunTickCallbacks() below.
bool CallTickCallback(const TickCallback& cb, int tickIndex)
{
    __try {
        cb(tickIndex);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void RunTickCallbacks(int tickIndex)
{
    std::vector<TickCallback> cbs;
    {
        std::lock_guard<std::mutex> lock(g_tickMutex);
        cbs = g_tickCallbacks;
    }
    for (auto& cb : cbs) {
        if (!CallTickCallback(cb, tickIndex)) {
            ETH_ERROR("tick callback raised an exception, disabling callbacks");
            std::lock_guard<std::mutex> lock(g_tickMutex);
            g_tickCallbacks.clear();
            break;
        }
    }
}

// ---------------------------------------------------------------------------
void __fastcall TickDetour(void* self, void* /*edx*/, int tickIndex)
{
    input::PumpKeys();
    input::PumpCapture();
    input::BeginNativeTick();
    __try { if (g_tickOrig) g_tickOrig(self, tickIndex); }
    __finally { input::EndNativeTick(); }

    patch::OnTick(game::TimeStep());

    overlay::OnGameTick();
    game::PlayerTracker::Get().Update();
    mods::ModManager::Get().Tick(tickIndex);

    RunTickCallbacks(tickIndex);
    // Camera targets/ownership are sampled only after tracker and all mod writes.
    patch::runtime::OnTickCompleted();
}

void __fastcall KeyboardTickDetour(void* self, void* /*edx*/)
{
    if (g_keyboardTickOrig) g_keyboardTickOrig(self);
    input::FilterNativeKeyboard(self);
}

// ---------------------------------------------------------------------------
HRESULT WINAPI EndSceneDetour(IDirect3DDevice9* device)
{
    uintptr_t ret = (uintptr_t)_ReturnAddress();
    uintptr_t movieLo = game::A(0x496C60), movieHi = game::A(0x496CC0);
    bool isMoviePath = ret >= movieLo && ret <= movieHi;

    __try {
        overlay::RenderFrame(device, isMoviePath);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        static bool once = false;
        if (!once) { once = true; ETH_ERROR("overlay render raised an exception"); }
        overlay::NotifyRenderException();
    }
    return g_endSceneOrig ? g_endSceneOrig(device) : D3D_OK;
}

HRESULT WINAPI ResetDetour(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp)
{
    const bool gameDevice = device && device == game::Device();
    D3DPRESENT_PARAMETERS requested{};
    bool captured = false;
    if (gameDevice && pp) {
        __try { memcpy(&requested, pp, sizeof(requested)); captured = true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { captured = false; }
    }
    if (gameDevice) overlay::OnDeviceReset(true, device);
    HRESULT hr = g_resetOrig ? g_resetOrig(device, pp) : D3D_OK;
    if (gameDevice) {
        patch::OnReset(captured ? &requested : nullptr, hr);
        patch::runtime::OnResetResult(captured ? &requested : nullptr, hr);
    }
    if (gameDevice && SUCCEEDED(hr))
        overlay::OnDeviceReset(false, device);
    return hr;
}

HRESULT WINAPI PresentDetour(IDirect3DDevice9* device, const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty)
{
    const uintptr_t ret = (uintptr_t)_ReturnAddress();
    if (device == game::Device() && ret == game::A(0x496CB6)) patch::runtime::NotifyMoviePresent();
    if (device == game::Device() && ret == game::A(0x44A485)) patch::runtime::BeforeGamePresent();
    const HRESULT hr = g_presentOrig ? g_presentOrig(device, source, dest, window, dirty) : D3DERR_INVALIDCALL;
    // Exact statically verified game/movie callers; no overlay FPS substitution.
    if (device == game::Device()) {
        __try { patch::OnPresent(device, hr, ret == game::A(0x44A485), ret == game::A(0x496CB6)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { /* Diagnostics never block presentation. */ }
    }
    return hr;
}

HRESULT WINAPI SetTransformDetour(IDirect3DDevice9* device, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix)
{
    const uintptr_t ret = (uintptr_t)_ReturnAddress();
    D3DMATRIX adjusted{};
    const D3DMATRIX* submitted = matrix;
    if (state == D3DTS_PROJECTION && device == game::Device() && patch::runtime::AdjustHudProjection(ret, matrix, adjusted))
        submitted = &adjusted;
    const HRESULT hr = g_setTransformOrig ? g_setTransformOrig(device, state, submitted) : D3DERR_INVALIDCALL;
    // Observe the exact main world projection submission, not later HUD or
    // ImGui state restoration. Preserve arguments, matrix bytes and HRESULT.
    if (SUCCEEDED(hr) && state == D3DTS_PROJECTION && ret == game::A(0x43F2DE) && device == game::Device()) {
        __try { patch::OnWorldProjection(matrix); }
        __except (EXCEPTION_EXECUTE_HANDLER) { /* Observation cannot block rendering. */ }
    }
    return hr;
}

// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProcDetour(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ACTIVATEAPP || msg == WM_KILLFOCUS)
        input::PumpCapture();
    if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && !wp)) input::ResetTransient();
    if (msg == WM_INPUT) {
        input::OnRawInput(hwnd, lp);
    }

    if (msg == WM_KEYDOWN && (int)wp == overlay::ToggleKey() && !(lp & 0x40000000)) {
        overlay::Toggle();
        return 0;
    }
    if (msg == WM_KEYUP && (int)wp == overlay::ToggleKey()) {
        return 0;
    }

    bool swallow = false;
    __try {
        swallow = overlay::OnWndProc(hwnd, msg, wp, lp);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        swallow = false;
    }

    if (swallow)
        return 0;

    return g_oldWndProc ? CallWindowProcA(g_oldWndProc, hwnd, msg, wp, lp)
                        : DefWindowProcA(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
void TryHookWindow()
{
    HWND hwnd = (HWND)game::GameHwnd();
    if (!hwnd || hwnd == g_hookedHwnd)
        return;

    g_oldWndProc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcDetour);
    if (!g_oldWndProc) {
        ETH_WARN("SetWindowLongPtr failed (%s)", ErrorCodeString(GetLastError()).c_str());
        return;
    }
    g_hookedHwnd = hwnd;
    g_wndHooked = true;
    input::SetWindow(hwnd);
    ETH_INFO("window hook installed (hwnd=%p)", hwnd);
}

void TryHookD3D()
{
    IDirect3DDevice9* device = (IDirect3DDevice9*)game::Device();
    if (!device || g_d3dHooked)
        return;

    void** vtbl = *(void***)device;
    if (!vtbl) return;

    void* targets[] = { vtbl[42], vtbl[16], vtbl[17], vtbl[44] };
    void* detours[] = { (void*)&EndSceneDetour, (void*)&ResetDetour, (void*)&PresentDetour, (void*)&SetTransformDetour };
    void** originals[] = { (void**)&g_endSceneOrig, (void**)&g_resetOrig, (void**)&g_presentOrig, (void**)&g_setTransformOrig };
    constexpr int hookCount = sizeof(targets) / sizeof(targets[0]);
    int created = 0;
    for (; created < hookCount; ++created) {
        const MH_STATUS status = MH_CreateHook(targets[created], detours[created], originals[created]);
        if (status != MH_OK) {
            ETH_WARN("D3D9 hook creation failed (%d): %s", created, MH_StatusToString(status));
            break;
        }
    }
    if (created == hookCount) {
        MH_STATUS status = MH_OK;
        for (int i = 0; i < hookCount; ++i) {
            status = MH_QueueEnableHook(targets[i]);
            if (status != MH_OK) break;
        }
        if (status == MH_OK) status = MH_ApplyQueued();
        if (status == MH_OK) {
            g_d3dHooked = true;
            ETH_INFO("D3D9 hooked: EndScene/Reset/Present/SetTransform; EthhhPatch observation only (device=%p)", device);
            return;
        }
        ETH_WARN("D3D9 hook enable failed: %s", MH_StatusToString(status));
    }
    for (int i = 0; i < created; ++i) { MH_DisableHook(targets[i]); MH_RemoveHook(targets[i]); }
    g_endSceneOrig = nullptr; g_resetOrig = nullptr; g_presentOrig = nullptr; g_setTransformOrig = nullptr;
}

DWORD WINAPI WatcherThread(LPVOID)
{
    ETH_INFO("watcher: waiting for game window / D3D9 device");

    bool loggedCamera = false;
    for (int i = 0; i < 900; ++i) {
        if (!g_wndHooked) __try { TryHookWindow(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (!g_d3dHooked) __try { TryHookD3D(); }   __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (!loggedCamera) {
            game::CameraView cam;
            if (game::ReadCamera(cam)) {
                loggedCamera = true;
                ETH_INFO("camera ready: pos=(%.2f, %.2f, %.2f) fov=%.3f rad", cam.x, cam.y, cam.z, cam.fov);
            }
        }

        if (g_wndHooked && g_d3dHooked) {
            ETH_INFO("watcher: all hooks installed");
            return 0;
        }
        Sleep(100);
    }
    ETH_WARN("watcher: timed out (wnd=%d d3d=%d)", (int)g_wndHooked, (int)g_d3dHooked);
    return 0;
}

} // namespace

bool Init()
{
    if (g_mhInit) return true;
    MH_STATUS s = MH_Initialize();
    if (s != MH_OK) {
        ETH_ERROR("MinHook init failed: %s", MH_StatusToString(s));
        return false;
    }
    g_mhInit = true;
    ETH_INFO("MinHook initialized");
    return true;
}

bool InstallTickHook()
{
    if (g_tickHooked) return true;
    if (!g_mhInit && !Init()) return false;

    void* target = (void*)game::A(game::off::Fn_Tick);
    MH_STATUS s = MH_CreateHook(target, (void*)&TickDetour, (void**)&g_tickOrig);
    if (s != MH_OK) {
        ETH_ERROR("Fn_Tick hook failed: %s", MH_StatusToString(s));
        return false;
    }
    s = MH_EnableHook(target);
    if (s != MH_OK) {
        ETH_ERROR("Fn_Tick enable failed: %s", MH_StatusToString(s));
        return false;
    }
    g_tickHooked = true;
    ETH_INFO("Fn_Tick hooked at %p", target);
    return true;
}

void StartGameWatcher()
{
    if (g_watcherStarted) return;
    g_watcherStarted = true;
    HANDLE h = CreateThread(nullptr, 0, WatcherThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

bool InstallMovementInputHook()
{
    if (g_keyboardTickHooked) return true;
    if (!g_tickHooked || !g_mhInit) {
        ETH_WARN("HL keyboard hook unavailable: tick=%d minhook=%d", (int)g_tickHooked, (int)g_mhInit);
        return false;
    }
    char exe[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        ETH_WARN("HL keyboard hook refused: executable path unavailable (%lu)", (unsigned long)length);
        return false;
    }
    constexpr const char* kSupportedExeSha256 = "49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0";
    const std::string sha = Sha256FileHex(exe);
    if (sha != kSupportedExeSha256) {
        ETH_WARN("HL keyboard hook refused: %s SHA-256 %s (expected %s)", exe,
                 sha.empty() ? "<unreadable>" : sha.c_str(), kSupportedExeSha256);
        return false;
    }
    // Supported image: keyboard.cfm's DirectInput keyboard poll. Not a generic
    // COM/device hook; refuse any foreign patch before installing the detour.
    const uintptr_t target = game::A(0x4427A0);
    uint8_t bytes[13]{};
    uint8_t expected[13] = { 0xa1, 0x14, 0x92, 0x52, 0x00, 0x53, 0x56, 0x8b, 0xf1, 0x80, 0x78, 0x0d, 0x01 };
    const uint32_t global = (uint32_t)game::A(0x529214);
    memcpy(expected + 1, &global, sizeof(global));
    if (!SafeReadBytes(target, bytes, sizeof(bytes)) || memcmp(bytes, expected, sizeof(bytes)) != 0) {
        ETH_WARN("HL keyboard poll fingerprint mismatch; native movement keys not owned"); return false;
    }
    MH_STATUS status = MH_CreateHook((void*)target, (void*)&KeyboardTickDetour, (void**)&g_keyboardTickOrig);
    if (status != MH_OK) {
        ETH_WARN("HL keyboard hook create failed at %p: %s", (void*)target, MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook((void*)target);
    if (status != MH_OK) {
        ETH_WARN("HL keyboard hook enable failed at %p: %s", (void*)target, MH_StatusToString(status));
        MH_RemoveHook((void*)target); g_keyboardTickOrig = nullptr; return false;
    }
    g_keyboardTickHooked = true;
    ETH_INFO("Native keyboard bridge installed at %p (mouse attacks / opt-in one-tick HL WASD lease)", (void*)target);
    return true;
}

void Shutdown()
{
    input::LeaseMovementKeys(0, false);
    input::EndNativeTick();
    patch::runtime::Shutdown();
    if (g_hookedHwnd && g_oldWndProc) {
        SetWindowLongPtrA(g_hookedHwnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
        g_oldWndProc = nullptr;
        g_hookedHwnd = nullptr;
        g_wndHooked = false;
    }
    if (g_mhInit) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_mhInit = false;
    }
    g_tickHooked = g_d3dHooked = false;
    g_keyboardTickHooked = false; g_keyboardTickOrig = nullptr;
    ClearTickCallbacks();
}

bool TickHooked()   { return g_tickHooked; }
bool D3DHooked()    { return g_d3dHooked; }
bool WndProcHooked(){ return g_wndHooked; }

void AddTickCallback(TickCallback cb)
{
    std::lock_guard<std::mutex> lock(g_tickMutex);
    g_tickCallbacks.push_back(std::move(cb));
}

void ClearTickCallbacks()
{
    std::lock_guard<std::mutex> lock(g_tickMutex);
    g_tickCallbacks.clear();
}

} // namespace eth::hooks
