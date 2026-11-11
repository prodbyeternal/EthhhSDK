#pragma once
#include "precomp.h"

namespace eth::overlay {

// Called after the original Fn_Tick. Handles pause/camera-freeze and mod ticks
// are dispatched separately by hooks.cpp -> ModManager.
void OnGameTick();

// EndScene render path (called from the D3D9 vtable hook).
void RenderFrame(IDirect3DDevice9* device, bool moviePath);

// Device Reset notifications, before and after the real Reset.
void OnDeviceReset(bool before, IDirect3DDevice9* device);

// Returns true when the message was consumed by the overlay.
bool OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void Toggle();
void SetVisible(bool visible);
bool IsVisible();
int  ToggleKey();
void SetToggleKey(int vk);
// Time-scale requests while paused change the resume value, not the pause.
bool DeferTimeScale(float value);

void NotifyRenderException();
void Shutdown();

} // namespace eth::overlay
