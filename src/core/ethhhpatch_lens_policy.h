#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace eth::patch::runtime {

// Renderer_prepare tail-jumps to View_Render. The return address is the
// App_render CALL's continuation, NOT an address inside Renderer_prepare.
class PrimaryViewPass {
public:
    void Reset() { seen_ = false; }
    bool Acquire(bool inFrame, uintptr_t caller, uintptr_t expected,
        uintptr_t self, uintptr_t liveView) {
        if (!inFrame || seen_ || caller != expected || !self || self != liveView) return false;
        seen_ = true;
        return true;
    }
private:
    bool seen_ = false;
};

inline float NormalFovDegrees(float value) {
    return std::isfinite(value) ? (value < 30 ? 30 : value > 120 ? 120 : value) : 60;
}

// Render-only View lens: Camera+0x108 remains native for simulation and scripted
// lens-cut detection. Projection and the sphere culler read these SAME values.
struct RenderLensBackup {
    uintptr_t view = 0;
    float values[4]{}, projection[16]{}, installedValues[4]{};
    bool pending = false, installed = false;

    template<class Read, class Write> bool Apply(uintptr_t address, float degrees, Read read, Write write) {
        if (pending || !address || !std::isfinite(degrees) || degrees < 30 || degrees > 120) return false;
        if (!read(address + 0x08, values, sizeof(values)) ||
            !read(address + 0x120, projection, sizeof(projection))) return false;
        for (float value : values) if (!std::isfinite(value)) return false;
        const float angle = degrees * 0.017453292519943295f;
        installedValues[0] = installedValues[1] = angle;
        installedValues[2] = std::tan(angle * 0.5f);
        installedValues[3] = 1.0f / std::cos(angle * 0.5f);
        view = address; pending = true; installed = false;
        if (!write(address + 0x08, installedValues, sizeof(installedValues))) return false;
        installed = true;
        return true;
    }
    template<class Read, class Write> bool Restore(uintptr_t liveView, Read read, Write write) {
        if (!pending) return true;
        pending = false;
        if (liveView != view) return false;
        float current[4]{};
        if (installed && (!read(view + 0x08, current, sizeof(current)) ||
            std::memcmp(current, installedValues, sizeof(current)))) return false;
        const bool a = write(view + 0x08, values, sizeof(values));
        const bool b = write(view + 0x120, projection, sizeof(projection));
        return a && b;
    }
};

} // namespace eth::patch::runtime
