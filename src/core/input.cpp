#include "input.h"
#include "util.h"
#include "log.h"
#include "config.h"

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
std::array<bool, 256> g_keyNow{};
std::array<bool, 256> g_keyPrev{};
float g_sensitivity = 1.0f;
LONG g_absoluteX = 0, g_absoluteY = 0;
bool g_haveAbsolute = false;

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
    SetWindow(hwnd);
}

void Shutdown()
{
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
    PumpCapture();
    ConsumeMouseDelta();
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
    bool want = g_captureRequested && !g_overlayOpen && Focused();
    if (want != g_capture) {
        g_capture = want;
        if (want) {
            if (!g_cursorHidden) { ShowCursor(FALSE); g_cursorHidden = true; }
        } else {
            ClipCursor(nullptr);
            if (g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; }
            ConsumeMouseDelta();
        }
    }
    if (g_capture) ApplyCaptureClip();
}

void ResetTransient()
{
    std::lock_guard<std::mutex> lock(g_mouseMutex);
    g_mouseDX = 0.0f;
    g_mouseDY = 0.0f;
}

bool KeyDown(int vk)
{
    if (vk <= 0 || vk >= 256) return false;
    return Focused() && (GetAsyncKeyState(vk) & 0x8000) != 0;
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
