#pragma once
#include <cmath>
#include <cstdint>

namespace eth::patch {

// I-Ninja's fixed-step timer lives at App+0x08 (App = [0x50FFA4]):
//   +0x00 float milliseconds per tick   (1000 / 60 at the default rate)
//   +0x04 float ticks per millisecond   (0.06)
//   +0x08 float unconsumed remainder in milliseconds
//   +0x10      performance-counter clock (0x10 bytes)
// App+0x28 is the render interpolation fraction read by View_Render at
// 0x43F2E4..0x43F31F for position and by 0x4B3450 for orientation.
//
// The render step writes 1.0 into App+0x28 at 0x43E1DC, so the engine always
// renders the newest simulated pose. We replace that value only for the
// duration of View_Render with remainder / tick, which blends the previous
// pose (View+0x20/+0x30) into the current pose (View+0x40/+0x50) without
// running any additional simulation tick.
inline float RenderInterpolationFraction(float tickMilliseconds, float ticksPerMillisecond,
    float remainderMilliseconds)
{
    if (!std::isfinite(tickMilliseconds) || !std::isfinite(ticksPerMillisecond) ||
        !std::isfinite(remainderMilliseconds)) return 1.0f;
    // Reject an uninitialised/foreign timer layout rather than blending with
    // arbitrary memory. At 60 Hz these fields are 16.6667 and 0.06.
    if (tickMilliseconds < 1.0f || tickMilliseconds > 1000.0f) return 1.0f;
    if (ticksPerMillisecond <= 0.0f || ticksPerMillisecond > 10.0f) return 1.0f;
    if (std::abs(double(tickMilliseconds) * double(ticksPerMillisecond) - 1.0) > 0.02)
        return 1.0f;

    float fraction = remainderMilliseconds / tickMilliseconds;
    if (!std::isfinite(fraction) || fraction <= 0.0f) return 0.0f;
    // Timer_Update floors whole ticks, so remainder < tick in normal motion.
    // Keep the render strictly behind the next tick even if a debugger or a
    // timer-rate change hands us a slightly larger value.
    if (fraction >= 1.0f) return 0.999999f;
    return fraction;
}

} // namespace eth::patch
