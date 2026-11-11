#pragma once
#include "precomp.h"

namespace eth::input {

void Init(HWND hwnd);
void Shutdown();
void SetWindow(HWND hwnd);
HWND Window();

// Message plumbing (called from the window procedure hook).
void OnRawInput(HWND hwnd, LPARAM lParam);
void OnOverlayToggled(bool overlayOpen);

// Per-frame / per-tick.
void PumpKeys();                 // update key edge states
void PumpCapture();              // keep clip rect valid while capturing
void ResetTransient();
// Experimental, manual exact-Ninja WASD lease; must be renewed every mod tick.
bool LeaseMovementKeys(uintptr_t player, bool enabled);
void BeginNativeTick();
void EndNativeTick();
void FilterNativeKeyboard(void* keyboard);

bool KeyDown(int vk);
bool KeyPressed(int vk);         // true once per press (since last PumpKeys)
bool MouseDown(int button);      // 0=left 1=right 2=middle

void GetMouseDelta(float& dx, float& dy);   // accumulated raw input, not consumed
void ConsumeMouseDelta();
void GetMouseWheel(float& notches);         // accumulated wheel notches, not consumed
void ConsumeMouseWheel();

// Mouse capture (FPS camera). Clips the cursor to the game window and hides it.
bool SetCapture(bool on);
bool CaptureActive();
bool OverlayOpen();

float SensitivityScale();        // config helper (core/mouse_scale)
void  SetSensitivityScale(float value);

const char* KeyName(int vk);

} // namespace eth::input
