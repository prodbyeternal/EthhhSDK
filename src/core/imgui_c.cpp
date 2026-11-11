// ===========================================================================
//  C ABI for Dear ImGui (used by C# mods via P/Invoke).
// ===========================================================================
#include "precomp.h"
#include "imgui_ext.h"
#include "util.h"
#include <cfloat>

using namespace eth;

namespace {

ImVec2 V2(float x, float y) { return ImVec2(x, y); }

void PushVec2Out(float* x, float* y, ImVec2 v)
{
    if (x) *x = v.x;
    if (y) *y = v.y;
}

ImFont* FontByName(const char* name)
{
    if (!name) return ui::FontRegular();
    if (!strcmp(name, "small")) return ui::FontSmall();
    if (!strcmp(name, "bold"))  return ui::FontBold();
    if (!strcmp(name, "title")) return ui::FontTitle();
    return ui::FontRegular();
}

} // namespace

// ---------------------------------------------------------------- context ---
ETH_CAPI(void, EthImGui_EnsureContext, (void)) { ui::EnsureContext(); }
ETH_CAPI(const char*, EthImGui_GetVersion, (void)) { return ImGui::GetVersion(); }
ETH_CAPI(float, EthImGui_GetScale, (void)) { return ui::Scale(); }
ETH_CAPI(unsigned int, EthImGui_GetAccent, (void)) { return ui::Accent(); }
ETH_CAPI(unsigned int, EthImGui_RGBA, (float r, float g, float b, float a))
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}
ETH_CAPI(unsigned int, EthImGui_GetStyleColor, (int index))
{
    if (index < 0 || index >= ImGuiCol_COUNT) return IM_COL32_WHITE;
    return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4((ImGuiCol)index));
}
ETH_CAPI(void, EthImGui_PushAccentText, (void))
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::Accent()));
}

// ---------------------------------------------------------------- windows ---
ETH_CAPI(int, EthImGui_Begin, (const char* name, int flags))
{
    return ImGui::Begin(name, nullptr, (ImGuiWindowFlags)flags) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_BeginClosable, (const char* name, int* open, int flags))
{
    bool value = open ? (*open != 0) : true;
    bool visible = ImGui::Begin(name, open ? &value : nullptr, (ImGuiWindowFlags)flags);
    if (open) *open = value ? 1 : 0;
    return visible ? 1 : 0;
}
ETH_CAPI(void, EthImGui_End, (void)) { ImGui::End(); }

ETH_CAPI(int, EthImGui_BeginChild, (const char* id, float w, float h, int border))
{
    return ImGui::BeginChild(id, V2(w, h), border ? ImGuiChildFlags_Borders : 0) ? 1 : 0;
}
ETH_CAPI(void, EthImGui_EndChild, (void)) { ImGui::EndChild(); }

ETH_CAPI(int, EthImGui_BeginTabBar, (const char* id)) { return ImGui::BeginTabBar(id) ? 1 : 0; }
ETH_CAPI(void, EthImGui_EndTabBar, (void)) { ImGui::EndTabBar(); }
ETH_CAPI(int, EthImGui_BeginTabItem, (const char* label, int selected))
{
    return ImGui::BeginTabItem(label, nullptr, selected ? ImGuiTabItemFlags_SetSelected : 0) ? 1 : 0;
}
ETH_CAPI(void, EthImGui_EndTabItem, (void)) { ImGui::EndTabItem(); }

ETH_CAPI(int, EthImGui_BeginTable, (const char* id, int columns, int flags))
{
    return ImGui::BeginTable(id, columns, (ImGuiTableFlags)flags) ? 1 : 0;
}
ETH_CAPI(void, EthImGui_EndTable, (void)) { ImGui::EndTable(); }
ETH_CAPI(void, EthImGui_TableSetupColumn, (const char* label, int flags, float width))
{
    ImGui::TableSetupColumn(label, (ImGuiTableColumnFlags)flags, width);
}
ETH_CAPI(void, EthImGui_TableHeadersRow, (void)) { ImGui::TableHeadersRow(); }
ETH_CAPI(void, EthImGui_TableNextRow, (void)) { ImGui::TableNextRow(); }
ETH_CAPI(int, EthImGui_TableNextColumn, (void)) { return ImGui::TableNextColumn() ? 1 : 0; }
ETH_CAPI(void, EthImGui_TableSetColumnEnabled, (int column, int enabled))
{
    ImGui::TableSetColumnEnabled(column, enabled != 0);
}

// ------------------------------------------------------------------- text ---
ETH_CAPI(void, EthImGui_Text, (const char* text))
{
    ImGui::TextUnformatted(text ? text : "");
}
ETH_CAPI(void, EthImGui_TextColored, (unsigned int col, const char* text))
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col));
    ImGui::TextUnformatted(text ? text : "");
    ImGui::PopStyleColor();
}
ETH_CAPI(void, EthImGui_TextDisabled, (const char* text)) { ImGui::TextDisabled("%s", text ? text : ""); }
ETH_CAPI(void, EthImGui_LabelText, (const char* label, const char* text))
{
    ImGui::LabelText(label, "%s", text ? text : "");
}
ETH_CAPI(void, EthImGui_BulletText, (const char* text)) { ImGui::BulletText("%s", text ? text : ""); }
ETH_CAPI(void, EthImGui_Separator, (void)) { ImGui::Separator(); }
ETH_CAPI(void, EthImGui_SeparatorText, (const char* text)) { ImGui::SeparatorText(text ? text : ""); }
ETH_CAPI(void, EthImGui_SameLine, (float offset, float spacing)) { ImGui::SameLine(offset, spacing); }
ETH_CAPI(void, EthImGui_Spacing, (void)) { ImGui::Spacing(); }
ETH_CAPI(void, EthImGui_Dummy, (float w, float h)) { ImGui::Dummy(V2(w, h)); }
ETH_CAPI(void, EthImGui_NewLine, (void)) { ImGui::NewLine(); }
ETH_CAPI(void, EthImGui_AlignTextToFramePadding, (void)) { ImGui::AlignTextToFramePadding(); }
ETH_CAPI(void, EthImGui_Indent, (float amount)) { ImGui::Indent(amount); }
ETH_CAPI(void, EthImGui_Unindent, (float amount)) { ImGui::Unindent(amount); }
ETH_CAPI(void, EthImGui_PushTextWrapPos, (float pos)) { ImGui::PushTextWrapPos(pos); }
ETH_CAPI(void, EthImGui_PopTextWrapPos, (void)) { ImGui::PopTextWrapPos(); }

// ---------------------------------------------------------------- widgets ---
ETH_CAPI(int, EthImGui_Button, (const char* label, float w, float h))
{
    return ImGui::Button(label, V2(w, h)) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_SmallButton, (const char* label)) { return ImGui::SmallButton(label) ? 1 : 0; }
ETH_CAPI(int, EthImGui_InvisibleButton, (const char* id, float w, float h))
{
    return ImGui::InvisibleButton(id, V2(w, h)) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_Checkbox, (const char* label, int* value))
{
    bool v = value && *value;
    bool changed = ImGui::Checkbox(label, &v);
    if (value) *value = v ? 1 : 0;
    return changed ? 1 : 0;
}
ETH_CAPI(int, EthImGui_RadioButton, (const char* label, int active))
{
    return ImGui::RadioButton(label, active != 0) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_Selectable, (const char* label, int selected))
{
    return ImGui::Selectable(label, selected != 0) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_SliderFloat, (const char* label, float* value, float minV, float maxV, const char* fmt))
{
    if (!value) return 0;
    return ImGui::SliderFloat(label, value, minV, maxV, fmt ? fmt : "%.3f") ? 1 : 0;
}
ETH_CAPI(int, EthImGui_SliderInt, (const char* label, int* value, int minV, int maxV))
{
    if (!value) return 0;
    return ImGui::SliderInt(label, value, minV, maxV) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_SliderFloat4, (const char* label, float* values, float minV, float maxV))
{
    if (!values) return 0;
    return ImGui::SliderFloat4(label, values, minV, maxV) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_DragFloat, (const char* label, float* value, float speed, float minV, float maxV, const char* fmt))
{
    if (!value) return 0;
    return ImGui::DragFloat(label, value, speed, minV, maxV, fmt ? fmt : "%.3f") ? 1 : 0;
}
ETH_CAPI(int, EthImGui_DragInt, (const char* label, int* value, float speed, int minV, int maxV))
{
    if (!value) return 0;
    return ImGui::DragInt(label, value, speed, minV, maxV) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_InputText, (const char* label, char* buffer, int bufferSize))
{
    if (!buffer || bufferSize <= 0) return 0;
    return ImGui::InputText(label, buffer, (size_t)bufferSize) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_InputInt, (const char* label, int* value))
{
    if (!value) return 0;
    return ImGui::InputInt(label, value) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_InputFloat, (const char* label, float* value))
{
    if (!value) return 0;
    return ImGui::InputFloat(label, value) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_BeginCombo, (const char* label, const char* preview))
{
    return ImGui::BeginCombo(label, preview ? preview : "") ? 1 : 0;
}
ETH_CAPI(void, EthImGui_EndCombo, (void)) { ImGui::EndCombo(); }
ETH_CAPI(int, EthImGui_Combo, (const char* label, int* current, const char* const* items, int count))
{
    if (!current || !items || count <= 0) return 0;
    int cur = std::clamp(*current, 0, count - 1);
    bool changed = false;
    if (ImGui::BeginCombo(label, items[cur])) {
        for (int i = 0; i < count; ++i) {
            bool selected = i == cur;
            if (ImGui::Selectable(items[i], selected)) { cur = i; changed = true; }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    *current = cur;
    return changed ? 1 : 0;
}
ETH_CAPI(void, EthImGui_ProgressBar, (float fraction, float w, float h, const char* overlay))
{
    ImGui::ProgressBar(fraction, V2(w, h), overlay);
}
ETH_CAPI(void, EthImGui_PlotLines, (const char* label, const float* values, int count, float height))
{
    if (!values || count <= 0) return;
    ImGui::PlotLines(label, values, count, 0, nullptr, FLT_MAX, FLT_MAX, V2(0, height));
}
ETH_CAPI(int, EthImGui_ColorEdit3, (const char* label, float* rgb))
{
    if (!rgb) return 0;
    return ImGui::ColorEdit3(label, rgb) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_ColorEdit4, (const char* label, float* rgba))
{
    if (!rgba) return 0;
    return ImGui::ColorEdit4(label, rgba) ? 1 : 0;
}

ETH_CAPI(int, EthImGui_CollapsingHeader, (const char* label, int defaultOpen))
{
    return ImGui::CollapsingHeader(label, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_TreeNode, (const char* label, int flags))
{
    return ImGui::TreeNodeEx(label, (ImGuiTreeNodeFlags)flags) ? 1 : 0;
}
ETH_CAPI(void, EthImGui_TreePop, (void)) { ImGui::TreePop(); }
ETH_CAPI(void, EthImGui_SetNextItemOpen, (int open)) { ImGui::SetNextItemOpen(open != 0); }

// -------------------------------------------------------------------- misc ---
ETH_CAPI(void, EthImGui_PushIDStr, (const char* id)) { ImGui::PushID(id); }
ETH_CAPI(void, EthImGui_PushIDInt, (int id)) { ImGui::PushID(id); }
ETH_CAPI(void, EthImGui_PopID, (void)) { ImGui::PopID(); }
ETH_CAPI(void, EthImGui_PushItemWidth, (float width)) { ImGui::PushItemWidth(width); }
ETH_CAPI(void, EthImGui_PopItemWidth, (void)) { ImGui::PopItemWidth(); }
ETH_CAPI(void, EthImGui_SetNextItemWidth, (float width)) { ImGui::SetNextItemWidth(width); }
ETH_CAPI(void, EthImGui_PushStyleColor, (int index, unsigned int color))
{
    ImGui::PushStyleColor((ImGuiCol)index, ImGui::ColorConvertU32ToFloat4(color));
}
ETH_CAPI(void, EthImGui_PopStyleColor, (int count)) { ImGui::PopStyleColor(count); }
ETH_CAPI(void, EthImGui_PushStyleVarFloat, (int index, float value))
{
    ImGui::PushStyleVar((ImGuiStyleVar)index, value);
}
ETH_CAPI(void, EthImGui_PushStyleVarVec2, (int index, float x, float y))
{
    ImGui::PushStyleVar((ImGuiStyleVar)index, V2(x, y));
}
ETH_CAPI(void, EthImGui_PopStyleVar, (int count)) { ImGui::PopStyleVar(count); }
ETH_CAPI(void, EthImGui_PushFont, (const char* which)) { ImGui::PushFont(FontByName(which)); }
ETH_CAPI(void, EthImGui_PopFont, (void)) { ImGui::PopFont(); }

ETH_CAPI(void, EthImGui_SetNextWindowPos, (float x, float y, int cond))
{
    ImGui::SetNextWindowPos(V2(x, y), (ImGuiCond)cond);
}
ETH_CAPI(void, EthImGui_SetNextWindowSize, (float w, float h, int cond))
{
    ImGui::SetNextWindowSize(V2(w, h), (ImGuiCond)cond);
}
ETH_CAPI(void, EthImGui_SetNextWindowBgAlpha, (float alpha)) { ImGui::SetNextWindowBgAlpha(alpha); }

ETH_CAPI(int, EthImGui_BeginTooltip, (void)) { return ImGui::BeginTooltip() ? 1 : 0; }
ETH_CAPI(void, EthImGui_EndTooltip, (void)) { ImGui::EndTooltip(); }
ETH_CAPI(void, EthImGui_SetTooltip, (const char* text)) { ImGui::SetTooltip("%s", text ? text : ""); }
ETH_CAPI(int, EthImGui_BeginPopup, (const char* id)) { return ImGui::BeginPopup(id) ? 1 : 0; }
ETH_CAPI(void, EthImGui_EndPopup, (void)) { ImGui::EndPopup(); }
ETH_CAPI(void, EthImGui_OpenPopup, (const char* id)) { ImGui::OpenPopup(id); }
ETH_CAPI(void, EthImGui_CloseCurrentPopup, (void)) { ImGui::CloseCurrentPopup(); }
ETH_CAPI(int, EthImGui_BeginPopupContextItem, (const char* id))
{
    return ImGui::BeginPopupContextItem(id) ? 1 : 0;
}
ETH_CAPI(int, EthImGui_BeginMenuBar, (void)) { return ImGui::BeginMenuBar() ? 1 : 0; }
ETH_CAPI(void, EthImGui_EndMenuBar, (void)) { ImGui::EndMenuBar(); }
ETH_CAPI(int, EthImGui_BeginMenu, (const char* label)) { return ImGui::BeginMenu(label) ? 1 : 0; }
ETH_CAPI(void, EthImGui_EndMenu, (void)) { ImGui::EndMenu(); }
ETH_CAPI(int, EthImGui_MenuItem, (const char* label, const char* shortcut, int selected, int enabled))
{
    return ImGui::MenuItem(label, shortcut, selected != 0, enabled != 0) ? 1 : 0;
}

ETH_CAPI(void, EthImGui_GetCursorScreenPos, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetCursorScreenPos()); }
ETH_CAPI(void, EthImGui_SetCursorScreenPos, (float x, float y)) { ImGui::SetCursorScreenPos(V2(x, y)); }
ETH_CAPI(void, EthImGui_GetCursorPos, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetCursorPos()); }
ETH_CAPI(void, EthImGui_SetCursorPos, (float x, float y)) { ImGui::SetCursorPos(V2(x, y)); }
ETH_CAPI(void, EthImGui_GetContentRegionAvail, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetContentRegionAvail()); }
ETH_CAPI(void, EthImGui_GetWindowPos, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetWindowPos()); }
ETH_CAPI(void, EthImGui_GetWindowSize, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetWindowSize()); }
ETH_CAPI(void, EthImGui_CalcTextSize, (const char* text, float* w, float* h))
{
    ImVec2 s = ImGui::CalcTextSize(text ? text : "");
    PushVec2Out(w, h, s);
}
ETH_CAPI(float, EthImGui_GetTextLineHeight, (void)) { return ImGui::GetTextLineHeight(); }
ETH_CAPI(float, EthImGui_GetFrameHeight, (void)) { return ImGui::GetFrameHeight(); }
ETH_CAPI(void, EthImGui_GetItemRectMin, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetItemRectMin()); }
ETH_CAPI(void, EthImGui_GetItemRectMax, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetItemRectMax()); }
ETH_CAPI(void, EthImGui_GetItemRectSize, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetItemRectSize()); }

ETH_CAPI(int, EthImGui_IsItemHovered, (void)) { return ImGui::IsItemHovered() ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsItemClicked, (int button)) { return ImGui::IsItemClicked(button) ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsItemActive, (void)) { return ImGui::IsItemActive() ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsItemEdited, (void)) { return ImGui::IsItemEdited() ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsItemVisible, (void)) { return ImGui::IsItemVisible() ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsWindowHovered, (void)) { return ImGui::IsWindowHovered() ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsWindowFocused, (void)) { return ImGui::IsWindowFocused() ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsAnyItemActive, (void)) { return ImGui::IsAnyItemActive() ? 1 : 0; }
ETH_CAPI(void, EthImGui_SetItemDefaultFocus, (void)) { ImGui::SetItemDefaultFocus(); }
ETH_CAPI(void, EthImGui_SetKeyboardFocusHere, (void)) { ImGui::SetKeyboardFocusHere(); }
ETH_CAPI(int, EthImGui_IsKeyDown, (int key)) { return ImGui::IsKeyDown((ImGuiKey)key) ? 1 : 0; }

// ---------------------------------------------------------------------- io ---
ETH_CAPI(void, EthImGui_GetMousePos, (float* x, float* y)) { PushVec2Out(x, y, ImGui::GetIO().MousePos); }
ETH_CAPI(int, EthImGui_IsMouseDown, (int button)) { return ImGui::IsMouseDown(button) ? 1 : 0; }
ETH_CAPI(int, EthImGui_IsMouseClicked, (int button)) { return ImGui::IsMouseClicked(button) ? 1 : 0; }
ETH_CAPI(void, EthImGui_GetIO, (float* deltaTime, float* framerate, float* w, float* h))
{
    ImGuiIO& io = ImGui::GetIO();
    if (deltaTime) *deltaTime = io.DeltaTime;
    if (framerate) *framerate = io.Framerate;
    if (w) *w = io.DisplaySize.x;
    if (h) *h = io.DisplaySize.y;
}
ETH_CAPI(int, EthImGui_WantCaptureMouse, (void)) { return ImGui::GetIO().WantCaptureMouse ? 1 : 0; }
ETH_CAPI(int, EthImGui_WantCaptureKeyboard, (void)) { return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0; }
ETH_CAPI(void, EthImGui_SetMouseCursor, (int cursor)) { ImGui::SetMouseCursor((ImGuiMouseCursor)cursor); }
ETH_CAPI(float, EthImGui_GetTime, (void)) { return ImGui::GetTime(); }

// ---------------------------------------------------------------- drawlist ---
ETH_CAPI(void, EthImGui_DrawLine, (float x1, float y1, float x2, float y2, unsigned int col, float thickness))
{
    ImGui::GetWindowDrawList()->AddLine(V2(x1, y1), V2(x2, y2), col, thickness);
}
ETH_CAPI(void, EthImGui_DrawRect, (float x1, float y1, float x2, float y2, unsigned int col, float rounding, float thickness))
{
    ImGui::GetWindowDrawList()->AddRect(V2(x1, y1), V2(x2, y2), col, rounding, ImDrawFlags_None, thickness);
}
ETH_CAPI(void, EthImGui_DrawRectFilled, (float x1, float y1, float x2, float y2, unsigned int col, float rounding))
{
    ImGui::GetWindowDrawList()->AddRectFilled(V2(x1, y1), V2(x2, y2), col, rounding);
}
ETH_CAPI(void, EthImGui_DrawCircle, (float cx, float cy, float radius, unsigned int col, int segments, float thickness))
{
    ImGui::GetWindowDrawList()->AddCircle(V2(cx, cy), radius, col, segments, thickness);
}
ETH_CAPI(void, EthImGui_DrawCircleFilled, (float cx, float cy, float radius, unsigned int col, int segments))
{
    ImGui::GetWindowDrawList()->AddCircleFilled(V2(cx, cy), radius, col, segments);
}
ETH_CAPI(void, EthImGui_DrawText, (float x, float y, unsigned int col, const char* text))
{
    ImGui::GetWindowDrawList()->AddText(V2(x, y), col, text ? text : "");
}
ETH_CAPI(void, EthImGui_DrawTriangle, (float x1, float y1, float x2, float y2, float x3, float y3, unsigned int col, float thickness))
{
    ImGui::GetWindowDrawList()->AddTriangle(V2(x1, y1), V2(x2, y2), V2(x3, y3), col, thickness);
}
ETH_CAPI(void, EthImGui_DrawTriangleFilled, (float x1, float y1, float x2, float y2, float x3, float y3, unsigned int col))
{
    ImGui::GetWindowDrawList()->AddTriangleFilled(V2(x1, y1), V2(x2, y2), V2(x3, y3), col);
}
ETH_CAPI(void, EthImGui_DrawQuad, (float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, unsigned int col, float thickness))
{
    ImGui::GetWindowDrawList()->AddQuad(V2(x1, y1), V2(x2, y2), V2(x3, y3), V2(x4, y4), col, thickness);
}
ETH_CAPI(void, EthImGui_DrawQuadFilled, (float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, unsigned int col))
{
    ImGui::GetWindowDrawList()->AddQuadFilled(V2(x1, y1), V2(x2, y2), V2(x3, y3), V2(x4, y4), col);
}
