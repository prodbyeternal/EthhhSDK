#pragma once
#include "precomp.h"

namespace eth {

// Simple ordered INI store (sections -> keys -> values). Comments are not
// preserved; the SDK owns these files.
class Config {
public:
    bool Load(const std::string& path);
    bool Save() const;

    std::string GetString(const std::string& section, const std::string& key, const std::string& def = "") const;
    int         GetInt(const std::string& section, const std::string& key, int def = 0) const;
    float       GetFloat(const std::string& section, const std::string& key, float def = 0.0f) const;
    bool        GetBool(const std::string& section, const std::string& key, bool def = false) const;

    void SetString(const std::string& section, const std::string& key, const std::string& value);
    void SetInt(const std::string& section, const std::string& key, int value);
    void SetFloat(const std::string& section, const std::string& key, float value);
    void SetBool(const std::string& section, const std::string& key, bool value);

    void SetDefaults();
    void ResetToDefaults();          // keep path, wipe values, re-apply defaults
    const std::string& Path() const { return path_; }

    static Config& Get();

private:
    std::string path_;
    std::map<std::string, std::map<std::string, std::string>> data_;
};

} // namespace eth
