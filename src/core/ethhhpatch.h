#pragma once
#include "precomp.h"
#include "ethhhpatch_policy.h"
#include "ethhhpatch_projection.h"
#include "ethhhpatch_cadence.h"

namespace eth::patch {

struct MonitorInfo { int width = 0, height = 0, hz = 0; };
struct Status {
    bool sampled = false, deviceReady = false;
    unsigned width = 0, height = 0, viewportWidth = 0, viewportHeight = 0;
    bool windowed = true;
    unsigned presentInterval = 0, fullscreenHz = 0;
    double tickCallsPerSecond = 0, activeTicksPerSecond = 0;
    double gamePresentsPerSecond = 0, simulationSecondsPerSecond = 0;
    uint64_t failedPresents = 0, moviePresents = 0;
    float engineAspect = 0;
    bool projectionValid = false;
    uint64_t projectionSamples = 0;
    ProjectionMetrics projection;
    float projectionSourceAspect = 0, projectionSourceFov = 0;
    double projectionAspectError = 0, projectionFovErrorDegrees = 0;
    double observationAgeSeconds = -1;
    CadenceCounts cadence;
    uint64_t resetAttempts = 0, resetSuccesses = 0, resetFailures = 0;
    int32_t lastResetHresult = 0;
    bool lastResetParametersValid = false;
    unsigned resetWidth = 0, resetHeight = 0, resetInterval = 0, resetFullscreenHz = 0;
    bool resetWindowed = true;
};

MonitorInfo ReadMonitor(HWND window);
Status ReadStatus();
void OnTick(float timeStep);
void OnPresent(IDirect3DDevice9* device, HRESULT result, bool gamePath, bool moviePath);
void OnReset(const D3DPRESENT_PARAMETERS* parameters, HRESULT result);
void OnWorldProjection(const D3DMATRIX* matrix);

#ifdef ETH_TESTING
// Hidden layout fixture only, absent from the native SDK build/export table.
void SetPreviewStatus(const Status& status);
bool HasPreviewStatus();
#endif

// Foundation only: no display writes, limiter edits, timer replacement, engine
// invocation, SetWindowPos or forced Reset. The manager can save a future plan.
DisplayPlan LoadPlan();
bool SavePlan(const DisplayPlan& plan);

} // namespace eth::patch
