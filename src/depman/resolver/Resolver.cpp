#include <abt/toolchain/DepManager.h>
#include <abt/toolchain/NDKBridge.h>
#include <abt/engine/ProcessPool.h>
#include <abt/common/Logger.h>

#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <regex>

// libcurl — optional. Compile with -DABT_NO_CURL to disable network fetching.
#ifndef ABT_NO_CURL
#  include <curl/curl.h>
#endif

namespace abt {

// ── MavenCoord ────────────────────────────────────────────────────────────────
MavenCoord MavenCoord::parse(const std::string& spec) {
    MavenCoord c;
    // Local path check
    if (spec.empty()) return c;
    if (spec[0] == '/' || spec[0] == '.' ||
        spec.find(".aar") != std::string::npos) {
        c.isLocal    = true;
        c.localPath  = spec;
        return c;
    }
    // group:artifact:version
    std::istringstream ss(spec);
    std::string tok;
    int n = 0;
    while (std::getline(ss, tok, ':')) {
        switch (n++) {
            case 0: c.group    = tok; break;
            case 1: c.artifact = tok; break;
            case 2: c.version  = tok; break;
        }
    }
    return c;
}

std::string MavenCoord::cacheKey() const {
    if (isLocal) return localPath;
    return group + ":" + artifact + ":" + version;
}

std::string MavenCoord::aarFilename() const {
    return artifact + "-" + version + ".aar";
}

std::string MavenCoord::mavenUrl(const std::string& repoBase) const {
    // com.example → com/example
    std::string groupPath = group;
    for (char& c : groupPath) if (c == '.') c = '/';
    return repoBase + "/" + groupPath + "/" + artifact + "/"
         + version + "/" + aarFilename();
}

// ── Fetcher ───────────────────────────────────────────────────────────────────
const std::string Resolver::kDefaultRepo = "https://repo1.maven.org/maven2";

Fetcher::Fetcher(const std::string& cacheDir) : cacheDir_(cacheDir) {
    system(("mkdir -p " + cacheDir_).c_str());
}

size_t Fetcher::writeCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::ofstream*>(userdata);
    f->write(static_cast<char*>(ptr), static_cast<std::streamsize>(size * nmemb));
    return size * nmemb;
}

static std::string urlToFilename(const std::string& url) {
    // hash the url to get a unique filename
    size_t h = std::hash<std::string>{}(url);
    std::ostringstream ss;
    ss << std::hex << h;
    // preserve the original filename
    size_t slash = url.rfind('/');
    std::string base = (slash != std::string::npos) ? url.substr(slash + 1) : url;
    return ss.str() + "_" + base;
}

bool Fetcher::isCached(const std::string& url) const {
    std::string local = cacheDir_ + "/" + urlToFilename(url);
    struct stat st;
    return stat(local.c_str(), &st) == 0 && st.st_size > 0;
}

Result Fetcher::fetch(const std::string& url, std::string& outPath) {
    outPath = cacheDir_ + "/" + urlToFilename(url);

    if (isCached(url)) {
        ABT_INFO("Dep cache hit: %s", url.c_str());
        return Result::success();
    }

    ABT_INFO("Fetching: %s", url.c_str());

#ifdef ABT_NO_CURL
    return Result::fail(AbtError::FETCH_FAILED,
        "libcurl not available. Install with: pkg install libcurl");
#else
    CURL* curl = curl_easy_init();
    if (!curl)
        return Result::fail(AbtError::FETCH_FAILED, "curl_easy_init failed");

    std::ofstream f(outPath, std::ios::binary);
    if (!f.is_open()) {
        curl_easy_cleanup(curl);
        return Result::fail(AbtError::IO_ERROR, "Cannot write to " + outPath);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "abt/1.0");

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    f.close();

    if (res != CURLE_OK || httpCode != 200) {
        remove(outPath.c_str());
        return Result::fail(AbtError::FETCH_FAILED,
            std::string("HTTP ") + std::to_string(httpCode)
            + ": " + curl_easy_strerror(res));
    }

    return Result::success();
#endif
}

// ── Resolver ──────────────────────────────────────────────────────────────────
Resolver::Resolver(const std::string& cacheDir, const std::string& extractDir)
    : fetcher_(cacheDir), extractDir_(extractDir) {
    system(("mkdir -p " + extractDir_).c_str());
}

Result Resolver::extractAar(
    const std::string& aarPath,
    TargetArch arch,
    const std::string& outDir,
    ResolvedDep& dep)
{
    // .aar is a zip; extract jni/<abi>/*.so and prefab/modules/*/include
    std::string abi = NDKBridge::abiString(arch);
    system(("mkdir -p " + outDir).c_str());

    // Use unzip to list and extract relevant entries
    auto r = spawnProcess({"unzip", "-o", aarPath, "jni/" + abi + "/*.so",
                           "prefab/modules/*/include/*",
                           "-d", outDir});
    // unzip returns 11 if some files weren't found — that's OK
    if (r.exitCode != 0 && r.exitCode != 11) {
        ABT_WARN("unzip warning (exit %d): %s", r.exitCode, r.stderrStr.c_str());
    }

    // Set paths for the resolved dep
    std::string soDir = outDir + "/jni/" + abi;
    dep.soPath = soDir;

    // Collect include dirs
    std::string prefabDir = outDir + "/prefab/modules";
    DIR* d = opendir(prefabDir.c_str());
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d))) {
            if (ent->d_name[0] == '.') continue;
            std::string incDir = prefabDir + "/" + ent->d_name + "/include";
            struct stat st;
            if (stat(incDir.c_str(), &st) == 0)
                dep.includeDirs.push_back(incDir);
        }
        closedir(d);
    }

    return Result::success();
}

Result Resolver::resolve(const std::string& spec, TargetArch arch, ResolvedDep& out) {
    MavenCoord coord = MavenCoord::parse(spec);

    out.name    = coord.isLocal ? spec : coord.artifact;
    out.version = coord.version;

    if (coord.isLocal) {
        // Local .aar file
        std::string extractTo = extractDir_ + "/" + std::to_string(std::hash<std::string>{}(spec)) + "_local";
        return extractAar(coord.localPath, arch, extractTo, out);
    }

    // Remote: try Maven Central, then Google Maven
    std::vector<std::string> repos = {
        kDefaultRepo,
        "https://maven.google.com"
    };

    std::string aarPath;
    for (const auto& repo : repos) {
        std::string url = coord.mavenUrl(repo);
        auto r = fetcher_.fetch(url, aarPath);
        if (r.ok()) break;
        aarPath.clear();
    }

    if (aarPath.empty())
        return Result::fail(AbtError::FETCH_FAILED,
            "Could not fetch: " + spec);

    std::string extractTo = extractDir_ + "/" + coord.group + "_" + coord.artifact
        + "_" + coord.version;
    return extractAar(aarPath, arch, extractTo, out);
}

} // namespace abt
