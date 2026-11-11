#pragma once
#include "precomp.h"

namespace eth::hooks {

// MinHook bootstrap. Safe to call once, after the loader has released its lock
// (i.e. from EthhhSDK_EntryGate).
bool Init();

// Fn_Tick hook - the game's fixed 60 Hz logic tick. Runs 1..4 times per frame.
bool InstallTickHook();
bool InstallMovementInputHook(); // lazy, exact keyboard-poll bytes only

// Watches for the game window + D3D9 device and installs WndProc/EndScene hooks.
void StartGameWatcher();

void Shutdown();

bool TickHooked();
bool D3DHooked();
bool WndProcHooked();

using TickCallback = std::function<void(int tickIndex)>;
void AddTickCallback(TickCallback cb);
void ClearTickCallbacks();

} // namespace eth::hooks
