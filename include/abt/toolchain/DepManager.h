#pragma once

#include <abt/common/Types.h>
#include <string>
#include <vector>

namespace abt {

// A resolved dependency — either from cache or freshly fetched
struct ResolvedDep {
    std::string name;
    std::string version;
    std::string soPath;           // Path to the extracted .so
    std::vector<std::string> includeDirs; // Extracted header dirs
};

// ── Maven coordinate parser ───────────────────────────────────────────────────
// Parses "com.example:lib:1.0.0" or local path "libs/foo.aar"
struct MavenCoord {
    std::string group;
    std::string artifact;
    std::string version;
    bool        isLocal = false;
    std::string localPath;

    static MavenCoord parse(const std::string& spec);
    std::string cacheKey() const;
    std::string aarFilename() const;
    std::string mavenUrl(const std::string& repoBase) const;
};

// ── HTTP fetcher (libcurl) ────────────────────────────────────────────────────
class Fetcher {
public:
    explicit Fetcher(const std::string& cacheDir);

    // Download url to cacheDir/<filename>. Returns local path.
    Result fetch(const std::string& url, std::string& outPath);

    // Check if already cached
    bool isCached(const std::string& url) const;

private:
    std::string cacheDir_;
    static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
};

// ── AAR resolver ─────────────────────────────────────────────────────────────
// Fetches .aar files, extracts only jni/<abi>/*.so and headers/
class Resolver {
public:
    Resolver(const std::string& cacheDir, const std::string& extractDir);

    // Resolve a dependency spec → ResolvedDep
    Result resolve(const std::string& spec, TargetArch arch, ResolvedDep& out);

private:
    Fetcher     fetcher_;
    std::string extractDir_;

    // Unzip an .aar and extract native libs + headers
    Result extractAar(
        const std::string& aarPath,
        TargetArch arch,
        const std::string& outDir,
        ResolvedDep& dep
    );

    static const std::string kDefaultRepo;
};

} // namespace abt
