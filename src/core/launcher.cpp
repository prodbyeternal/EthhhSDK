// ===========================================================================
//  Pre-game Mod Manager (launcher)
//
//  Runs on the game's main thread BEFORE the game entry point: creates its own
//  borderless window + D3D9 device, draws the custom-chrome ImGui manager and
//  waits for Start (Enter / button) or Exit. When it returns, the caller
//  restores the game entry point and the game boots normally.
// ===========================================================================
#include "launcher.h"
#include "imgui_ext.h"
#include "ui_pages.h"
#include "mods.h"
#include "config.h"
#include "util.h"
#include "log.h"
#include "input.h"
#include "overlay.h"
#include "hooks.h"
#include "game.h"

#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "liquid_glass.h"
#include "ui_audio.h"
#include "ui_motion_policy.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace eth::launcher {

namespace {

HWND  g_hwnd = nullptr;
bool  g_running = false;
bool  g_started = false;
bool  g_dragging = false;
POINT g_dragCursor = {};
POINT g_dragWindow = {};
float g_scale = 1.0f;

void RequestStart()
{
    ui::PlayUiSound(ui::Sound::Start);
    g_started = true;
    g_running = false;
}

void RequestExit()
{
    g_started = false;
    g_running = false;
}

void ApplyFrameRequests(bool startRequested, bool exitRequested)
{
    if (exitRequested) RequestExit();
    else if (startRequested) RequestStart();
}

LRESULT CALLBACK LauncherWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui::GetCurrentContext()) {
        if (msg == WM_KEYDOWN) {
            const bool typing = ImGui::GetIO().WantTextInput;
            if (wp == VK_RETURN && !typing) { RequestStart(); return 0; }
            if (wp == VK_ESCAPE && !typing) { RequestExit(); return 0; }
        }
        if (msg == WM_SYSKEYDOWN && wp == VK_F4) { RequestExit(); return 0; }
    }

    switch (msg) {
    case WM_CLOSE:    RequestExit(); return 0;
    // This is also the game's main thread. A queued WM_QUIT here would make
    // its first message loop exit immediately after the launcher hands over.
    case WM_DESTROY:  return 0;
    case WM_ERASEBKGND: return 1;
    default: break;
    }

    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return 1;

    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool CreateDevice(IDirect3D9* d3d, HWND hwnd, int width, int height, IDirect3DDevice9** outDevice, D3DPRESENT_PARAMETERS& pp)
{
    pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferWidth = (UINT)width;
    pp.BackBufferHeight = (UINT)height;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = hwnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.EnableAutoDepthStencil = FALSE;

    DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &pp, outDevice);
    if (FAILED(hr)) {
        ETH_WARN("launcher: HAL device failed (%08lX), trying software vertex processing", (unsigned long)hr);
        flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE;
        hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &pp, outDevice);
    }
    return SUCCEEDED(hr);
}

void RenderGameNotReadyHint()
{
    // On the Mods page the user may want to know nothing is running yet.
    ImGui::Dummy(ImVec2(0, 4 * ui::Scale()));
}

} // namespace

#if defined(ETH_TESTING)
void TestReset() { g_running = true; g_started = false; }
int TestState() { return g_running ? 2 : (g_started ? 1 : 0); }
void TestApplyRequests(bool start, bool exit) { ApplyFrameRequests(start, exit); }
void TestMessage(HWND hwnd, UINT msg, WPARAM wp) { LauncherWndProc(hwnd, msg, wp, 0); }
#endif

int Run()
{
    g_running = true;
    g_started = false;

    HINSTANCE inst = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LauncherWndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = "EthhhSDKLauncherWnd";
    if (!RegisterClassExA(&wc)) {
        ETH_ERROR("launcher: RegisterClassEx failed (%s)", ErrorCodeString(GetLastError()).c_str());
        return 1;   // fall through into the game
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    const float desktopScale=GetDpiScale(nullptr);
    int width=std::min(int(1152*desktopScale),std::max(320,screenW-40));
    int height=std::min(int(704*desktopScale),std::max(240,screenH-40));
    int posX = (screenW - width) / 2;
    int posY = (screenH - height) / 2;

    g_hwnd = CreateWindowExA(WS_EX_APPWINDOW, "EthhhSDKLauncherWnd", "EthhhSDK - Mod Manager",
                             WS_POPUP, posX, posY, width, height, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) {
        ETH_ERROR("launcher: CreateWindowEx failed (%s)", ErrorCodeString(GetLastError()).c_str());
        return 1;
    }

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        ETH_ERROR("launcher: Direct3DCreate9 failed");
        DestroyWindow(g_hwnd);
        return 1;
    }

    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    if (!CreateDevice(d3d, g_hwnd, width, height, &device, pp)) {
        ETH_ERROR("launcher: CreateDevice failed, skipping the launcher");
        d3d->Release();
        DestroyWindow(g_hwnd);
        return 1;
    }

    g_scale = GetDpiScale(g_hwnd);
    ui::EnsureContext();
    float fontScale = Config::Get().GetFloat("core", "font_scale", 1.0f);
    if (fontScale < 0.8f || fontScale > 1.6f) fontScale = 1.0f;
    ui::LoadFonts(g_scale * fontScale);
    ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplWin32_EnableAlphaCompositing(g_hwnd);
    ImGui_ImplDX9_Init(device);
    ui::InitGlass(device);
    Glass::g->SetTransparentTarget(true);
    ui::motion::Transition appearance;appearance.Target(true);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);

    ETH_INFO("launcher: window %dx%d at (%d,%d), scale %.2f", width, height, posX, posY, g_scale);

    while (g_running) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_running) break;
        if (!IsWindow(g_hwnd)) break;
        const HRESULT cooperative=device->TestCooperativeLevel();
        if(cooperative==D3DERR_DEVICELOST) {Sleep(30);continue;}
        if(cooperative==D3DERR_DEVICENOTRESET) {
            ui::InvalidateGlass(); ImGui_ImplDX9_InvalidateDeviceObjects();
            if(FAILED(device->Reset(&pp))) {Sleep(30);continue;}
            ImGui_ImplDX9_CreateDeviceObjects();ui::RecreateGlass();
        } else if(FAILED(cooperative)) {Sleep(30);continue;}
        if(IsIconic(g_hwnd)) {Sleep(30);continue;}

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ui::BeginGlassFrame();

        ImGuiIO& io = ImGui::GetIO();
        appearance.Advance(io.DeltaTime,Config::Get().GetBool("theme","reduce_motion",false));
        ui::SetManagerOpacity(appearance.Value());
        bool open = true;
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImVec2 size(io.DisplaySize.x, io.DisplaySize.y);
        bool visible = ui::BeginChromeWindow("##launcher", "EthhhSDK", "I-Ninja / Pre-game mod manager",
                                             &open, size,
                                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        if (!visible) {
            ImGui::EndFrame();
        } else {
            bool startRequested = false, exitRequested = !open;
            ui::pages::DrawManagerLayout(true, &startRequested, &exitRequested);

            // Drag the borderless window by its custom header.
            float headerH = 64.0f * ui::Scale();
            ImVec2 mouse = io.MousePos;
            bool onHeader = mouse.y >= 0 && mouse.y < headerH && mouse.x >= 0 && mouse.x < io.DisplaySize.x;
            if (ImGui::IsMouseClicked(0) && onHeader && mouse.x < io.DisplaySize.x - 62.0f * ui::Scale()) {
                g_dragging = true;
                GetCursorPos(&g_dragCursor);
                RECT wr;
                GetWindowRect(g_hwnd, &wr);
                g_dragWindow = { wr.left, wr.top };
            }
            if (!ImGui::IsMouseDown(0))
                g_dragging = false;
            if (g_dragging) {
                POINT p;
                GetCursorPos(&p);
                SetWindowPos(g_hwnd, nullptr,
                             g_dragWindow.x + (p.x - g_dragCursor.x),
                             g_dragWindow.y + (p.y - g_dragCursor.y),
                             0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            ui::EndChromeWindow();
            ApplyFrameRequests(startRequested, exitRequested);
        }

        ImGui::Render();

        if(SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0)) &&
           SUCCEEDED(device->BeginScene())) {
            ui::RenderGlassDrawData(device,ImGui::GetDrawData());
            device->EndScene();
            device->Present(nullptr, nullptr, nullptr, nullptr);
        } else Sleep(30);
    }

    ETH_INFO("launcher: %s", g_started ? "start requested" : "exit requested");

    ui::ShutdownGlass();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();

    device->Release();
    d3d->Release();
    if (IsWindow(g_hwnd))
        DestroyWindow(g_hwnd);
    g_hwnd = nullptr;

    return g_started ? 1 : 0;
}

} // namespace eth::launcher
