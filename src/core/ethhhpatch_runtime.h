#pragma once
#include "precomp.h"
#include "ethhhpatch_policy.h"

namespace eth::patch::runtime {

// Hooks are installed in pass-through mode. Activation is deliberately separate
// and is not currently exposed by the manager while the remaining HUD/movie and
// rollback gates are unfinished.
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

// Internal display transaction foundation. The manager intentionally does not
// call this until HUD/movie compatibility and user-facing confirmation are done.
bool RequestDisplayPlan(const DisplayPlan& plan, int monitorHz, std::string& error);
bool RequestRestore(std::string& error);
void OnResetResult(const D3DPRESENT_PARAMETERS* parameters, HRESULT result);
ApplyStatus ReadApplyStatus();
void SetExternalCameraOwned(bool owned);
void NotifyMoviePresent();

} // namespace eth::patch::runtime
