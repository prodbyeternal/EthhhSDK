#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace eth::patch {

constexpr double kSimulationHz = 60.0;
constexpr double kSimulationStep = 1.0 / kSimulationHz;

struct DisplayPlan {
    int width = 1920, height = 1080;
    int renderHz = 0; // 0 means monitor refresh, not unlimited simulation.
    bool windowed = false;
    bool mouseLook = true;
};

inline bool ValidPlan(const DisplayPlan& p)
{
    return p.width >= 640 && p.width <= 7680 && p.height >= 480 && p.height <= 4320 &&
        (p.renderHz == 0 || (p.renderHz >= 30 && p.renderHz <= 360));
}

inline int TargetRenderHz(const DisplayPlan& p, int monitorHz)
{
    if (!ValidPlan(p)) return 0;
    if (p.renderHz) return p.renderHz;
    return monitorHz >= 30 && monitorHz <= 360 ? monitorHz : 0; // Unknown is not 60 Hz evidence.
}

inline bool AppliedModeMatches(const DisplayPlan& plan, int targetHz, unsigned width,
    unsigned height, bool windowed, unsigned fullscreenHz)
{
    if (!ValidPlan(plan) || targetHz < 30 || targetHz > 360 ||
        width != (unsigned)plan.width || height != (unsigned)plan.height || windowed != plan.windowed)
        return false;
    // D3D9 windowed presentation reports refresh 0 and follows the desktop.
    return windowed ? fullscreenHz == 0 : fullscreenHz == (unsigned)targetHz;
}

inline double Aspect(int width, int height)
{
    return width > 0 && height > 0 ? double(width) / double(height) : 0.0;
}

inline double HorizontalFov(double verticalDegrees, double aspect)
{
    if (!std::isfinite(verticalDegrees) || !std::isfinite(aspect) ||
        verticalDegrees <= 0 || verticalDegrees >= 179 || aspect <= 0 || aspect > 16) return 0;
    constexpr double pi = 3.14159265358979323846;
    return 2 * std::atan(std::tan(verticalDegrees * pi / 360) * aspect) * 180 / pi;
}

// Offline scheduler prototype only. The runtime DOES NOT replace App_Run with
// this until render-only paths and engine frame stamps are verified. Excess
// whole ticks after a stall are dropped; fractional debt survives, max 4 ticks.
class Fixed60Prototype {
public:
    struct Step { int ticks = 0; uint64_t dropped = 0; double alpha = 0; };
    Step Advance(double wallSeconds)
    {
        Step result;
        if (!std::isfinite(wallSeconds) || wallSeconds < 0 || wallSeconds > 60) return result;
        const double total = debt_ + wallSeconds * kSimulationHz;
        const uint64_t due = static_cast<uint64_t>(std::floor(total + 1e-9));
        debt_ = (std::max)(0.0, total - double(due));
        result.ticks = int((std::min)(uint64_t(4), due));
        result.dropped = due - result.ticks;
        result.alpha = debt_;
        return result;
    }
    void Reset() { debt_ = 0; }
private:
    double debt_ = 0;
};

} // namespace eth::patch
