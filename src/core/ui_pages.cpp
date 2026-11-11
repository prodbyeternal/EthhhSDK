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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 28, 35, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * s, 16.0f * s));
    // Content-sized cards: long descriptions and loader errors stay readable.
    if (ImGui::BeginChild("##card", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollbar)) {
        if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 100.0f * s);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushFont(FontBold());
            ImGui::TextWrapped("%s", m.name.c_str());
            ImGui::PopFont();
            ImGui::PushFont(FontSmall());
            Badge(TypeBadge(m), TypeBadgeColor(m));
            ImGui::SameLine();
            ImGui::TextDisabled("v%s", m.version.c_str());
            if (!m.author.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("/ %s", m.author.c_str());
            }
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", m.description.empty() ? "No description." : m.description.c_str());
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
            ImGui::TextDisabled(enabled ? "Enabled" : "Disabled");
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
    PageTitle("Your mods", launcherMode ? "Choose what loads when the game starts." :
              "Manage this session. C# mods first enabled in-game need a restart.");

    auto& mgr = mods::ModManager::Get();
    auto& list = mgr.List();

    const float s = ui::Scale();
    if (ImGui::BeginTable("##mod_toolbar", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, (launcherMode ? 216.0f : 132.0f) * s);
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##searchmods", "Search mods...", g_modFilter, sizeof(g_modFilter));
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
        return;
    }

    ImGui::BeginChild("##modlist", ImVec2(0, 0), false);
    std::string filter = ToLower(g_modFilter);
    size_t shown = 0;
    for (auto& m : list) {
        if (!filter.empty() && ToLower(m.name + " " + m.id + " " + m.description).find(filter) == std::string::npos)
            continue;
        ++shown;
        DrawModCard(m, launcherMode);
        ImGui::Dummy(ImVec2(0, 2 * s));
    }
    if (!shown) ImGui::TextDisabled("No mods match this search.");
    ImGui::EndChild();
}

void DrawInterfacesPage()
{
    PageTitle("Interfaces", "Live mod interfaces. F9 closes the overlay.");
    auto& mgr = mods::ModManager::Get();
    if (mgr.CountLoaded() == 0) {
        ImGui::TextDisabled("No mod interfaces are active.");
        ImGui::TextDisabled("Enable a Lua or C# mod on the Mods page, then restart the game.");
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

    SectionHeader("OVERLAY", "How the in-game menu behaves");
    bool watermark = cfg.GetBool("core", "watermark", true);
    if (ui::ToggleSwitch("##watermark", &watermark)) { cfg.SetBool("core", "watermark", watermark); changed = true; }
    ImGui::SameLine(); ImGui::TextUnformatted("Always-on EthhhSDK watermark");

    bool pause = cfg.GetBool("core", "pause_on_overlay", true);
    if (ui::ToggleSwitch("##pause", &pause)) { cfg.SetBool("core", "pause_on_overlay", pause); changed = true; }
    ImGui::SameLine(); ImGui::TextUnformatted("Pause game while the overlay is open");

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
        if (ImGui::Checkbox(flags[i].name, &on)) {
            game::ToggleDebugFlag(flags[i].bit, on);
            changed = true;
        }
        if (i % 3 != 2) ImGui::SameLine(0, 26 * ui::Scale());
        else ImGui::Dummy(ImVec2(0, 2 * ui::Scale()));
    }

    Divider();

    SectionHeader("PLAYER DETECTION", "Exact class Ninja is preferred; pin it before experimental HL activation.");
    int mode = (int)game::PlayerTracker::Get().Mode();
    const char* modes[] = { "Auto (nearest to camera)", "Manual", "Learning" };
    ImGui::SetNextItemWidth(240 * ui::Scale());
    if (ImGui::Combo("Mode", &mode, modes, 3)) {
        game::PlayerTracker::Get().SetMode((game::PlayerMode)mode);
    }
    ImGui::SameLine();
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
    ImGui::SameLine(); ImGui::TextUnformatted("Verbose debug logging");

    bool blockInput = cfg.GetBool("core", "block_game_input", true);
    if (ui::ToggleSwitch("##blockinput", &blockInput)) { cfg.SetBool("core", "block_game_input", blockInput); changed = true; }
    ImGui::SameLine(); ImGui::TextUnformatted("Swallow game window input while overlay is open");

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
    ImGui::Checkbox("Auto-scroll", &g_logAutoScroll);
    ImGui::SameLine();
    if (ui::GhostButton("Clear", ImVec2(80 * ui::Scale(), 0))) {
        LogClear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu lines", LogSnapshot().size());

    ImGui::BeginChild("##console", ImVec2(0, 0), true);
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
    const char* modes[] = { "Auto", "Manual", "Learning" };
    ImGui::SetNextItemWidth(160 * ui::Scale());
    if (ImGui::Combo("##playermode", &mode, modes, 3))
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
    if (pt.Mode() == game::PlayerMode::Manual && pt.Valid() && game::ReadStrat(pt.Addr(), selected)) {
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
                    ImGui::BulletText("%s %08X follows this owner handle", c.strat.className, (unsigned)c.strat.addr);
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
    if (g_probeAddr && GetTickCount64()-g_probeTime < 30000 && pt.Mode() == game::PlayerMode::Manual &&
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
    ImVec2 pos = ImGui::GetCursorScreenPos();
    DrawLogo(pos, 54.0f * ui::Scale());
    ImGui::Dummy(ImVec2(66.0f * ui::Scale(), 54.0f * ui::Scale()));
    ImGui::SameLine();
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
    ImGui::TextDisabled("Game directory");
    ImGui::SameLine(180 * ui::Scale());
    ImGui::TextUnformatted(GetGameDir().c_str());
    ImGui::TextDisabled("Mods directory");
    ImGui::SameLine(180 * ui::Scale());
    ImGui::TextUnformatted(GetModsDir().c_str());
    ImGui::TextDisabled("Injection");
    ImGui::SameLine(180 * ui::Scale());
    ImGui::TextUnformatted("dinput8.dll proxy -> EthhhSDK.dll entry gate");
    ImGui::TextDisabled("Overlay key");
    ImGui::SameLine(180 * ui::Scale());
    ImGui::TextUnformatted(input::KeyName(overlay::ToggleKey()));

    Divider();
    SectionHeader("CREDITS", nullptr);
    ImGui::BulletText("Dear ImGui by Omar Cornut and contributors (MIT).");
    ImGui::BulletText("MinHook by Tsuda Kageyu (BSD 2-Clause).");
    ImGui::BulletText("Lua 5.4 by Lua.org, PUC-Rio (MIT).");
    ImGui::BulletText("Half-Life movement reference: Valve HLSDK (half-life-steam_legacy).");
    ImGui::TextDisabled("Valve HLSDK agreement text is pending; this is a local experimental build.");

    Divider();
    ImGui::TextDisabled("This SDK is a fan project and is not affiliated with Namco / Argonaut Games.");
    ImGui::TextDisabled("I-Ninja (c) 2003 Namco Hometek Inc. / Argonaut Games plc.");

    ImGui::EndChild();
}

void DrawPatchPage(bool launcherMode)
{
    PageTitle("EthhhPatch", "Experimental resolution, widescreen and high-refresh patch with fixed-60-Hz simulation.");
#ifdef ETH_TESTING
    if (patch::HasPreviewStatus()) ImGui::TextWrapped("OFF-SCREEN FIXTURE: synthetic readings below, not live game FPS or an unlock.");
#endif
    ImGui::BeginChild("##patch", ImVec2(0, 0), false);
    ImGui::PushTextWrapPos(0);
    ImGui::TextWrapped("Experimental display application is opt-in. Saving a plan alone does not apply it; application uses exact mode checks, the game's Reset lifecycle, fixed-60 simulation and automatic rollback.");
    static patch::DisplayPlan plan = patch::LoadPlan();
    static std::string message;
    const auto monitor = patch::ReadMonitor(launcherMode ? GetActiveWindow() : (HWND)game::GameHwnd());
    SectionHeader("DISPLAY PLAN", "Presets are future targets, not active display modes.");
    if (ImGui::Button("1080p")) plan.width = 1920, plan.height = 1080;
    ImGui::SameLine();
    if (ImGui::Button("4K")) plan.width = 3840, plan.height = 2160;
    ImGui::SameLine();
    if (ImGui::Button("Monitor size") && monitor.width && monitor.height) plan.width = monitor.width, plan.height = monitor.height;
    ImGui::SetNextItemWidth(180 * ui::Scale()); ImGui::InputInt("Width", &plan.width);
    ImGui::SetNextItemWidth(180 * ui::Scale()); ImGui::InputInt("Height", &plan.height);
    ImGui::SetNextItemWidth(180 * ui::Scale()); ImGui::InputInt("Render Hz (0 = monitor)", &plan.renderHz);
    ImGui::Checkbox("Windowed target", &plan.windowed);
    ImGui::Checkbox("Per-render mouse look outside HL mode", &plan.mouseLook);
    if (monitor.hz) ImGui::TextDisabled("OS monitor: %dx%d @ %d Hz (not measured game FPS)", monitor.width, monitor.height, monitor.hz);
    else ImGui::TextDisabled("Monitor refresh unknown; no assumed 60-Hz fallback.");
    const bool valid = patch::ValidPlan(plan);
    if (valid) {
        const int target = patch::TargetRenderHz(plan, monitor.hz);
        ImGui::Text("Target aspect %.4f  |  %s render %s  |  simulation fixed 60 Hz", patch::Aspect(plan.width, plan.height),
            plan.windowed ? "windowed" : "fullscreen", target ? Format("%d Hz", target).c_str() : "unknown");
        ImGui::TextDisabled("World FOV preview: vertical 60 deg -> horizontal %.2f deg; CPU culling repair still required",
            patch::HorizontalFov(60, patch::Aspect(plan.width, plan.height)));
    } else ImGui::TextWrapped("Invalid plan: width 640..7680, height 480..4320, render Hz 0 or 30..360.");
    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Save future plan (not applied)")) message = patch::SavePlan(plan) ? "Plan saved. No runtime display/timing change." : "Plan could not be saved.";
    ImGui::EndDisabled();
    if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
    const auto apply = patch::runtime::ReadApplyStatus();
    const char* applyName = "Idle";
    switch (apply.state) {
    case patch::runtime::ApplyState::Applying: applyName = "Applying / waiting for Reset"; break;
    case patch::runtime::ApplyState::Active: applyName = "Active"; break;
    case patch::runtime::ApplyState::RollingBack: applyName = "Rolling back"; break;
    case patch::runtime::ApplyState::RolledBack: applyName = "Rolled back"; break;
    case patch::runtime::ApplyState::Failed: applyName = "Failed"; break;
    default: break;
    }
    ImGui::TextDisabled("Runtime implementation: compatibility hooks %s | transaction %s",
        patch::runtime::HooksReady() ? "active/ready" : "lazy (installed only when applying)", applyName);
    if (!apply.detail.empty()) ImGui::TextWrapped("%s", apply.detail.c_str());
    static bool acknowledgeExperimental = false;
    ImGui::Checkbox("I understand this display patch is experimental", &acknowledgeExperimental);
    const bool pending = apply.state == patch::runtime::ApplyState::Applying || apply.state == patch::runtime::ApplyState::RollingBack;
    ImGui::BeginDisabled(launcherMode || !valid || !acknowledgeExperimental || pending || apply.state == patch::runtime::ApplyState::Active);
    if (ImGui::Button("Apply experimental patch")) {
        std::string error;
        if (!patch::SavePlan(plan)) message = "Plan save failed; display was not changed.";
        else if (patch::runtime::RequestDisplayPlan(plan, monitor.hz, error)) message = "Display transaction requested; waiting for exact Reset verification.";
        else message = "Apply refused: " + error;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(launcherMode || apply.state != patch::runtime::ApplyState::Active);
    if (ImGui::Button("Restore previous display")) {
        std::string error;
        message = patch::runtime::RequestRestore(error) ? "Restore requested." : "Restore refused: " + error;
    }
    ImGui::EndDisabled();
    if (launcherMode) ImGui::TextDisabled("Start the game, then use F9 to apply or restore on the render thread.");
    Divider();
    SectionHeader("LIVE OBSERVATIONS", "Successful game Present calls, not ImGui FPS. Movies are counted separately.");
    const auto status = patch::ReadStatus();
    if (launcherMode || !status.sampled) ImGui::TextWrapped("Waiting for game presentation samples. This page never starts, resets or resizes the game.");
    else {
        ImGui::Text("Game presents %.2f/s  |  tick calls %.2f/s  |  active ticks %.2f/s",
            status.gamePresentsPerSecond, status.tickCallsPerSecond, status.activeTicksPerSecond);
        ImGui::Text("Simulation advance %.5f game seconds / wall second (pause and time-scale aware)", status.simulationSecondsPerSecond);
        ImGui::Text("Backbuffer %ux%u  |  viewport %ux%u  |  %s", status.width, status.height,
            status.viewportWidth, status.viewportHeight, status.windowed ? "windowed" : "fullscreen");
        ImGui::TextDisabled("PresentationInterval %08X  fullscreen refresh %u  engine aspect %.6f",
            status.presentInterval, status.fullscreenHz, status.engineAspect);
    }
    ImGui::TextDisabled("Failed presents %llu  |  movie presents %llu", (unsigned long long)status.failedPresents, (unsigned long long)status.moviePresents);
    ImGui::TextDisabled("Cadence: %llu zero-tick engine frames, %llu repeated-frame presents, %llu catch-up frames (since Reset)",
        (unsigned long long)status.cadence.zeroTickFrames, (unsigned long long)status.cadence.repeatedFramePresents,
        (unsigned long long)status.cadence.catchupFrames);
    ImGui::TextDisabled("Device Reset: %llu attempts, %llu succeeded, %llu failed; last HRESULT 0x%08X",
        (unsigned long long)status.resetAttempts, (unsigned long long)status.resetSuccesses,
        (unsigned long long)status.resetFailures, (unsigned)status.lastResetHresult);
    if (status.lastResetParametersValid)
        ImGui::TextDisabled("Last Reset request: %ux%u, %s, refresh %u, interval 0x%08X",
            status.resetWidth, status.resetHeight, status.resetWindowed ? "windowed" : "fullscreen",
            status.resetFullscreenHz, status.resetInterval);
    if (!status.sampled && status.observationAgeSeconds >= 0)
        ImGui::TextDisabled("Last timing sample is %.1f seconds old; stale rates are not shown as live FPS.", status.observationAgeSeconds);
    if (status.projectionValid && !launcherMode) {
        const auto& projection = status.projection;
        ImGui::Text("Submitted world projection: aspect %.6f  |  vertical FOV %.2f deg  |  horizontal %.2f deg",
            projection.aspect, projection.verticalFovDegrees, projection.horizontalFovDegrees);
        ImGui::TextDisabled("Near %.5f  far %.2f (float estimate)  |  %llu checked submissions",
            projection.nearPlane, projection.farPlane, (unsigned long long)status.projectionSamples);
        ImGui::TextDisabled("Matrix/source errors: aspect %.8f, vertical FOV %.6f deg (non-atomic reads)",
            status.projectionAspectError, status.projectionFovErrorDegrees);
        if (status.width && status.height && std::abs(projection.aspect - patch::Aspect(status.width, status.height)) > 0.01)
            ImGui::TextWrapped("Warning: world projection aspect differs from the backbuffer. This is diagnostic evidence, not an applied widescreen repair.");
    } else ImGui::TextDisabled("Waiting for a checked world-perspective submission (not HUD or ImGui matrices).");
    Divider();
    SectionHeader("SAFETY GATES", nullptr);
    ImGui::BulletText("No timer/limiter edits: App_Run currently waits for >= 1 tick before rendering.");
    ImGui::BulletText("No automatic native calls for Ninja root/model/collision refresh.");
    ImGui::BulletText("No display writes or forced device Reset; 1080p/4K are not claimed as working yet.");
    ImGui::BulletText("One CPU culler ignores aspect; changing only projection can clip edge objects.");
    ImGui::BulletText("A render-path HUD pulse adds 0.1 per call; more renders could accelerate it.");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

} // namespace

// ---------------------------------------------------------------------------
//  Public helpers (declared in ui_pages.h)
// ---------------------------------------------------------------------------
void DrawStatusStrip()
{
    ImGui::Separator();
    ImGui::PushFont(FontSmall());
    ImGui::TextDisabled("SDK %s  /  ImGui %s", kVersion, ImGui::GetVersion());
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

    ImGui::BeginChild("##sidebar", ImVec2(width, 0), false);

    ImGui::PushFont(FontSmall());
    ImGui::TextDisabled(launcherMode ? "BEFORE YOU PLAY" : "THIS SESSION");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 6 * ui::Scale()));

    struct Item { Page page; const char* label; const char* hint; };
    Item items[] = {
        { Page::Mods, "Mods", "" },
        { Page::Interfaces, "Mod interfaces", "" },
        { Page::Inspector, "Inspector", "" },
        { Page::Settings, "Settings", "" },
        { Page::Console, "Console", "" },
        { Page::About, "About", "" },
        { Page::Patch, "EthhhPatch", "" },
    };
    for (const auto& it : items) {
        if (launcherMode && (it.page == Page::Interfaces || it.page == Page::Inspector))
            continue;
        if (NavItem(it.label, it.label, g_page == it.page, it.hint[0] ? it.hint : nullptr)) {
            g_page = it.page;
            cfg.SetInt("launcher", "last_page", (int)it.page);
        }
    }

    // spacer to bottom
    float bottomH = launcherMode ? 150.0f * ui::Scale() : 70.0f * ui::Scale();
    float y = ImGui::GetWindowHeight() - bottomH;
    if (y > ImGui::GetCursorPosY())
        ImGui::SetCursorPosY(y);

    if (launcherMode) {
        if (ui::AccentButton("Start game", ImVec2(-FLT_MIN, 42 * ui::Scale()))) {
            if (startGame) *startGame = true;
        }
        ImGui::Dummy(ImVec2(0, 4 * ui::Scale()));
        if (ui::GhostButton("Exit", ImVec2(-FLT_MIN, 32 * ui::Scale()))) {
            if (exitApp) *exitApp = true;
        }
        ImGui::Dummy(ImVec2(0, 2 * ui::Scale()));
        ImGui::PushFont(FontSmall());
        ImGui::TextDisabled("Enter to start");
        ImGui::PopFont();
    } else {
        ImGui::Separator();
        ImGui::PushFont(FontSmall());
        ImGui::TextDisabled("%s closes the manager", input::KeyName(overlay::ToggleKey()));
        ImGui::TextDisabled("%.0f FPS  /  tick %u", ImGui::GetIO().Framerate, game::TickCount());
        ImGui::PopFont();
    }

    ImGui::EndChild();
}

void DrawContent(const ImVec2& avail, bool launcherMode, Page page)
{
    ImGui::BeginChild("##content", avail, false);
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

// ---------------------------------------------------------------------------
void DrawWatermark()
{
    auto& cfg = Config::Get();
    if (!cfg.GetBool("core", "watermark", true))
        return;

    ImGuiIO& io = ImGui::GetIO();
    int detail = cfg.GetInt("core", "watermark_detail", 1);
    int pos = cfg.GetInt("core", "watermark_pos", 0);
    float margin = 12.0f * ui::Scale();

    ImGui::SetNextWindowBgAlpha(0.80f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove;
    ImVec2 size(0, 0);
    ImGui::SetNextWindowPos(ImVec2(pos == 0 || pos == 2 ? margin : io.DisplaySize.x - margin,
                                   pos < 2 ? margin : io.DisplaySize.y - margin),
                            ImGuiCond_Always, ImVec2(pos == 0 || pos == 2 ? 0.0f : 1.0f,
                                                     pos < 2 ? 0.0f : 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.045f, 0.06f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.17f, 0.21f, 0.28f, 0.9f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * ui::Scale(), 7.0f * ui::Scale()));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f * ui::Scale());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::Begin("##ethhh_watermark", nullptr, flags);

    ImGui::PushFont(FontBold());
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::Accent()), "%s", kWatermark);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::PushFont(FontSmall());
    ImGui::TextDisabled("v%s", kVersion);

    if (detail >= 1) {
        ImGui::Text("FPS %.0f   tick %u   frame %u", io.Framerate, game::TickCount(), game::FrameCount());
        ImGui::TextDisabled("time %.2fs   scale %.2fx", game::GameTime(), game::TimeScale());
        const auto rates = patch::ReadStatus();
        if (rates.sampled) ImGui::TextDisabled("game %.1f presents/s   sim %.1f active ticks/s", rates.gamePresentsPerSecond, rates.activeTicksPerSecond);
    }
    if (detail >= 2) {
        game::CameraView cam;
        if (game::ReadCamera(cam)) {
            ImGui::TextDisabled("cam %.1f %.1f %.1f", cam.x, cam.y, cam.z);
            ImGui::TextDisabled("fov %.1f deg   strats %zu", cam.fov * 180.0f / 3.14159265358979f,
                                game::CountStrats());
        } else {
            ImGui::TextDisabled("cam: waiting for level");
        }
        uintptr_t player = game::PlayerTracker::Get().Addr();
        ImGui::TextDisabled("player %s   mods %zu/%zu",
                            player ? Format("%08X", (unsigned)player).c_str() : "auto",
                            mods::ModManager::Get().CountLoaded(),
                            mods::ModManager::Get().CountEnabled());
        ImGui::TextDisabled("hooks tick:%d d3d:%d wnd:%d  %s",
                            hooks::TickHooked(), hooks::D3DHooked(), hooks::WndProcHooked(),
                            overlay::IsVisible() ? "menu open" : "F9 menu");
    }
    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace eth::ui::pages
