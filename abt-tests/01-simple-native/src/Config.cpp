#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace sn {

Config::Config(const std::string& storagePath, const Logger& log)
    : storagePath_(storagePath), log_(log) {
    // Seed defaults
    map_["app.fps_limit"]      = "60";
    map_["app.log_verbose"]    = "false";
    map_["app.touch_slop_px"]  = "8";
}

std::string Config::trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool Config::load() {
    std::ifstream f(storagePath_ + "/config.ini");
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string k = trim(line.substr(0, pos));
        std::string v = trim(line.substr(pos + 1));
        if (!k.empty()) map_[k] = v;
    }
    log_.info("Config: loaded %zu keys from %s", map_.size(), storagePath_.c_str());
    return true;
}

bool Config::save() const {
    std::ofstream f(storagePath_ + "/config.ini");
    if (!f) { log_.error("Config: cannot write to %s", storagePath_.c_str()); return false; }
    for (auto& [k, v] : map_) f << k << " = " << v << "\n";
    return true;
}

std::string Config::getString(const std::string& key, const std::string& def) const {
    auto it = map_.find(key);
    return it != map_.end() ? it->second : def;
}
int Config::getInt(const std::string& key, int def) const {
    auto it = map_.find(key);
    if (it == map_.end()) return def;
    try { return std::stoi(it->second); } catch(...) { return def; }
}
float Config::getFloat(const std::string& key, float def) const {
    auto it = map_.find(key);
    if (it == map_.end()) return def;
    try { return std::stof(it->second); } catch(...) { return def; }
}
bool Config::getBool(const std::string& key, bool def) const {
    auto it = map_.find(key);
    if (it == map_.end()) return def;
    const auto& v = it->second;
    return v == "true" || v == "1" || v == "yes";
}
void Config::set(const std::string& key, const std::string& value) { map_[key] = value; }
void Config::set(const std::string& key, int   v) { map_[key] = std::to_string(v); }
void Config::set(const std::string& key, float v) { map_[key] = std::to_string(v); }
void Config::set(const std::string& key, bool  v) { map_[key] = v ? "true" : "false"; }

} // namespace sn
