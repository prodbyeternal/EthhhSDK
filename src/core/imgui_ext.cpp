#include "imgui_ext.h"
#include "util.h"
#include "log.h"
#include "config.h"
#include "liquid_glass.h"
#include "ui_audio.h"
#include "imgui_internal.h"
#include "../third_party/liquid_glass/fonts.h"
#include <cfloat>

namespace eth::ui {

namespace {
ImU32 g_accent = IM_COL32(0, 122, 255, 255);
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

    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    // Font sizes already include DPI; applying it again wastes atlas space.
    cfg.RasterizerDensity = 1.0f;

    auto addFont = [&](std::vector<unsigned char>& bytes, float px) -> ImFont* {
        ImFontConfig c = cfg;
        c.FontDataOwnedByAtlas=false;
        return io.Fonts->AddFontFromMemoryTTF(bytes.data(),int(bytes.size()),px*g_scale,&c,io.Fonts->GetGlyphRangesDefault());
    };

    g_regular = addFont(inter_medium, 15.0f);
    g_small   = addFont(inter_medium, 12.0f);
    g_bold    = addFont(inter_semibold, 15.0f);
    g_title   = addFont(inter_semibold, 28.0f);
    Glass::SetIconFont(nullptr); Glass::SetIconFontBrands(nullptr);
    HMODULE module=nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&LoadFonts),&module);
    static const ImWchar icons[]={0xE000,0xF8FF,0};
    auto iconFont=[&](int id) -> ImFont* {
        HRSRC resource=FindResourceA(module,MAKEINTRESOURCEA(id),RT_RCDATA);
        if (!resource) return nullptr;
        void* bytes=LockResource(LoadResource(module,resource));
        ImFontConfig c=cfg; c.FontDataOwnedByAtlas=false;
        return bytes ? io.Fonts->AddFontFromMemoryTTF(bytes,int(SizeofResource(module,resource)),32*g_scale,&c,icons) : nullptr;
    };
    Glass::SetIconFont(iconFont(201)); Glass::SetIconFontBrands(iconFont(202));

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
    ETH_INFO("Liquid Glass fonts loaded: embedded Inter + Font Awesome (scale %.2f)", g_scale);
}

void ApplyTheme()
{
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle(); // no cumulative DPI scaling when the accent changes
    ImGui::StyleColorsDark(&st);

    st.WindowRounding = 10.0f;
    st.ChildRounding = 10.0f;
    st.PopupRounding = 8.0f;
    st.FrameRounding = 6.0f;
    st.ScrollbarRounding = 8.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.WindowBorderSize = 1.0f;
    st.ChildBorderSize = 1.0f;
    st.FrameBorderSize = 1.0f;
    st.PopupBorderSize = 1.0f;
    st.WindowPadding = ImVec2(20, 18);
    st.FramePadding = ImVec2(12, 8);
    st.CellPadding = ImVec2(8, 6);
    st.ItemSpacing = ImVec2(12, 9);
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

    set(ImGuiCol_Text,                 IM_COL32(230, 235, 234, 255));
    set(ImGuiCol_TextDisabled,         IM_COL32(143, 155, 154, 255));
    set(ImGuiCol_WindowBg,             IM_COL32(15, 19, 21, 255));
    set(ImGuiCol_ChildBg,              IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_PopupBg,              IM_COL32(25, 31, 33, 255));
    set(ImGuiCol_Border,               IM_COL32(43, 53, 55, 255));
    set(ImGuiCol_BorderShadow,         IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_FrameBg,              IM_COL32(23, 29, 31, 255));
    set(ImGuiCol_FrameBgHovered,       IM_COL32(33, 43, 44, 255));
    set(ImGuiCol_FrameBgActive,        IM_COL32(40, 52, 52, 255));
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
    set(ImGuiCol_Button,               IM_COL32(28, 37, 39, 255));
    set(ImGuiCol_ButtonHovered,        IM_COL32(39, 52, 52, 255));
    set(ImGuiCol_ButtonActive,         WithAlpha(g_accent, 0.55f));
    set(ImGuiCol_Header,               IM_COL32(31, 43, 43, 255));
    set(ImGuiCol_HeaderHovered,        IM_COL32(39, 54, 53, 255));
    set(ImGuiCol_HeaderActive,         WithAlpha(g_accent, 0.45f));
    set(ImGuiCol_Separator,            IM_COL32(39, 49, 51, 255));
    set(ImGuiCol_SeparatorHovered,     WithAlpha(g_accent, 0.6f));
    set(ImGuiCol_SeparatorActive,      g_accent);
    set(ImGuiCol_ResizeGrip,           IM_COL32(45, 53, 69, 160));
    set(ImGuiCol_ResizeGripHovered,    WithAlpha(g_accent, 0.45f));
    set(ImGuiCol_ResizeGripActive,     WithAlpha(g_accent, 0.75f));
    set(ImGuiCol_Tab,                  IM_COL32(22, 29, 31, 255));
    set(ImGuiCol_TabHovered,           IM_COL32(38, 53, 51, 255));
    set(ImGuiCol_TabSelected,          IM_COL32(34, 49, 47, 255));
    set(ImGuiCol_TabSelectedOverline,  g_accent);
    set(ImGuiCol_TabDimmed,            IM_COL32(19, 25, 27, 255));
    set(ImGuiCol_TabDimmedSelected,    IM_COL32(28, 39, 39, 255));
    set(ImGuiCol_TabDimmedSelectedOverline, WithAlpha(g_accent, 0.5f));
    set(ImGuiCol_PlotLines,            WithAlpha(g_accent, 0.9f));
    set(ImGuiCol_PlotHistogram,        WithAlpha(g_accent, 0.9f));
    set(ImGuiCol_TextSelectedBg,       WithAlpha(g_accent, 0.35f));
    set(ImGuiCol_DragDropTarget,       g_accent);
    set(ImGuiCol_NavCursor,            g_accent);
    set(ImGuiCol_TableHeaderBg,        IM_COL32(22, 29, 31, 255));
    set(ImGuiCol_TableBorderStrong,    IM_COL32(39, 49, 51, 255));
    set(ImGuiCol_TableBorderLight,     IM_COL32(29, 38, 40, 255));
    set(ImGuiCol_TableRowBg,           IM_COL32(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt,        IM_COL32(255, 255, 255, 4));
    set(ImGuiCol_ModalWindowDimBg,     IM_COL32(5, 7, 10, 170));

    // Global settings apply to launcher, F9, and existing mod ImGui calls.
    auto& cfg=Config::Get();
    const bool light=cfg.GetInt("theme","appearance",0)==1;
    const bool highContrast=cfg.GetBool("theme","high_contrast",true);
    Glass::SetAppearance(light ? 1 : 0);
    const ImVec4 accent=ImGui::ColorConvertU32ToFloat4(g_accent);
    Glass::SetAccent(accent.x,accent.y,accent.z);
    auto setting=[&](const char* key,float fallback,float min,float max) {
        const float value=cfg.GetFloat("theme",key,fallback);
        return std::isfinite(value) ? std::clamp(value,min,max) : fallback;
    };
    Glass::SetGlobalMaterial(setting("blur",1,0,1),setting("opacity",.306f,0,1),
        setting("saturation",1.35f,0,3),setting("refraction",24.49f,0,40),
        setting("chroma",30,0,40),setting("edge",.485f,0,2),setting("shadow",.046f,0,.8f));
    const bool reduced=cfg.GetBool("theme","reduce_motion",false);
    Glass::Spring::reduced_motion=reduced;
    Glass::SetLiquidFlow(reduced ? 0 : setting("flow",16,0,24));
    if(Glass::g) {
        // The actual example application presets, not just glass.h defaults.
        Glass::GlassEdgeConfig e;
        e.fresnel_exp=2.551f;e.bevel_scale=1;e.lip=.6f;e.specular_exp=62.42f;
        e.specular_amt=1.071f;e.front_amt=1.332f;e.back_amt=2.281f;e.sat_pop=1.439f;
        e.bevel_tilt=.5f;e.light_height=.2f;e.edge_shadow=2;e.cursor_size=40;
        e.sheen_amt=1.056f;e.grain_amt=4;e.ambient_rim=2;e.front_spread=4.305f;e.back_spread=3.417f;
        e.panel_br_spread=4.377f;e.panel_br_smooth=4;e.panel_rim_angle=330.6f;e.tbl_fill=2;
        e.adapt_strength=cfg.GetBool("theme","adaptive_tint",false) ? .35f : 0;
        e.lens_amount=1;e.lens_power=4;e.lens_a=1.505f;e.lens_b=2.020f;e.lens_c=2.143f;e.lens_d=3.214f;
        e.glow_weight=.388f;e.glow_phase=1.089f;e.glow_edge0=.102f;e.glow_edge1=-.571f;
        e.glow_anim=cfg.GetBool("theme","reduce_motion",false) ? 0 : .857f;
        Glass::g->SetEdgeConfig(e); Glass::g->SetLights(308.6f,.474f,130.4f,.490f);
        Glass::g->SetBackdropRefreshHz(setting("blur_hz",10,1,60));
    }
    InitUiAudio();Glass::SetSfxEnabled(cfg.GetBool("theme","sounds",true));
    Glass::SetInk(light ? IM_COL32(17,22,34,255) : IM_COL32(248,250,255,255),
        light ? IM_COL32(55,64,80,255) : (highContrast ? IM_COL32(204,213,229,255) : IM_COL32(166,179,200,255)));
    set(ImGuiCol_Text,ThemeInk()); set(ImGuiCol_TextDisabled,ThemeSoftInk());
    set(ImGuiCol_WindowBg,ThemeSurface()); set(ImGuiCol_PopupBg,ThemeSurface());
    const ImU32 frame=light ? IM_COL32(255,255,255,100) : IM_COL32(255,255,255,18);
    const ImU32 hover=light ? IM_COL32(255,255,255,180) : IM_COL32(255,255,255,38);
    for(auto col:{ImGuiCol_FrameBg,ImGuiCol_Button,ImGuiCol_Header,ImGuiCol_Tab,ImGuiCol_TabDimmed,ImGuiCol_TableHeaderBg}) set(col,frame);
    for(auto col:{ImGuiCol_FrameBgHovered,ImGuiCol_ButtonHovered,ImGuiCol_HeaderHovered,ImGuiCol_TabHovered}) set(col,hover);
    for(auto col:{ImGuiCol_FrameBgActive,ImGuiCol_ButtonActive,ImGuiCol_HeaderActive,ImGuiCol_TabSelected,ImGuiCol_TabDimmedSelected}) set(col,AccentDim(.30f));
    for(auto col:{ImGuiCol_TitleBg,ImGuiCol_TitleBgActive,ImGuiCol_TitleBgCollapsed,ImGuiCol_MenuBarBg}) set(col,ThemeSurface());
    const ImU32 line=light ? IM_COL32(20,30,50,48) : IM_COL32(255,255,255,48);
    for(auto col:{ImGuiCol_Border,ImGuiCol_Separator,ImGuiCol_TableBorderStrong,ImGuiCol_TableBorderLight}) set(col,line);
    st.WindowRounding=26*g_scale; st.ChildRounding=22*g_scale; st.PopupRounding=18*g_scale;
    st.FrameRounding=12*g_scale; st.GrabRounding=12*g_scale;
    st.FrameBorderSize=0; st.WindowBorderSize=0;
    st.ChildBorderSize=0;st.ItemSpacing=ImVec2(12*g_scale,10*g_scale);
    st.FramePadding=ImVec2(12*g_scale,8*g_scale);st.ScrollbarSize=7*g_scale;
    st.WindowRounding=30*g_scale;st.ChildRounding=0;
    set(ImGuiCol_Separator,light ? IM_COL32(0,0,0,14) : IM_COL32(255,255,255,16));
    set(ImGuiCol_ChildBg,IM_COL32(0,0,0,0));
}

#include "ui_widgets.inc"

// ---------------------------------------------------------------------------
//  Custom widgets
// ---------------------------------------------------------------------------
bool ToggleSwitch(const char* id, bool* value, float widthScale)
{
    float height = 26.0f * widthScale * g_scale;
    float width = 44.0f * widthScale * g_scale;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height), ImGuiButtonFlags_EnableNav);
    if (clicked) {*value = !*value;PlayUiSound(*value ? Sound::On : Sound::Off);}
    bool hovered = ImGui::IsItemHovered();

    ImGuiID sid = ImGui::GetID(id);
    const float t=std::clamp(Animate(sid,130,*value ? 1 : 0,*value ? 1 : 0),0.0f,1.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 trackOff = hovered ? WithAlpha(ThemeInk(),.22f) : WithAlpha(ThemeInk(),.14f);
    ImU32 track = LerpColor(trackOff, g_accent, t);

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), WithAlpha(track,ImGui::GetStyle().Alpha), height * 0.5f);
    float knobR = height * 0.5f - 3.0f * g_scale;
    float knobX = pos.x + height * 0.5f + t * (width - height);
    dl->AddCircleFilled(ImVec2(knobX, pos.y + height * 0.5f), knobR, WithAlpha(IM_COL32_WHITE,ImGui::GetStyle().Alpha), 32);
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
    return ConnectedButton(label,true,size);
}

bool GhostButton(const char* label, const ImVec2& size)
{
    return ConnectedButton(label,false,size);
}

bool NavItem(const char* id, const char* label, bool selected, const char* shortcut)
{
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float height = 40.0f * g_scale;
    float width = ImGui::GetContentRegionAvail().x;
    bool clicked = ImGui::InvisibleButton("##nav", ImVec2(width, height), ImGuiButtonFlags_EnableNav);
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 max(pos.x + width, pos.y + height);
    if (selected)
        dl->AddRectFilled(pos, max, WithAlpha(g_accent, 0.10f), 6.0f * g_scale);
    else if (hovered)
        dl->AddRectFilled(pos, max, IM_COL32(30, 39, 41, 255), 6.0f * g_scale);
    if (selected)
        dl->AddRect(pos, max, WithAlpha(g_accent, 0.25f), 6.0f * g_scale);

    float textH = ImGui::GetTextLineHeight();
    dl->AddCircleFilled(ImVec2(pos.x + 17*g_scale,pos.y+height*.5f),3*g_scale,
        selected ? g_accent : IM_COL32(88,106,106,255));
    dl->AddText(ImVec2(pos.x + 31.0f * g_scale, pos.y + (height - textH) * 0.5f),
                selected ? IM_COL32(232, 240, 237, 255) : IM_COL32(151, 166, 164, 255), label);
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
    GlassPanel(pos,size,rounding*g_scale);
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
    dl->AddRectFilled(pos, ImVec2(pos.x+size,pos.y+size), AccentDim(.12f), size*.18f);
    dl->AddRect(pos, ImVec2(pos.x+size,pos.y+size), AccentDim(.4f), size*.18f);
    // A geometric E mark: no font/icon dependency and crisp at every DPI.
    for (int i=0;i<3;++i) {
        const float y=pos.y+size*(.28f+i*.20f), width=i==1 ? .32f : .44f;
        dl->AddRectFilled(ImVec2(pos.x+size*.27f,y),ImVec2(pos.x+size*(.27f+width),y+size*.07f),g_accent);
    }
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    bool visible = ImGui::Begin(id, nullptr, flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);
    if (!visible) {
        ImGui::End();
        return false;
    }

    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    float headerH = 68.0f * g_scale;
    float pad = 26.0f * g_scale;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    g_managerFirstPrimitive=Glass::g ? Glass::g->PrimitiveCount() : 0;
    // One material surface. Header/sidebar/content are layout regions only.
    GlassPanel(pos,size,30*g_scale,Glass::Material::Regular);

    // A real header hit-target; dragging never steals a close-button click.
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("##header_drag", ImVec2(std::max(1.0f, size.x - 62.0f * g_scale), headerH));
    if (!(flags & ImGuiWindowFlags_NoMove) && ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        ImGui::SetWindowPos(ImVec2(pos.x + delta.x, pos.y + delta.y));
    }
    Glass::DrawIcon(dl,Glass::Icon::Spark,ImVec2(pos.x+pad+12*g_scale,pos.y+headerH*.5f),22,Accent());
    float titleX = pos.x + pad + 34.0f * g_scale;
    ImVec4 textClip(titleX, pos.y, pos.x + size.x - 68.0f * g_scale, pos.y + headerH);
    dl->AddText(FontBold(), FontBold()->FontSize, ImVec2(titleX, pos.y + 12.0f * g_scale),
                 ThemeInk(), title, nullptr, 0, &textClip);
    if (subtitle && *subtitle) {
        dl->AddText(FontSmall(), FontSmall()->FontSize, ImVec2(titleX, pos.y + 35.0f * g_scale),
                     ThemeSoftInk(), subtitle, nullptr, 0, &textClip);
    }

    // custom close button
    ImGui::PushID(id);
    ImVec2 closePos(pos.x + size.x - 52.0f * g_scale, pos.y + 16.0f * g_scale);
    ImGui::SetCursorScreenPos(closePos);
    bool closeClicked = ImGui::InvisibleButton("##close", ImVec2(32.0f * g_scale, 32.0f * g_scale), ImGuiButtonFlags_EnableNav);
    bool closeHovered = ImGui::IsItemHovered();
    if (closeHovered) {
        dl->AddRectFilled(closePos,
                           ImVec2(closePos.x + 32.0f * g_scale, closePos.y + 32.0f * g_scale),
                           WithAlpha(ThemeInk(),.10f), 16.0f * g_scale);
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
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
    // Fade every vertex in this one window tree, including custom/template
    // widgets. Their alpha cannot get ahead of the glass during transitions.
    ImGuiWindow* root=ImGui::GetCurrentWindow();
    if(g_managerOpacity<1) {
        for(ImGuiWindow* w:ImGui::GetCurrentContext()->Windows) {
            ImGuiWindow* parent=w;
            while(parent && parent!=root) parent=parent->ParentWindow;
            if(!parent || !w->Active) continue;
            for(auto& vertex:w->DrawList->VtxBuffer) vertex.col=WithAlpha(vertex.col,g_managerOpacity);
        }
        if(Glass::g) Glass::g->FadeSince(g_managerFirstPrimitive,g_managerOpacity);
    }
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
