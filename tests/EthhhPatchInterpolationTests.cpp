#include "ethhhpatch_interpolation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using eth::patch::RenderInterpolationFraction;
static int checks = 0;
static void Check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { fprintf(stderr, "FAIL %s\n", label); exit(1); }
}
static void Near(float actual, float expected, const char* label, float epsilon = 0.00001f)
{
    Check(std::isfinite(actual) && std::fabs(actual - expected) <= epsilon, label);
}

int main()
{
    const float tick60 = 1000.0f / 60.0f, rate60 = 0.06f;

    // Timer_Update leaves a non-negative remainder below one tick.
    Near(RenderInterpolationFraction(tick60, rate60, 0.0f), 0.0f, "zero remainder renders current pose");
    Near(RenderInterpolationFraction(tick60, rate60, tick60 * 0.5f), 0.5f, "half tick blends halfway");
    Near(RenderInterpolationFraction(tick60, rate60, tick60 * 0.25f), 0.25f, "quarter tick");
    Near(RenderInterpolationFraction(tick60, rate60, tick60 * 0.9999f), 0.9999f, "just below one tick");
    Near(RenderInterpolationFraction(tick60, rate60, tick60), 0.999999f, "full remainder clamps below next tick");
    Near(RenderInterpolationFraction(tick60, rate60, tick60 * 3.0f), 0.999999f, "oversized remainder clamps");
    Near(RenderInterpolationFraction(tick60, rate60, -1.0f), 0.0f, "negative remainder floors at zero");

    // The same rule must hold for other render rates; simulation stays 60 Hz.
    Near(RenderInterpolationFraction(10.0f, 0.1f, 2.5f), 0.25f, "100 Hz timer");
    Near(RenderInterpolationFraction(1000.0f / 144.0f, 0.144f, 500.0f / 144.0f), 0.5f, "144 Hz timer");

    // Reject foreign or uninitialised timer layouts instead of blending with
    // arbitrary memory. Callers keep the engine's own fraction (1.0).
    Near(RenderInterpolationFraction(0.0f, 0.0f, 0.0f), 1.0f, "uninitialised timer refused");
    Near(RenderInterpolationFraction(tick60, 0.0f, 5.0f), 1.0f, "missing tick rate refused");
    Near(RenderInterpolationFraction(0.0f, rate60, 5.0f), 1.0f, "missing milliseconds per tick refused");
    Near(RenderInterpolationFraction(-tick60, -rate60, 5.0f), 1.0f, "negative timer fields refused");
    Near(RenderInterpolationFraction(2000.0f, 0.0005f, 5.0f), 1.0f, "implausible tick length refused");
    Near(RenderInterpolationFraction(tick60, 0.05f, 5.0f), 1.0f, "inconsistent tick/rate product refused");
    Near(RenderInterpolationFraction(tick60, 0.5f, 5.0f), 1.0f, "implausible tick rate refused");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Near(RenderInterpolationFraction(nan, rate60, 5.0f), 1.0f, "NaN tick refused");
    Near(RenderInterpolationFraction(tick60, rate60, nan), 1.0f, "NaN remainder refused");
    Near(RenderInterpolationFraction(tick60, rate60, inf), 1.0f, "infinite remainder refused");

    printf("ethhhpatch interpolation: %d checks passed\n", checks);
    return 0;
}
