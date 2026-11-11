// Offline matrix inspection only. No native helper invocation or game writes.
#include "ethhhpatch_projection.h"
#include "ethhhpatch_policy.h"
#include "ethhhpatch_cadence.h"
#include "ethhhpatch_runtime_policy.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using namespace eth::patch;
static int checks = 0;
static void Check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { fprintf(stderr, "FAIL %s\n", label); exit(1); }
}

static void MakeMatrix(float (&m)[16], double verticalDegrees, double aspect, double nearPlane, double farPlane)
{
    memset(m, 0, sizeof(m));
    const double yScale = 1 / std::tan(verticalDegrees * 3.14159265358979323846 / 360);
    m[0] = (float)(yScale / aspect); m[5] = (float)yScale;
    m[10] = (float)(farPlane / (farPlane - nearPlane)); m[11] = 1;
    m[14] = (float)(-nearPlane * farPlane / (farPlane - nearPlane));
}

int main()
{
    ProjectionMetrics result;
    result.aspect = 123;
    Check(!InspectPerspectiveLH(nullptr, result) && result.aspect == 0, "null matrix clears metrics");
    float matrix[16] = {}, before[16];
    Check(!InspectPerspectiveLH(matrix, result), "zero matrix is not world perspective");
    for (double aspect : {1.0, 4.0/3, 16.0/9, 21.0/9, 32.0/9}) {
        for (double fov : {35.0, 60.0, 90.0, 113.6, 120.0, 150.0}) {
            MakeMatrix(matrix, fov, aspect, .01, 100);
            memcpy(before, matrix, sizeof(matrix));
            Check(InspectPerspectiveLH(matrix, result), "canonical LH perspective accepted");
            Check(!memcmp(before, matrix, sizeof(matrix)), "inspector never changes input bytes");
            Check(std::abs(result.aspect - aspect) < 1e-6, "float matrix aspect roundtrip");
            Check(std::abs(result.verticalFovDegrees - fov) < 1e-5, "full vertical FOV roundtrip");
            Check(std::abs(result.horizontalFovDegrees - HorizontalFov(fov, aspect)) < 1e-5, "Hor+ FOV from submitted matrix");
            Check(std::abs(result.nearPlane - .01) < 1e-8 && std::abs(result.farPlane - 100) < .1,
                "clip-plane estimates account for float precision");
        }
    }
    for (int index = 0; index < 16; ++index) {
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
            MakeMatrix(matrix, 60, 16.0/9, .01, 100); matrix[index] = invalid;
            result.aspect = 123;
            Check(!InspectPerspectiveLH(matrix, result) && result.aspect == 0 && result.farPlane == 0,
                "non-finite field cannot expose stale metrics");
        }
    }
    for (int index : {1,2,3,4,6,7,8,9,12,13,15}) {
        MakeMatrix(matrix, 60, 16.0/9, .01, 100); matrix[index] = .1f;
        Check(!InspectPerspectiveLH(matrix, result), "noncanonical or off-center matrix refused");
    }
    MakeMatrix(matrix, 60, 16.0/9, .01, 100); matrix[11] = -1;
    Check(!InspectPerspectiveLH(matrix, result), "RH matrix refused");
    MakeMatrix(matrix, 60, 16.0/9, .01, 100); matrix[11] = 0; matrix[15] = 1;
    Check(!InspectPerspectiveLH(matrix, result), "HUD orthographic matrix refused");
    for (int index : {0,5,10,14}) {
        MakeMatrix(matrix, 60, 16.0/9, .01, 100); matrix[index] = 0;
        Check(!InspectPerspectiveLH(matrix, result), "degenerate projection scale/depth refused");
    }
    MakeMatrix(matrix, 60, 16.0/9, .01, 100); matrix[10] = 1;
    Check(!InspectPerspectiveLH(matrix, result), "unrecoverable float far plane refused, not invented");

    // Counterexample for the ONE verified CPU sphere test at 0x449420, radius=0
    // and identity camera view. It tests X/Y against +/-Z, without aspect.
    // This is not a replacement culler or a model of all engine visibility.
    MakeMatrix(matrix, 90, 16.0/9, .01, 100);
    const double x = 15, y = 0, z = 10;
    const double ndcX = x * matrix[0] / z, ndcY = y * matrix[5] / z;
    const bool insideProjection = std::abs(ndcX) < 1 && std::abs(ndcY) < 1;
    const bool insideObservedCull = z >= 0 && z <= 100 && std::abs(x) <= z && std::abs(y) <= z;
    Check(insideProjection && !insideObservedCull, "widescreen-visible point rejected by aspect-blind CPU test");
    Check(.1 * 100 > .1 * 60, "render-driven HUD pulse advances faster at 100 than 60 renders");

    RenderCadence cadence;
    cadence.GamePresent(1);
    Check(!cadence.Counts().zeroTickFrames, "first sample is not a render-only claim");
    cadence.Tick(); cadence.GamePresent(2);
    Check(!cadence.Counts().zeroTickFrames && !cadence.Counts().catchupFrames, "one tick per new engine frame");
    cadence.GamePresent(3);
    Check(cadence.Counts().zeroTickFrames == 1, "new engine frame with no completed tick observed");
    cadence.GamePresent(3);
    Check(cadence.Counts().repeatedFramePresents == 1 && cadence.Counts().zeroTickFrames == 1,
        "repeated frame presentation is not a scene rebuild");
    cadence.Tick(); cadence.Tick(); cadence.GamePresent(4);
    Check(cadence.Counts().catchupFrames == 1, "catch-up frame counted independently");
    cadence.Tick(); cadence.GamePresent(5);
    Check(cadence.Counts().zeroTickFrames == 1, "paused completed tick is not zero-tick evidence");
    cadence.InvalidateBaseline(); cadence.GamePresent(100);
    Check(cadence.Counts().zeroTickFrames == 1, "movie or unreadable frame breaks cadence baseline");
    cadence.Reset(); cadence.GamePresent(UINT32_MAX); cadence.Tick(); cadence.GamePresent(0);
    Check(!cadence.Counts().zeroTickFrames && !cadence.Counts().repeatedFramePresents, "frame counter wrap remains a new frame");
    cadence.Reset();
    Check(!cadence.Counts().zeroTickFrames && !cadence.Counts().catchupFrames && !cadence.Counts().repeatedFramePresents,
        "device Reset clears all cadence evidence");
    Check(FreshObservation(100, 99) && FreshObservation(101.5, 99), "fresh rates remain available through grace interval");
    Check(!FreshObservation(102, 99) && !FreshObservation(100, 101) && !FreshObservation(100, 0),
        "stale, backward-clock or absent observation hidden");
    Check(!FreshObservation(std::numeric_limits<double>::quiet_NaN(), 99) &&
        !FreshObservation(100, std::numeric_limits<double>::infinity()) && !FreshObservation(100, 99, -1),
        "invalid observation timestamps refused");

    Check(PerspectiveSphereVisible(15, 0, 10, 0, 16.0f/9, 1, 100),
        "widescreen point accepted by repaired horizontal frustum");
    Check(!PerspectiveSphereVisible(18, 0, 10, 0, 16.0f/9, 1, 100),
        "point outside horizontal FOV rejected");
    Check(PerspectiveSphereVisible(0, 9, 10, 0, 16.0f/9, 1, 100) &&
        !PerspectiveSphereVisible(0, 11, 10, 0, 16.0f/9, 1, 100),
        "vertical point bound uses vertical FOV");
    Check(PerspectiveSphereVisible(5.7f, 0, 10, 0, 1, .577350269f, 100) &&
        !PerspectiveSphereVisible(6, 0, 10, 0, 1, .577350269f, 100),
        "point bound changes with submitted FOV");
    Check(PerspectiveSphereVisible(11, 0, 10, 1, 1, 1, 100),
        "sphere-plane margin includes radius and plane normalization");
    Check(PerspectiveSphereVisible(0, 0, -.5f, 1, 1, 1, 100) &&
        !PerspectiveSphereVisible(0, 0, -2, 1, 1, 1, 100), "near-plane sphere intersection");
    Check(PerspectiveSphereVisible(0, 0, 100.5f, 1, 1, 1, 100) &&
        !PerspectiveSphereVisible(0, 0, 102, 1, 1, 1, 100), "far-plane sphere intersection");
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        Check(!PerspectiveSphereVisible(invalid,0,1,0,1,1,100) &&
            !PerspectiveSphereVisible(0,0,1,0,invalid,1,100) &&
            !PerspectiveSphereVisible(0,0,1,0,1,invalid,100), "invalid culling inputs refused");
    }
    Check(!PerspectiveSphereVisible(0,0,1,-1,1,1,100) &&
        !PerspectiveSphereVisible(0,0,1,0,0,1,100), "negative radius and invalid aspect refused");

    PulseClock pulse;
    Check(pulse.Advance(10) == 1 && pulse.Advance(10) == 0 && pulse.Advance(11) == 1,
        "HUD pulse advances once per completed tick, not render");
    Check(pulse.Advance(14) == 3 && pulse.Advance(100) == 1,
        "HUD pulse follows bounded catch-up and rebases transitions");
    pulse = {}; pulse.initialized = true; pulse.lastTick = UINT32_MAX;
    Check(pulse.Advance(0) == 1, "HUD pulse tick wrap arithmetic");
    Check(std::abs(PulsePreAdjustment(0) + .1f) < 1e-7f && PulsePreAdjustment(1) == 0 &&
        std::abs(PulsePreAdjustment(4) - .3f) < 1e-6f, "pre-adjustment compensates original fixed increment");
    for (int renderHz : {60, 100, 120, 144, 240, 360}) {
        pulse = {}; unsigned advances = 0;
        for (int frame = 0; frame < renderHz; ++frame)
            advances += pulse.Advance((uint32_t)((int64_t)frame * 60 / renderHz));
        Check(advances == 60, "HUD phase remains 60-tick based at high render rates");
    }
    Check(std::abs(HudHorizontalScale(4.0f/3) - 1) < 1e-7f &&
        std::abs(HudHorizontalScale(16.0f/9) - .75f) < 1e-7f,
        "4:3 HUD remains unchanged at authored aspect and centered at 16:9");
    Check(HudHorizontalScale(32.0f/9) < HudHorizontalScale(16.0f/9),
        "ultrawide HUD receives stronger horizontal containment");
    Check(!HudHorizontalScale(0) && !HudHorizontalScale(std::numeric_limits<float>::quiet_NaN()),
        "invalid HUD aspect scaling refused");
    auto fit = FitCentered(640,480,1920,1080);
    Check(std::abs(fit.scale-2.25f)<1e-7f && std::abs(fit.x-240)<1e-6f && fit.y==0,
        "4:3 movie is pillarboxed and centered at 1080p");
    fit = FitCentered(640,480,3840,2160);
    Check(std::abs(fit.scale-4.5f)<1e-7f && std::abs(fit.x-480)<1e-6f && fit.y==0,
        "4:3 movie is pillarboxed and centered at 4K");
    fit = FitCentered(1920,1080,1920,1080);
    Check(fit.scale==1 && fit.x==0 && fit.y==0,"matching movie aspect fills target exactly");
    fit = FitCentered(1920,1080,1280,1024);
    Check(fit.scale>0 && fit.x==0 && fit.y>0,"wide movie is letterboxed on narrow target");
    Check(!FitCentered(0,480,1920,1080).scale && !FitCentered(640,480,0,1080).scale,
        "invalid movie dimensions refused");
    printf("PASS EthhhPatch projection/runtime policy: %d assertions (offline; runtime activation is user-gated)\n", checks);
}
