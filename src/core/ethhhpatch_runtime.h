#pragma once
#include "precomp.h"
#include "ethhhpatch_policy.h"
#include "ethhhpatch_camera_policy.h"
#include "ethhhpatch_lens_policy.h"

namespace eth::patch::runtime {

// Hooks are installed lazily for the acknowledged Apply/Restore transaction.
bool InstallHooks();
void Shutdown();
bool HooksReady();
bool Active();
bool ActivateRenderSplit(float aspect, unsigned width = 0, unsigned height = 0, int targetHz = 0);
void DeactivateRenderSplit();
void OnTickCompleted();
void BeforeGamePresent();
bool AdjustHudProjection(uintptr_t returnAddress, const D3DMATRIX* input, D3DMATRIX& output);

enum class ApplyState { Idle, Applying, Active, RollingBack, RolledBack, Failed };
struct ApplyStatus {
    ApplyState state = ApplyState::Idle;
    DisplayPlan requested;
    int targetHz = 0;
    std::string detail;
};

// Requests run on the game window/render thread, through the engine Reset path.
bool RequestDisplayPlan(const DisplayPlan& plan, int monitorHz, std::string& error);
bool RequestRestore(std::string& error);
void OnResetResult(const D3DPRESENT_PARAMETERS* parameters, HRESULT result);
ApplyStatus ReadApplyStatus();
void SetExternalCameraOwned(bool owned);
void NotifyMoviePresent();
void SetOrbitEnabled(bool enabled);
void ResumeOrbit();
OrbitState ReadOrbitState();

struct OrbitTuning { float sensitivity = 1.0f; bool invertY = false; };
struct OrbitTelemetry {
    bool enabled = false, following = false, capture = false;
    OrbitState state = OrbitState::Waiting;
    float nativeDistance = 0, orbitDistance = 0, yawDegrees = 0, pitchDegrees = 0;
    uint64_t transitions = 0;
    uint32_t renderGuardFailures = 0, renderGuardMask = 0;
};
OrbitTuning ReadOrbitTuning();
void SetOrbitTuning(float sensitivity, bool invertY);
OrbitTelemetry ReadOrbitTelemetry();
void SetOrbitDistance(float distance);
void ZoomOrbit(float notches);
struct NormalFovTuning { bool enabled = false; float verticalDegrees = 60; };
NormalFovTuning ReadNormalFovTuning();
void SetNormalFovTuning(bool enabled, float verticalDegrees);

struct EntityTelemetry {
    bool enabled = true;
    size_t sampled = 0, paired = 0;
    uint64_t interpolatedSubmissions = 0, nativeSubmissions = 0;
    size_t owners = 0, sampleQueries = 0, renderQueries = 0;
    double sampleMs = 0, renderMs = 0;
    uint64_t sampleBudgetStops = 0, renderBudgetStops = 0;
    bool listComplete = false;
};
EntityTelemetry ReadEntityTelemetry();
void SetEntityInterpolationEnabled(bool enabled);

} // namespace eth::patch::runtime
