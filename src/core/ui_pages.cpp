#include "ui_pages.h"
#include "imgui_ext.h"
#include "mods.h"
#include "config.h"
#include "util.h"
#include "log.h"
#include "game.h"
#include "input.h"
#include "overlay.h"
#include "hooks.h"
#include "ethhhpatch.h"
#include "ethhhpatch_runtime.h"
#include "pestcontrol.h"
#include "liquid_glass.h"
#include "load_island_policy.h"
#include "ui_audio.h"
#include "ui_motion_policy.h"

#include <cfloat>
#include <cctype>

namespace eth::ui::pages {

namespace {

Page g_page = Page::Mods;
bool g_pageInitialized = false;

char g_memAddr[32] = "00401000";
int  g_memType = 2;             // 0 byte, 1 short, 2 int32, 3 float, 4 double, 5 string
int  g_memWriteType = 2;        // 0 byte, 1 int32, 2 float
char g_memWriteValue[64] = "0";
std::string g_memResult;
bool g_memResultOk = false;

char g_stratFilter[64] = "";
char g_modFilter[96] = "";
uintptr_t g_probeAddr = 0;
void* g_probeClass = nullptr;
uint32_t g_probeHandle = 0;
float g_probeOriginal[3] = {};
ULONGLONG g_probeTime = 0;
int  g_logFilter = 0;
bool g_logAutoScroll = true;

bool g_showModInterfaces = true;

const char* kMemTypes[] = { "byte", "int16", "int32", "float", "double", "string" };
const char* kWriteTypes[] = { "byte", "int32", "float" };

const char* TypeBadge(mods::ModInfo const& m) { return m.IsCSharp() ? "C#" : "LUA"; }
ImU32 TypeBadgeColor(mods::ModInfo const& m) { return m.IsCSharp() ? IM_COL32(120, 200, 160, 255) : IM_COL32(110, 160, 250, 255); }

ImU32 StatusColor(const mods::ModInfo& m)
{
    if (m.errored) return IM_COL32(240, 100, 110, 255);
    if (m.loaded && m.enabled) return IM_COL32(120, 210, 150, 255);
    if (m.enabled) return IM_COL32(230, 190, 100, 255);
    return IM_COL32(140, 150, 168, 255);
}

void PageTitle(const char* title, const char* subtitle)
{
    ImGui::PushFont(FontTitle());
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (subtitle && *subtitle) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", subtitle);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, 8 * ui::Scale()));
}

void BulletWrapped(const char* text)
{
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextWrapped("%s", text);
}

void Metric(const char* label, const char* value, const char* hint)
{
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14*ui::Scale(),12*ui::Scale()));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0,72*ui::Scale()),ImVec2(FLT_MAX,FLT_MAX));
    ImGui::BeginChild("##metric", ImVec2(0,0), ImGuiChildFlags_AlwaysUseWindowPadding |
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
        ImGuiWindowFlags_NoScrollbar);
    ImGui::PushTextWrapPos(0);
    ImGui::PushFont(FontSmall()); ImGui::TextDisabled("%s", label); ImGui::PopFont();
    ImGui::PushFont(FontBold()); ImGui::TextUnformatted(value); ImGui::PopFont();
    ImGui::PushFont(FontSmall()); ImGui::TextWrapped("%s",hint); ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::PopID();
}

void ErrorBanner(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.32f, 0.10f, 0.13f, 0.65f));
    ImGui::BeginChild("##err", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 1.9f), true);
    ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.75f, 1.0f), "%s", text);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
void DrawModCard(mods::ModInfo& m, bool launcherMode)
{
    ImGui::PushID(m.id.c_str());
    const float s = ui::Scale();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f * s, 18.0f * s));
    // Content-sized cards: long descriptions and loader errors stay readable.
    if (ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
                          ImGuiWindowFlags_NoScrollbar)) {
        const ImVec2 icon=ImGui::GetCursorScreenPos();
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(icon,ImVec2(icon.x+38*s,icon.y+38*s),ui::WithAlpha(TypeBadgeColor(m),.16f),10*s);
        Glass::DrawIcon(draw,m.IsCSharp() ? Glass::Icon::Gamepad : Glass::Icon::Sliders,ImVec2(icon.x+19*s,icon.y+19*s),19,TypeBadgeColor(m));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+52*s);
        if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 62.0f * s);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushFont(FontBold());
            ImGui::TextWrapped("%s", m.name.c_str());
            ImGui::PopFont();
            ImGui::PushFont(FontSmall());
            ImGui::TextDisabled("v%s", m.version.c_str());
            if (!m.author.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("/ %s", m.author.c_str());
            }
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", m.description.empty() ? "No description provided." : m.description.c_str());
            ImGui::PopStyleColor();
            ImGui::PushFont(FontSmall());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(StatusColor(m)));
            ImGui::TextWrapped("%s", m.status.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::TableNextColumn();
            bool enabled = m.enabled;
            if (ui::ToggleSwitch("##enabled", &enabled))
                mods::ModManager::Get().SetEnabled(m.id, enabled);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s %s", enabled ? "Disable" : "Enable", m.name.c_str());
            ImGui::PushFont(FontSmall());
            ImGui::TextDisabled(enabled ? "Enabled" : "Off");
            ImGui::PopFont();
            if (!launcherMode && m.IsLua() && ImGui::SmallButton("Reload"))
                mods::ModManager::Get().ReloadLua(m.id);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PopID();
}

void DrawModsPage(bool launcherMode)
{
    // Scroll the complete library, not just a tiny card viewport below fixed
    // metrics. Tall error cards stay reachable on small displays/high DPI.
    ImGui::BeginChild("##modlist", ImVec2(0, 0), false);
    ImGui::PushTextWrapPos(0);
    PageTitle("Library", launcherMode ? "Your game. A little more possibility." :
              "Everything you need, right where you play.");

    auto& mgr = mods::ModManager::Get();
    auto& list = mgr.List();

    const float s = ui::Scale();
    ImGui::PushFont(FontSmall());ImGui::TextDisabled("%zu extensions  /  %zu enabled",list.size(),mgr.CountEnabled());ImGui::PopFont();
    ImGui::Dummy(ImVec2(0,4*s));
    if (ImGui::BeginTable("##mod_toolbar", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, (launcherMode ? 216.0f : 132.0f) * s);
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ui::SearchField("##library_search",g_modFilter,sizeof(g_modFilter),"Search your library");
        ImGui::TableNextColumn();
        if (ui::GhostButton("Open folder", ImVec2(126.0f * s, 0))) mgr.OpenModsFolder();
        if (launcherMode) {
            ImGui::SameLine();
            if (ui::GhostButton("Rescan", ImVec2(78.0f * s, 0))) {
                mgr.SaveConfig(); mgr.Scan(); mgr.LoadConfig();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PushFont(FontSmall());
    ImGui::TextDisabled("%zu installed  /  %zu enabled%s", list.size(), mgr.CountEnabled(),
                        launcherMode ? "" : Format("  /  %zu loaded", mgr.CountLoaded()).c_str());
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 4 * s));

    if (list.empty()) {
        ImGui::Dummy(ImVec2(0, 18 * ui::Scale()));
        ImGui::TextDisabled("No mods found.");
        ImGui::TextDisabled("Drop a folder with mod.json into:");
        ImGui::TextUnformatted(GetModsDir().c_str());
        if (ui::AccentButton("Open mods folder", ImVec2(190 * ui::Scale(), 0)))
            mgr.OpenModsFolder();
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        return;
    }

    std::string filter = ToLower(g_modFilter);
    size_t shown = 0;
    for (auto& m : list) {
        if (!filter.empty() && ToLower(m.name + " " + m.id + " " + m.description).find(filter) == std::string::npos)
            continue;
        ++shown;
        DrawModCard(m, launcherMode);
        if(shown<list.size()) ImGui::Separator();
    }
    if (!shown) ImGui::TextDisabled("No mods match this search.");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

void DrawInterfacesPage()
{
    PageTitle("Interfaces", "Live mod interfaces. F9 closes the overlay.");
    auto& mgr = mods::ModManager::Get();
    if (mgr.CountLoaded() == 0) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("No mod interfaces are active.");
        ImGui::TextDisabled("Enable a Lua or C# mod on the Mods page, then restart the game.");
        ImGui::PopTextWrapPos();
        return;
    }
    ImGui::BeginChild("##interfaces", ImVec2(0, 0), false);
    mgr.DrawGui();
    ImGui::EndChild();
}

void DrawSettingsPage()
{
    PageTitle("Settings", "Interface, input and game diagnostics.");
    // Kept separate so the flag toggles can use local bool copies cleanly.
    auto& cfg = Config::Get();
    bool changed = false;

    ImGui::BeginChild("##settings2", ImVec2(0, 0), false);
    ImGui::PushTextWrapPos(0);

    SectionHeader("LIQUID GLASS", "Global appearance for both managers and mod interfaces. Changes apply immediately.");
    bool themeChanged=false;
    int appearance=std::clamp(cfg.GetInt("theme","appearance",0),0,1);
    const char* appearances[]={"Dark glass","Light glass"};
    ImGui::SetNextItemWidth(std::min(240*ui::Scale(),ImGui::GetContentRegionAvail().x));
    if (Glass::Dropdown("Appearance",appearances,2,&appearance)) { cfg.SetInt("theme","appearance",appearance); themeChanged=true; }
    float color[3]; const ImVec4 accent=ImGui::ColorConvertU32ToFloat4(ui::Accent()); color[0]=accent.x;color[1]=accent.y;color[2]=accent.z;
    if (ImGui::ColorEdit3("Accent",color,ImGuiColorEditFlags_NoInputs)) {
        char hex[7]; ui::U32ToHex(ImGui::ColorConvertFloat4ToU32(ImVec4(color[0],color[1],color[2],1)),hex);
        cfg.SetString("core","accent",hex); ui::SetAccent(ui::HexToU32(hex)); themeChanged=true;
    }
    auto toggleTheme=[&](const char* label,const char* key,bool fallback) {
        bool value=cfg.GetBool("theme",key,fallback);
        if (ui::ToggleSwitch(key,&value)) {cfg.SetBool("theme",key,value);themeChanged=true;}
        ImGui::SameLine(); ImGui::TextWrapped("%s",label);
    };
    toggleTheme("High-contrast text","high_contrast",true);
    toggleTheme("Reduce motion","reduce_motion",false);
    toggleTheme("Interface sounds","sounds",true);
    toggleTheme("Adaptive tint over bright scenes","adaptive_tint",false);
    auto sliderTheme=[&](const char* label,const char* key,float fallback,float min,float max) {
        float value=cfg.GetFloat("theme",key,fallback);
        ImGui::SetNextItemWidth(std::max(100.0f,ImGui::GetContentRegionAvail().x*.65f));
        if (ui::GlassSlider(label,&value,min,max,"%.2f")) {cfg.SetFloat("theme",key,value);themeChanged=true;}
    };
    sliderTheme("Frost / blur","blur",1,0,1);
    sliderTheme("Tint opacity / readability","opacity",.306f,0,.95f);
    sliderTheme("Saturation","saturation",1.35f,0,3);
    sliderTheme("Refraction","refraction",24.49f,0,40);
    sliderTheme("Chromatic fringe","chroma",30,0,40);
    sliderTheme("Edge lighting","edge",.485f,0,2);
    sliderTheme("Shadow","shadow",.046f,0,.8f);
    sliderTheme("Liquid flow","flow",16,0,24);
    sliderTheme("Backdrop refresh Hz","blur_hz",10,1,60);
    if (themeChanged) {ui::ApplyTheme(); changed=true;}
    Divider();

    SectionHeader("OVERLAY", "How the in-game menu behaves");
    Badge("ETHHHSDK",ui::Accent()); ImGui::SameLine();
    ImGui::TextWrapped("Load-success island transitions into the gameplay watermark");

    bool pause = cfg.GetBool("core", "pause_on_overlay", true);
    if (ui::ToggleSwitch("##pause", &pause)) { cfg.SetBool("core", "pause_on_overlay", pause); changed = true; }
    ImGui::SameLine(); ImGui::TextWrapped("Pause game while the overlay is open");

    static const char* keys[] = { "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12" };
    int keyIdx = overlay::ToggleKey() - VK_F1;
    if (keyIdx < 0 || keyIdx > 11) keyIdx = 8;
    ImGui::SetNextItemWidth(130.0f * ui::Scale());
    if (ImGui::Combo("Overlay key", &keyIdx, keys, 12)) {
        overlay::SetToggleKey(VK_F1 + keyIdx);
        cfg.SetInt("core", "overlay_key", VK_F1 + keyIdx);
        changed = true;
    }

    int detail = cfg.GetInt("core", "watermark_detail", 1);
    ImGui::SetNextItemWidth(180.0f * ui::Scale());
    const char* detailNames[] = { "Minimal", "Standard", "Detailed" };
    if (ImGui::Combo("Watermark detail", &detail, detailNames, 3)) {
        cfg.SetInt("core", "watermark_detail", detail);
        changed = true;
    }

    static const char* positions[] = { "Top left", "Top right", "Bottom left", "Bottom right" };
    int pos = cfg.GetInt("core", "watermark_pos", 0);
    ImGui::SetNextItemWidth(180.0f * ui::Scale());
    if (ImGui::Combo("Watermark position", &pos, positions, 4)) {
        cfg.SetInt("core", "watermark_pos", pos);
        changed = true;
    }

    Divider();

    SectionHeader("GAME", "Fixed-tick engine helpers");
    float ts = game::TimeScale();
    if (ImGui::SliderFloat("Time scale", &ts, 0.0f, 3.0f, "%.2fx")) {
        game::SetTimeScale(ts);
    }

    ImGui::Dummy(ImVec2(0, 4 * ui::Scale()));
    ImGui::TextUnformatted("Debug flags");
    static const struct { uint16_t bit; const char* name; } flags[] = {
        { 0x001, "Level select" }, { 0x002, "Demo level select" }, { 0x004, "Sound" },
        { 0x008, "PAL" }, { 0x010, "Debug cheats" }, { 0x020, "Debug text" },
        { 0x040, "Debug fly" }, { 0x080, "Demo build" }, { 0x100, "Demo magazine" },
    };
    uint16_t current = game::DebugFlags();
    for (int i = 0; i < (int)(sizeof(flags) / sizeof(flags[0])); ++i) {
        bool on = (current & flags[i].bit) != 0;
        if (ui::Checkbox(flags[i].name, &on)) {
            game::ToggleDebugFlag(flags[i].bit, on);
            changed = true;
        }
        if (i + 1 < (int)(sizeof(flags) / sizeof(flags[0]))) {
            const float required = ImGui::CalcTextSize(flags[i + 1].name).x + 58 * ui::Scale();
            const float remaining = ImGui::GetContentRegionAvail().x - (ImGui::GetItemRectMax().x - ImGui::GetCursorScreenPos().x);
            if (remaining >= required) ImGui::SameLine(0, 26 * ui::Scale());
        }
    }

    Divider();

    SectionHeader("PLAYER DETECTION", "Exact class Ninja is preferred; pin it before experimental HL activation.");
    int mode = (int)game::PlayerTracker::Get().Mode();
    const char* modes[] = { "Auto (unique exact Ninja)", "Manual", "Learning", "Auto+ (detect and pin)" };
    ImGui::SetNextItemWidth(240 * ui::Scale());
    if (ImGui::Combo("Mode", &mode, modes, 4)) {
        game::PlayerTracker::Get().SetMode((game::PlayerMode)mode);
    }
    if (ui::GhostButton("Learn candidates (idle / move / stop)", ImVec2(0, 0))) {
        game::PlayerTracker::Get().BeginLearning();
    }
    uintptr_t addr = game::PlayerTracker::Get().Addr();
    ImGui::TextDisabled("Player: %s", addr ? Format("%08X", (unsigned)addr).c_str() : "not detected");
    if (game::PlayerTracker::Get().LearningActive())
        ImGui::TextWrapped("Close F9: idle 1s, move with WASD/arrows 3s, stop 2s. Then confirm a candidate in Inspector > Player.");

    Divider();

    SectionHeader("ADVANCED", nullptr);
    bool verbose = cfg.GetBool("core", "verbose", false);
    if (ui::ToggleSwitch("##verbose", &verbose)) { cfg.SetBool("core", "verbose", verbose); changed = true; }
    ImGui::SameLine(); ImGui::TextWrapped("Verbose debug logging");

    bool blockInput = cfg.GetBool("core", "block_game_input", true);
    if (ui::ToggleSwitch("##blockinput", &blockInput)) { cfg.SetBool("core", "block_game_input", blockInput); changed = true; }
    ImGui::SameLine(); ImGui::TextWrapped("Swallow game window input while overlay is open");

    ImGui::Dummy(ImVec2(0, 8 * ui::Scale()));
    float fontScale = cfg.GetFloat("core", "font_scale", 1.0f);
    ImGui::SetNextItemWidth(180 * ui::Scale());
    if (ImGui::SliderFloat("Interface scale", &fontScale, 0.8f, 1.6f, "%.2fx")) {
        cfg.SetFloat("core", "font_scale", fontScale);
        changed = true;
    }
    HelpMarker("Font sizes are rebuilt on the next launch.");

    ImGui::Dummy(ImVec2(0, 12 * ui::Scale()));
    if (ui::GhostButton("Reset all settings", ImVec2(180 * ui::Scale(), 0))) {
        cfg.ResetToDefaults();
        cfg.Save();
        overlay::SetToggleKey(cfg.GetInt("core", "overlay_key", VK_F9));
    }

    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    if (changed)
        cfg.Save();
}

void DrawConsolePage()
{
    PageTitle("Console", "Live SDK log. Lua print() and C# logs land here.");

    static const char* filters[] = { "All", "Info", "Warn", "Error", "Debug" };
    ImGui::SetNextItemWidth(140 * ui::Scale());
    ImGui::Combo("Filter", &g_logFilter, filters, 5);
    ImGui::SameLine();
    ui::Checkbox("Auto-scroll", &g_logAutoScroll);
    ImGui::SameLine();
    if (ui::GhostButton("Clear", ImVec2(80 * ui::Scale(), 0))) {
        LogClear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu lines", LogSnapshot().size());

    ImGui::BeginChild("##console", ImVec2(0, 0), true);
    ImGui::PushTextWrapPos(0);
    auto lines = LogSnapshot();
    for (const auto& l : lines) {
        if (g_logFilter == 1 && l.level != LogLevel::Info) continue;
        if (g_logFilter == 2 && l.level != LogLevel::Warn) continue;
        if (g_logFilter == 3 && l.level != LogLevel::Error) continue;
        if (g_logFilter == 4 && l.level != LogLevel::Debug) continue;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(LogLevelColor(l.level)));
        ImGui::TextUnformatted(l.text.c_str());
        ImGui::PopStyleColor();
    }
    if (g_logAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f)
        ImGui::SetScrollHereY(1.0f);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

void DrawCameraInspector()
{
    game::CameraView cam;
    if (!game::ReadCamera(cam)) {
        ImGui::TextDisabled("Camera not available yet (load a level).");
        return;
    }
    ImGui::TextDisabled("Camera object");
    ImGui::SameLine(160 * ui::Scale());
    ImGui::Text("%08X", (unsigned)cam.addr);
    ImGui::TextDisabled("Position");
    ImGui::SameLine(160 * ui::Scale());
    ImGui::Text("%.3f  %.3f  %.3f", cam.x, cam.y, cam.z);
    ImGui::TextDisabled("FOV");
    ImGui::SameLine(160 * ui::Scale());
    ImGui::Text("%.3f rad   (%.2f deg)", cam.fov, cam.fov * 180.0f / 3.14159265358979f);
    ImGui::TextDisabled("param104");
    ImGui::SameLine(160 * ui::Scale());
    ImGui::Text("%.3f", cam.param104);

    ImGui::Dummy(ImVec2(0, 4 * ui::Scale()));
    auto row = [&](const char* label, const float* v) {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(160 * ui::Scale());
        ImGui::Text("% .3f  % .3f  % .3f", v[0], v[1], v[2]);
    };
    row("Right", cam.world + 0);
    row("Up", cam.world + 4);
    row("Forward", cam.world + 8);
}

void DrawPlayerInspector()
{
    auto& pt = game::PlayerTracker::Get();
    int mode = (int)pt.Mode();
    const char* modes[] = { "Auto", "Manual", "Learning", "Auto+" };
    ImGui::SetNextItemWidth(160 * ui::Scale());
    if (ImGui::Combo("##playermode", &mode, modes, 4))
        pt.SetMode((game::PlayerMode)mode);

    ImGui::SameLine();
    if (ui::GhostButton("Learn candidates", ImVec2(140 * ui::Scale(), 0)))
        pt.BeginLearning();
    ImGui::SameLine();
    uintptr_t addr = pt.Addr();
    ImGui::Text("%s", addr ? Format("%08X", (unsigned)addr).c_str() : "not detected");
    ImGui::TextWrapped("%s", pt.DetectionInfo().c_str());

    ImGui::TextWrapped("Learn is a shortlist, not a player-ID guarantee. Close F9, idle for 1s, move with WASD/arrows for 3s, stop for 2s. Reopen this page and try a candidate.");
    ImGui::TextWrapped("Pin exact class Ninja. With HL off, a small +X nudge checks visible root/body behavior; it is still a raw post-tick write, not a verified model/collision update. Attachments are not player roots.");
    auto* hl = mods::ModManager::Get().Find("halflife_movement");
    const bool hlActive = hl && hl->enabled && hl->status.rfind("active", 0) == 0;
    game::StratView selected;
    if (pt.Pinned() && pt.Valid() && game::ReadStrat(pt.Addr(), selected)) {
        game::StratIdentity identity;
        if (game::ReadStratIdentity(selected.addr, identity)) {
            ImGui::Text("%s  |  self handle %08X  |  class %08X", selected.className, identity.selfHandle, identity.pClass);
            ImGui::TextDisabled("model %08X  animation %08X  node matrices %08X (read-only addresses)",
                identity.model, identity.animation, identity.nodeMatrices);
            if (identity.ownerFieldKnown)
                ImGui::TextDisabled("owner handle %08X -> %08X (%s)", identity.rawOwnerHandle, identity.ownerAddr,
                    identity.ownerAddr ? "slot generation + live list checked" : "unresolved; no native call");
            float cached[3] = {};
            if (SafeReadBytes(selected.addr + 0x40, cached, sizeof(cached)) &&
                std::isfinite(cached[0]) && std::isfinite(cached[1]) && std::isfinite(cached[2])) {
                const float dx=selected.pos[0]-cached[0], dy=selected.pos[1]-cached[1], dz=selected.pos[2]-cached[2];
                ImGui::TextDisabled("root/cache gap %.6f game units (non-atomic; not a sync guarantee)", sqrtf(dx*dx+dy*dy+dz*dz));
            }
            for (const auto& c : pt.Candidates()) {
                if (!game::HasKnownOwnerField(c.strat.className)) continue;
                game::StratIdentity linked;
                if (game::ReadStratIdentity(c.strat.addr, linked) && linked.ownerAddr == selected.addr)
                    BulletWrapped(Format("%s %08X follows this owner handle", c.strat.className, (unsigned)c.strat.addr).c_str());
            }
        }
        const bool exactNinja = game::ClassRole(selected.className) == game::StratRole::Ninja;
        ImGui::BeginDisabled(hlActive || !exactNinja);
        if (ImGui::Button("Nudge selected +X (0.25 game units)")) {
            bool ok = game::WriteStratPos(selected.addr, selected.pos[0]+0.25f, selected.pos[1], selected.pos[2]);
            if (ok) {
                g_probeAddr = selected.addr; g_probeClass = selected.pClass;
                g_probeHandle = selected.selfHandle;
                memcpy(g_probeOriginal, selected.pos, sizeof(g_probeOriginal)); g_probeTime = GetTickCount64();
            }
            ETH_INFO("player probe: nudge %08X class %p by +0.25 X: %s (visual confirmation required)",
                     (unsigned)selected.addr,selected.pClass,ok ? "written" : "failed");
        }
        ImGui::EndDisabled();
        if (hlActive) ImGui::TextDisabled("F3 stops HL before a player-ID probe.");
        if (!exactNinja) ImGui::TextDisabled("Only exact Ninja can use the root/body probe or activate HL.");
    }
    game::StratView probed;
    if (g_probeAddr && GetTickCount64()-g_probeTime < 30000 && pt.Pinned() &&
        pt.Valid() && game::ReadStrat(g_probeAddr,probed) &&
        probed.pClass == g_probeClass && probed.selfHandle == g_probeHandle && pt.Addr() == g_probeAddr &&
        game::ClassRole(probed.className) == game::StratRole::Ninja) {
        ImGui::BeginDisabled(hlActive);
        if (ImGui::Button("Return probed object to saved position")) {
            bool ok = game::WriteStratPos(g_probeAddr,g_probeOriginal[0],g_probeOriginal[1],g_probeOriginal[2]);
            ETH_INFO("player probe: return %08X to saved position: %s",(unsigned)g_probeAddr,ok ? "written" : "failed");
            g_probeAddr=0;
        }
        ImGui::EndDisabled();
    } else {
        // Expiry, selection changes and unlinking invalidate the saved return.
        g_probeAddr=0;
    }

    ImGui::Dummy(ImVec2(0, 4 * ui::Scale()));
    auto& cands = pt.Candidates();
    if (ImGui::BeginTable("##candidates", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Class");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Camera dist");
        ImGui::TableSetupColumn("Score / run u/s");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        int shown = 0;
        for (const auto& c : cands) {
            if (shown++ >= 24) break;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%08X", (unsigned)c.strat.addr);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(c.strat.className[0] ? c.strat.className : "<unreadable>");
            ImGui::TextDisabled("%08X", (unsigned)(uintptr_t)c.strat.pClass);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f %.1f %.1f", c.strat.pos[0], c.strat.pos[1], c.strat.pos[2]);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", c.distanceToCamera);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f / %.2f", c.learnScore, c.activeSpeed);
            ImGui::TableNextColumn();
            ImGui::PushID((int)c.strat.addr);
            if (ImGui::SmallButton("Pin")) pt.Select(c.strat.addr);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void DrawStratInspector()
{
    ImGui::SetNextItemWidth(220 * ui::Scale());
    ImGui::InputTextWithHint("##stratfilter", "filter by name / address / value", g_stratFilter, sizeof(g_stratFilter));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu strats", game::CountStrats());

    game::CameraView cam;
    bool hasCam = game::ReadCamera(cam);
    auto strats = game::SnapshotStrats(400);

    std::string filter = ToLower(g_stratFilter);
    ImGui::BeginChild("##strattable", ImVec2(0, 0), true);
    if (ImGui::BeginTable("##strats", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Class name / address");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableHeadersRow();

        for (const auto& s : strats) {
            std::string addrStr = Format("%08X", (unsigned)s.addr);
            std::string classStr = Format("%08X", (unsigned)(uintptr_t)s.pClass);
            if (!filter.empty()) {
                std::string hay = ToLower(addrStr + " " + classStr + " " + s.className + " " + Format("%.1f %.1f %.1f", s.pos[0], s.pos[1], s.pos[2]));
                if (hay.find(filter) == std::string::npos) continue;
            }
            float d = 0.0f;
            if (hasCam) {
                float dx = s.pos[0] - cam.x, dy = s.pos[1] - cam.y, dz = s.pos[2] - cam.z;
                d = sqrtf(dx * dx + dy * dy + dz * dz);
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(addrStr.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(s.className[0] ? s.className : "<unreadable>");
            ImGui::TextDisabled("%s", classStr.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.1f %.1f %.1f", s.pos[0], s.pos[1], s.pos[2]);
            ImGui::TableNextColumn(); ImGui::Text("%X", s.flags);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", d);
            ImGui::TableNextColumn();
            ImGui::PushID((int)s.addr);
            if (ImGui::SmallButton("Use")) game::PlayerTracker::Get().Select(s.addr);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void DrawMemoryInspector()
{
    ImGui::TextDisabled("Address (hex)");
    ImGui::SameLine(120 * ui::Scale());
    ImGui::SetNextItemWidth(140 * ui::Scale());
    ImGui::InputText("##addr", g_memAddr, sizeof(g_memAddr));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110 * ui::Scale());
    ImGui::Combo("##type", &g_memType, kMemTypes, 6);
    ImGui::SameLine();
    if (ui::GhostButton("Read", ImVec2(80 * ui::Scale(), 0))) {
        uintptr_t addr = (uintptr_t)strtoul(g_memAddr, nullptr, 16);
        g_memResultOk = false;
        g_memResult.clear();
        switch (g_memType) {
        case 0: { uint8_t v; if (SafeRead(addr, v)) { g_memResult = Format("%u (0x%02X)", v, v); g_memResultOk = true; } break; }
        case 1: { int16_t v; if (SafeRead(addr, v)) { g_memResult = Format("%d (0x%04X)", v, (uint16_t)v); g_memResultOk = true; } break; }
        case 2: { int32_t v; if (SafeRead(addr, v)) { g_memResult = Format("%d (0x%08X)", v, (uint32_t)v); g_memResultOk = true; } break; }
        case 3: { float v; if (SafeRead(addr, v)) { g_memResult = Format("%.6f", v); g_memResultOk = true; } break; }
        case 4: { double v; if (SafeRead(addr, v)) { g_memResult = Format("%.6f", v); g_memResultOk = true; } break; }
        case 5: { char buf[65] = {}; if (SafeReadBytes(addr, buf, 64)) { g_memResult = buf; g_memResultOk = true; } break; }
        default: break;
        }
        if (!g_memResultOk) g_memResult = "unreadable";
    }

    ImGui::Dummy(ImVec2(0, 4 * ui::Scale()));
    if (!g_memResult.empty())
        ImGui::TextColored(g_memResultOk ? ImVec4(0.5f, 0.9f, 0.65f, 1.0f) : ImVec4(0.95f, 0.45f, 0.45f, 1.0f),
                           "%s", g_memResult.c_str());

    Divider();
    ImGui::TextDisabled("Write");
    ImGui::SameLine(120 * ui::Scale());
    ImGui::SetNextItemWidth(140 * ui::Scale());
    ImGui::InputText("##waddr", g_memAddr, sizeof(g_memAddr));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90 * ui::Scale());
    ImGui::Combo("##wtype", &g_memWriteType, kWriteTypes, 3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140 * ui::Scale());
    ImGui::InputText("##wvalue", g_memWriteValue, sizeof(g_memWriteValue));
    ImGui::SameLine();
    if (ui::GhostButton("Write", ImVec2(80 * ui::Scale(), 0))) {
        uintptr_t addr = (uintptr_t)strtoul(g_memAddr, nullptr, 16);
        bool ok = false;
        if (g_memWriteType == 0) { uint8_t v = (uint8_t)strtoul(g_memWriteValue, nullptr, 0); ok = SafeWrite(addr, v); }
        else if (g_memWriteType == 1) { int32_t v = (int32_t)strtol(g_memWriteValue, nullptr, 0); ok = SafeWrite(addr, v); }
        else { float v = (float)atof(g_memWriteValue); ok = SafeWrite(addr, v); }
        g_memResult = ok ? "written" : "write failed";
        g_memResultOk = ok;
    }
}

void DrawInspectorPage(bool launcherMode)
{
    PageTitle("Inspector", "Live engine state, strats, player detection and memory.");

    if (launcherMode) {
        ImGui::TextDisabled("The Inspector becomes available once the game is running.");
        return;
    }

    ImGui::BeginChild("##inspector", ImVec2(0, 0), false);

    if (ImGui::BeginTabBar("##inspectortabs")) {
        if (ImGui::BeginTabItem("Runtime")) {
            ImGui::Dummy(ImVec2(0, 6 * ui::Scale()));
            game::CameraView cam;
            if (game::ReadCamera(cam)) {
                ImGui::TextDisabled("Tick");
                ImGui::SameLine(180 * ui::Scale());
                ImGui::Text("%u", game::TickCount());
                ImGui::TextDisabled("Frame");
                ImGui::SameLine(180 * ui::Scale());
                ImGui::Text("%u", game::FrameCount());
                ImGui::TextDisabled("Time");
                ImGui::SameLine(180 * ui::Scale());
                ImGui::Text("%.3f s (step %.5f)", game::GameTime(), game::TimeStep());
                ImGui::TextDisabled("Time scale");
                ImGui::SameLine(180 * ui::Scale());
                float ts = game::TimeScale();
                ImGui::SetNextItemWidth(180 * ui::Scale());
                if (ImGui::SliderFloat("##ts", &ts, 0.0f, 3.0f, "%.2fx")) game::SetTimeScale(ts);
                ImGui::TextDisabled("Hooks");
                ImGui::SameLine(180 * ui::Scale());
                ImGui::Text("tick %s   d3d %s   wnd %s",
                            hooks::TickHooked() ? "on" : "off",
                            hooks::D3DHooked() ? "on" : "off",
                            hooks::WndProcHooked() ? "on" : "off");
            } else {
                ImGui::TextDisabled("Engine not ready (load a level).");
            }
            Divider();
            SectionHeader("CAMERA", nullptr);
            DrawCameraInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Player")) {
            ImGui::Dummy(ImVec2(0, 6 * ui::Scale()));
            DrawPlayerInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Strats")) {
            ImGui::Dummy(ImVec2(0, 6 * ui::Scale()));
            DrawStratInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory")) {
            ImGui::Dummy(ImVec2(0, 6 * ui::Scale()));
            DrawMemoryInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Stats")) {
            ImGui::Dummy(ImVec2(0, 6 * ui::Scale()));
            ImGui::TextDisabled("Profile integer table (first 32 entries)");
            for (int i = 0; i < 32; ++i) {
                int v = 0;
                if (!game::GetStatInt(i, v)) continue;
                ImGui::Text("%02d", i);
                ImGui::SameLine(60 * ui::Scale());
                ImGui::Text("%d", v);
                if (i % 4 != 3) ImGui::SameLine(180 * ui::Scale() + (i % 4) * 120 * ui::Scale());
                else ImGui::Dummy(ImVec2(0, 1));
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

void DrawAboutPage()
{
    PageTitle("About", "I-Ninja modding API and SDK.");

    ImGui::BeginChild("##about", ImVec2(0, 0), false);
    ImGui::PushTextWrapPos(0);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(pos,ImVec2(pos.x+54*ui::Scale(),pos.y+54*ui::Scale()),ui::AccentDim(.16f),15*ui::Scale());
    Glass::DrawIcon(ImGui::GetWindowDrawList(),Glass::Icon::Spark,ImVec2(pos.x+27*ui::Scale(),pos.y+27*ui::Scale()),27,ui::Accent());
    ImGui::Dummy(ImVec2(66.0f * ui::Scale(), 54.0f * ui::Scale()));
    if (ImGui::GetContentRegionAvail().x >= 420 * ui::Scale()) ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(FontTitle());
    ImGui::TextUnformatted("EthhhSDK");
    ImGui::PopFont();
    ImGui::TextDisabled("v%s  -  build %s", kVersion, ETH_BUILD_DATE);
    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0, 12 * ui::Scale()));
    ImGui::TextWrapped("A modding SDK for I-Ninja (2003, Windows). It hosts a pre-game mod manager, "
                       "a Lua scripting API and a managed .NET mod host, all drawn with Dear ImGui on top "
                       "of the game's Direct3D 9 device.");
    ImGui::Dummy(ImVec2(0, 8 * ui::Scale()));

    SectionHeader("BUILD", nullptr);
    auto detail=[](const char* label,const std::string& value) {
        ImGui::PushFont(FontSmall());ImGui::TextDisabled("%s",label);ImGui::PopFont();
        ImGui::TextWrapped("%s",value.c_str());ImGui::Dummy(ImVec2(0,3*ui::Scale()));
    };
    detail("Game directory",GetGameDir());detail("Mods directory",GetModsDir());
    detail("Injection","dinput8.dll proxy -> EthhhSDK.dll entry gate");
    detail("Overlay key",input::KeyName(overlay::ToggleKey()));

    Divider();
    SectionHeader("CREDITS", nullptr);
    BulletWrapped("Dear ImGui by Omar Cornut and contributors (MIT).");
    BulletWrapped("MinHook by Tsuda Kageyu (BSD 2-Clause).");
    BulletWrapped("Lua 5.4 by Lua.org, PUC-Rio (MIT).");
    BulletWrapped("Liquid Glass template: liquidDX11 (MIT). Fonts/icons: Inter and Font Awesome.");
    BulletWrapped("Half-Life movement reference: Valve HLSDK (halflife-steam_legacy).");
    ImGui::TextDisabled("Full notices are bundled. Valve/audio distribution rights still need review.");

    Divider();
    ImGui::TextDisabled("This SDK is a fan project and is not affiliated with Namco / Argonaut Games.");
    ImGui::TextDisabled("I-Ninja (c) 2003 Namco Hometek Inc. / Argonaut Games plc.");

    ImGui::PopTextWrapPos();ImGui::EndChild();
}

void DrawPatchPage(bool launcherMode)
{
    PageTitle("EthhhPatch", "A smoother I-Ninja. Display, free-look camera and render-only animation blending.");
#ifdef ETH_TESTING
    if (patch::HasPreviewStatus()) ImGui::TextWrapped("OFF-SCREEN FIXTURE: synthetic readings below, not live game FPS or an unlock.");
#endif
    ImGui::BeginChild("##patch", ImVec2(0, 0), false);
    ImGui::PushTextWrapPos(0);
    static patch::DisplayPlan plan = patch::LoadPlan();
    static std::string message;
    auto& cfg=Config::Get();
    const auto monitor = patch::ReadMonitor(launcherMode ? GetActiveWindow() : (HWND)game::GameHwnd());
    const auto apply = patch::runtime::ReadApplyStatus();
    const auto status=patch::ReadStatus();
    const char* applyName = "Idle";
    switch (apply.state) {
    case patch::runtime::ApplyState::Applying: applyName = "Applying / waiting for Reset"; break;
    case patch::runtime::ApplyState::Active: applyName = "Active"; break;
    case patch::runtime::ApplyState::RollingBack: applyName = "Rolling back"; break;
    case patch::runtime::ApplyState::RolledBack: applyName = "Rolled back"; break;
    case patch::runtime::ApplyState::Failed: applyName = "Failed"; break;
    default: break;
    }
    if (ImGui::BeginTable("##patch_metrics",3,ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow(); ImGui::TableNextColumn(); Metric("TRANSACTION",applyName,"Opt-in + reversible");
        ImGui::TableNextColumn(); Metric("GAME RENDER",status.sampled ? Format("%.0f FPS",status.gamePresentsPerSecond).c_str() : "Waiting","Measured game presents");
        ImGui::TableNextColumn(); Metric("SIMULATION",status.sampled ? Format("%.0f Hz",status.activeTicksPerSecond).c_str() : "60 Hz target","No extra animation ticks");
        ImGui::EndTable();
    }
    if (!apply.detail.empty()) ImGui::TextWrapped("%s",apply.detail.c_str());
    const bool compact=ImGui::GetContentRegionAvail().x<420*ui::Scale();
    const auto checkbox=[compact](const char* label,bool* value) {
        if (!compact) return ui::Checkbox(label,value);
        ImGui::PushID(label);
        bool changed=ui::Checkbox("##value",value);
        ImGui::SameLine(); ImGui::TextWrapped("%s",label);
        if (ImGui::IsItemClicked()) { *value=!*value; changed=true; }
        ImGui::PopID(); return changed;
    };
    if (ImGui::BeginTabBar("##patch_tabs")) {
        if (ImGui::BeginTabItem("Display")) {
            SectionHeader("BUILD A DISPLAY PLAN","Saving is harmless. Apply changes the display only after an explicit acknowledgement.");
            if (ImGui::Button("1080p")) plan.width=1920,plan.height=1080;
            ImGui::SameLine(); if (ImGui::Button("4K")) plan.width=3840,plan.height=2160;
            if (!compact) ImGui::SameLine(); if (ImGui::Button("Monitor size") && monitor.width && monitor.height) plan.width=monitor.width,plan.height=monitor.height;
            if (compact) {
                ImGui::TextUnformatted("Width"); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputInt("##width",&plan.width);
                ImGui::TextUnformatted("Height"); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputInt("##height",&plan.height);
                ImGui::TextWrapped("Render Hz (0 = monitor)"); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputInt("##render_hz",&plan.renderHz);
            } else {
                ImGui::SetNextItemWidth(140*ui::Scale()); ImGui::InputInt("Width",&plan.width);
                ImGui::SameLine(); ImGui::SetNextItemWidth(140*ui::Scale()); ImGui::InputInt("Height",&plan.height);
                ImGui::SetNextItemWidth(180*ui::Scale()); ImGui::InputInt("Render Hz (0 = monitor)",&plan.renderHz);
            }
            checkbox("Windowed target",&plan.windowed);
            if (monitor.hz) ImGui::TextWrapped("OS monitor: %dx%d @ %d Hz. This is not measured game FPS.",monitor.width,monitor.height,monitor.hz);
            else ImGui::TextWrapped("Monitor refresh unknown. No assumed fallback.");
            const bool valid=patch::ValidPlan(plan);
            if (valid) ImGui::TextWrapped("Target aspect %.4f. Simulation remains fixed at 60 Hz.",patch::Aspect(plan.width,plan.height));
            else ImGui::TextWrapped("Invalid plan: width 640..7680, height 480..4320, render Hz 0 or 30..360.");
            ImGui::BeginDisabled(!valid);
            if (ui::GhostButton(compact ? "Save future plan" : "Save future plan (not applied)")) message=patch::SavePlan(plan) ? "Plan saved. No runtime display change." : "Plan could not be saved.";
            ImGui::EndDisabled();
            Divider(4);
            static bool acknowledgeExperimental=false;
            checkbox("I understand this display patch is experimental",&acknowledgeExperimental);
            const bool pending=apply.state==patch::runtime::ApplyState::Applying || apply.state==patch::runtime::ApplyState::RollingBack;
            ImGui::BeginDisabled(launcherMode || !valid || !acknowledgeExperimental || pending || apply.state==patch::runtime::ApplyState::Active);
            if (ui::AccentButton(compact ? "Apply patch" : "Apply experimental patch")) {
                std::string error;
                if (!patch::SavePlan(plan)) message="Plan save failed; display was not changed.";
                else if (patch::runtime::RequestDisplayPlan(plan, monitor.hz, error)) message="Waiting for exact Reset verification.";
                else message="Apply refused: "+error;
            }
            ImGui::EndDisabled();
            if (!compact) ImGui::SameLine(); ImGui::BeginDisabled(launcherMode || apply.state!=patch::runtime::ApplyState::Active);
            if (ui::GhostButton(compact ? "Restore display" : "Restore previous display")) {
                std::string error; message=patch::runtime::RequestRestore(error) ? "Restore requested." : "Restore refused: "+error;
            }
            ImGui::EndDisabled();
            if (launcherMode) ImGui::TextWrapped("Start the game manually, then open F9 to apply or restore.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera")) {
            const auto slider=[compact](const char* label,float* value,float low,float high,const char* format) {
                if (!compact) { ImGui::SetNextItemWidth(210*ui::Scale()); return ImGui::SliderFloat(label,value,low,high,format); }
                ImGui::TextUnformatted(label); ImGui::PushID(label); ImGui::SetNextItemWidth(-FLT_MIN);
                const bool changed=ImGui::SliderFloat("##value",value,low,high,format);
                ImGui::PopID(); return changed;
            };
            SectionHeader("NORMAL-MODE FOV", "Vertical field of view for I-Ninja gameplay. HL keeps its own camera settings.");
            bool fovEnabled=cfg.GetBool("ethhhpatch","normal_fov_enabled",false);
            float fov=patch::runtime::NormalFovDegrees(cfg.GetFloat("ethhhpatch","normal_fov_degrees",60));
            bool lensChanged=checkbox("Override normal-mode FOV",&fovEnabled);
            lensChanged|=slider("Normal-mode FOV",&fov,30,120,"%.0f degrees");
            if (lensChanged) {
                cfg.SetBool("ethhhpatch","normal_fov_enabled",fovEnabled); cfg.SetFloat("ethhhpatch","normal_fov_degrees",fov); cfg.Save();
                patch::runtime::SetNormalFovTuning(fovEnabled,fov);
            }
            ImGui::TextWrapped("Requires the active display patch. Applied after closing F9; movies, scripted shots and HL retain native/HL framing.");
            bool mouseAttacks=cfg.GetBool("ethhhpatch","mouse_attacks",true);
            if (checkbox("Mouse attacks: M1 slash / M2 spin",&mouseAttacks)) {
                cfg.SetBool("ethhhpatch","mouse_attacks",mouseAttacks); cfg.Save(); input::SetMouseAttacksEnabled(mouseAttacks);
            }
            ImGui::TextWrapped("Hold Mouse 2 for native sword hover. Keyboard/gamepad attacks still work; release mouse buttons after closing F9 or returning to the game.");
            Divider(4);
            SectionHeader("FREE-LOOK ORBIT","Move the mouse to look around Ninja. Scroll the wheel to zoom.");
            if (checkbox("Third-person mouse orbit outside HL mode",&plan.mouseLook)) {
                patch::SavePlan(plan);
                if (!launcherMode && patch::runtime::Active()) patch::runtime::SetOrbitEnabled(plan.mouseLook);
            }
            float sensitivity=cfg.GetFloat("ethhhpatch","orbit_sensitivity",1);
            bool invert=cfg.GetBool("ethhhpatch","orbit_invert_y",false);
            bool changed=false;
            changed|=slider("Sensitivity",&sensitivity,.05f,5,"%.2fx");
            changed|=checkbox("Invert vertical look",&invert);
            if (changed) {
                patch::runtime::SetOrbitTuning(sensitivity,invert);
                cfg.SetFloat("ethhhpatch","orbit_sensitivity",sensitivity); cfg.SetBool("ethhhpatch","orbit_invert_y",invert); cfg.Save();
            }
            const auto orbit=patch::runtime::ReadOrbitTelemetry();
            Divider(4); Badge(orbit.following ? "FOLLOWING" : "NATIVE CAMERA",ui::Accent());
            ImGui::TextWrapped("%s",orbit.enabled ? patch::runtime::OrbitStateName(orbit.state) : "Mouse orbit disabled; native camera pose retained.");
            ImGui::TextWrapped("Mouse capture %s. Native distance %.2f / orbit %.2f units. Yaw %.1f / pitch %.1f degrees.",
                orbit.capture ? "active" : "released",orbit.nativeDistance,orbit.orbitDistance,orbit.yawDegrees,orbit.pitchDegrees);
            if (orbit.renderGuardFailures)
                ImGui::TextWrapped("Render ownership checks failed %u times; last mask 0x%02X (1 player, 2 class, 4 handle, 8 camera, 16 View).",
                    orbit.renderGuardFailures,orbit.renderGuardMask);
            float distance=orbit.orbitDistance;
            ImGui::BeginDisabled(!orbit.following);
            if (slider("Orbit distance",&distance,2.5f,15,"%.1f units")) patch::runtime::SetOrbitDistance(distance);
            ImGui::EndDisabled();
            ImGui::BeginDisabled(launcherMode || !patch::runtime::Active() || !plan.mouseLook);
            if (ui::GhostButton(compact ? "Resume orbit" : "Resume orbit after scripted camera")) patch::runtime::ResumeOrbit();
            ImGui::EndDisabled();
            ImGui::TextWrapped("Camera cuts and off-target pans yield to native framing with a temporary hold. Stable gameplay reacquires automatically. HL and movies always have priority.");
            ImGui::TextWrapped("Native movement follows the displayed orbit (%llu routed headings, %llu guarded refusals). Ctrl keeps native traversal priority, including a 30-tick release grace. No speed or player-pose override.",
                (unsigned long long)orbit.headingRouted,(unsigned long long)orbit.headingRefused);
            ImGui::TextWrapped("Cutscene detection is heuristic, not a verified game flag. If a cinematic looks wrong, disable orbit. Live cutscene validation is still required.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animation")) {
            SectionHeader("ENTITY POSE INTERPOLATION","Smooth rendered motion between completed simulation poses, including bone rotation.");
            bool enabled=cfg.GetBool("ethhhpatch","entity_interpolation",true);
            if (ui::ToggleSwitch("##entity_interpolation",&enabled)) {
                cfg.SetBool("ethhhpatch","entity_interpolation",enabled); cfg.Save(); patch::runtime::SetEntityInterpolationEnabled(enabled);
            }
            ImGui::SameLine(); ImGui::TextWrapped("Render-only entity animation blending");
            const auto animation=patch::runtime::ReadEntityTelemetry();
            Divider(4);
            ImGui::TextWrapped("%zu entities sampled / %zu continuous pose pairs",animation.sampled,animation.paired);
            ImGui::TextWrapped("%llu interpolated submissions / %llu native fallbacks",
                (unsigned long long)animation.interpolatedSubmissions,(unsigned long long)animation.nativeSubmissions);
            ImGui::TextWrapped("Last gameplay SDK sample %.3f ms / render %.3f ms; page queries %zu / %zu. Last list: %zu owners, %s.",
                animation.sampleMs,animation.renderMs,animation.sampleQueries,animation.renderQueries,
                animation.owners,animation.listComplete ? "complete" : "unavailable/incomplete");
            ImGui::TextWrapped("2 ms soft work budgets: %llu sampling stops / %llu render stops. Remaining entities keep native rendering.",
                (unsigned long long)animation.sampleBudgetStops,(unsigned long long)animation.renderBudgetStops);
            ImGui::TextWrapped("Requires the active high-refresh display patch. Native scripts, collision and animation still run only at the game's fixed tick rate.");
            BulletWrapped("Quaternion rotation blending preserves bone scale and reflection.");
            BulletWrapped("SDK-owned matrices are passed only to the renderer. Native bone and root memory is never overwritten.");
            BulletWrapped("New entities, teleports, changed owners, unsupported matrices and invalid bounds use native poses.");
            ImGui::TextWrapped("Current scope is the verified owner-aware mesh renderer. Custom deformers, particles and other unverified animation paths are not claimed as interpolated.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("PestControl")) {
            SectionHeader("PESTCONTROL", "Find exact Rat strats and remove them through the game's native lifecycle.");
            bool enabled = cfg.GetBool("ethhhpatch", "pestcontrol", false);
            if (ui::ToggleSwitch("##pestcontrol", &enabled)) {
                cfg.SetBool("ethhhpatch", "pestcontrol", enabled); cfg.Save();
                patch::pest::SetEnabled(enabled);
            }
            ImGui::SameLine(); ImGui::TextUnformatted(enabled ? "Enabled" : "Off");
            ImGui::TextWrapped("Searches four times per simulation second. Only the exact Rat class is targeted; Ninja, attachments and similarly named classes are untouched.");
            ImGui::TextWrapped("Deletion is permanent for the current spawn. Turning this off stops future requests; it does not restore removed Rats. No death effects or rewards are synthesized by the SDK.");
            const auto pests = patch::pest::ReadStatus();
            Divider(4);
            if (launcherMode) ImGui::TextWrapped("Saved for your next game session. No entities exist in the pre-game manager.");
            else if (pests.faulted) ImGui::TextWrapped("Native request guard failed. PestControl is blocked for this run.");
            else if (pests.enabled && !pests.verified) ImGui::TextWrapped("Unsupported executable or deletion lifecycle. Nothing will be deleted.");
            else ImGui::TextWrapped("%llu native requests / %llu verified removals / %zu pending teardown",
                (unsigned long long)pests.requested, (unsigned long long)pests.removed, pests.pending);
            ImGui::TextWrapped("Last search: %zu Rats / %zu strats, %s, %.3f ms. %llu budget stops / %llu identity refusals.",
                pests.detected, pests.visited, pests.listComplete ? "complete" : "not sampled/incomplete", pests.searchMs,
                (unsigned long long)pests.budgetStops, (unsigned long long)pests.refused);
            ImGui::TextWrapped("Independent of Auto+, HL and display/animation patches. Native teardown occurs on a following fixed simulation tick; no manual freeing or list unlinking.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Diagnostics")) {
            SectionHeader("LIVE ENGINE OBSERVATIONS","Actual game presents and active simulation ticks. Movies are counted separately.");
            if (!status.sampled) ImGui::TextWrapped("Waiting for fresh game timing samples.");
            else {
                ImGui::TextWrapped("Game %.2f presents/s / %.2f active ticks/s / simulation %.5f seconds per wall second.",
                    status.gamePresentsPerSecond,status.activeTicksPerSecond,status.simulationSecondsPerSecond);
                ImGui::TextWrapped("Backbuffer %ux%u / viewport %ux%u / %s",status.width,status.height,status.viewportWidth,status.viewportHeight,status.windowed ? "windowed" : "fullscreen");
            }
            ImGui::TextWrapped("Cadence: %llu zero-tick frames / %llu repeated presents / %llu catch-up frames",
                (unsigned long long)status.cadence.zeroTickFrames,(unsigned long long)status.cadence.repeatedFramePresents,(unsigned long long)status.cadence.catchupFrames);
            ImGui::TextWrapped("Reset: %llu succeeded / %llu failed / HRESULT 0x%08X",
                (unsigned long long)status.resetSuccesses,(unsigned long long)status.resetFailures,(unsigned)status.lastResetHresult);
            ImGui::TextWrapped("Movie presents %llu / failed presents %llu",(unsigned long long)status.moviePresents,(unsigned long long)status.failedPresents);
            if (status.projectionValid) ImGui::TextWrapped("World projection: aspect %.6f / vertical FOV %.2f / horizontal FOV %.2f degrees",status.projection.aspect,status.projection.verticalFovDegrees,status.projection.horizontalFovDegrees);
            ImGui::TextWrapped("Hooks %s. Exact Reset verification and rollback gate activation. Extra renders never call a simulation or animation update.",patch::runtime::HooksReady() ? "ready" : "inactive");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (!message.empty()) ImGui::TextWrapped("%s",message.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

} // namespace

// ---------------------------------------------------------------------------
//  Public helpers (declared in ui_pages.h)
// ---------------------------------------------------------------------------
void DrawStatusStrip()
{
    ImGui::PushFont(FontSmall());
    ImGui::TextDisabled("EthhhSDK %s", kVersion);
    std::string right = Format("%zu mods selected", mods::ModManager::Get().CountEnabled());
    float x = ImGui::GetWindowWidth() - ImGui::CalcTextSize(right.c_str()).x - 10.0f * ui::Scale();
    if (x > ImGui::GetCursorPosX() + 260.0f * ui::Scale()) {
        ImGui::SameLine(x); ImGui::TextDisabled("%s", right.c_str());
    }
    ImGui::PopFont();
}

// ---------------------------------------------------------------------------
Page CurrentPage() { return g_page; }
void SetCurrentPage(Page page) { g_page = page; }

void DrawSidebar(float width, bool launcherMode, bool* startGame, bool* exitApp)
{
    auto& cfg = Config::Get();
    if (!g_pageInitialized) {
        g_pageInitialized = true;
        int last = cfg.GetInt("launcher", "last_page", 0);
        if (!launcherMode)
            g_page = Page::Interfaces;
        else if (last >= 0 && last < (int)Page::Count)
            g_page = (Page)last;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16*ui::Scale(),20*ui::Scale()));
    ImGui::BeginChild("##sidebar", ImVec2(width, 0), ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
    const float s=ui::Scale(),bottomH=(launcherMode ? 150.0f : 74.0f)*s;
    ImGui::BeginChild("##destinations",ImVec2(0,-bottomH),false);

    struct Item { Page page; const char* label; const char* hint; };
    Item items[] = {
        { Page::Mods, "Mod library", "01" },
        { Page::Interfaces, "Live interfaces", "02" },
        { Page::Patch, "EthhhPatch", "03" },
        { Page::Inspector, "Inspector", "04" },
        { Page::Console, "Console", "05" },
        { Page::Settings, "Preferences", "06" },
        { Page::About, "About", "07" },
    };
    const Glass::Icon itemIcons[]={Glass::Icon::Cube,Glass::Icon::LayerGroup,Glass::Icon::Gamepad,Glass::Icon::Crosshairs,
        Glass::Icon::Terminal,Glass::Icon::Palette,Glass::Icon::InfoCircle};
    const char* shortLabels[]={"Mods","Live UI","Patch","Inspect","Console","Theme","About"};
    Glass::Icon icons[7]; const char* labels[7]; Page pages[7]; int count=0,current=-1;
    for (int index=0;index<7;++index) {
        const auto& it=items[index];
        if (launcherMode && (it.page == Page::Interfaces || it.page == Page::Inspector))
            continue;
        icons[count]=itemIcons[index]; labels[count]=width<215*ui::Scale() ? shortLabels[index] : it.label;
        pages[count]=it.page; if(g_page==it.page) current=count; ++count;
    }
    const float rowH=42*s;const bool iconOnly=width<140*s;
    const ImVec2 origin=ImGui::GetCursorScreenPos();auto* dl=ImGui::GetWindowDrawList();
    const ImGuiID selectionId=ImGui::GetID("##selection");
    auto& selection=Glass::g->springs().Get(selectionId,200,Glass::SpringStyle::CriticalFast,float(std::max(current,0)));
    selection.target=float(std::max(current,0));selection.Tick(ImGui::GetIO().DeltaTime);
    const float navWidth=ImGui::GetContentRegionAvail().x;
    if(current>=0) dl->AddRectFilled(ImVec2(origin.x,origin.y+rowH*selection.x),
        ImVec2(origin.x+navWidth,origin.y+rowH*(selection.x+1)-4*s),ui::AccentDim(.18f),12*s);
    int chosen=current;
    for(int i=0;i<count;++i) {
        ImGui::PushID(i);const ImVec2 p=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("##destination",ImVec2(navWidth,rowH-4*s),ImGuiButtonFlags_EnableNav)) {chosen=i;PlayUiSound(Sound::Click);}
        if(ImGui::IsItemHovered() && i!=current) dl->AddRectFilled(p,ImVec2(p.x+navWidth,p.y+rowH-4*s),ui::WithAlpha(ThemeInk(),.05f),12*s);
        const ImU32 ink=i==current ? ui::Accent() : ThemeSoftInk();
        Glass::DrawIcon(dl,icons[i],ImVec2(p.x+(iconOnly ? navWidth*.5f : 19*s),p.y+(rowH-4*s)*.5f),17,ink);
        if(!iconOnly) dl->AddText(ImVec2(p.x+38*s,p.y+(rowH-4*s-ImGui::GetFontSize())*.5f),i==current ? ThemeInk() : ThemeSoftInk(),labels[i]);
        else if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",labels[i]);
        ImGui::Dummy(ImVec2(0,0));ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+(i+1)*rowH));
        ImGui::PopID();
    }
    if(chosen>=0 && chosen<count && chosen!=current) {
        g_page=pages[chosen]; cfg.SetInt("launcher","last_page",int(g_page));
    }

    ImGui::EndChild();
    ImGui::SetCursorPosY(ImGui::GetWindowHeight()-bottomH);

    if (launcherMode) {
        if (ui::AccentButton("Start game", ImVec2(-FLT_MIN, 42 * ui::Scale()))) {
            if (startGame) *startGame = true;
        }
        if (ui::GhostButton("Exit", ImVec2(-FLT_MIN, 32 * ui::Scale()))) {
            if (exitApp) *exitApp = true;
        }
        ImGui::PushFont(FontSmall());
        if(!iconOnly) ImGui::TextWrapped("Enter to play");
        ImGui::PopFont();
    } else {
        ImGui::PushFont(FontSmall());
        ImGui::TextWrapped("%s",input::KeyName(overlay::ToggleKey()));
        if(!iconOnly) ImGui::TextWrapped("Back to your game");
        ImGui::PopFont();
    }

    ImGui::EndChild();
}

void DrawContent(const ImVec2& avail, bool launcherMode, Page page)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(26*ui::Scale(),22*ui::Scale()));
    ImGui::BeginChild("##content", avail, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    switch (page) {
    case Page::Mods:       DrawModsPage(launcherMode); break;
    case Page::Interfaces: if (launcherMode) DrawModsPage(true); else DrawInterfacesPage(); break;
    case Page::Inspector:  DrawInspectorPage(launcherMode); break;
    case Page::Settings:   DrawSettingsPage(); break;
    case Page::Console:    DrawConsolePage(); break;
    case Page::About:      DrawAboutPage(); break;
    case Page::Patch:      DrawPatchPage(launcherMode); break;
    default: break;
    }
    ImGui::EndChild();
}

void DrawManagerLayout(bool launcherMode, bool* startGame, bool* exitApp)
{
    const float s=ui::Scale();
    const ImVec2 avail=ImGui::GetContentRegionAvail();
    const float width=avail.x<760*s ? 86*s : std::min(210*s,avail.x*.24f);
    ImGui::BeginChild("##workspace",ImVec2(0,0),false,ImGuiWindowFlags_NoScrollbar);
    DrawSidebar(width,launcherMode,startGame,exitApp);
    ImGui::SameLine(0,0);
    DrawContent(ImGui::GetContentRegionAvail(),launcherMode,CurrentPage());
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
#include "ui_watermark.inc"

} // namespace eth::ui::pages
