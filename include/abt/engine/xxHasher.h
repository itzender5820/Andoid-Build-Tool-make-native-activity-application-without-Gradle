#pragma once

#include <abt/common/Types.h>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace abt {

// ── xxHash3-based file fingerprinting ────────────────────────────────────────
class xxHasher {
public:
    // Hash a file's contents. Returns 0 on error.
    static uint64_t hashFile(const std::string& path);

    // Hash a string buffer directly.
    static uint64_t hashBuffer(const void* data, size_t len);
};

// ── On-disk hash cache ────────────────────────────────────────────────────────
// Persisted to .abt/cache/hashes.bin between builds.
class HashCache {
public:
    explicit HashCache(const std::string& cacheDir);

    // Load from disk (call once at startup)
    Result load();
    // Persist to disk
    Result save() const;

    // Returns true if the file's current hash matches the cached value.
    // Side-effect: updates the in-memory cache entry.
    bool isUpToDate(const std::string& filePath);

    // Overload: filePath is read for hashing; cacheKey is what is stored/looked
    // up in the cache. Use when the same source file is compiled for multiple
    // architectures so each arch gets an independent staleness entry.
    bool isUpToDate(const std::string& filePath, const std::string& cacheKey);

    // Force-mark a file as up to date (after it is written by a task)
    void markClean(const std::string& filePath);

    size_t cacheSize() const { return cache_.size(); }

private:
    std::string cacheDir_;
    std::string cachePath_;
    std::unordered_map<std::string, uint64_t> cache_;
};

} // namespace abt
