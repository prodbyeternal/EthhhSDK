#pragma once
#include <cmath>
#include <initializer_list>

namespace eth::patch {

struct ProjectionMetrics {
    double aspect = 0, verticalFovDegrees = 0, horizontalFovDegrees = 0;
    double nearPlane = 0, farPlane = 0;
};

// Inspect only the canonical row-major LH perspective matrix produced by the
// verified 0x45ED0F builder. HUD/orthographic/off-center/invalid matrices are not
// treated as world perspective. No engine/D3D writes or calls.
inline bool InspectPerspectiveLH(const float* m, ProjectionMetrics& out)
{
    out = {};
    if (!m) return false;
    for (int i = 0; i < 16; ++i) if (!std::isfinite(m[i])) return false;
    for (int i : { 1, 2, 3, 4, 6, 7, 8, 9, 12, 13, 15 })
        if (std::abs(m[i]) > 1e-6f) return false;
    if (std::abs(m[11] - 1.0f) > 1e-6f || m[0] <= 0 || m[5] <= 0 || m[10] <= 1 || m[14] >= 0)
        return false;
    constexpr double degreesPerRadian = 57.2957795130823208768;
    ProjectionMetrics result;
    result.aspect = double(m[5]) / m[0];
    result.verticalFovDegrees = 2 * std::atan(1.0 / m[5]) * degreesPerRadian;
    result.horizontalFovDegrees = 2 * std::atan(1.0 / m[0]) * degreesPerRadian;
    result.nearPlane = -double(m[14]) / m[10];
    result.farPlane = -double(m[14]) / (double(m[10]) - 1);
    if (!std::isfinite(result.aspect) || result.aspect < 0.1 || result.aspect > 16 ||
        result.nearPlane <= 0 || !std::isfinite(result.farPlane) || result.farPlane <= result.nearPlane)
        return false;
    out = result;
    return true;
}

} // namespace eth::patch
