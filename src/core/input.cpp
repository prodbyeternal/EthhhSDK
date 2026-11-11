#include "input.h"
#include "util.h"
#include "log.h"
#include "config.h"
#include "game.h"
#include "hooks.h"
#include "movement_input_policy.h"
#include "mouse_attack_policy.h"

namespace eth::input {

namespace {
HWND  g_hwnd = nullptr;
bool  g_rawRegistered = false;
bool  g_capture = false;
bool  g_captureRequested = false;
bool  g_overlayOpen = false;
bool  g_cursorHidden = false;
std::mutex        g_mouseMutex;
float             g_mouseDX = 0.0f;
float             g_mouseDY = 0.0f;
float             g_mouseWheel = 0.0f;
std::array<bool, 256> g_keyNow{};
std::array<bool, 256> g_keyPrev{};
float g_sensitivity = 1.0f;
LONG g_absoluteX = 0, g_absoluteY = 0;
bool g_haveAbsolute = false;
std::mutex g_movementMutex;
MovementKeysLease g_movementLease;
bool g_nativeMovementKeysOwned = false; // game-tick thread only
std::atomic<bool> g_movementFilterFailed{false};
std::atomic<bool> g_mouseAttacksEnabled{true};
bool g_attackHookAttempted = false, g_attackHookReady = false;
MouseAttackPolicy g_mouseAttacks;
game::StratIdentity g_attackOwner;
bool g_nativeTickActive = false;
uintptr_t g_attackKeyboard = 0;

bool Focused()
{
    return g_hwnd && GetForegroundWindow() == g_hwnd;
}

void ApplyCaptureClip()
{
    if (!g_capture || !g_hwnd || !IsWindow(g_hwnd)) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    POINT tl{ rc.left, rc.top }, br{ rc.right, rc.bottom };
    ClientToScreen(g_hwnd, &tl);
    ClientToScreen(g_hwnd, &br);
    RECT screen{ tl.x, tl.y, br.x, br.y };
    ClipCursor(&screen);
}

} // namespace

void Init(HWND hwnd)
{
    g_mouseAttacksEnabled.store(Config::Get().GetBool("ethhhpatch", "mouse_attacks", true));
    SetWindow(hwnd);
}

void Shutdown()
{
    g_mouseAttacks.Reset();
    LeaseMovementKeys(0, false);
    EndNativeTick();
    SetCapture(false);
    RAWINPUTDEVICE rid{ 0x01, 0x02, RIDEV_REMOVE, nullptr };
    if (g_rawRegistered) RegisterRawInputDevices(&rid, 1, sizeof(rid));
    g_rawRegistered = false;
}

void SetWindow(HWND hwnd)
{
    if (hwnd == g_hwnd && g_rawRegistered) return;
    g_hwnd = hwnd;
    if (!hwnd) return;

    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02; // mouse
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    g_rawRegistered = RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
    if (!g_rawRegistered)
        ETH_WARN("raw mouse registration failed (%s)", ErrorCodeString(GetLastError()).c_str());
    else
        ETH_INFO("raw mouse registered");
}

HWND Window() { return g_hwnd; }

void OnRawInput(HWND, LPARAM lParam)
{
    if (!g_rawRegistered || !Focused() || g_overlayOpen) return;
    UINT size = 0;
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0)
        return;
    if (size == 0 || size > 512) return;

    BYTE buffer[512];
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) != size)
        return;
    auto* raw = (RAWINPUT*)buffer;
    if (raw->header.dwType != RIM_TYPEMOUSE) return;

    const RAWMOUSE& m = raw->data.mouse;
    if (m.usButtonFlags & RI_MOUSE_WHEEL) {
        std::lock_guard<std::mutex> lock(g_mouseMutex);
        g_mouseWheel += (float)(short)m.usButtonData / (float)WHEEL_DELTA;
    }
    if (m.usFlags & MOUSE_MOVE_ABSOLUTE) {
        bool virtualDesktop = (m.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
        LONG x = MulDiv(m.lLastX, GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN), 65535);
        LONG y = MulDiv(m.lLastY, GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN), 65535);
        LONG dx = g_haveAbsolute ? x - g_absoluteX : 0;
        LONG dy = g_haveAbsolute ? y - g_absoluteY : 0;
        g_absoluteX = x;
        g_absoluteY = y;
        g_haveAbsolute = true;
        if (abs(dx) < 5000 && abs(dy) < 5000) {
            std::lock_guard<std::mutex> lock(g_mouseMutex);
            g_mouseDX += (float)dx;
            g_mouseDY += (float)dy;
        }
    } else {
        g_haveAbsolute = false;
        std::lock_guard<std::mutex> lock(g_mouseMutex);
        g_mouseDX += (float)m.lLastX;
        g_mouseDY += (float)m.lLastY;
    }
}

void OnOverlayToggled(bool overlayOpen)
{
    g_overlayOpen = overlayOpen;
    g_mouseAttacks.Reset();
    PumpCapture();
    ConsumeMouseDelta();
    ConsumeMouseWheel();
}

void PumpKeys()
{
    bool focused = Focused();
    for (int vk = 1; vk < 256; ++vk) {
        bool down = focused && (GetAsyncKeyState(vk) & 0x8000) != 0;
        g_keyPrev[vk] = g_keyNow[vk];
        g_keyNow[vk] = down;
    }
}

void PumpCapture()
{
    if (!Focused()) g_mouseAttacks.Reset();
    bool want = g_captureRequested && !g_overlayOpen && Focused();
    if (want != g_capture) {
        g_capture = want;
        if (want) {
            if (!g_cursorHidden) { ShowCursor(FALSE); g_cursorHidden = true; }
        } else {
            ClipCursor(nullptr);
            if (g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; }
            ConsumeMouseDelta();
            ConsumeMouseWheel();
        }
    }
    if (g_capture) ApplyCaptureClip();
}

void ResetTransient()
{
    g_mouseAttacks.Reset();
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_mouseDX = 0.0f;
    g_mouseDY = 0.0f;
    g_mouseWheel = 0.0f;
}

bool KeyDown(int vk)
{
    if (vk <= 0 || vk >= 256) return false;
    return Focused() && (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool LeaseMovementKeys(uintptr_t player, bool enabled)
{
    {
        std::lock_guard<std::mutex> lock(g_movementMutex);
        g_movementLease.Reset(); // failed/replaced requests revoke old consent
    }
    if (!enabled) {
        return true;
    }
    auto& tracker = game::PlayerTracker::Get();
    game::StratIdentity identity;
    const char* refusal = nullptr;
    if (g_movementFilterFailed.load())
        refusal = "native keyboard filter failed earlier; restart required";
    else if (tracker.Mode() != game::PlayerMode::Manual)
        refusal = "player is not manually pinned";
    else if (!tracker.Valid())
        refusal = "pinned player is no longer valid";
    else if (tracker.Addr() != player)
        refusal = "lease player address does not match the pin";
    else if (!game::ReadStratIdentity(player, identity))
        refusal = "exact-Ninja identity could not be re-read";
    else if (identity.role != game::StratRole::Ninja)
        refusal = "pinned strat is not the exact Ninja class";
    else if (!identity.selfHandle)
        refusal = "pinned strat has no live handle";
    else if (!hooks::InstallMovementInputHook())
        refusal = "native keyboard hook unavailable";
    if (refusal) {
        ETH_WARN("HL WASD lease refused: %s (player %08X)", refusal, (unsigned)player);
        return false;
    }
    std::lock_guard<std::mutex> lock(g_movementMutex);
    return g_movementLease.Request(identity.addr, identity.pClass, identity.selfHandle);
}

namespace {
void SampleAttackOwner()
{
    const auto previous = g_attackOwner;
    g_attackOwner = {};
    const float step = game::TimeStep();
    if (!g_mouseAttacksEnabled.load() || !g_attackHookReady || !Focused() || g_overlayOpen ||
        KeyDown(VK_F9) || !std::isfinite(step) || step <= 0 || step > 0.1f) {
        g_mouseAttacks.Reset(); return;
    }
    auto& tracker = game::PlayerTracker::Get();
    game::StratIdentity identity;
    // Addr comes from the tracker's complete-list selection/manual pin. Recheck
    // exact class/handle/live membership, not its allocating inspector snapshot.
    if (tracker.Mode() != game::PlayerMode::Learning && game::ReadStratIdentity(tracker.Addr(), identity) &&
        identity.role == game::StratRole::Ninja && identity.selfHandle) g_attackOwner = identity;
    if (previous.addr != g_attackOwner.addr || previous.pClass != g_attackOwner.pClass ||
        previous.selfHandle != g_attackOwner.selfHandle || !g_attackOwner.addr) g_mouseAttacks.Reset();
}
}

void BeginNativeTick()
{
    g_nativeTickActive = true;
    if (g_mouseAttacksEnabled.load() && !g_attackHookAttempted) {
        g_attackHookAttempted = true;
        g_attackHookReady = hooks::InstallMovementInputHook();
    }
    if (!Focused() || g_overlayOpen || !g_mouseAttacksEnabled.load()) g_mouseAttacks.Reset();
    SampleAttackOwner(); // once per native tick, never per rendered frame
    g_nativeMovementKeysOwned = false;
    std::lock_guard<std::mutex> lock(g_movementMutex);
    if (!g_movementLease.Pending()) return; // inactive path does no identity read
    auto& tracker = game::PlayerTracker::Get();
    game::StratIdentity identity;
    const bool valid = tracker.Valid() && game::ReadStratIdentity(tracker.Addr(), identity);
    g_nativeMovementKeysOwned = g_movementLease.Consume(tracker.Mode() == game::PlayerMode::Manual,
        valid, valid && identity.role == game::StratRole::Ninja, Focused(), g_overlayOpen,
        KeyDown(VK_CONTROL) || KeyDown(VK_F2) || KeyDown(VK_F3), game::TimeStep(),
        identity.addr, identity.pClass, identity.selfHandle);
}

void EndNativeTick() { g_nativeMovementKeysOwned = false; g_nativeTickActive = false; }

bool NativeMovementKeysOwned() { return g_nativeMovementKeysOwned; }

namespace {
void FailMovementFilter(const char* reason, const void* keyboard)
{
    if (!g_movementFilterFailed.exchange(true))
        ETH_WARN("Native keyboard bridge disabled: %s (keyboard %p)", reason, keyboard);
}
}

void FilterNativeKeyboard(void* keyboard)
{
    const float step = game::TimeStep();
    const bool attacks = g_mouseAttacksEnabled.load() && g_attackHookReady &&
        g_nativeTickActive && g_attackOwner.addr && Focused() && !g_overlayOpen &&
        !KeyDown(VK_F9) && std::isfinite(step) && step > 0 && step <= 0.1f;
    const bool movement = g_nativeMovementKeysOwned && Focused() && !g_overlayOpen && !KeyDown(VK_CONTROL);
    if (!attacks) g_mouseAttacks.Reset();
    if ((!attacks && !movement) || g_movementFilterFailed.load()) return;
    uintptr_t app = 0, input = 0;
    if (!SafeRead(game::A(0x50FFA4), app) || !app || !SafeRead(app + 0x2c, input) ||
        !input || input > UINTPTR_MAX - 8 || (uintptr_t)keyboard != input + 8) {
        FailMovementFilter("keyboard owner path does not match the verified poll", keyboard); return;
    }
    // The verified poll has just written this exact 512-byte down/edge buffer.
    // Refuse non-writable/unbounded memory, without page-protection changes.
    MEMORY_BASIC_INFORMATION mbi{};
    const uintptr_t addr = (uintptr_t)keyboard;
    if (!addr || !VirtualQuery(keyboard, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
        FailMovementFilter("keyboard buffer is not committed/readable", keyboard); return;
    }
    const uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    const DWORD protection = mbi.Protect & 0xff;
    if (addr >= end || 512 > end - addr ||
        (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
         protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)) {
        FailMovementFilter("keyboard buffer is not a bounded writable region", keyboard); return;
    }
    bool raised = false;
    AttackBindingTable table;
    // Use the LIVE keyboard binding for these actions, including custom keys.
    // Input+0x240 is the action-pair table, stride 2*0x1C (0x441D9C).
    uintptr_t pressureKeyboard = 0;
    uint32_t cls = 0, handle = 0;
    uint8_t keyboardSuppressed = 1;
    const bool bindings = attacks && game::PlayerTracker::Get().Addr() == g_attackOwner.addr &&
        SafeRead(g_attackOwner.addr + game::off::StratClass, cls) && cls == g_attackOwner.pClass &&
        SafeRead(g_attackOwner.addr + game::off::StratSelfHandle, handle) && handle == g_attackOwner.selfHandle &&
        SafeRead(game::A(0x51C470), pressureKeyboard) && pressureKeyboard == addr &&
        SafeRead(input + 0x37cc, keyboardSuppressed) && !keyboardSuppressed &&
        SafeReadBytes(input + 0x240, table.data(), sizeof(table)) && AttackBindingsSupported(table);
    if (bindings && g_attackKeyboard != addr) { g_mouseAttacks.Reset(); g_attackKeyboard = addr; }
    __try {
        FilterMovementKeys((uint8_t*)keyboard, 512, movement);
        g_mouseAttacks.Apply((uint8_t*)keyboard, 512, table[kSlashAction * 2],
            table[kSpinAction * 2], bindings, MouseDown(0), MouseDown(1));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { raised = true; }
    if (raised) FailMovementFilter("keyboard buffer write raised an exception", keyboard);
}

bool KeyPressed(int vk)
{
    if (vk <= 0 || vk >= 256) return false;
    return Focused() && g_keyNow[vk] && !g_keyPrev[vk];
}

bool MouseDown(int button)
{
    static const int keys[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    return button >= 0 && button < 5 && KeyDown(keys[button]);
}

bool MouseAttacksEnabled() { return g_mouseAttacksEnabled.load(); }
void SetMouseAttacksEnabled(bool enabled) { g_mouseAttacksEnabled.store(enabled); g_mouseAttacks.Reset(); }

void GetMouseDelta(float& dx, float& dy)
{
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    dx = g_mouseDX;
    dy = g_mouseDY;
}

void ConsumeMouseDelta()
{
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_mouseDX = 0.0f;
    g_mouseDY = 0.0f;
    g_haveAbsolute = false;
}

void GetMouseWheel(float& notches)
{
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    notches = g_mouseWheel;
}

void ConsumeMouseWheel()
{
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_mouseWheel = 0.0f;
}

bool SetCapture(bool on)
{
    if (on && (!g_hwnd || !IsWindow(g_hwnd))) return false;
    g_captureRequested = on;
    PumpCapture();
    return g_capture;
}

bool CaptureActive() { return g_capture; }
bool OverlayOpen()   { return g_overlayOpen; }

float SensitivityScale() { return g_sensitivity; }
void SetSensitivityScale(float value) { g_sensitivity = std::max(0.01f, value); }

const char* KeyName(int vk)
{
    static char buf[32];
    switch (vk) {
    case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3";
    case VK_F4: return "F4"; case VK_F5: return "F5"; case VK_F6: return "F6";
    case VK_F7: return "F7"; case VK_F8: return "F8"; case VK_F9: return "F9";
    case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
    case VK_SPACE: return "Space"; case VK_SHIFT: return "Shift"; case VK_CONTROL: return "Ctrl";
    case VK_ESCAPE: return "Esc"; case VK_TAB: return "Tab"; case VK_RETURN: return "Enter";
    case VK_LEFT: return "Left"; case VK_RIGHT: return "Right"; case VK_UP: return "Up"; case VK_DOWN: return "Down";
    default: sprintf_s(buf, "0x%02X", vk); return buf;
    }
}

} // namespace eth::input
