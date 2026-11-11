#include "imgui_ext.h"
#include "util.h"
#include "log.h"
#include "config.h"

namespace eth::ui {

namespace {
ImU32 g_accent = IM_COL32(108, 140, 255, 255);
float g_scale = 1.0f;
float g_dpi = 1.0f;

ImFont* g_regular = nullptr;
ImFont* g_small = nullptr;
ImFont* g_bold = nullptr;
ImFont* g_title = nullptr;

bool g_chromeOpen = false;
bool g_chromePushed = false;

void DrawCheckMark(ImDrawList* dl, ImVec2 center, float size, ImU32 col)
{
    dl->AddLine(ImVec2(center.x - size * 0.42f, center.y + size * 0.02f),
                ImVec2(center.x - size * 0.10f, center.y + size * 0.34f), col, 2.0f * g_scale);
    dl->AddLine(ImVec2(center.x - size * 0.10f, center.y + size * 0.34f),
                ImVec2(center.x + size * 0.46f, center.y - size * 0.28f), col, 2.0f * g_scale);
}

ImU32 LerpColor(ImU32 a, ImU32 b, float t)
{
    auto ch = [](ImU32 c, int shift) { return (int)((c >> shift) & 0xFF); };
    int r = (int)(ch(a, IM_COL32_R_SHIFT) + (ch(b, IM_COL32_R_SHIFT) - ch(a, IM_COL32_R_SHIFT)) * t);
    int g = (int)(ch(a, IM_COL32_G_SHIFT) + (ch(b, IM_COL32_G_SHIFT) - ch(a, IM_COL32_G_SHIFT)) * t);
    int bl = (int)(ch(a, IM_COL32_B_SHIFT) + (ch(b, IM_COL32_B_SHIFT) - ch(a, IM_COL32_B_SHIFT)) * t);
    int al = (int)(ch(a, IM_COL32_A_SHIFT) + (ch(b, IM_COL32_A_SHIFT) - ch(a, IM_COL32_A_SHIFT)) * t);
    return IM_COL32(r, g, bl, al);
}
} // namespace

float Scale() { return g_scale; }
float Dpi()   { return g_dpi; }
ImU32 Accent() { return g_accent; }
ImU32 AccentDim(float alpha) { return WithAlpha(g_accent, alpha); }
ImFont* FontRegular() { return g_regular; }
ImFont* FontSmall()   { return g_small ? g_small : g_regular; }
ImFont* FontBold()    { return g_bold ? g_bold : g_regular; }
ImFont* FontTitle()   { return g_title ? g_title : g_bold; }

ImU32 WithAlpha(ImU32 col, float alpha)
{
    ImU32 a = (ImU32)((col >> IM_COL32_A_SHIFT) & 0xFF);
    a = (ImU32)(a * std::clamp(alpha, 0.0f, 1.0f));
    return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

ImU32 HexToU32(const char* hex, ImU32 fallback)
{
    if (!hex) return fallback;
    while (*hex == '#') ++hex;
    unsigned r = 0, g = 0, b = 0, a = 255;
    int n = (int)strlen(hex);
    if (n >= 6) {
        sscanf_s(hex, "%02x%02x%02x", &r, &g, &b);
        if (n >= 8) sscanf_s(hex + 6, "%02x", &a);
        return IM_COL32(r, g, b, a);
    }
    return fallback;
}

ImVec4 HexToColor(const char* hex)
{
    ImU32 c = HexToU32(hex);
    return ImVec4(((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
}

void U32ToHex(ImU32 col, char out[7])
{
    sprintf_s(out, 7, "%02X%02X%02X",
              (col >> IM_COL32_R_SHIFT) & 0xFF,
              (col >> IM_COL32_G_SHIFT) & 0xFF,
              (col >> IM_COL32_B_SHIFT) & 0xFF);
}

void SetAccent(ImU32 color)
{
    g_accent = color;
    ApplyTheme();
}

// ---------------------------------------------------------------------------
void EnsureContext()
{
    if (ImGui::GetCurrentContext()) return;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;          // the SDK owns window layout, no imgui.ini
    io.LogFilename = nullptr;
    ApplyTheme();
}

void LoadFonts(float scale)
{
    EnsureContext();
    g_scale = std::clamp(scale, 0.75f, 2.5f);
    g_dpi = scale;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    std::string fontDir = "C:\\Windows\\Fonts";
    std::string regularPath = JoinPath(fontDir, "segoeui.ttf");
    std::string boldPath = JoinPath(fontDir, "segoeuib.ttf");
    if (!FileExists(regularPath)) regularPath = JoinPath(fontDir, "tahoma.ttf");
    if (!FileExists(regularPath)) regularPath = JoinPath(fontDir, "arial.ttf");
    if (!FileExists(boldPath)) boldPath = regularPath;

    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    // Font sizes already include DPI; applying it again wastes atlas space.
    cfg.RasterizerDensity = 1.0f;

    auto addFont = [&](const std::string& path, float px) -> ImFont* {
        if (!FileExists(path)) return nullptr;
        ImFontConfig c = cfg;
        return io.Fonts->AddFontFromFileTTF(path.c_str(), px * g_scale, &c, io.Fonts->GetGlyphRangesDefault());
    };

    g_regular = addFont(regularPath, 16.0f);
    g_small   = addFont(regularPath, 14.0f);
    g_bold    = addFont(boldPath, 17.0f);
    g_title   = addFont(boldPath, 26.0f);

    if (!g_regular) {
        ETH_WARN("no system TTF font found, falling back to ImGui default");
        g_regular = io.Fonts->AddFontDefault();
        g_small = g_bold = g_title = g_regular;
    }
    if (!g_small) g_small = g_regular;
    if (!g_bold) g_bold = g_regular;
    if (!g_title) g_title = g_bold;

    io.FontDefault = g_regular;
    io.Fonts->Build();
    ApplyTheme();
    ETH_INFO("fonts loaded: %s (scale %.2f)", regularPath.c_str(), g_scale);
}

void ApplyTheme()
{
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle(); // no cumulative DPI scaling when the accent changes
    ImGui::StyleColorsDark(&st);

    st.WindowRounding = 12.0f;
    st.ChildRounding = 8.0f;
    st.PopupRounding = 8.0f;
    st.FrameRounding = 5.0f;
    st.ScrollbarRounding = 8.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.WindowBorderSize = 1.0f;
    st.ChildBorderSize = 1.0f;
    st.FrameBorderSize = 0.0f;
    st.PopupBorderSize = 1.0f;
    st.WindowPadding = ImVec2(18, 16);
    st.FramePadding = ImVec2(12, 7);
    st.CellPadding = ImVec2(8, 6);
    st.ItemSpacing = ImVec2(12, 10);
    st.ItemInnerSpacing = ImVec2(8, 6);
    st.IndentSpacing = 18.0f;
    st.ScrollbarSize = 12.0f;
    st.GrabMinSize = 12.0f;
    st.SeparatorTextBorderSize = 1.0f;
    st.SeparatorTextPadding = ImVec2(0, 6);
    st.DisabledAlpha = 0.60f;
    st.AntiAliasedLines = true;
    st.AntiAliasedLinesUseTex = true;
    st.AntiAliasedFill = true;
    st.CurveTessellationTol = 1.0f * g_scale;

    st.ScaleAllSizes(g_scale);

    auto set = [&](ImGuiCol idx, ImU32 c) { st.Colors[idx] = ImGui::ColorConvertU32ToFloat4(c); };

    set(ImGuiCol_Text,                 IM_COL32(235, 238, 244, 255));
    set(ImGuiCol_TextDisabled,         IM_COL32(155, 164, 179, 255));
    set(ImGuiCol_WindowBg,             IM_COL32(18, 20, 25, 255));
    set(ImGuiCol_ChildBg,              IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_PopupBg,              IM_COL32(27, 30, 37, 255));
    set(ImGuiCol_Border,               IM_COL32(46, 50, 60, 255));
    set(ImGuiCol_BorderShadow,         IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_FrameBg,              IM_COL32(34, 38, 47, 255));
    set(ImGuiCol_FrameBgHovered,       IM_COL32(44, 49, 61, 255));
    set(ImGuiCol_FrameBgActive,        IM_COL32(51, 58, 73, 255));
    set(ImGuiCol_TitleBg,              IM_COL32(16, 19, 26, 255));
    set(ImGuiCol_TitleBgActive,        IM_COL32(16, 19, 26, 255));
    set(ImGuiCol_TitleBgCollapsed,     IM_COL32(16, 19, 26, 200));
    set(ImGuiCol_MenuBarBg,            IM_COL32(14, 17, 23, 255));
    set(ImGuiCol_ScrollbarBg,          IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_ScrollbarGrab,        IM_COL32(53, 62, 80, 190));
    set(ImGuiCol_ScrollbarGrabHovered, IM_COL32(72, 84, 108, 220));
    set(ImGuiCol_ScrollbarGrabActive,  g_accent);
    set(ImGuiCol_CheckMark,            g_accent);
    set(ImGuiCol_SliderGrab,           g_accent);
    set(ImGuiCol_SliderGrabActive,     WithAlpha(g_accent, 0.85f));
    set(ImGuiCol_Button,               IM_COL32(29, 35, 48, 255));
    set(ImGuiCol_ButtonHovered,        IM_COL32(38, 47, 64, 255));
    set(ImGuiCol_ButtonActive,         WithAlpha(g_accent, 0.55f));
    set(ImGuiCol_Header,               IM_COL32(33, 41, 56, 255));
    set(ImGuiCol_HeaderHovered,        IM_COL32(42, 53, 72, 255));
    set(ImGuiCol_HeaderActive,         WithAlpha(g_accent, 0.45f));
    set(ImGuiCol_Separator,            IM_COL32(34, 40, 53, 255));
    set(ImGuiCol_SeparatorHovered,     WithAlpha(g_accent, 0.6f));
    set(ImGuiCol_SeparatorActive,      g_accent);
    set(ImGuiCol_ResizeGrip,           IM_COL32(45, 53, 69, 160));
    set(ImGuiCol_ResizeGripHovered,    WithAlpha(g_accent, 0.45f));
    set(ImGuiCol_ResizeGripActive,     WithAlpha(g_accent, 0.75f));
    set(ImGuiCol_Tab,                  IM_COL32(21, 25, 34, 255));
    set(ImGuiCol_TabHovered,           IM_COL32(38, 47, 64, 255));
    set(ImGuiCol_TabSelected,          IM_COL32(33, 41, 56, 255));
    set(ImGuiCol_TabSelectedOverline,  g_accent);
    set(ImGuiCol_TabDimmed,            IM_COL32(18, 21, 29, 255));
    set(ImGuiCol_TabDimmedSelected,    IM_COL32(27, 33, 45, 255));
    set(ImGuiCol_TabDimmedSelectedOverline, WithAlpha(g_accent, 0.5f));
    set(ImGuiCol_PlotLines,            WithAlpha(g_accent, 0.9f));
    set(ImGuiCol_PlotHistogram,        WithAlpha(g_accent, 0.9f));
    set(ImGuiCol_TextSelectedBg,       WithAlpha(g_accent, 0.35f));
    set(ImGuiCol_DragDropTarget,       g_accent);
    set(ImGuiCol_NavCursor,            g_accent);
    set(ImGuiCol_TableHeaderBg,        IM_COL32(20, 24, 33, 255));
    set(ImGuiCol_TableBorderStrong,    IM_COL32(34, 40, 53, 255));
    set(ImGuiCol_TableBorderLight,     IM_COL32(27, 32, 43, 255));
    set(ImGuiCol_TableRowBg,           IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt,        IM_COL32(255, 255, 255, 4));
    set(ImGuiCol_ModalWindowDimBg,     IM_COL32(5, 7, 10, 170));
}

// ---------------------------------------------------------------------------
//  Custom widgets
// ---------------------------------------------------------------------------
bool ToggleSwitch(const char* id, bool* value, float widthScale)
{
    float height = 22.0f * widthScale * g_scale;
    float width = 42.0f * widthScale * g_scale;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height), ImGuiButtonFlags_EnableNav);
    if (clicked) *value = !*value;
    bool hovered = ImGui::IsItemHovered();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID sid = ImGui::GetID(id);
    float t = storage->GetFloat(sid, *value ? 1.0f : 0.0f);
    float target = *value ? 1.0f : 0.0f;
    t += (target - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    storage->SetFloat(sid, t);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 trackOff = hovered ? IM_COL32(60, 70, 88, 255) : IM_COL32(44, 52, 67, 255);
    ImU32 track = LerpColor(trackOff, g_accent, t);

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), track, height * 0.5f);
    float knobR = height * 0.5f - 3.0f * g_scale;
    float knobX = pos.x + height * 0.5f + t * (width - height);
    dl->AddCircleFilled(ImVec2(knobX, pos.y + height * 0.5f), knobR, IM_COL32(248, 250, 255, 255), 24);
    return clicked;
}

void SectionHeader(const char* title, const char* subtitle)
{
    ImGui::PushFont(FontBold());
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (subtitle && *subtitle) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", subtitle);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, 2.0f * g_scale));
}

void Divider(float spacing)
{
    ImGui::Dummy(ImVec2(0, spacing * g_scale));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, spacing * g_scale));
}

void Badge(const char* text, ImU32 color)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(text);
    float padX = 7.0f * g_scale, padY = 2.0f * g_scale;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(textSize.x + padX * 2, textSize.y + padY * 2);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), WithAlpha(color, 0.16f), 5.0f * g_scale);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), WithAlpha(color, 0.5f), 5.0f * g_scale);
    dl->AddText(ImVec2(pos.x + padX, pos.y + padY), color, text);
    ImGui::Dummy(size);
}

void HelpMarker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool AccentButton(const char* label, const ImVec2& size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, g_accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(g_accent, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(g_accent, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(8, 10, 14, 255));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return r;
}

bool GhostButton(const char* label, const ImVec2& size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 16));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 28));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return r;
}

bool NavItem(const char* id, const char* label, bool selected, const char* shortcut)
{
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float height = 42.0f * g_scale;
    float width = ImGui::GetContentRegionAvail().x;
    bool clicked = ImGui::InvisibleButton("##nav", ImVec2(width, height), ImGuiButtonFlags_EnableNav);
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 max(pos.x + width, pos.y + height);
    if (selected)
        dl->AddRectFilled(pos, max, WithAlpha(g_accent, 0.15f), 6.0f * g_scale);
    else if (hovered)
        dl->AddRectFilled(pos, max, IM_COL32(29, 33, 41, 255), 6.0f * g_scale);
    if (selected)
        dl->AddRectFilled(ImVec2(pos.x, pos.y + height * 0.22f), ImVec2(pos.x + 3.0f * g_scale, pos.y + height * 0.78f),
                          g_accent, 2.0f * g_scale);

    float textH = ImGui::GetTextLineHeight();
    dl->AddText(ImVec2(pos.x + 16.0f * g_scale, pos.y + (height - textH) * 0.5f),
                selected ? IM_COL32(240, 243, 250, 255) : IM_COL32(168, 177, 195, 255), label);
    if (shortcut && *shortcut) {
        ImVec2 s = ImGui::CalcTextSize(shortcut);
        dl->AddText(ImVec2(pos.x + width - s.x - 14.0f * g_scale, pos.y + (height - s.y) * 0.5f),
                    IM_COL32(120, 130, 150, 255), shortcut);
    }
    ImGui::PopID();
    return clicked;
}

void StatRow(const char* label, const char* value)
{
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.55f);
    ImGui::TextUnformatted(value);
}

void DrawCardBg(ImVec2 pos, ImVec2 size, ImU32 color, float rounding)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, rounding * g_scale);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(38, 44, 57, 255), rounding * g_scale);
}

bool BeginCard(const char* id, const ImVec2& size)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImVec2(size.x <= 0 ? ImGui::GetContentRegionAvail().x : size.x,
                       size.y);
    DrawCardBg(pos, sz, IM_COL32(20, 24, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    bool open = ImGui::BeginChild(id, sz, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    return open;
}

void EndCard()
{
    ImGui::EndChild();
}

void DrawLogo(ImVec2 pos, float size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), g_accent, size * 0.26f);
    ImDrawList* d = dl;
    float cx = pos.x + size * 0.5f, cy = pos.y + size * 0.5f;
    d->AddText(ImVec2(cx - ImGui::CalcTextSize("E").x * 0.5f, cy - ImGui::GetTextLineHeight() * 0.5f),
               IM_COL32(8, 10, 14, 255), "E");
}

// ---------------------------------------------------------------------------
//  Custom chrome window
// ---------------------------------------------------------------------------
bool BeginChromeWindow(const char* id, const char* title, const char* subtitle,
                       bool* open, const ImVec2& defaultSize, ImGuiWindowFlags flags)
{
    EnsureContext();
    ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    bool visible = ImGui::Begin(id, nullptr, flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    if (!visible) {
        ImGui::End();
        return false;
    }

    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    float headerH = 76.0f * g_scale;
    float pad = 20.0f * g_scale;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + headerH), IM_COL32(22, 25, 31, 255), 12.0f * g_scale,
                      ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(pos.x, pos.y + headerH), ImVec2(pos.x + size.x, pos.y + headerH),
                IM_COL32(46, 50, 60, 255), 1.0f);

    // A real header hit-target; dragging never steals a close-button click.
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("##header_drag", ImVec2(std::max(1.0f, size.x - 62.0f * g_scale), headerH));
    if (!(flags & ImGuiWindowFlags_NoMove) && ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        ImGui::SetWindowPos(ImVec2(pos.x + delta.x, pos.y + delta.y));
    }
    DrawLogo(ImVec2(pos.x + pad, pos.y + 18.0f * g_scale), 40.0f * g_scale);
    float titleX = pos.x + pad + 54.0f * g_scale;
    ImVec4 textClip(titleX, pos.y, pos.x + size.x - 68.0f * g_scale, pos.y + headerH);
    dl->AddText(FontBold(), FontBold()->FontSize, ImVec2(titleX, pos.y + 17.0f * g_scale),
                IM_COL32(235, 238, 244, 255), title, nullptr, 0, &textClip);
    if (subtitle && *subtitle) {
        dl->AddText(FontSmall(), FontSmall()->FontSize, ImVec2(titleX, pos.y + 43.0f * g_scale),
                    IM_COL32(155, 164, 179, 255), subtitle, nullptr, 0, &textClip);
    }

    // custom close button
    ImGui::PushID(id);
    ImVec2 closePos(pos.x + size.x - 52.0f * g_scale, pos.y + 22.0f * g_scale);
    ImGui::SetCursorScreenPos(closePos);
    bool closeClicked = ImGui::InvisibleButton("##close", ImVec2(32.0f * g_scale, 32.0f * g_scale), ImGuiButtonFlags_EnableNav);
    bool closeHovered = ImGui::IsItemHovered();
    if (closeHovered) {
        dl->AddRectFilled(closePos,
                           ImVec2(closePos.x + 32.0f * g_scale, closePos.y + 32.0f * g_scale),
                          IM_COL32(64, 28, 34, 255), 6.0f * g_scale);
    }
    float xc = closePos.x + 16.0f * g_scale, yc = closePos.y + 16.0f * g_scale;
    ImU32 xcol = closeHovered ? IM_COL32(255, 150, 160, 255) : IM_COL32(140, 150, 168, 255);
    float xr = 5.0f * g_scale;
    dl->AddLine(ImVec2(xc - xr, yc - xr), ImVec2(xc + xr, yc + xr), xcol, 1.6f * g_scale);
    dl->AddLine(ImVec2(xc - xr, yc + xr), ImVec2(xc + xr, yc - xr), xcol, 1.6f * g_scale);
    if (closeHovered) ImGui::SetTooltip("Close manager");
    ImGui::PopID();
    if (closeClicked && open) *open = false;

    // content child
    ImGui::SetCursorPos(ImVec2(0, headerH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, 20.0f * g_scale));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    g_chromeOpen = true;
    g_chromePushed = true;
    if (!ImGui::BeginChild("##chrome_content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
        // child is always visible, but keep the pairing correct anyway
    }
    return true;
}

void EndChromeWindow()
{
    if (!g_chromeOpen) return;
    ImGui::EndChild();
    if (g_chromePushed) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    g_chromeOpen = false;
    g_chromePushed = false;
    ImGui::End();
}

// ---------------------------------------------------------------------------
bool BeginTabBarC(const char* id)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 7));
    bool r = ImGui::BeginTabBar(id, ImGuiTabBarFlags_None);
    ImGui::PopStyleVar();
    return r;
}

bool TabItemC(const char* label, bool selected)
{
    ImGuiTabItemFlags f = selected ? ImGuiTabItemFlags_SetSelected : 0;
    return ImGui::BeginTabItem(label, nullptr, f);
}

bool InputTextString(const char* label, std::string& value, size_t maxLen)
{
    char buf[1024];
    strncpy_s(buf, sizeof(buf), value.c_str(), _TRUNCATE);
    if (maxLen + 1 < sizeof(buf)) buf[maxLen] = 0;
    if (ImGui::InputText(label, buf, sizeof(buf))) {
        value = buf;
        return true;
    }
    return false;
}

bool InputTextMultilineString(const char* label, std::string& value, const ImVec2& size)
{
    char buf[8192];
    strncpy_s(buf, sizeof(buf), value.c_str(), _TRUNCATE);
    if (ImGui::InputTextMultiline(label, buf, sizeof(buf), size)) {
        value = buf;
        return true;
    }
    return false;
}

bool ComboString(const char* label, int* current, const std::vector<std::string>& items)
{
    if (items.empty()) return false;
    if (*current < 0 || *current >= (int)items.size()) *current = 0;
    bool changed = false;
    if (ImGui::BeginCombo(label, items[*current].c_str())) {
        for (int i = 0; i < (int)items.size(); ++i) {
            bool sel = (i == *current);
            if (ImGui::Selectable(items[i].c_str(), sel)) {
                *current = i;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

} // namespace eth::ui
