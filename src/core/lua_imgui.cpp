// ===========================================================================
//  Lua ImGui bindings - direct access to Dear ImGui 1.91.8
// ===========================================================================
#include "lua_host.h"
#include "imgui_ext.h"
#include "util.h"
#include "log.h"
#include <cfloat>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace eth::lua {

namespace {

std::map<std::string, bool> g_closableState;

ImVec2 CheckVec2(lua_State* L, int idx)
{
    if (lua_istable(L, idx)) {
        lua_getfield(L, idx, "x");
        float x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, idx, "y");
        float y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        return ImVec2(x, y);
    }
    float x = (float)luaL_checknumber(L, idx);
    float y = (float)luaL_checknumber(L, idx + 1);
    return ImVec2(x, y);
}

void PushVec2(lua_State* L, ImVec2 v)
{
    lua_newtable(L);
    lua_pushnumber(L, v.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y); lua_setfield(L, -2, "y");
}

ImVec4 CheckVec4(lua_State* L, int idx)
{
    if (lua_istable(L, idx)) {
        lua_getfield(L, idx, "x"); float x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "y"); float y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "z"); float z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "w"); float w = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        return ImVec4(x, y, z, w);
    }
    float x = (float)luaL_checknumber(L, idx);
    float y = (float)luaL_checknumber(L, idx + 1);
    float z = (float)luaL_checknumber(L, idx + 2);
    float w = (float)luaL_optnumber(L, idx + 3, 1.0);
    return ImVec4(x, y, z, w);
}

ImU32 CheckColor(lua_State* L, int idx, ImU32 def = IM_COL32_WHITE)
{
    if (lua_isnoneornil(L, idx)) return def;
    return (ImU32)luaL_checkinteger(L, idx);
}

// ------------------------------------------------------------------ context
int i_create_context(lua_State*) { ui::EnsureContext(); return 0; }
int i_get_version(lua_State* L) { lua_pushstring(L, ImGui::GetVersion()); return 1; }

// ------------------------------------------------------------------- windows
int i_begin(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    ImGuiWindowFlags flags = (ImGuiWindowFlags)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, ImGui::Begin(name, nullptr, flags));
    return 1;
}

int i_begin_closable(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    ImGuiWindowFlags flags = (ImGuiWindowFlags)luaL_optinteger(L, 2, 0);
    auto it = g_closableState.find(name);
    if (it == g_closableState.end())
        it = g_closableState.emplace(name, true).first;
    bool& open = it->second;
    if (!lua_isnoneornil(L, 3)) open = lua_toboolean(L, 3) != 0;
    bool visible = ImGui::Begin(name, &open, flags);
    lua_pushboolean(L, visible);
    lua_pushboolean(L, open);
    return 2;
}

int i_end(lua_State*) { ImGui::End(); return 0; }

int i_begin_child(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    ImVec2 size(0, 0);
    if (!lua_isnoneornil(L, 2)) {
        if (lua_istable(L, 2)) size = CheckVec2(L, 2);
        else size = ImVec2((float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
    }
    bool border = lua_isnoneornil(L, 4) ? false : lua_toboolean(L, 4) != 0;
    lua_pushboolean(L, ImGui::BeginChild(id, size, border ? ImGuiChildFlags_Borders : 0));
    return 1;
}
int i_end_child(lua_State*) { ImGui::EndChild(); return 0; }

int i_begin_tab_bar(lua_State* L)
{
    lua_pushboolean(L, ImGui::BeginTabBar(luaL_checkstring(L, 1)));
    return 1;
}
int i_end_tab_bar(lua_State*) { ImGui::EndTabBar(); return 0; }
int i_begin_tab_item(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    ImGuiTabItemFlags f = lua_toboolean(L, 2) ? ImGuiTabItemFlags_SetSelected : 0;
    lua_pushboolean(L, ImGui::BeginTabItem(label, nullptr, f));
    return 1;
}
int i_end_tab_item(lua_State*) { ImGui::EndTabItem(); return 0; }

int i_begin_table(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    int columns = (int)luaL_checkinteger(L, 2);
    ImGuiTableFlags flags = (ImGuiTableFlags)luaL_optinteger(L, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp);
    lua_pushboolean(L, ImGui::BeginTable(id, columns, flags));
    return 1;
}
int i_end_table(lua_State*) { ImGui::EndTable(); return 0; }
int i_table_setup_column(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    ImGuiTableColumnFlags flags = (ImGuiTableColumnFlags)luaL_optinteger(L, 2, 0);
    float width = (float)luaL_optnumber(L, 3, 0.0);
    ImGui::TableSetupColumn(label, flags, width);
    return 0;
}
int i_table_headers_row(lua_State*) { ImGui::TableHeadersRow(); return 0; }
int i_table_next_row(lua_State*) { ImGui::TableNextRow(); return 0; }
int i_table_next_column(lua_State* L) { lua_pushboolean(L, ImGui::TableNextColumn()); return 1; }
int i_table_set_column_enabled(lua_State* L) { ImGui::TableSetColumnEnabled((int)luaL_checkinteger(L, 1), lua_toboolean(L, 2) != 0); return 0; }

// --------------------------------------------------------------------- text
int i_text(lua_State* L)
{
    luaL_tolstring(L, 1, nullptr);
    ImGui::TextUnformatted(lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}
int i_text_colored(lua_State* L)
{
    ImU32 col = CheckColor(L, 1);
    luaL_tolstring(L, 2, nullptr);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col));
    ImGui::TextUnformatted(lua_tostring(L, -1));
    ImGui::PopStyleColor();
    lua_pop(L, 1);
    return 0;
}
int i_text_disabled(lua_State* L)
{
    luaL_tolstring(L, 1, nullptr);
    ImGui::TextDisabled("%s", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}
int i_text_wrapped(lua_State* L)
{
    luaL_tolstring(L, 1, nullptr);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(lua_tostring(L, -1));
    ImGui::PopTextWrapPos();
    lua_pop(L, 1);
    return 0;
}
int i_label_text(lua_State* L) { ImGui::LabelText(luaL_checkstring(L, 1), "%s", luaL_checkstring(L, 2)); return 0; }
int i_bullet_text(lua_State* L) { ImGui::BulletText("%s", luaL_checkstring(L, 1)); return 0; }
int i_bullet(lua_State*) { ImGui::Bullet(); return 0; }
int i_separator(lua_State*) { ImGui::Separator(); return 0; }
int i_separator_text(lua_State* L) { ImGui::SeparatorText(luaL_checkstring(L, 1)); return 0; }
int i_same_line(lua_State* L)
{
    float offset = (float)luaL_optnumber(L, 1, 0.0);
    float spacing = (float)luaL_optnumber(L, 2, -1.0);
    ImGui::SameLine(offset, spacing);
    return 0;
}
int i_spacing(lua_State*) { ImGui::Spacing(); return 0; }
int i_dummy(lua_State* L) { ImGui::Dummy(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2))); return 0; }
int i_new_line(lua_State*) { ImGui::NewLine(); return 0; }
int i_align_text_to_frame_padding(lua_State*) { ImGui::AlignTextToFramePadding(); return 0; }
int i_indent(lua_State* L) { ImGui::Indent((float)luaL_optnumber(L, 1, 0.0)); return 0; }
int i_unindent(lua_State* L) { ImGui::Unindent((float)luaL_optnumber(L, 1, 0.0)); return 0; }
int i_push_text_wrap_pos(lua_State* L) { ImGui::PushTextWrapPos((float)luaL_optnumber(L, 1, 0.0)); return 0; }
int i_pop_text_wrap_pos(lua_State*) { ImGui::PopTextWrapPos(); return 0; }

// ------------------------------------------------------------------ widgets
int i_button(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    ImVec2 size(0, 0);
    if (!lua_isnoneornil(L, 2))
        size = ImVec2((float)luaL_checknumber(L, 2), (float)luaL_optnumber(L, 3, 0.0));
    lua_pushboolean(L, ImGui::Button(label, size));
    return 1;
}
int i_small_button(lua_State* L) { lua_pushboolean(L, ImGui::SmallButton(luaL_checkstring(L, 1))); return 1; }
int i_invisible_button(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    ImVec2 size = CheckVec2(L, 2);
    lua_pushboolean(L, ImGui::InvisibleButton(id, size));
    return 1;
}
int i_arrow_button(lua_State* L) { lua_pushboolean(L, ImGui::ArrowButton(luaL_checkstring(L, 1), (ImGuiDir)luaL_checkinteger(L, 2))); return 1; }

int i_checkbox(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    bool v = lua_toboolean(L, 2) != 0;
    bool changed = ui::Checkbox(label, &v);
    lua_pushboolean(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_checkbox_flags(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    unsigned int v = (unsigned int)luaL_checkinteger(L, 2);
    unsigned int flags = (unsigned int)luaL_checkinteger(L, 3);
    bool changed = ui::CheckboxFlags(label, &v, flags);
    // Preserve the existing signed 32-bit Lua return contract (e.g. bit 31),
    // while manipulating the mask as unsigned storage without aliasing casts.
    lua_pushinteger(L, static_cast<int32_t>(v));
    lua_pushboolean(L, changed);
    return 2;
}
int i_radio_button(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    bool active = lua_toboolean(L, 2) != 0;
    lua_pushboolean(L, ImGui::RadioButton(label, active));
    return 1;
}
int i_selectable(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    bool selected = lua_toboolean(L, 2) != 0;
    ImGuiSelectableFlags flags = (ImGuiSelectableFlags)luaL_optinteger(L, 3, 0);
    lua_pushboolean(L, ImGui::Selectable(label, selected, flags));
    return 1;
}

int i_slider_float(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    float v = (float)luaL_checknumber(L, 2);
    float lo = (float)luaL_checknumber(L, 3);
    float hi = (float)luaL_checknumber(L, 4);
    const char* fmt = luaL_optstring(L, 5, "%.3f");
    bool changed = ImGui::SliderFloat(label, &v, lo, hi, fmt);
    lua_pushnumber(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_slider_int(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    int v = (int)luaL_checkinteger(L, 2);
    int lo = (int)luaL_checkinteger(L, 3);
    int hi = (int)luaL_checkinteger(L, 4);
    bool changed = ImGui::SliderInt(label, &v, lo, hi);
    lua_pushinteger(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_drag_float(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    float v = (float)luaL_checknumber(L, 2);
    float speed = (float)luaL_optnumber(L, 3, 1.0);
    float lo = (float)luaL_optnumber(L, 4, 0.0);
    float hi = (float)luaL_optnumber(L, 5, 0.0);
    const char* fmt = luaL_optstring(L, 6, "%.3f");
    bool changed = ImGui::DragFloat(label, &v, speed, lo, hi, fmt);
    lua_pushnumber(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_drag_int(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    int v = (int)luaL_checkinteger(L, 2);
    float speed = (float)luaL_optnumber(L, 3, 1.0);
    int lo = (int)luaL_optinteger(L, 4, 0);
    int hi = (int)luaL_optinteger(L, 5, 0);
    bool changed = ImGui::DragInt(label, &v, speed, lo, hi);
    lua_pushinteger(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_input_text(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* text = luaL_checklstring(L, 2, &len);
    int maxLen = (int)luaL_optinteger(L, 3, 256);
    if (maxLen < 1) maxLen = 1;
    if (maxLen > 8192) maxLen = 8192;
    std::vector<char> buf((size_t)maxLen + 1, 0);
    size_t copy = len < (size_t)maxLen ? len : (size_t)maxLen;
    memcpy(buf.data(), text, copy);
    bool changed = ImGui::InputText(label, buf.data(), buf.size());
    lua_pushstring(L, buf.data());
    lua_pushboolean(L, changed);
    return 2;
}
int i_input_int(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    int v = (int)luaL_checkinteger(L, 2);
    bool changed = ImGui::InputInt(label, &v);
    lua_pushinteger(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_input_float(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    float v = (float)luaL_checknumber(L, 2);
    bool changed = ImGui::InputFloat(label, &v);
    lua_pushnumber(L, v);
    lua_pushboolean(L, changed);
    return 2;
}
int i_input_text_multiline(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);
    ImVec2 size = CheckVec2(L, 3);
    std::vector<char> buf(8192, 0);
    strncpy_s(buf.data(), buf.size(), text, _TRUNCATE);
    bool changed = ImGui::InputTextMultiline(label, buf.data(), buf.size(), size);
    lua_pushstring(L, buf.data());
    lua_pushboolean(L, changed);
    return 2;
}

int i_begin_combo(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    const char* preview = luaL_checkstring(L, 2);
    lua_pushboolean(L, ImGui::BeginCombo(label, preview));
    return 1;
}
int i_end_combo(lua_State*) { ImGui::EndCombo(); return 0; }

int i_combo(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    int current = (int)luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 3);
    std::vector<std::string> items;
    items.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 3, i);
        items.push_back(lua_tostring(L, -1) ? lua_tostring(L, -1) : "");
        lua_pop(L, 1);
    }
    if (current < 0 || current >= n) current = 0;
    bool changed = false;
    const char* preview = n ? items[current].c_str() : "";
    if (ImGui::BeginCombo(label, preview)) {
        for (int i = 0; i < n; ++i) {
            bool sel = i == current;
            if (ImGui::Selectable(items[i].c_str(), sel)) { current = i; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    lua_pushinteger(L, current);
    lua_pushboolean(L, changed);
    return 2;
}

int i_progress_bar(lua_State* L)
{
    float frac = (float)luaL_checknumber(L, 1);
    ImVec2 size(-FLT_MIN, 0);
    if (!lua_isnoneornil(L, 2))
        size = ImVec2((float)luaL_checknumber(L, 2), (float)luaL_optnumber(L, 3, 0.0));
    const char* overlay = luaL_optstring(L, 4, nullptr);
    ImGui::ProgressBar(frac, size, overlay);
    return 0;
}

int i_plot_lines(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 2);
    std::vector<float> vals((size_t)n);
    for (int i = 0; i < n; ++i) {
        lua_rawgeti(L, 2, i + 1);
        vals[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    ImGui::PlotLines(label, vals.data(), n, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, (float)luaL_optnumber(L, 3, 60.0)));
    return 0;
}

int i_color_edit3(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    float c[3] = { (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4) };
    bool changed = ImGui::ColorEdit3(label, c);
    lua_pushnumber(L, c[0]); lua_pushnumber(L, c[1]); lua_pushnumber(L, c[2]);
    lua_pushboolean(L, changed);
    return 4;
}
int i_color_edit4(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    float c[4] = { (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
                   (float)luaL_checknumber(L, 4), (float)luaL_optnumber(L, 5, 1.0) };
    bool changed = ImGui::ColorEdit4(label, c);
    lua_pushnumber(L, c[0]); lua_pushnumber(L, c[1]); lua_pushnumber(L, c[2]); lua_pushnumber(L, c[3]);
    lua_pushboolean(L, changed);
    return 5;
}
int i_color_button(lua_State* L)
{
    const char* desc = luaL_checkstring(L, 1);
    ImVec4 col = CheckVec4(L, 2);
    ImGuiColorEditFlags flags = (ImGuiColorEditFlags)luaL_optinteger(L, 3, 0);
    ImVec2 size((float)luaL_optnumber(L, 4, 0.0), (float)luaL_optnumber(L, 5, 0.0));
    lua_pushboolean(L, ImGui::ColorButton(desc, col, flags, size));
    return 1;
}

int i_collapsing_header(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    ImGuiTreeNodeFlags flags = lua_toboolean(L, 2) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    lua_pushboolean(L, ImGui::CollapsingHeader(label, flags));
    return 1;
}
int i_tree_node(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    ImGuiTreeNodeFlags flags = (ImGuiTreeNodeFlags)luaL_optinteger(L, 2, ImGuiTreeNodeFlags_None);
    lua_pushboolean(L, ImGui::TreeNodeEx(label, flags));
    return 1;
}
int i_tree_pop(lua_State*) { ImGui::TreePop(); return 0; }
int i_set_next_item_open(lua_State* L) { ImGui::SetNextItemOpen(lua_toboolean(L, 1) != 0); return 0; }

// --------------------------------------------------------------------- misc
int i_push_id(lua_State* L)
{
    if (lua_isnumber(L, 1)) ImGui::PushID((int)lua_tointeger(L, 1));
    else ImGui::PushID(luaL_checkstring(L, 1));
    return 0;
}
int i_pop_id(lua_State*) { ImGui::PopID(); return 0; }
int i_push_item_width(lua_State* L) { ImGui::PushItemWidth((float)luaL_checknumber(L, 1)); return 0; }
int i_pop_item_width(lua_State*) { ImGui::PopItemWidth(); return 0; }
int i_set_next_item_width(lua_State* L) { ImGui::SetNextItemWidth((float)luaL_checknumber(L, 1)); return 0; }
int i_set_next_window_pos(lua_State* L)
{
    ImVec2 p = CheckVec2(L, 1);
    ImGuiCond cond = (ImGuiCond)luaL_optinteger(L, 3, 0);
    ImGui::SetNextWindowPos(p, cond);
    return 0;
}
int i_set_next_window_size(lua_State* L)
{
    ImVec2 s = CheckVec2(L, 1);
    ImGuiCond cond = (ImGuiCond)luaL_optinteger(L, 3, 0);
    ImGui::SetNextWindowSize(s, cond);
    return 0;
}
int i_set_next_window_bg_alpha(lua_State* L) { ImGui::SetNextWindowBgAlpha((float)luaL_checknumber(L, 1)); return 0; }
int i_push_style_color(lua_State* L)
{
    ImGuiCol idx = (ImGuiCol)luaL_checkinteger(L, 1);
    ImU32 col = CheckColor(L, 2);
    ImGui::PushStyleColor(idx, ImGui::ColorConvertU32ToFloat4(col));
    return 0;
}
int i_pop_style_color(lua_State* L) { ImGui::PopStyleColor((int)luaL_optinteger(L, 1, 1)); return 0; }
int i_push_style_var(lua_State* L)
{
    ImGuiStyleVar idx = (ImGuiStyleVar)luaL_checkinteger(L, 1);
    if (lua_istable(L, 2)) ImGui::PushStyleVar(idx, CheckVec2(L, 2));
    else ImGui::PushStyleVar(idx, (float)luaL_checknumber(L, 2));
    return 0;
}
int i_pop_style_var(lua_State* L) { ImGui::PopStyleVar((int)luaL_optinteger(L, 1, 1)); return 0; }
int i_push_font(lua_State* L)
{
    const char* which = luaL_optstring(L, 1, "regular");
    ImFont* f = nullptr;
    if (!strcmp(which, "small")) f = ui::FontSmall();
    else if (!strcmp(which, "bold")) f = ui::FontBold();
    else if (!strcmp(which, "title")) f = ui::FontTitle();
    else f = ui::FontRegular();
    if (f) ImGui::PushFont(f);
    else ImGui::PushFont(nullptr);
    return 0;
}
int i_pop_font(lua_State*) { ImGui::PopFont(); return 0; }

int i_get_cursor_screen_pos(lua_State* L) { PushVec2(L, ImGui::GetCursorScreenPos()); return 1; }
int i_set_cursor_screen_pos(lua_State* L) { ImGui::SetCursorScreenPos(CheckVec2(L, 1)); return 0; }
int i_get_cursor_pos(lua_State* L) { PushVec2(L, ImGui::GetCursorPos()); return 1; }
int i_set_cursor_pos(lua_State* L) { ImGui::SetCursorPos(CheckVec2(L, 1)); return 0; }
int i_get_content_region_avail(lua_State* L) { PushVec2(L, ImGui::GetContentRegionAvail()); return 1; }
int i_get_window_pos(lua_State* L) { PushVec2(L, ImGui::GetWindowPos()); return 1; }
int i_get_window_size(lua_State* L) { PushVec2(L, ImGui::GetWindowSize()); return 1; }
int i_get_text_line_height(lua_State* L) { lua_pushnumber(L, ImGui::GetTextLineHeight()); return 1; }
int i_get_frame_height(lua_State* L) { lua_pushnumber(L, ImGui::GetFrameHeight()); return 1; }
int i_calc_text_size(lua_State* L)
{
    ImVec2 s = ImGui::CalcTextSize(luaL_checkstring(L, 1));
    PushVec2(L, s);
    return 1;
}
int i_get_item_rect_min(lua_State* L) { PushVec2(L, ImGui::GetItemRectMin()); return 1; }
int i_get_item_rect_max(lua_State* L) { PushVec2(L, ImGui::GetItemRectMax()); return 1; }
int i_get_item_rect_size(lua_State* L) { PushVec2(L, ImGui::GetItemRectSize()); return 1; }

int i_is_item_hovered(lua_State* L) { lua_pushboolean(L, ImGui::IsItemHovered()); return 1; }
int i_is_item_clicked(lua_State* L) { lua_pushboolean(L, ImGui::IsItemClicked((ImGuiMouseButton)luaL_optinteger(L, 1, 0))); return 1; }
int i_is_item_active(lua_State* L) { lua_pushboolean(L, ImGui::IsItemActive()); return 1; }
int i_is_item_edited(lua_State* L) { lua_pushboolean(L, ImGui::IsItemEdited()); return 1; }
int i_is_item_visible(lua_State* L) { lua_pushboolean(L, ImGui::IsItemVisible()); return 1; }
int i_is_window_hovered(lua_State* L) { lua_pushboolean(L, ImGui::IsWindowHovered()); return 1; }
int i_is_window_focused(lua_State* L) { lua_pushboolean(L, ImGui::IsWindowFocused()); return 1; }
int i_is_any_item_active(lua_State* L) { lua_pushboolean(L, ImGui::IsAnyItemActive()); return 1; }
int i_set_item_default_focus(lua_State*) { ImGui::SetItemDefaultFocus(); return 0; }
int i_set_keyboard_focus_here(lua_State*) { ImGui::SetKeyboardFocusHere(); return 0; }

int i_begin_tooltip(lua_State* L) { lua_pushboolean(L, ImGui::BeginTooltip()); return 1; }
int i_end_tooltip(lua_State*) { ImGui::EndTooltip(); return 0; }
int i_set_tooltip(lua_State* L) { ImGui::SetTooltip("%s", luaL_checkstring(L, 1)); return 0; }
int i_begin_item_tooltip(lua_State* L) { lua_pushboolean(L, ImGui::BeginItemTooltip()); return 1; }
int i_begin_popup(lua_State* L) { lua_pushboolean(L, ImGui::BeginPopup(luaL_checkstring(L, 1))); return 1; }
int i_end_popup(lua_State*) { ImGui::EndPopup(); return 0; }
int i_open_popup(lua_State* L) { ImGui::OpenPopup(luaL_checkstring(L, 1)); return 0; }
int i_close_current_popup(lua_State*) { ImGui::CloseCurrentPopup(); return 0; }
int i_begin_popup_context_item(lua_State* L) { lua_pushboolean(L, ImGui::BeginPopupContextItem(luaL_optstring(L, 1, nullptr))); return 1; }

int i_begin_menu_bar(lua_State* L) { lua_pushboolean(L, ImGui::BeginMenuBar()); return 1; }
int i_end_menu_bar(lua_State*) { ImGui::EndMenuBar(); return 0; }
int i_begin_menu(lua_State* L) { lua_pushboolean(L, ImGui::BeginMenu(luaL_checkstring(L, 1))); return 1; }
int i_end_menu(lua_State*) { ImGui::EndMenu(); return 0; }
int i_menu_item(lua_State* L)
{
    const char* label = luaL_checkstring(L, 1);
    const char* shortcut = luaL_optstring(L, 2, nullptr);
    bool selected = lua_isnoneornil(L, 3) ? false : lua_toboolean(L, 3) != 0;
    bool enabled = lua_isnoneornil(L, 4) ? true : lua_toboolean(L, 4) != 0;
    lua_pushboolean(L, ImGui::MenuItem(label, shortcut, selected, enabled));
    return 1;
}

// ----------------------------------------------------------------------- io
int i_get_mouse_pos(lua_State* L) { PushVec2(L, ImGui::GetIO().MousePos); return 1; }
int i_get_mouse_down(lua_State* L)
{
    int b = (int)luaL_optinteger(L, 1, 0);
    if (b < 0 || b > 4) b = 0;
    lua_pushboolean(L, ImGui::IsMouseDown(b));
    return 1;
}
int i_get_mouse_clicked(lua_State* L)
{
    int b = (int)luaL_optinteger(L, 1, 0);
    if (b < 0 || b > 4) b = 0;
    lua_pushboolean(L, ImGui::IsMouseClicked(b));
    return 1;
}
int i_get_delta_time(lua_State* L) { lua_pushnumber(L, ImGui::GetIO().DeltaTime); return 1; }
int i_get_framerate(lua_State* L) { lua_pushnumber(L, ImGui::GetIO().Framerate); return 1; }
int i_get_display_size(lua_State* L) { PushVec2(L, ImGui::GetIO().DisplaySize); return 1; }
int i_want_capture_mouse(lua_State* L) { lua_pushboolean(L, ImGui::GetIO().WantCaptureMouse); return 1; }
int i_want_capture_keyboard(lua_State* L) { lua_pushboolean(L, ImGui::GetIO().WantCaptureKeyboard); return 1; }
int i_set_mouse_cursor(lua_State* L) { ImGui::SetMouseCursor((ImGuiMouseCursor)luaL_checkinteger(L, 1)); return 0; }
int i_get_time(lua_State* L) { lua_pushnumber(L, ImGui::GetTime()); return 1; }

// ------------------------------------------------------------------ drawlist
int d_line(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                CheckColor(L, 5), (float)luaL_optnumber(L, 6, 1.0));
    return 0;
}
int d_rect(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                CheckColor(L, 5), (float)luaL_optnumber(L, 6, 0.0), ImDrawFlags_None,
                (float)luaL_optnumber(L, 7, 1.0));
    return 0;
}
int d_rect_filled(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                      ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                      CheckColor(L, 5), (float)luaL_optnumber(L, 6, 0.0));
    return 0;
}
int d_circle(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircle(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                  (float)luaL_checknumber(L, 3), CheckColor(L, 4),
                  (int)luaL_optinteger(L, 5, 0), (float)luaL_optnumber(L, 6, 1.0));
    return 0;
}
int d_circle_filled(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                        (float)luaL_checknumber(L, 3), CheckColor(L, 4), (int)luaL_optinteger(L, 5, 0));
    return 0;
}
int d_text(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                CheckColor(L, 3), luaL_checkstring(L, 4));
    return 0;
}
int d_triangle(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddTriangle(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                    ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                    ImVec2((float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)),
                    CheckColor(L, 7), (float)luaL_optnumber(L, 8, 1.0));
    return 0;
}
int d_triangle_filled(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddTriangleFilled(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                          ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                          ImVec2((float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)),
                          CheckColor(L, 7));
    return 0;
}
int d_quad(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddQuad(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                ImVec2((float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)),
                ImVec2((float)luaL_checknumber(L, 7), (float)luaL_checknumber(L, 8)),
                CheckColor(L, 9), (float)luaL_optnumber(L, 10, 1.0));
    return 0;
}
int d_quad_filled(lua_State* L)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddQuadFilled(ImVec2((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)),
                      ImVec2((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)),
                      ImVec2((float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)),
                      ImVec2((float)luaL_checknumber(L, 7), (float)luaL_checknumber(L, 8)),
                      CheckColor(L, 9));
    return 0;
}

// ------------------------------------------------------------------- colors
int i_rgba(lua_State* L)
{
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    float a = (float)luaL_optnumber(L, 4, 1.0);
    lua_pushinteger(L, (lua_Integer)ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)));
    return 1;
}
int i_hex(lua_State* L)
{
    lua_pushinteger(L, (lua_Integer)ui::HexToU32(luaL_checkstring(L, 1)));
    return 1;
}

void SetInt(lua_State* L, const char* name, int value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}

void RegisterTable(lua_State* L, const char* name, const luaL_Reg* funcs)
{
    lua_newtable(L);
    for (const luaL_Reg* f = funcs; f->name; ++f) {
        lua_pushcfunction(L, f->func);
        lua_setfield(L, -2, f->name);
    }
    lua_setglobal(L, name);
}

} // namespace

void RegisterImGui(lua_State* L)
{
    static const luaL_Reg fns[] = {
        { "create_context", i_create_context },
        { "get_version", i_get_version },
        { "begin", i_begin }, { "begin_closable", i_begin_closable }, { "end_", i_end },
        { "begin_child", i_begin_child }, { "end_child", i_end_child },
        { "begin_tab_bar", i_begin_tab_bar }, { "end_tab_bar", i_end_tab_bar },
        { "begin_tab_item", i_begin_tab_item }, { "end_tab_item", i_end_tab_item },
        { "begin_table", i_begin_table }, { "end_table", i_end_table },
        { "table_setup_column", i_table_setup_column }, { "table_headers_row", i_table_headers_row },
        { "table_next_row", i_table_next_row }, { "table_next_column", i_table_next_column },
        { "table_set_column_enabled", i_table_set_column_enabled },
        { "text", i_text }, { "text_colored", i_text_colored }, { "text_disabled", i_text_disabled },
        { "text_wrapped", i_text_wrapped }, { "label_text", i_label_text }, { "bullet_text", i_bullet_text },
        { "bullet", i_bullet }, { "separator", i_separator }, { "separator_text", i_separator_text },
        { "same_line", i_same_line }, { "spacing", i_spacing }, { "dummy", i_dummy }, { "new_line", i_new_line },
        { "align_text_to_frame_padding", i_align_text_to_frame_padding },
        { "indent", i_indent }, { "unindent", i_unindent },
        { "push_text_wrap_pos", i_push_text_wrap_pos }, { "pop_text_wrap_pos", i_pop_text_wrap_pos },
        { "button", i_button }, { "small_button", i_small_button }, { "invisible_button", i_invisible_button },
        { "arrow_button", i_arrow_button },
        { "checkbox", i_checkbox }, { "checkbox_flags", i_checkbox_flags }, { "radio_button", i_radio_button },
        { "selectable", i_selectable },
        { "slider_float", i_slider_float }, { "slider_int", i_slider_int },
        { "drag_float", i_drag_float }, { "drag_int", i_drag_int },
        { "input_text", i_input_text }, { "input_int", i_input_int }, { "input_float", i_input_float },
        { "input_text_multiline", i_input_text_multiline },
        { "begin_combo", i_begin_combo }, { "end_combo", i_end_combo }, { "combo", i_combo },
        { "progress_bar", i_progress_bar }, { "plot_lines", i_plot_lines },
        { "color_edit3", i_color_edit3 }, { "color_edit4", i_color_edit4 }, { "color_button", i_color_button },
        { "collapsing_header", i_collapsing_header }, { "tree_node", i_tree_node }, { "tree_pop", i_tree_pop },
        { "set_next_item_open", i_set_next_item_open },
        { "push_id", i_push_id }, { "pop_id", i_pop_id },
        { "push_item_width", i_push_item_width }, { "pop_item_width", i_pop_item_width },
        { "set_next_item_width", i_set_next_item_width },
        { "set_next_window_pos", i_set_next_window_pos }, { "set_next_window_size", i_set_next_window_size },
        { "set_next_window_bg_alpha", i_set_next_window_bg_alpha },
        { "push_style_color", i_push_style_color }, { "pop_style_color", i_pop_style_color },
        { "push_style_var", i_push_style_var }, { "pop_style_var", i_pop_style_var },
        { "push_font", i_push_font }, { "pop_font", i_pop_font },
        { "get_cursor_screen_pos", i_get_cursor_screen_pos }, { "set_cursor_screen_pos", i_set_cursor_screen_pos },
        { "get_cursor_pos", i_get_cursor_pos }, { "set_cursor_pos", i_set_cursor_pos },
        { "get_content_region_avail", i_get_content_region_avail },
        { "get_window_pos", i_get_window_pos }, { "get_window_size", i_get_window_size },
        { "get_text_line_height", i_get_text_line_height }, { "get_frame_height", i_get_frame_height },
        { "calc_text_size", i_calc_text_size },
        { "get_item_rect_min", i_get_item_rect_min }, { "get_item_rect_max", i_get_item_rect_max },
        { "get_item_rect_size", i_get_item_rect_size },
        { "is_item_hovered", i_is_item_hovered }, { "is_item_clicked", i_is_item_clicked },
        { "is_item_active", i_is_item_active }, { "is_item_edited", i_is_item_edited },
        { "is_item_visible", i_is_item_visible },
        { "is_window_hovered", i_is_window_hovered }, { "is_window_focused", i_is_window_focused },
        { "is_any_item_active", i_is_any_item_active },
        { "set_item_default_focus", i_set_item_default_focus }, { "set_keyboard_focus_here", i_set_keyboard_focus_here },
        { "begin_tooltip", i_begin_tooltip }, { "end_tooltip", i_end_tooltip }, { "set_tooltip", i_set_tooltip },
        { "begin_item_tooltip", i_begin_item_tooltip },
        { "begin_popup", i_begin_popup }, { "end_popup", i_end_popup }, { "open_popup", i_open_popup },
        { "close_current_popup", i_close_current_popup }, { "begin_popup_context_item", i_begin_popup_context_item },
        { "begin_menu_bar", i_begin_menu_bar }, { "end_menu_bar", i_end_menu_bar },
        { "begin_menu", i_begin_menu }, { "end_menu", i_end_menu },
        { "menu_item", i_menu_item },
        { "get_mouse_pos", i_get_mouse_pos }, { "get_mouse_down", i_get_mouse_down },
        { "get_mouse_clicked", i_get_mouse_clicked },
        { "get_delta_time", i_get_delta_time }, { "get_framerate", i_get_framerate },
        { "get_display_size", i_get_display_size },
        { "want_capture_mouse", i_want_capture_mouse }, { "want_capture_keyboard", i_want_capture_keyboard },
        { "set_mouse_cursor", i_set_mouse_cursor }, { "get_time", i_get_time },
        { "rgba", i_rgba }, { "hex", i_hex },
        { nullptr, nullptr }
    };
    RegisterTable(L, "imgui", fns);

    // draw.* works on the current window draw list
    static const luaL_Reg drawFns[] = {
        { "line", d_line }, { "rect", d_rect }, { "rect_filled", d_rect_filled },
        { "circle", d_circle }, { "circle_filled", d_circle_filled }, { "text", d_text },
        { "triangle", d_triangle }, { "triangle_filled", d_triangle_filled },
        { "quad", d_quad }, { "quad_filled", d_quad_filled },
        { nullptr, nullptr }
    };
    RegisterTable(L, "draw", drawFns);

    lua_getglobal(L, "imgui");

    // style colors
    lua_newtable(L);
    SetInt(L, "Text", ImGuiCol_Text);
    SetInt(L, "TextDisabled", ImGuiCol_TextDisabled);
    SetInt(L, "WindowBg", ImGuiCol_WindowBg);
    SetInt(L, "ChildBg", ImGuiCol_ChildBg);
    SetInt(L, "PopupBg", ImGuiCol_PopupBg);
    SetInt(L, "Border", ImGuiCol_Border);
    SetInt(L, "FrameBg", ImGuiCol_FrameBg);
    SetInt(L, "FrameBgHovered", ImGuiCol_FrameBgHovered);
    SetInt(L, "FrameBgActive", ImGuiCol_FrameBgActive);
    SetInt(L, "TitleBg", ImGuiCol_TitleBg);
    SetInt(L, "Button", ImGuiCol_Button);
    SetInt(L, "ButtonHovered", ImGuiCol_ButtonHovered);
    SetInt(L, "ButtonActive", ImGuiCol_ButtonActive);
    SetInt(L, "CheckMark", ImGuiCol_CheckMark);
    SetInt(L, "SliderGrab", ImGuiCol_SliderGrab);
    SetInt(L, "SliderGrabActive", ImGuiCol_SliderGrabActive);
    SetInt(L, "Header", ImGuiCol_Header);
    SetInt(L, "HeaderHovered", ImGuiCol_HeaderHovered);
    SetInt(L, "HeaderActive", ImGuiCol_HeaderActive);
    SetInt(L, "Separator", ImGuiCol_Separator);
    SetInt(L, "Tab", ImGuiCol_Tab);
    SetInt(L, "TabHovered", ImGuiCol_TabHovered);
    SetInt(L, "TabSelected", ImGuiCol_TabSelected);
    SetInt(L, "TableHeaderBg", ImGuiCol_TableHeaderBg);
    SetInt(L, "TableRowBg", ImGuiCol_TableRowBg);
    SetInt(L, "TableRowBgAlt", ImGuiCol_TableRowBgAlt);
    SetInt(L, "PlotLines", ImGuiCol_PlotLines);
    SetInt(L, "PlotHistogram", ImGuiCol_PlotHistogram);
    SetInt(L, "TextSelectedBg", ImGuiCol_TextSelectedBg);
    lua_setfield(L, -2, "col");

    // style vars
    lua_newtable(L);
    SetInt(L, "Alpha", ImGuiStyleVar_Alpha);
    SetInt(L, "WindowPadding", ImGuiStyleVar_WindowPadding);
    SetInt(L, "WindowRounding", ImGuiStyleVar_WindowRounding);
    SetInt(L, "WindowMinSize", ImGuiStyleVar_WindowMinSize);
    SetInt(L, "FramePadding", ImGuiStyleVar_FramePadding);
    SetInt(L, "FrameRounding", ImGuiStyleVar_FrameRounding);
    SetInt(L, "ItemSpacing", ImGuiStyleVar_ItemSpacing);
    SetInt(L, "ItemInnerSpacing", ImGuiStyleVar_ItemInnerSpacing);
    SetInt(L, "CellPadding", ImGuiStyleVar_CellPadding);
    SetInt(L, "IndentSpacing", ImGuiStyleVar_IndentSpacing);
    SetInt(L, "ScrollbarSize", ImGuiStyleVar_ScrollbarSize);
    SetInt(L, "GrabMinSize", ImGuiStyleVar_GrabMinSize);
    SetInt(L, "ChildRounding", ImGuiStyleVar_ChildRounding);
    SetInt(L, "ChildBorderSize", ImGuiStyleVar_ChildBorderSize);
    SetInt(L, "PopupRounding", ImGuiStyleVar_PopupRounding);
    SetInt(L, "TabRounding", ImGuiStyleVar_TabRounding);
    lua_setfield(L, -2, "style_var");

    // window flags
    lua_newtable(L);
    SetInt(L, "NoTitleBar", ImGuiWindowFlags_NoTitleBar);
    SetInt(L, "NoResize", ImGuiWindowFlags_NoResize);
    SetInt(L, "NoMove", ImGuiWindowFlags_NoMove);
    SetInt(L, "NoScrollbar", ImGuiWindowFlags_NoScrollbar);
    SetInt(L, "NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse);
    SetInt(L, "NoCollapse", ImGuiWindowFlags_NoCollapse);
    SetInt(L, "AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize);
    SetInt(L, "NoBackground", ImGuiWindowFlags_NoBackground);
    SetInt(L, "NoSavedSettings", ImGuiWindowFlags_NoSavedSettings);
    SetInt(L, "NoInputs", ImGuiWindowFlags_NoInputs);
    SetInt(L, "MenuBar", ImGuiWindowFlags_MenuBar);
    SetInt(L, "HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar);
    SetInt(L, "NoDecoration", ImGuiWindowFlags_NoDecoration);
    lua_setfield(L, -2, "win_flags");

    // cond
    lua_newtable(L);
    SetInt(L, "Always", ImGuiCond_Always);
    SetInt(L, "Once", ImGuiCond_Once);
    SetInt(L, "FirstUseEver", ImGuiCond_FirstUseEver);
    SetInt(L, "Appearing", ImGuiCond_Appearing);
    lua_setfield(L, -2, "cond");

    lua_pop(L, 1);

    // Convenience: mirror style colors directly on `imgui` (imgui.Text, ...)
    lua_getglobal(L, "imgui");
    int imguiIdx = lua_gettop(L);
    lua_getfield(L, imguiIdx, "col");
    int colIdx = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, colIdx) != 0) {
        lua_pushvalue(L, -2);          // key
        lua_pushvalue(L, -2);          // value
        lua_settable(L, imguiIdx);     // imgui[key] = value
        lua_pop(L, 1);                 // pop value
    }
    lua_pop(L, 2);
}

} // namespace eth::lua
