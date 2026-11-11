#include "mods.h"
#include "lua_host.h"
#include "clr_host.h"
#include "util.h"
#include "log.h"
#include "game.h"

#include <shellapi.h>
#include <set>

namespace eth::mods {

namespace {

std::string SanitizeId(const std::string& raw)
{
    std::string s;
    for (char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')
            s.push_back(c);
        else if (c == ' ')
            s.push_back('_');
    }
    return s.empty() ? "mod" : s;
}

} // namespace

ModManager& ModManager::Get()
{
    static ModManager mgr;
    return mgr;
}

void ModManager::Scan()
{
    std::string dir = GetModsDir();
    CreateDirectories(dir);

    std::vector<ModInfo> previous = std::move(mods_);
    mods_.clear();

    for (const auto& folder : ListDirectories(dir)) {
        std::string modDir = JoinPath(dir, folder);
        std::string manifest = JoinPath(modDir, "mod.json");
        if (!FileExists(manifest))
            continue;

        ModInfo mod;
        mod.id = SanitizeId(folder);
        mod.name = folder;
        mod.dir = modDir;
        mod.type = "lua";
        ApplyManifest(mod, manifest);
        if (mod.id.empty())
            mod.id = SanitizeId(folder);

        // preserve runtime state if this mod was loaded before a rescan
        for (const auto& old : previous) {
            if (old.id == mod.id) {
                mod.loaded = old.loaded;
                mod.errored = old.errored;
                mod.status = old.status;
                break;
            }
        }
        mods_.push_back(mod);
    }

    std::sort(mods_.begin(), mods_.end(), [](const ModInfo& a, const ModInfo& b) {
        if (a.name != b.name) return ToLower(a.name) < ToLower(b.name);
        return a.id < b.id;
    });

    scanned_ = true;
    ETH_INFO("mods: scanned %zu mod(s) in %s", mods_.size(), dir.c_str());
}

void ModManager::ApplyManifest(ModInfo& mod, const std::string& manifestPath)
{
    std::string text;
    if (!ReadFileToString(manifestPath, text)) {
        mod.status = "cannot read mod.json";
        mod.errored = true;
        return;
    }

    Json json;
    std::string err;
    if (!json.Parse(text, &err)) {
        mod.status = "mod.json: " + err;
        mod.errored = true;
        ETH_ERROR("mods: %s: %s", manifestPath.c_str(), err.c_str());
        return;
    }

    std::string id = json.GetString("id");
    if (!id.empty()) mod.id = SanitizeId(id);
    mod.name        = json.GetString("name", mod.name);
    mod.version     = json.GetString("version", "1.0.0");
    mod.author      = json.GetString("author", "unknown");
    mod.description = json.GetString("description");
    mod.type        = ToLower(json.GetString("type"));
    mod.entry       = json.GetString("entry");
    mod.apiLevel    = (int)json.GetNumber("api", 1);

    if (mod.type.empty()) {
        if (!mod.entry.empty()) {
            std::string ext = GetFileExt(mod.entry);
            mod.type = ext == "dll" ? "csharp" : "lua";
        } else {
            mod.type = FileExists(JoinPath(mod.dir, "mod.lua")) ? "lua" : "csharp";
        }
    }
    if (mod.type != "lua" && mod.type != "csharp")
        mod.type = "lua";

    if (mod.entry.empty())
        mod.entry = mod.IsCSharp() ? "Mod.dll" : "mod.lua";

    std::string entryPath = JoinPath(mod.dir, mod.entry);
    if (!FileExists(entryPath)) {
        mod.status = "missing entry: " + mod.entry;
        mod.errored = true;
    }
    // Loaders consume an absolute path, not the manifest-relative filename.
    mod.entry = entryPath;

    bool manifestEnabled = json.GetBool("enabled", true);
    std::string cfg = enabledConfig_.GetString("enabled", mod.id, "");
    mod.enabled = cfg.empty() ? manifestEnabled : (cfg == "1" || ToLower(cfg) == "true");
    if (mod.status.empty())
        mod.status = mod.enabled ? "ready" : "disabled";
}

void ModManager::LoadConfig()
{
    CreateDirectories(GetConfigDir());
    enabledConfig_.Load(JoinPath(GetConfigDir(), "mods.ini"));
    settingsConfig_.Load(JoinPath(GetConfigDir(), "mod_settings.ini"));
}

void ModManager::SaveConfig()
{
    for (const auto& m : mods_)
        enabledConfig_.SetInt("enabled", m.id, m.enabled ? 1 : 0);
    enabledConfig_.Save();
}

void ModManager::SetLauncherMode(bool on)
{
    launcherMode_ = on;
}

void ModManager::ApplyDefaultsIfNeeded()
{
}

ModInfo* ModManager::Find(const std::string& id)
{
    for (auto& m : mods_)
        if (m.id == id) return &m;
    return nullptr;
}

bool ModManager::IsEnabled(const std::string& id) const
{
    for (const auto& m : mods_)
        if (m.id == id) return m.enabled;
    return false;
}

void ModManager::SetEnabled(const std::string& id, bool enabled)
{
    ModInfo* m = Find(id);
    if (!m || m->enabled == enabled) return;
    m->enabled = enabled;

    if (launcherMode_) {
        m->status = enabled ? "ready" : "disabled";
        SaveConfig();
        ETH_INFO("mods: %s %s (launcher)", id.c_str(), enabled ? "enabled" : "disabled");
        return;
    }

    if (enabled) {
        if (m->IsLua()) {
            if (m->errored && m->status.rfind("missing", 0) == 0) {
                // still offer a retry; the file may have been added
            }
            lua::LoadMod(*m);
        } else {
            m->status = clr::Running() ? "restart required (C#)" : "managed host unavailable";
        }
    } else {
        if (m->IsLua())
            lua::UnloadMod(id);
        if (m->IsCSharp())
            m->status = "disabled (restart to unload C#)";
        else if (m->status == "loaded")
            m->status = "disabled";
    }

    SaveConfig();
    ETH_INFO("mods: %s %s", id.c_str(), enabled ? "enabled" : "disabled");
}

void ModManager::InitEnabledMods()
{
    launcherMode_ = false;
    if (!scanned_)
        Scan();
    LoadConfig();

    // Re-apply the enabled state loaded from mods.ini.
    for (auto& m : mods_) {
        std::string cfg = enabledConfig_.GetString("enabled", m.id, "");
        if (!cfg.empty())
            m.enabled = (cfg == "1" || ToLower(cfg) == "true");
        m.loaded = false;
        if (!m.errored)
            m.status = m.enabled ? "pending" : "disabled";
    }

    std::vector<ModInfo> csharp;
    for (auto& m : mods_) {
        if (!m.enabled) continue;
        if (m.errored) continue;
        if (!FileExists(m.entry)) {
            m.status = "missing entry: " + m.entry;
            m.errored = true;
            continue;
        }
        if (m.IsLua()) {
            if (!lua::LoadMod(m)) {
                m.errored = true;
                if (m.status.empty()) m.status = "load failed";
            }
        } else {
            csharp.push_back(m);
        }
    }

    if (!csharp.empty()) {
        bool ok = clr::Start(csharp);
        for (auto& m : mods_) {
            if (!m.enabled || !m.IsCSharp() || m.errored) continue;
            if (!ok) {
                m.status = "managed host unavailable";
                m.errored = true;
            } else if (!m.errored) {
                m.loaded = true;
                m.status = "loaded (managed)";
            }
        }
    }

    ETH_INFO("mods: %zu enabled, %zu loaded (lua %zu, csharp %zu)",
             CountEnabled(), CountLoaded(), CountLuaLoaded(), CountCSharpLoaded());
}

void ModManager::Shutdown()
{
    lua::Shutdown();
    clr::Shutdown();
    for (auto& m : mods_) {
        m.loaded = false;
        if (m.enabled && m.status.find("loaded") != std::string::npos)
            m.status = "stopped";
    }
}

void ModManager::Tick(int tickIndex)
{
    float dt = game::TimeStep();
    if (!(dt > 0.0f)) dt = 0.0f;
    for (const auto& m : mods_) {
        if (m.enabled && m.loaded && m.IsLua())
            lua::TickMod(m.id, tickIndex, dt);
    }
    clr::Tick(tickIndex);
}

void ModManager::DrawGui()
{
    for (const auto& m : mods_) {
        if (!m.enabled || !m.loaded || !m.IsLua()) continue;
        ImGui::PushID(m.id.c_str());
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader(m.name.c_str())) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
            lua::DrawModGui(m.id);
            ImGui::PopStyleVar();
        }
        ImGui::PopID();
    }
    clr::DrawGui();
}

void ModManager::OnOverlayChanged(bool open)
{
    for (const auto& m : mods_) {
        if (m.enabled && m.loaded && m.IsLua())
            lua::OnOverlayChanged(m.id, open);
    }
    clr::OnOverlayChanged(open);
}

void ModManager::ReloadLua(const std::string& id)
{
    ModInfo* m = Find(id);
    if (!m || !m->IsLua()) return;
    if (!m->enabled) {
        lua::UnloadMod(id);
        return;
    }
    m->errored = false;
    m->status = "reloading";
    lua::Reload(id);
}

void ModManager::ReportError(const std::string& id, const std::string& message)
{
    ModInfo* m = Find(id);
    if (m) {
        m->errored = true;
        m->loaded = false;
        m->status = "error: " + message;
    }
    ETH_ERROR("mods[%s]: %s", id.c_str(), message.c_str());
}

void ModManager::SetStatus(const std::string& id, const std::string& status)
{
    if (ModInfo* m = Find(id))
        m->status = status;
}

size_t ModManager::CountEnabled() const
{
    size_t n = 0;
    for (const auto& m : mods_) if (m.enabled) ++n;
    return n;
}

size_t ModManager::CountLoaded() const
{
    size_t n = 0;
    for (const auto& m : mods_) if (m.enabled && m.loaded) ++n;
    return n;
}

size_t ModManager::CountLuaLoaded() const
{
    size_t n = 0;
    for (const auto& m : mods_) if (m.enabled && m.loaded && m.IsLua()) ++n;
    return n;
}

size_t ModManager::CountCSharpLoaded() const
{
    size_t n = 0;
    for (const auto& m : mods_) if (m.enabled && m.loaded && m.IsCSharp()) ++n;
    return n;
}

std::string ModManager::GetSetting(const std::string& id, const std::string& key, const std::string& def) const
{
    return settingsConfig_.GetString("mod." + id, key, def);
}

void ModManager::SetSetting(const std::string& id, const std::string& key, const std::string& value)
{
    settingsConfig_.SetString("mod." + id, key, value);
    SaveSettings();
}

float ModManager::GetSettingFloat(const std::string& id, const std::string& key, float def) const
{
    return settingsConfig_.GetFloat("mod." + id, key, def);
}

void ModManager::SetSettingFloat(const std::string& id, const std::string& key, float value)
{
    settingsConfig_.SetFloat("mod." + id, key, value);
    SaveSettings();
}

bool ModManager::GetSettingBool(const std::string& id, const std::string& key, bool def) const
{
    return settingsConfig_.GetBool("mod." + id, key, def);
}

void ModManager::SetSettingBool(const std::string& id, const std::string& key, bool value)
{
    settingsConfig_.SetBool("mod." + id, key, value);
    SaveSettings();
}

void ModManager::SaveSettings()
{
    settingsConfig_.Save();
}

void ModManager::OpenModsFolder()
{
    std::string dir = GetModsDir();
    CreateDirectories(dir);
    ShellExecuteA(nullptr, "open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace eth::mods
