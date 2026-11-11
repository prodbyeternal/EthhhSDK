#include "config.h"
#include "util.h"
#include "log.h"

namespace eth {

Config& Config::Get()
{
    static Config cfg;
    return cfg;
}

bool Config::Load(const std::string& path)
{
    path_ = path;
    data_.clear();

    std::string text;
    if (!ReadFileToString(path, text)) {
        ETH_INFO("config: %s not found, creating defaults", path.c_str());
        SetDefaults();
        Save();
        return false;
    }

    std::string section = "core";
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? text.size() + 1 : end + 1;
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            if (section.empty()) section = "core";
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        if (!key.empty()) data_[section][key] = val;
    }

    return true;
}

bool Config::Save() const
{
    std::string out;
    out += "; EthhhSDK configuration\r\n";
    out += "; Delete this file to restore defaults.\r\n\r\n";
    for (const auto& sec : data_) {
        out += "[" + sec.first + "]\r\n";
        for (const auto& kv : sec.second)
            out += kv.first + "=" + kv.second + "\r\n";
        out += "\r\n";
    }
    return WriteStringToFile(path_, out);
}

std::string Config::GetString(const std::string& section, const std::string& key, const std::string& def) const
{
    auto s = data_.find(section);
    if (s == data_.end()) return def;
    auto k = s->second.find(key);
    return k == s->second.end() ? def : k->second;
}

int Config::GetInt(const std::string& section, const std::string& key, int def) const
{
    std::string v = GetString(section, key, "");
    return v.empty() ? def : atoi(v.c_str());
}

float Config::GetFloat(const std::string& section, const std::string& key, float def) const
{
    std::string v = GetString(section, key, "");
    return v.empty() ? def : (float)atof(v.c_str());
}

bool Config::GetBool(const std::string& section, const std::string& key, bool def) const
{
    std::string v = ToLower(GetString(section, key, ""));
    if (v.empty()) return def;
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

void Config::SetString(const std::string& section, const std::string& key, const std::string& value)
{
    data_[section][key] = value;
}

void Config::SetInt(const std::string& section, const std::string& key, int value)
{
    data_[section][key] = std::to_string(value);
}

void Config::SetFloat(const std::string& section, const std::string& key, float value)
{
    char buf[64];
    sprintf_s(buf, "%.5g", value);
    data_[section][key] = buf;
}

void Config::SetBool(const std::string& section, const std::string& key, bool value)
{
    data_[section][key] = value ? "1" : "0";
}

void Config::SetDefaults()
{
    auto& c = *this;
    // core
    c.SetBool("core", "launcher", true);
    c.SetInt("core", "overlay_key", VK_F9);
    c.SetBool("core", "pause_on_overlay", true);
    c.SetBool("core", "watermark", true);
    c.SetInt("core", "watermark_pos", 0);
    c.SetInt("core", "watermark_detail", 1);
    c.SetFloat("core", "font_scale", 1.0f);
    c.SetString("core", "accent", "6C8CFF");
    c.SetBool("core", "verbose", false);
    c.SetBool("core", "safe_mode", false);
    c.SetBool("core", "debug_console", true);
    c.SetBool("core", "show_console_on_error", true);
    c.SetBool("core", "block_game_input", true);
    // launcher
    c.SetInt("launcher", "last_page", 0);
    // game helpers
    c.SetBool("game", "auto_player", true);
    c.SetInt("core", "config_version", 1);
}

void Config::ResetToDefaults()
{
    std::string p = path_;
    data_.clear();
    path_ = p;
    SetDefaults();
}

} // namespace eth
