#pragma once
#include "precomp.h"
#include "config.h"

namespace eth::mods {

struct ModInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string type;          // "lua" | "csharp"
    std::string dir;           // absolute mod directory
    std::string entry;         // absolute entry file (mod.lua / Mod.dll)
    std::string status;        // human readable load status
    bool enabled = false;
    bool loaded = false;
    bool errored = false;
    int  apiLevel = 1;

    bool IsLua() const    { return type == "lua"; }
    bool IsCSharp() const { return type == "csharp"; }
};

class ModManager {
public:
    static ModManager& Get();

    void Scan();                 // discover mods/*/mod.json
    void LoadConfig();
    void SaveConfig();
    void SetLauncherMode(bool on);   // true before the game starts (no code loading)

    std::vector<ModInfo>& List() { return mods_; }
    ModInfo* Find(const std::string& id);
    bool IsEnabled(const std::string& id) const;
    void SetEnabled(const std::string& id, bool enabled);

    void InitEnabledMods();
    void Shutdown();

    void Tick(int tickIndex);
    void DrawGui();
    void OnOverlayChanged(bool open);

    void ReloadLua(const std::string& id);
    void ReportError(const std::string& id, const std::string& message);
    void SetStatus(const std::string& id, const std::string& status);

    size_t CountEnabled() const;
    size_t CountLoaded() const;
    size_t CountLuaLoaded() const;
    size_t CountCSharpLoaded() const;

    // Per-mod persistent settings (config/mods.ini, section "mod.<id>").
    std::string GetSetting(const std::string& id, const std::string& key, const std::string& def = "") const;
    void SetSetting(const std::string& id, const std::string& key, const std::string& value);
    float GetSettingFloat(const std::string& id, const std::string& key, float def) const;
    void SetSettingFloat(const std::string& id, const std::string& key, float value);
    bool GetSettingBool(const std::string& id, const std::string& key, bool def) const;
    void SetSettingBool(const std::string& id, const std::string& key, bool value);
    void SaveSettings();

    void OpenModsFolder();

private:
    ModManager() = default;
    void ApplyManifest(ModInfo& mod, const std::string& manifestPath);
    void ApplyDefaultsIfNeeded();

    std::vector<ModInfo> mods_;
    Config  enabledConfig_;   // config/mods.ini, [enabled]
    Config  settingsConfig_;  // config/mod_settings.ini, [mod.<id>]
    bool    scanned_ = false;
    bool    launcherMode_ = true;
};

} // namespace eth::mods
