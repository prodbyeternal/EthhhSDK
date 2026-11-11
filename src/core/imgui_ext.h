#pragma once
#include "precomp.h"

namespace eth::ui {

// ------------------------------------------------------------------ context
void   EnsureContext();
void   LoadFonts(float scale);
void   ApplyTheme();
void   SetAccent(ImU32 color);
ImU32  Accent();
ImU32  AccentDim(float alpha = 0.25f);
ImU32  WithAlpha(ImU32 col, float alpha);
float  Scale();
float  Dpi();

ImFont* FontRegular();
ImFont* FontSmall();
ImFont* FontBold();
ImFont* FontTitle();

ImU32 HexToU32(const char* hex, ImU32 fallback = IM_COL32(108, 140, 255, 255));
ImVec4 HexToColor(const char* hex);
void  U32ToHex(ImU32 col, char out[7]);

// ------------------------------------------------------------------ widgets
bool ToggleSwitch(const char* id, bool* value, float widthScale = 1.0f);
bool Checkbox(const char* label, bool* value);
bool CheckboxFlags(const char* label, unsigned int* value, unsigned int flags);
bool GlassSlider(const char* label, float* value, float low, float high, const char* format = "%.2f");
bool SearchField(const char* id, char* value, size_t capacity, const char* hint);
void SetManagerOpacity(float alpha);
void SectionHeader(const char* title, const char* subtitle = nullptr);
void Divider(float spacing = 8.0f);
void Badge(const char* text, ImU32 color);
void HelpMarker(const char* text);
bool AccentButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool GhostButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool NavItem(const char* id, const char* label, bool selected, const char* shortcut = nullptr);
void StatRow(const char* label, const char* value);
bool BeginCard(const char* id, const ImVec2& size);
void EndCard();
void DrawCardBg(ImVec2 pos, ImVec2 size, ImU32 color, float rounding = 10.0f);
void DrawLogo(ImVec2 pos, float size);

// Custom-chrome window (title/subtitle/close, draggable header, padded content).
// If this returns true, the caller must draw content and call EndChromeWindow().
bool BeginChromeWindow(const char* id, const char* title, const char* subtitle,
                       bool* open, const ImVec2& defaultSize,
                       ImGuiWindowFlags flags = 0);
void EndChromeWindow();

// ------------------------------------------------------------------ helpers
bool BeginTabBarC(const char* id);
bool TabItemC(const char* label, bool selected);
bool InputTextString(const char* label, std::string& value, size_t maxLen = 256);
bool InputTextMultilineString(const char* label, std::string& value, const ImVec2& size);
bool ComboString(const char* label, int* current, const std::vector<std::string>& items);

} // namespace eth::ui
