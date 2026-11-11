#pragma once
#include "precomp.h"

namespace eth {

namespace mods { struct ModInfo; }

namespace clr {

// Manages the .NET Framework runtime and the EthhhSDK.Managed.dll host.
// The managed host owns the per-mod C# objects and registers three callbacks
// (tick / gui / overlay) through EthHooks_SetManagedCallbacks.
bool Start(const std::vector<mods::ModInfo>& csharpMods);
void Shutdown();

bool Running();
bool CallbacksRegistered();

void Tick(int tickIndex);
void DrawGui();
void OnOverlayChanged(bool open);

// Called from EthHooks_SetManagedCallbacks (c_api.cpp).
void SetCallbacks(void* tick, void* gui, void* overlay);

// Called from the managed host to surface errors in the mod manager UI.
void ReportManagedError(const char* modId, const char* message);

} // namespace clr
} // namespace eth
