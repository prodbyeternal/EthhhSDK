#include "ethhhpatch.h"
#include "config.h"
#include "game.h"
#include "log.h"
#include "util.h"

namespace eth::patch {
namespace {
std::mutex g_mutex;
Status g_status;
uint64_t g_ticks = 0, g_activeTicks = 0, g_presents = 0;
double g_simulationSeconds = 0, g_begin = 0;
double g_sampleAt = 0, g_projectionAt = 0;
RenderCadence g_cadence;
std::atomic<uint32_t> g_lastProjectionFrame{0};
#ifdef ETH_TESTING
Status g_previewStatus;
bool g_hasPreviewStatus = false;
#endif

double Now()
{
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency) || !frequency.QuadPart) return 0;
    return double(counter.QuadPart) / double(frequency.QuadPart);
}

bool ReadFrameCountRaw(uint32_t& frame)
{
    __try { frame = *(const uint32_t*)game::A(game::off::FrameCount); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { frame = 0; return false; }
}

void ResetSample(double now)
{
    g_ticks = g_activeTicks = g_presents = 0;
    g_simulationSeconds = 0;
    g_begin = now;
}

void SampleDevice(IDirect3DDevice9* device)
{
    IDirect3DSurface9* buffer = nullptr;
    D3DSURFACE_DESC desc{};
    if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &buffer)) && buffer) {
        if (SUCCEEDED(buffer->GetDesc(&desc))) {
            g_status.width = desc.Width; g_status.height = desc.Height; g_status.deviceReady = true;
        }
        buffer->Release();
    }
    D3DVIEWPORT9 viewport{};
    if (SUCCEEDED(device->GetViewport(&viewport))) {
        g_status.viewportWidth = viewport.Width; g_status.viewportHeight = viewport.Height;
    }
    IDirect3DSwapChain9* swap = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    if (SUCCEEDED(device->GetSwapChain(0, &swap)) && swap) {
        if (SUCCEEDED(swap->GetPresentParameters(&pp))) {
            g_status.windowed = pp.Windowed != FALSE;
            g_status.presentInterval = pp.PresentationInterval;
            g_status.fullscreenHz = pp.FullScreen_RefreshRateInHz;
        }
        swap->Release();
    }
    uintptr_t view = 0;
    float aspect = 0;
    if (SafeRead(game::A(game::off::pView), view) && view && SafeRead(view, aspect) && std::isfinite(aspect))
        g_status.engineAspect = aspect;
}
} // namespace

MonitorInfo ReadMonitor(HWND window)
{
    MonitorInfo result;
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info{}; info.cbSize = sizeof(info);
    DEVMODEW mode{}; mode.dmSize = sizeof(mode);
    if (GetMonitorInfoW(monitor, &info) && EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
        result.width = (int)mode.dmPelsWidth; result.height = (int)mode.dmPelsHeight;
        if (mode.dmDisplayFrequency >= 30 && mode.dmDisplayFrequency <= 360) result.hz = (int)mode.dmDisplayFrequency;
    }
    return result;
}

Status ReadStatus()
{
    std::lock_guard<std::mutex> lock(g_mutex);
#ifdef ETH_TESTING
    if (g_hasPreviewStatus) return g_previewStatus;
#endif
    Status status = g_status;
    const double now = Now();
    status.sampled = status.sampled && FreshObservation(now, g_sampleAt);
    status.projectionValid = status.projectionValid && FreshObservation(now, g_projectionAt);
    if (g_sampleAt > 0 && now >= g_sampleAt) status.observationAgeSeconds = now - g_sampleAt;
    status.cadence = g_cadence.Counts();
    return status;
}

#ifdef ETH_TESTING
void SetPreviewStatus(const Status& status)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_previewStatus = status;
    g_hasPreviewStatus = true;
}
bool HasPreviewStatus()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_hasPreviewStatus;
}
#endif

void OnTick(float timeStep)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_begin) ResetSample(Now());
    ++g_ticks;
    g_cadence.Tick();
    if (std::isfinite(timeStep) && timeStep > 0 && timeStep <= 0.25f) {
        ++g_activeTicks;
        g_simulationSeconds += timeStep;
    }
}

void OnPresent(IDirect3DDevice9* device, HRESULT result, bool gamePath, bool moviePath)
{
    if (!device || (!gamePath && !moviePath)) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (FAILED(result)) { ++g_status.failedPresents; return; }
    if (moviePath) {
        ++g_status.moviePresents;
        g_status.sampled = false;
        g_status.projectionValid = false;
        g_cadence.InvalidateBaseline();
        ResetSample(Now()); // Do not count movie frames as unlocked game renders.
        return;
    }
    const double now = Now();
    uint32_t engineFrame = 0;
    if (SafeRead(game::A(game::off::FrameCount), engineFrame)) g_cadence.GamePresent(engineFrame);
    else g_cadence.InvalidateBaseline();
    if (!g_begin || !now || now < g_begin) { ResetSample(now); return; }
    ++g_presents;
    const double elapsed = now - g_begin;
    if (elapsed < 1.0) return;
    g_status.tickCallsPerSecond = g_ticks / elapsed;
    g_status.activeTicksPerSecond = g_activeTicks / elapsed;
    g_status.gamePresentsPerSecond = g_presents / elapsed;
    g_status.simulationSecondsPerSecond = g_simulationSeconds / elapsed;
    g_status.sampled = true;
    g_sampleAt = now;
    SampleDevice(device); // Render-thread read-only D3D queries; no reset or retained COM pointers.
    ResetSample(now);
}

void OnReset(const D3DPRESENT_PARAMETERS* parameters, HRESULT result)
{
    D3DPRESENT_PARAMETERS captured{};
    const bool parametersValid = parameters && SafeReadBytes((uintptr_t)parameters, &captured, sizeof(captured));
    std::lock_guard<std::mutex> lock(g_mutex);
    Status status;
    status.resetAttempts = g_status.resetAttempts;
    status.resetSuccesses = g_status.resetSuccesses;
    status.resetFailures = g_status.resetFailures;
    if (status.resetAttempts != UINT64_MAX) ++status.resetAttempts;
    if (SUCCEEDED(result)) {
        if (status.resetSuccesses != UINT64_MAX) ++status.resetSuccesses;
    } else if (status.resetFailures != UINT64_MAX) ++status.resetFailures;
    status.lastResetHresult = (int32_t)result;
    status.lastResetParametersValid = parametersValid;
    if (parametersValid) {
        status.resetWidth = captured.BackBufferWidth;
        status.resetHeight = captured.BackBufferHeight;
        status.resetInterval = captured.PresentationInterval;
        status.resetFullscreenHz = captured.FullScreen_RefreshRateInHz;
        status.resetWindowed = captured.Windowed != FALSE;
    }
    g_status = status;
    g_sampleAt = g_projectionAt = 0;
    g_lastProjectionFrame.store(0, std::memory_order_relaxed);
    g_cadence.Reset();
    ResetSample(Now());
}

void OnWorldProjection(const D3DMATRIX* matrix)
{
    // Only called after a successful SetTransform(PROJECTION) at the checked
    // world call site. This argument is View+0x120, before later HUD matrices.
    // Reads remain non-atomic; verify source pointer/parameters around the copy.
    uint32_t frame = 0;
    if (!ReadFrameCountRaw(frame)) return;
    uint32_t previous = g_lastProjectionFrame.load(std::memory_order_relaxed);
    if (frame - previous < 30 || !g_lastProjectionFrame.compare_exchange_strong(previous, frame, std::memory_order_relaxed))
        return; // Projection telemetry is diagnostic; never validate every render.
    uintptr_t view = 0, checkView = 0;
    float parameters[4] = {}, checkParameters[4] = {}, values[16] = {};
    ProjectionMetrics metrics;
    const bool valid = SafeRead(game::A(game::off::pView), view) && view &&
        view <= UINTPTR_MAX - 0x160 && (uintptr_t)matrix == view + 0x120 &&
        SafeReadBytes(view, parameters, sizeof(parameters)) &&
        SafeReadBytes((uintptr_t)matrix, values, sizeof(values)) &&
        SafeReadBytes(view, checkParameters, sizeof(checkParameters)) &&
        SafeRead(game::A(game::off::pView), checkView) && view == checkView &&
        !memcmp(parameters, checkParameters, sizeof(parameters)) &&
        std::isfinite(parameters[0]) && parameters[0] > 0 &&
        std::isfinite(parameters[3]) && parameters[3] > 0 && parameters[3] < 3.12414f &&
        InspectPerspectiveLH(values, metrics);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.projectionValid = valid;
    g_projectionAt = valid ? Now() : 0;
    if (!valid) return;
    ++g_status.projectionSamples;
    g_status.projection = metrics;
    g_status.projectionSourceAspect = parameters[0];
    g_status.projectionSourceFov = parameters[3];
    g_status.projectionAspectError = std::abs(metrics.aspect - parameters[0]);
    g_status.projectionFovErrorDegrees = std::abs(metrics.verticalFovDegrees - double(parameters[3]) * 57.29577951308232);
}

DisplayPlan LoadPlan()
{
    auto& cfg = Config::Get();
    DisplayPlan plan;
    plan.width = cfg.GetInt("ethhhpatch", "width", 1920);
    plan.height = cfg.GetInt("ethhhpatch", "height", 1080);
    plan.renderHz = cfg.GetInt("ethhhpatch", "render_hz", 0);
    plan.windowed = cfg.GetBool("ethhhpatch", "windowed", false);
    plan.mouseLook = cfg.GetBool("ethhhpatch", "mouse_look", true);
    return ValidPlan(plan) ? plan : DisplayPlan{};
}

bool SavePlan(const DisplayPlan& plan)
{
    if (!ValidPlan(plan)) return false;
    auto& cfg = Config::Get();
    cfg.SetInt("ethhhpatch", "width", plan.width);
    cfg.SetInt("ethhhpatch", "height", plan.height);
    cfg.SetInt("ethhhpatch", "render_hz", plan.renderHz);
    cfg.SetBool("ethhhpatch", "windowed", plan.windowed);
    cfg.SetBool("ethhhpatch", "mouse_look", plan.mouseLook);
    const bool saved = cfg.Save();
    ETH_INFO("EthhhPatch: future %s display plan %dx%d @ %d Hz (%s); NOT applied, simulation untouched",
        plan.windowed ? "windowed" : "fullscreen", plan.width, plan.height, plan.renderHz, saved ? "saved" : "save failed");
    return saved;
}
} // namespace eth::patch
