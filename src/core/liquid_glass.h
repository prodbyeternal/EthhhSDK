#pragma once
#include "precomp.h"
#include "glass.h"

namespace eth::ui {
bool InitGlass(IDirect3DDevice9* device);
void ShutdownGlass();
void InvalidateGlass();
bool RecreateGlass();
void BeginGlassFrame();
void RenderGlassDrawData(IDirect3DDevice9* device, ImDrawData* data);
// Original abstract backdrop for the launcher only; never captures the desktop.
void DrawLauncherBackdrop(IDirect3DDevice9* device, int width, int height);
void GlassPanel(ImVec2 pos, ImVec2 size, float radius, Glass::Material material = Glass::Material::Regular, float fade = 1);
ImU32 ThemeInk();
ImU32 ThemeSoftInk();
ImU32 ThemeSurface();
}
