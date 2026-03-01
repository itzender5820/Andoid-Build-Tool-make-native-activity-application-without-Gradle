#pragma once
#include "Types.h"
#include "Logger.h"
#include <string>
#include <unordered_map>

namespace sn {

// ── Config ────────────────────────────────────────────────────────────────────
// Key-value configuration store. Values are set at startup from defaults;
// runtime overrides can be pushed from the Java side via a JNI bridge or
// persisted to internal storage (path = context.getFilesDir()).
class Config {
public:
    explicit Config(const std::string& storagePath, const Logger& log);

    // Load from persisted INI-style file; returns false if not found (ok).
    bool load();
    // Persist current values; returns false on I/O error.
    bool save() const;

    // Typed accessors — return default if key missing
    std::string getString(const std::string& key, const std::string& def = "") const;
    int         getInt   (const std::string& key, int   def = 0)              const;
    float       getFloat (const std::string& key, float def = 0.f)            const;
    bool        getBool  (const std::string& key, bool  def = false)          const;

    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, int    value);
    void set(const std::string& key, float  value);
    void set(const std::string& key, bool   value);

    size_t size() const { return map_.size(); }

private:
    std::string                              storagePath_;
    const Logger&                            log_;
    std::unordered_map<std::string,std::string> map_;

    static std::string trim(const std::string& s);
};

} // namespace sn
