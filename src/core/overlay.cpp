// ===========================================================================
//  In-game overlay - Dear ImGui on the game's own D3D9 device.
//
//  Installed lazily from the EndScene detour (hooks.cpp) once the game has a
//  window and a device. Provides:
//    * F9 toggle (key configurable)
//    * pause + camera freeze while the menu is open
//    * custom-chrome manager window (sidebar + pages)
//    * always-on EthhhSDK watermark
//    * WndProc routing so the game ignores input while the menu is open
// ===========================================================================
#include "overlay.h"
#include "imgui_ext.h"
#include "ui_pages.h"
#include "mods.h"
#include "config.h"
#include "game.h"
#include "input.h"
#include "util.h"
#include "log.h"

#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "liquid_glass.h"
#include "ui_motion_policy.h"
#include "ui_audio.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace eth::overlay {

namespace {

bool             g_visible = false;
bool             g_backendReady = false;
bool             g_fontsLoaded = false;
float            g_fontScale = 0.0f;
HWND             g_hwnd = nullptr;
IDirect3DDevice9* g_device = nullptr;
int              g_toggleKey = VK_F9;

bool  g_paused = false;
float g_savedTimeScale = 1.0f;
float g_savedWorld[16] = {};
bool  g_savedWorldValid = false;

int  g_renderErrors = 0;
bool g_renderDisabled = false;
bool g_loggedRenderException = false;
int  g_focusFrames = 0;
ui::motion::Transition g_menuTransition;

// ---------------------------------------------------------------------------
bool InitBackends(IDirect3DDevice9* device, HWND hwnd)
{
    if (g_backendReady && g_device == device && g_hwnd == hwnd)
        return true;

    if (g_backendReady) {
        ui::ShutdownGlass();
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        g_backendReady = false;
    }

    ui::EnsureContext();

    if (!ImGui_ImplWin32_Init(hwnd)) {
        ETH_ERROR("overlay: ImGui_ImplWin32_Init failed");
        return false;
    }
    if (!ImGui_ImplDX9_Init(device)) {
        ImGui_ImplWin32_Shutdown();
        ETH_ERROR("overlay: ImGui_ImplDX9_Init failed");
        return false;
    }

    const float dpi = GetDpiScale(hwnd);
    float fontScale = Config::Get().GetFloat("core", "font_scale", 1.0f);
    if (fontScale < 0.8f || fontScale > 1.6f) fontScale = 1.0f;
    const float scale = dpi * fontScale;
    if (!g_fontsLoaded || fabsf(scale - g_fontScale) > 0.01f) {
        ui::LoadFonts(scale);
        g_fontsLoaded = true;
        g_fontScale = scale;
    } else {
        ui::ApplyTheme();
    }

    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;

    g_hwnd = hwnd;
    g_device = device;
    ui::InitGlass(device);
    g_backendReady = true;
    ETH_INFO("overlay: ImGui backends ready (hwnd=%p device=%p scale %.2f)", hwnd, device, scale);
    return true;
}

// ---------------------------------------------------------------------------
void ApplyPause(bool on)
{
    bool shouldPause = on && Config::Get().GetBool("core", "pause_on_overlay", true);
    if (shouldPause) {
        if (g_paused) return;
        g_savedTimeScale = game::TimeScale();

        game::CameraView cam;
        g_savedWorldValid = game::ReadCamera(cam);
        if (g_savedWorldValid)
            memcpy(g_savedWorld, cam.world, sizeof(g_savedWorld));

        game::SetTimeScale(0.0f);
        g_paused = true;
        ETH_INFO("overlay: game paused (time scale %.2f, camera %s)",
                 g_savedTimeScale, g_savedWorldValid ? "frozen" : "unavailable");
    } else if (g_paused) {
        g_paused = false;
        game::SetTimeScale(g_savedTimeScale);
        g_savedWorldValid = false;
        ETH_INFO("overlay: game resumed (time scale %.2f)", g_savedTimeScale);
    }
}

void SyncPauseState()
{
    bool want = g_visible && Config::Get().GetBool("core", "pause_on_overlay", true);
    if (want != g_paused)
        ApplyPause(want);
}

// ---------------------------------------------------------------------------
void DrawManagerWindow()
{
    ImGuiIO& io = ImGui::GetIO();
    const float s = ui::Scale();
    if (io.DisplaySize.x < 320.0f || io.DisplaySize.y < 240.0f)
        return;

    float margin = 22.0f * s;
    ImVec2 want(1200.0f * s, 780.0f * s);
    ImVec2 size(std::min(want.x, io.DisplaySize.x - margin * 2.0f),
                std::min(want.y, io.DisplaySize.y - margin * 2.0f));
    if (size.x < 520.0f * s) size.x = io.DisplaySize.x;
    if (size.y < 320.0f * s) size.y = io.DisplaySize.y;

    const float alpha=g_menuTransition.Value();
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - size.x) * 0.5f,
                                   (io.DisplaySize.y - size.y) * 0.5f+(1-alpha)*16*s),ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ui::SetManagerOpacity(alpha);
    if (g_focusFrames > 0) {
        ImGui::SetNextWindowFocus();
        --g_focusFrames;
    }

    bool open = true;
    bool visible = ui::BeginChromeWindow("##ethhh_overlay", "EthhhSDK",
                                         "I-Ninja / In-game mod manager",
                                         &open, size,
                                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                          (!g_visible ? ImGuiWindowFlags_NoInputs : 0));
    if (!visible) {
        if (!open)
            SetVisible(false);
        return;
    }

    ImGui::BeginDisabled(!g_visible);
    ui::pages::DrawManagerLayout(false);
    ImGui::EndDisabled();

    ui::EndChromeWindow();

    if (!open)
        SetVisible(false);
}

// ---------------------------------------------------------------------------
bool IsGameInputMessage(UINT msg)
{
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR: case WM_UNICHAR:
    case WM_MOUSEMOVE: case WM_NCMOUSEMOVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

} // namespace

// ---------------------------------------------------------------------------
bool OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (!g_backendReady || !ImGui::GetCurrentContext())
        return false;

    // Let ImGui see the message first (mouse pos, keys, chars, wheel).
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);

    if (!g_visible)
        return false;

    // Escape closes the menu unless the user is typing in a field.
    if (msg == WM_KEYDOWN && (int)wp == VK_ESCAPE && !ImGui::GetIO().WantTextInput) {
        SetVisible(false);
        return true;
    }

    if (Config::Get().GetBool("core", "block_game_input", true) && IsGameInputMessage(msg))
        return true;

    return false;
}

// ---------------------------------------------------------------------------
void RenderFrame(IDirect3DDevice9* device, bool moviePath)
{
    if (!device || g_renderDisabled)
        return;

    // Do not draw over Bink movies unless the player explicitly opened the menu.
    if (moviePath && !g_visible)
        return;

    HWND hwnd = (HWND)game::GameHwnd();
    if (!hwnd)
        return;

    if (!InitBackends(device, hwnd))
        return;
    if (!ImGui::GetCurrentContext())
        return;

    input::PumpCapture();
    SyncPauseState();

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = g_visible;

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::BeginGlassFrame();

    if (io.DisplaySize.x < 16.0f || io.DisplaySize.y < 16.0f) {
        ImGui::EndFrame();
        return;
    }

    g_menuTransition.Advance(io.DeltaTime,Config::Get().GetBool("theme","reduce_motion",false));
    if (g_menuTransition.Active())
        DrawManagerWindow();

    ui::pages::DrawStartupWatermark();

    ImGui::Render();
    ui::RenderGlassDrawData(device,ImGui::GetDrawData());

    if (g_visible)
        SetCursor(LoadCursorA(nullptr, IDC_ARROW));
}

// ---------------------------------------------------------------------------
void OnGameTick()
{
    if (!g_visible)
        return;

    SyncPauseState();

    // Fn_Tick runs after the original tick, so re-writing the saved matrix here
    // keeps the camera exactly where it was when the menu opened.
    if (g_paused && g_savedWorldValid)
        game::SetCameraWorld(g_savedWorld);
}

// ---------------------------------------------------------------------------
void OnDeviceReset(bool before, IDirect3DDevice9* device)
{
    if (!g_backendReady || device == nullptr)
        return;
    if (before) {
        ui::InvalidateGlass();
        ImGui_ImplDX9_InvalidateDeviceObjects();
    } else {
        ImGui_ImplDX9_CreateDeviceObjects();
        ui::RecreateGlass();
    }
}

// ---------------------------------------------------------------------------
void Toggle()
{
    SetVisible(!g_visible);
}

void SetVisible(bool visible)
{
    if (visible == g_visible)
        return;

    g_visible = visible;
    g_menuTransition.Target(visible);
    ui::PlayUiSound(ui::Sound::Click);

    if (visible) {
        g_focusFrames = 1;
        input::ConsumeMouseDelta();
        input::OnOverlayToggled(true);
        ApplyPause(true);
    } else {
        ApplyPause(false);
        input::OnOverlayToggled(false);
        input::ConsumeMouseDelta();
        g_focusFrames = 0;
    }

    mods::ModManager::Get().OnOverlayChanged(visible);
    ETH_INFO("overlay: %s", visible ? "opened" : "closed");
}

bool IsVisible() { return g_visible; }
int  ToggleKey() { return g_toggleKey; }

bool DeferTimeScale(float value)
{
    if (!g_paused) return false;
    g_savedTimeScale = value;
    return true;
}

void SetToggleKey(int vk)
{
    if (vk > 0 && vk < 256)
        g_toggleKey = vk;
}

// ---------------------------------------------------------------------------
void NotifyRenderException()
{
    ++g_renderErrors;
    if (!g_loggedRenderException) {
        g_loggedRenderException = true;
        ETH_ERROR("overlay: render frame raised an exception (attempt %d)", g_renderErrors);
    }
    if (g_renderErrors >= 3) {
        g_renderDisabled = true;
        ETH_ERROR("overlay: disabling overlay rendering after %d exceptions", g_renderErrors);
    }
}

void Shutdown()
{
    if (g_visible)
        SetVisible(false);

    if (g_backendReady) {
        ui::ShutdownGlass();
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        g_backendReady = false;
    }
    g_device = nullptr;
    g_hwnd = nullptr;
    g_fontsLoaded = false;
    g_fontScale = 0.0f;
    g_menuTransition.Snap(false);
    ui::ShutdownUiAudio();
}

} // namespace eth::overlay
