#include "ethhhpatch_runtime.h"
#include "ethhhpatch_runtime_policy.h"
#include "game.h"
#include "log.h"
#include "util.h"
#include "input.h"

#include <MinHook.h>

namespace eth::patch::runtime {
void UninstallHooks();
namespace {
constexpr uintptr_t kZeroTickWaitBranch = 0x43D96C;
constexpr uint8_t kWaitBytes[2] = { 0x7C, 0xF2 };
constexpr uint8_t kZeroTickBytes[2] = { 0x90, 0x90 };
constexpr uintptr_t kSphereCull = 0x449420;
constexpr uintptr_t kHudPulse = 0x4483B0;
constexpr uintptr_t kMovieDraw = 0x497D40;
constexpr uintptr_t kViewRender = 0x43F290;
constexpr float kOriginalAspect = 4.0f / 3.0f;

using SphereCullFn = bool(__stdcall*)(const float*, const float*);
using HudPulseFn = void(__thiscall*)(void*);
using MovieDrawFn = void(__cdecl*)(void*, float, float, int, float, float, float);
using ViewRenderFn = void(__thiscall*)(void*);
SphereCullFn g_sphereCullOrig = nullptr;
HudPulseFn g_hudPulseOrig = nullptr;
MovieDrawFn g_movieDrawOrig = nullptr;
ViewRenderFn g_viewRenderOrig = nullptr;
std::atomic<bool> g_hooksReady{false};
std::atomic<bool> g_active{false};
std::atomic<float> g_aspect{kOriginalAspect};
std::atomic<unsigned> g_targetWidth{0}, g_targetHeight{0};
std::atomic<int> g_targetHz{0};
int64_t g_nextPresentCounter = 0;
std::atomic<bool> g_externalCameraOwned{false};
bool g_mouseLookEnabled = false, g_mouseLookInitialized = false, g_mouseCaptureOwned = false;
float g_mouseYaw = 0, g_mousePitch = 0;
float g_orbitDistance = 5.0f, g_orbitCollisionDistance = 5.0f;
float g_targetPrevious[3] = {}, g_targetCurrent[3] = {};
uint32_t g_orbitTick = 0;
double g_orbitTickTime = 0;
bool g_cutsceneSuspended = false;
int g_nearPlayerTicks = 0;
std::atomic<int64_t> g_movieUntilCounter{0};
PulseClock g_pulseClock;
std::mutex g_applyMutex;
ApplyStatus g_applyStatus;
D3DPRESENT_PARAMETERS g_previousParameters{};
RECT g_previousWindowRect{};
bool g_havePreviousParameters = false, g_havePreviousWindowRect = false;

using ConfigureDisplayFn = BOOL(__thiscall*)(void*, bool, int, int, int);

bool ReadWrapper(uintptr_t& wrapper)
{
    uintptr_t renderer = 0;
    return SafeRead(game::A(game::off::pRenderer), renderer) && renderer &&
        SafeRead(renderer + 0x7B0, wrapper) && wrapper;
}

bool ConfigureDisplay(bool windowed, int width, int height, int hz)
{
    uintptr_t wrapper = 0;
    if (!ReadWrapper(wrapper)) return false;
    BOOL posted = FALSE;
    __try {
        posted = ((ConfigureDisplayFn)game::A(0x452300))((void*)wrapper, !windowed, width, height,
            windowed ? 0 : hz);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return posted != FALSE;
}

bool CapturePresentParameters(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS& out)
{
    IDirect3DSwapChain9* swap = nullptr;
    const HRESULT chainResult = device ? device->GetSwapChain(0, &swap) : D3DERR_INVALIDCALL;
    if (FAILED(chainResult) || !swap) return false;
    const HRESULT result = swap->GetPresentParameters(&out);
    swap->Release();
    return SUCCEEDED(result);
}

bool ExactFullscreenModeSupported(IDirect3DDevice9* device, int width, int height, int hz)
{
    D3DDEVICE_CREATION_PARAMETERS creation{};
    IDirect3D9* d3d = nullptr;
    if (!device || FAILED(device->GetCreationParameters(&creation)) || FAILED(device->GetDirect3D(&d3d)) || !d3d)
        return false;
    D3DDISPLAYMODE desktop{};
    bool found = false;
    if (SUCCEEDED(d3d->GetAdapterDisplayMode(creation.AdapterOrdinal, &desktop))) {
        const UINT count = d3d->GetAdapterModeCount(creation.AdapterOrdinal, desktop.Format);
        for (UINT i = 0; i < count; ++i) {
            D3DDISPLAYMODE mode{};
            if (SUCCEEDED(d3d->EnumAdapterModes(creation.AdapterOrdinal, desktop.Format, i, &mode)) &&
                mode.Width == (UINT)width && mode.Height == (UINT)height && mode.RefreshRate == (UINT)hz) {
                found = true; break;
            }
        }
    }
    d3d->Release();
    return found;
}

bool ResizeClient(HWND window, int width, int height)
{
    if (!window || width <= 0 || height <= 0) return false;
    const DWORD style = (DWORD)GetWindowLongPtrW(window, GWL_STYLE);
    const DWORD exStyle = (DWORD)GetWindowLongPtrW(window, GWL_EXSTYLE);
    RECT outer{0, 0, width, height};
    if (!AdjustWindowRectEx(&outer, style, GetMenu(window) != nullptr, exStyle)) return false;
    if (!SetWindowPos(window, nullptr, 0, 0, outer.right - outer.left, outer.bottom - outer.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)) return false;
    RECT client{};
    if (!GetClientRect(window, &client)) return false;
    const int actualWidth = client.right - client.left, actualHeight = client.bottom - client.top;
    if (actualWidth == width && actualHeight == height) return true;
    // DPI virtualization/theme borders can make the first adjustment imperfect.
    RECT current{};
    if (!GetWindowRect(window, &current)) return false;
    return SetWindowPos(window, nullptr, 0, 0, current.right - current.left + width - actualWidth,
        current.bottom - current.top + height - actualHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) &&
        GetClientRect(window, &client) && client.right - client.left == width && client.bottom - client.top == height;
}

bool StartRollback(const char* reason)
{
    D3DPRESENT_PARAMETERS previous{}; RECT windowRect{};
    bool haveParameters = false, haveRect = false;
    {
        std::lock_guard<std::mutex> lock(g_applyMutex);
        previous = g_previousParameters; windowRect = g_previousWindowRect;
        haveParameters = g_havePreviousParameters; haveRect = g_havePreviousWindowRect;
        g_applyStatus.state = ApplyState::RollingBack;
        g_applyStatus.detail = reason ? reason : "display application failed";
    }
    DeactivateRenderSplit();
    HWND window = (HWND)game::GameHwnd();
    if (haveRect && previous.Windowed && window)
        SetWindowPos(window, nullptr, windowRect.left, windowRect.top,
            windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    if (!haveParameters || !ConfigureDisplay(previous.Windowed != FALSE,
        (int)previous.BackBufferWidth, (int)previous.BackBufferHeight,
        (int)previous.FullScreen_RefreshRateInHz)) {
        {
            std::lock_guard<std::mutex> lock(g_applyMutex);
            g_applyStatus.state = ApplyState::Failed;
            g_applyStatus.detail += "; rollback request failed";
        }
        UninstallHooks();
        return false;
    }
    return true;
}

bool ExactCodeWrite(uintptr_t address, const uint8_t (&expected)[2], const uint8_t (&replacement)[2])
{
    uint8_t current[2] = {};
    if (!SafeReadBytes(address, current, sizeof(current)) || memcmp(current, expected, sizeof(current))) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect((void*)address, sizeof(current), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    memcpy((void*)address, replacement, sizeof(current));
    FlushInstructionCache(GetCurrentProcess(), (void*)address, sizeof(current));
    DWORD ignored = 0;
    VirtualProtect((void*)address, sizeof(current), oldProtect, &ignored);
    return true;
}

bool __stdcall SphereCullDetour(const float* center, const float* radius)
{
    if (!g_active.load(std::memory_order_relaxed))
        return g_sphereCullOrig ? g_sphereCullOrig(center, radius) : false;
    float p[3] = {}, r = 0, matrix[16] = {}, viewValues[5] = {};
    __try {
        if (!center || !radius) return false;
        memcpy(p, center, sizeof(p)); r = *radius;
        const uintptr_t camera = *(uintptr_t*)game::A(game::off::pCamera);
        const uintptr_t view = *(uintptr_t*)game::A(game::off::pView);
        if (!camera || !view) return g_sphereCullOrig ? g_sphereCullOrig(center, radius) : false;
        memcpy(matrix, (const void*)(camera + game::off::CamViewMat), sizeof(matrix));
        memcpy(viewValues, (const void*)view, sizeof(viewValues));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return g_sphereCullOrig ? g_sphereCullOrig(center, radius) : false;
    }
    const float x = matrix[0] * p[0] + matrix[4] * p[1] + matrix[8] * p[2] + matrix[12];
    const float y = matrix[1] * p[0] + matrix[5] * p[1] + matrix[9] * p[2] + matrix[13];
    const float z = matrix[2] * p[0] + matrix[6] * p[1] + matrix[10] * p[2] + matrix[14];
    return PerspectiveSphereVisible(x, y, z, r, g_aspect.load(std::memory_order_relaxed),
        viewValues[4], viewValues[1]); // View+0x10 tan(FOV/2), View+0x04 far
}

void __fastcall HudPulseDetour(void* self, void*)
{
    if (g_active.load(std::memory_order_relaxed) && self) {
        uint32_t tick = 0;
        float phase = 0;
        __try {
            tick = *(uint32_t*)game::A(game::off::TickCount);
            phase = *(float*)((uint8_t*)self + 0xE8);
            const float adjustment = PulsePreAdjustment(g_pulseClock.Advance(tick));
            if (std::isfinite(phase) && std::isfinite(adjustment))
                *(float*)((uint8_t*)self + 0xE8) = phase + adjustment;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (g_hudPulseOrig) g_hudPulseOrig(self);
}

void __cdecl MovieDrawDetour(void* surface, float x, float y, int height,
    float scaleX, float scaleY, float alpha)
{
    if (g_active.load(std::memory_order_relaxed) && surface) {
        int sourceWidth = 0, sourceHeight = 0;
        __try {
            sourceWidth = *(int*)((uint8_t*)surface + 0x14);
            sourceHeight = *(int*)((uint8_t*)surface + 0x18);
        } __except (EXCEPTION_EXECUTE_HANDLER) { sourceWidth = sourceHeight = 0; }
        const unsigned targetWidth = g_targetWidth.load(std::memory_order_relaxed);
        const unsigned targetHeight = g_targetHeight.load(std::memory_order_relaxed);
        if (sourceWidth > 0 && sourceWidth <= 8192 && sourceHeight > 0 && sourceHeight <= 8192 &&
            targetWidth && targetHeight) {
            const AspectFit fit = FitCentered(sourceWidth, sourceHeight, (int)targetWidth, (int)targetHeight);
            if (fit.scale > 0) {
                scaleX = scaleY = fit.scale;
                x = fit.x; y = fit.y;
            }
        }
    }
    if (g_movieDrawOrig) g_movieDrawOrig(surface, x, y, height, scaleX, scaleY, alpha);
}

void ApplyRenderMouseLook()
{
    if (!g_active.load(std::memory_order_relaxed) || !g_mouseLookEnabled || g_externalCameraOwned.load()) {
        g_mouseLookInitialized = false; g_mouseCaptureOwned = false; return;
    }
    const uintptr_t player = game::PlayerTracker::Get().Addr();
    const uint32_t tick = game::TickCount();
    const bool newTick = tick != g_orbitTick;
    uintptr_t camera = 0;
    float world[16] = {};
    if (!SafeRead(game::A(game::off::pCamera), camera) || !camera ||
        !SafeReadBytes(camera + game::off::CamMat, world, sizeof(world))) return;
    LARGE_INTEGER counter{},frequency{};
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    const bool movie = frequency.QuadPart && counter.QuadPart < g_movieUntilCounter.load();
    if (newTick) {
        StratView ninja;
        StratIdentity identity;
        if (!player || !game::ReadStrat(player,ninja) ||
            !game::ReadStratIdentity(player,identity) || identity.role != StratRole::Ninja) {
            if (g_mouseCaptureOwned) input::SetCapture(false);
            g_mouseCaptureOwned=false;g_mouseLookInitialized=false;return;
        }
        memcpy(g_targetPrevious,g_targetCurrent,sizeof(g_targetCurrent));
        g_targetCurrent[0]=ninja.pos[0];g_targetCurrent[1]=ninja.pos[1]+1.3f;g_targetCurrent[2]=ninja.pos[2];
        if (!g_orbitTick) memcpy(g_targetPrevious,g_targetCurrent,sizeof(g_targetCurrent));
        g_orbitTick=tick;g_orbitTickTime=frequency.QuadPart?double(counter.QuadPart)/frequency.QuadPart:0;
        const float sx=world[12]-g_targetCurrent[0],sy=world[13]-g_targetCurrent[1],sz=world[14]-g_targetCurrent[2];
        const float scriptDistance=std::sqrt(sx*sx+sy*sy+sz*sz);
        if (movie || !std::isfinite(scriptDistance) || scriptDistance>20.0f) {
            g_cutsceneSuspended=true;g_nearPlayerTicks=0;
        } else if (g_cutsceneSuspended && ++g_nearPlayerTicks>=15) {
            g_cutsceneSuspended=false;g_mouseLookInitialized=false;g_nearPlayerTicks=0;
        }
    }
    if (movie || g_cutsceneSuspended) {
        if (g_mouseCaptureOwned) input::SetCapture(false);
        g_mouseCaptureOwned=false;input::ConsumeMouseDelta();return;
    }
    if (!g_mouseCaptureOwned) { input::SetCapture(true); g_mouseCaptureOwned = true; }
    float dx = 0, dy = 0;
    input::GetMouseDelta(dx, dy);
    if (!g_mouseLookInitialized) {
        const float fx=g_targetCurrent[0]-world[12],fy=g_targetCurrent[1]-world[13],fz=g_targetCurrent[2]-world[14];
        const float length=std::sqrt(fx*fx+fy*fy+fz*fz);
        if(length>0.1f&&std::isfinite(length)){g_mouseYaw=std::atan2(fx,fz);g_mousePitch=std::asin(std::clamp(fy/length,-1.0f,1.0f));g_orbitDistance=std::clamp(length,2.0f,12.0f);}
        else {g_mouseYaw=0;g_mousePitch=0;g_orbitDistance=5.0f;}
        g_orbitCollisionDistance=g_orbitDistance;
        g_mouseLookInitialized = true;
    }
    if (input::CaptureActive()) {
        constexpr float sensitivity = 0.030f * 0.01745329251994329577f;
        g_mouseYaw += dx * sensitivity;
        g_mousePitch = std::clamp(g_mousePitch - dy * sensitivity, -1.55334306f, 1.55334306f);
    }
    input::ConsumeMouseDelta();
    float alpha=1.0f;
    if(frequency.QuadPart&&g_orbitTickTime>0) alpha=std::clamp(float((double(counter.QuadPart)/frequency.QuadPart-g_orbitTickTime)*60.0),0.0f,1.0f);
    float target[3]={g_targetPrevious[0]+(g_targetCurrent[0]-g_targetPrevious[0])*alpha,
        g_targetPrevious[1]+(g_targetCurrent[1]-g_targetPrevious[1])*alpha,
        g_targetPrevious[2]+(g_targetCurrent[2]-g_targetPrevious[2])*alpha};
    const float cp=std::cos(g_mousePitch),fx=std::sin(g_mouseYaw)*cp,fy=std::sin(g_mousePitch),fz=std::cos(g_mouseYaw)*cp;
    if(newTick) {
        TraceSegment segment{{target[0],target[1],target[2]},{target[0]-fx*g_orbitDistance,target[1]-fy*g_orbitDistance,target[2]-fz*g_orbitDistance}};
        TraceHit hit;
        if(game::TraceWorldSegments(player,&segment,1,&hit)&&hit.hit){
            const float hx=hit.point[0]-target[0],hy=hit.point[1]-target[1],hz=hit.point[2]-target[2];
            g_orbitCollisionDistance=std::max(0.5f,std::sqrt(hx*hx+hy*hy+hz*hz)-0.1f);
        } else g_orbitCollisionDistance=g_orbitDistance;
    }
    const float distance=std::min(g_orbitDistance,g_orbitCollisionDistance);
    float adjusted[16] = {};
    if (game::MakeCameraWorld(target[0]-fx*distance,target[1]-fy*distance,target[2]-fz*distance,g_mouseYaw,g_mousePitch,0,adjusted))
        game::SetCameraWorld(adjusted);
}

void __fastcall ViewRenderDetour(void* self, void*)
{
    ApplyRenderMouseLook();
    if (g_viewRenderOrig) g_viewRenderOrig(self);
}
} // namespace

bool InstallHooks()
{
    if (g_hooksReady.load()) return true;
    void* sphere = (void*)game::A(kSphereCull), *pulse = (void*)game::A(kHudPulse),
        *movie = (void*)game::A(kMovieDraw), *viewRender = (void*)game::A(kViewRender);
    MH_STATUS status = MH_CreateHook(sphere, (void*)&SphereCullDetour, (void**)&g_sphereCullOrig);
    if (status != MH_OK) { ETH_ERROR("EthhhPatch sphere hook failed: %s", MH_StatusToString(status)); return false; }
    status = MH_CreateHook(pulse, (void*)&HudPulseDetour, (void**)&g_hudPulseOrig);
    if (status != MH_OK) {
        MH_RemoveHook(sphere); g_sphereCullOrig = nullptr;
        ETH_ERROR("EthhhPatch HUD pulse hook failed: %s", MH_StatusToString(status)); return false;
    }
    status = MH_CreateHook(movie, (void*)&MovieDrawDetour, (void**)&g_movieDrawOrig);
    if (status != MH_OK) {
        MH_RemoveHook(sphere); MH_RemoveHook(pulse); g_sphereCullOrig = nullptr; g_hudPulseOrig = nullptr;
        ETH_ERROR("EthhhPatch movie hook failed: %s", MH_StatusToString(status)); return false;
    }
    status = MH_CreateHook(viewRender, (void*)&ViewRenderDetour, (void**)&g_viewRenderOrig);
    if (status != MH_OK) {
        MH_RemoveHook(sphere); MH_RemoveHook(pulse); MH_RemoveHook(movie);
        g_sphereCullOrig=nullptr;g_hudPulseOrig=nullptr;g_movieDrawOrig=nullptr;
        ETH_ERROR("EthhhPatch render camera hook failed: %s", MH_StatusToString(status)); return false;
    }
    if (MH_QueueEnableHook(sphere) != MH_OK || MH_QueueEnableHook(pulse) != MH_OK ||
        MH_QueueEnableHook(movie) != MH_OK || MH_QueueEnableHook(viewRender) != MH_OK || MH_ApplyQueued() != MH_OK) {
        MH_DisableHook(sphere); MH_DisableHook(pulse); MH_DisableHook(movie); MH_DisableHook(viewRender);
        MH_RemoveHook(sphere); MH_RemoveHook(pulse); MH_RemoveHook(movie); MH_RemoveHook(viewRender);
        g_sphereCullOrig = nullptr; g_hudPulseOrig = nullptr; g_movieDrawOrig = nullptr; g_viewRenderOrig=nullptr;
        ETH_ERROR("EthhhPatch compatibility hooks could not be enabled"); return false;
    }
    g_hooksReady.store(true);
    ETH_INFO("EthhhPatch pass-through compatibility hooks ready (culling/HUD pulse/movie fit); render split inactive");
    return true;
}

void UninstallHooks()
{
    if (!g_hooksReady.exchange(false)) return;
    DeactivateRenderSplit();
    void* targets[] = { (void*)game::A(kSphereCull), (void*)game::A(kHudPulse), (void*)game::A(kMovieDraw), (void*)game::A(kViewRender) };
    for (void* target : targets) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    g_sphereCullOrig = nullptr; g_hudPulseOrig = nullptr; g_movieDrawOrig = nullptr; g_viewRenderOrig=nullptr;
    ETH_INFO("EthhhPatch compatibility hooks removed");
}

bool ActivateRenderSplit(float aspect, unsigned width, unsigned height, int targetHz)
{
    if (!g_hooksReady.load() || !std::isfinite(aspect) || aspect < 0.5f || aspect > 8.0f ||
        targetHz < 30 || targetHz > 360) return false;
    if (g_active.load()) return std::abs(g_aspect.load() - aspect) < 1e-6f;
    g_aspect.store(aspect);
    g_targetWidth.store(width);
    g_targetHeight.store(height);
    g_targetHz.store(targetHz);
    g_nextPresentCounter = 0;
    g_pulseClock = {};
    if (!ExactCodeWrite(game::A(kZeroTickWaitBranch), kWaitBytes, kZeroTickBytes)) {
        ETH_ERROR("EthhhPatch refused zero-tick branch patch: executable bytes differ");
        g_aspect.store(kOriginalAspect); g_targetWidth.store(0); g_targetHeight.store(0);
        g_targetHz.store(0); return false;
    }
    g_active.store(true);
    OnTickCompleted();
    ETH_INFO("EthhhPatch render split activated internally (aspect %.6f); display application still gated", aspect);
    return true;
}

void DeactivateRenderSplit()
{
    if (!g_active.exchange(false)) return;
    if (!ExactCodeWrite(game::A(kZeroTickWaitBranch), kZeroTickBytes, kWaitBytes))
        ETH_ERROR("EthhhPatch could not restore original wait branch bytes");
    g_aspect.store(kOriginalAspect);
    g_targetWidth.store(0); g_targetHeight.store(0);
    g_targetHz.store(0); g_nextPresentCounter = 0;
    if (g_mouseCaptureOwned && !g_externalCameraOwned.load()) input::SetCapture(false);
    g_mouseCaptureOwned=false;g_mouseLookInitialized=false;g_mouseLookEnabled=false;
    g_orbitTick=0;g_cutsceneSuspended=false;g_nearPlayerTicks=0;
    g_pulseClock = {};
}

void OnTickCompleted()
{
    if (!g_active.load(std::memory_order_relaxed)) return;
    uintptr_t view = 0;
    const float aspect = g_aspect.load(std::memory_order_relaxed);
    if (!SafeRead(game::A(game::off::pView), view) || !view || !SafeWrite(view, aspect)) {
        ETH_ERROR("EthhhPatch aspect write failed; disabling render split");
        DeactivateRenderSplit();
    }
}

void BeforeGamePresent()
{
    if (!g_active.load(std::memory_order_relaxed)) return;
    const int hz = g_targetHz.load(std::memory_order_relaxed);
    LARGE_INTEGER frequency{}, now{};
    if (hz < 30 || !QueryPerformanceFrequency(&frequency) || !frequency.QuadPart || !QueryPerformanceCounter(&now)) return;
    const int64_t period = std::max<int64_t>(1, frequency.QuadPart / hz);
    if (!g_nextPresentCounter || now.QuadPart > g_nextPresentCounter + period * 4)
        g_nextPresentCounter = now.QuadPart;
    while (now.QuadPart < g_nextPresentCounter) {
        const double milliseconds = 1000.0 * double(g_nextPresentCounter - now.QuadPart) / frequency.QuadPart;
        if (milliseconds > 2.0) Sleep((DWORD)(milliseconds - 1.0));
        else SwitchToThread();
        if (!QueryPerformanceCounter(&now)) break;
    }
    g_nextPresentCounter += period;
}

void Shutdown() { DeactivateRenderSplit(); UninstallHooks(); }
bool HooksReady() { return g_hooksReady.load(); }
bool Active() { return g_active.load(); }

bool AdjustHudProjection(uintptr_t returnAddress, const D3DMATRIX* input, D3DMATRIX& output)
{
    if (!g_active.load(std::memory_order_relaxed) || !input) return false;
    if (returnAddress != game::A(0x44726B) && returnAddress != game::A(0x4474D6) &&
        returnAddress != game::A(0x44884A)) return false;
    __try { memcpy(&output, input, sizeof(output)); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    const float* m = &output._11;
    for (int i = 0; i < 16; ++i) if (!std::isfinite(m[i])) return false;
    // These three checked sites submit identity projection for fixed 4:3-authored
    // 2D geometry. Scale around NDC center; world/orthographic/movie sites remain untouched.
    constexpr float epsilon = 1e-6f;
    for (int i = 0; i < 16; ++i) {
        const float expected = i % 5 == 0 ? 1.0f : 0.0f;
        if (std::abs(m[i] - expected) > epsilon) return false;
    }
    const float scale = HudHorizontalScale(g_aspect.load(std::memory_order_relaxed));
    if (!(scale > 0)) return false;
    output._11 = scale;
    return true;
}

bool RequestDisplayPlan(const DisplayPlan& plan, int monitorHz, std::string& error)
{
    error.clear();
    const int targetHz = TargetRenderHz(plan, monitorHz);
    if (!ValidPlan(plan) || !targetHz) { error = "invalid target or unknown monitor refresh"; return false; }
    {
        std::lock_guard<std::mutex> lock(g_applyMutex);
        if (g_applyStatus.state == ApplyState::Applying || g_applyStatus.state == ApplyState::RollingBack) {
            error = "another display transaction is pending"; return false;
        }
        if (g_applyStatus.state == ApplyState::Active) {
            error = "restore the active display transaction before applying another"; return false;
        }
    }
    IDirect3DDevice9* device = (IDirect3DDevice9*)game::Device();
    HWND window = (HWND)game::GameHwnd();
    DWORD process = 0;
    if (!device || !window || GetWindowThreadProcessId(window, &process) != GetCurrentThreadId()) {
        error = "display request must run on the game window thread"; return false;
    }
    D3DPRESENT_PARAMETERS previous{};
    RECT previousRect{};
    if (!CapturePresentParameters(device, previous) || !GetWindowRect(window, &previousRect)) {
        error = "could not capture rollback state"; return false;
    }
    if (plan.windowed) {
        if (targetHz != monitorHz) { error = "windowed D3D9 follows monitor refresh; explicit differing Hz refused"; return false; }
        if (!InstallHooks()) { error = "compatibility hooks could not be installed"; return false; }
        if (!ResizeClient(window, plan.width, plan.height)) {
            SetWindowPos(window, nullptr, previousRect.left, previousRect.top,
                previousRect.right - previousRect.left, previousRect.bottom - previousRect.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            error = "could not establish exact window client size; previous window rectangle restored";
            UninstallHooks();
            return false;
        }
    } else if (!ExactFullscreenModeSupported(device, plan.width, plan.height, targetHz)) {
        error = "exact fullscreen mode is not reported by this adapter"; return false;
    } else if (!InstallHooks()) { error = "compatibility hooks could not be installed"; return false; }
    {
        std::lock_guard<std::mutex> lock(g_applyMutex);
        g_previousParameters = previous; g_previousWindowRect = previousRect;
        g_havePreviousParameters = g_havePreviousWindowRect = true;
        g_applyStatus = {};
        g_applyStatus.state = ApplyState::Applying;
        g_applyStatus.requested = plan;
        g_applyStatus.targetHz = targetHz;
        g_applyStatus.detail = "waiting for the game Reset lifecycle";
        g_mouseLookEnabled = plan.mouseLook;
    }
    if (!ConfigureDisplay(plan.windowed, plan.width, plan.height, targetHz)) {
        error = "engine mode request/PostMessage failed";
        StartRollback(error.c_str());
        return false;
    }
    return true;
}

bool RequestRestore(std::string& error)
{
    error.clear();
    HWND window = (HWND)game::GameHwnd();
    DWORD process = 0;
    if (!window || GetWindowThreadProcessId(window, &process) != GetCurrentThreadId()) {
        error = "restore must run on the game window thread"; return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_applyMutex);
        if (g_applyStatus.state != ApplyState::Active) { error = "no active EthhhPatch display transaction"; return false; }
    }
    if (!StartRollback("user requested restore")) { error = "rollback request failed"; return false; }
    return true;
}

void OnResetResult(const D3DPRESENT_PARAMETERS* parameters, HRESULT result)
{
    ApplyStatus status;
    {
        std::lock_guard<std::mutex> lock(g_applyMutex);
        status = g_applyStatus;
    }
    if (status.state == ApplyState::Applying) {
        if (SUCCEEDED(result) && parameters && AppliedModeMatches(status.requested, status.targetHz,
            parameters->BackBufferWidth, parameters->BackBufferHeight, parameters->Windowed != FALSE,
            parameters->FullScreen_RefreshRateInHz) &&
            ActivateRenderSplit((float)Aspect(status.requested.width, status.requested.height),
                parameters->BackBufferWidth, parameters->BackBufferHeight, status.targetHz)) {
            std::lock_guard<std::mutex> lock(g_applyMutex);
            g_applyStatus.state = ApplyState::Active;
            g_applyStatus.detail = "display Reset matched; render split/culling/HUD timing active";
            return;
        }
        StartRollback(FAILED(result) ? "display Reset failed" : "Reset result did not exactly match the request");
    } else if (status.state == ApplyState::RollingBack) {
        bool matches = SUCCEEDED(result) && parameters;
        if (matches) {
            std::lock_guard<std::mutex> lock(g_applyMutex);
            matches = g_havePreviousParameters &&
                parameters->BackBufferWidth == g_previousParameters.BackBufferWidth &&
                parameters->BackBufferHeight == g_previousParameters.BackBufferHeight &&
                parameters->Windowed == g_previousParameters.Windowed &&
                parameters->FullScreen_RefreshRateInHz == g_previousParameters.FullScreen_RefreshRateInHz;
        }
        {
            std::lock_guard<std::mutex> lock(g_applyMutex);
            g_applyStatus.state = matches ? ApplyState::RolledBack : ApplyState::Failed;
            g_applyStatus.detail += matches ? "; previous display restored exactly" : "; rollback Reset failed or mismatched";
        }
        UninstallHooks();
    } else if (status.state == ApplyState::Active && SUCCEEDED(result) && parameters) {
        if (!AppliedModeMatches(status.requested, status.targetHz, parameters->BackBufferWidth,
            parameters->BackBufferHeight, parameters->Windowed != FALSE,
            parameters->FullScreen_RefreshRateInHz)) {
            DeactivateRenderSplit();
            {
                std::lock_guard<std::mutex> lock(g_applyMutex);
                g_applyStatus.state = ApplyState::Failed;
                g_applyStatus.detail = "a later game/device Reset changed the verified mode; compatibility patch disabled";
            }
            UninstallHooks();
        } else {
            g_targetWidth.store(parameters->BackBufferWidth);
            g_targetHeight.store(parameters->BackBufferHeight);
            g_nextPresentCounter = 0;
        }
    }
}

ApplyStatus ReadApplyStatus()
{
    std::lock_guard<std::mutex> lock(g_applyMutex);
    return g_applyStatus;
}

void SetExternalCameraOwned(bool owned)
{
    g_externalCameraOwned.store(owned);
    if (owned) { g_mouseLookInitialized=false; g_mouseCaptureOwned=false; }
}

void NotifyMoviePresent()
{
    LARGE_INTEGER counter{},frequency{};
    if(QueryPerformanceCounter(&counter)&&QueryPerformanceFrequency(&frequency)&&frequency.QuadPart)
        g_movieUntilCounter.store(counter.QuadPart+frequency.QuadPart*2);
}

} // namespace eth::patch::runtime
