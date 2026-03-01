#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace abt {

// ── Error handling ──────────────────────────────────────────────────────────
enum class AbtError {
    OK = 0,
    NDK_NOT_FOUND,
    SDK_NOT_FOUND,
    PARSE_FAILED,
    COMPILE_FAILED,
    LINK_FAILED,
    PACKAGE_FAILED,
    SIGN_FAILED,
    FETCH_FAILED,
    IO_ERROR,
    CYCLE_DETECTED,
    UNKNOWN
};

struct Result {
    AbtError code  = AbtError::OK;
    std::string msg;

    bool ok() const { return code == AbtError::OK; }

    static Result success()                       { return {AbtError::OK, ""}; }
    static Result fail(AbtError e, std::string m) { return {e, std::move(m)}; }
};

// ── Build target ────────────────────────────────────────────────────────────
enum class TargetArch { ARM64, X86_64, ARM32, X86 };
enum class BuildType  { DEBUG, RELEASE };

// ── Host architecture (the machine running abt) ─────────────────────────────
// ARM64  → Termux on ARM64 device — use system aapt2 from PATH (native arm64 binary)
// X86_64 → Linux/WSL/x86_64 device — use aapt2 from SDK build-tools
// AUTO   → detect at runtime from /proc/self or uname
enum class HostArch { AUTO, ARM64, X86_64 };

struct BuildConfig {
    std::string             projectName;
    std::string             projectRoot;
    std::string             ndkRoot;
    std::string             sdkRoot;
    std::string             outputDir;          // default: <root>/build
    std::string             packageName;        // e.g. com.example.app
    int                     minSdkVersion = 24;
    int                     targetSdkVersion = 34;
    BuildType               buildType     = BuildType::DEBUG;
    std::vector<TargetArch> architectures = {TargetArch::ARM64};
    std::vector<std::string> cppSources;
    std::vector<std::string> includeDirs;
    std::vector<std::string> linkLibraries;
    std::vector<std::string> dependencies;     // Maven coords or local paths
    std::string             versionName = "1.0";
    int                     versionCode = 1;
    HostArch                hostArch    = HostArch::AUTO;  // set by --64 / --86 flags
};

// ── Task identifiers ────────────────────────────────────────────────────────
enum class TaskType {
    ENV_PROBE,
    HASH_CHECK,
    COMPILE_CPP,
    LINK_SO,
    COMPILE_RES,
    PACKAGE_APK,
    SIGN_APK,
    FETCH_DEP
};

using TaskId = uint64_t;

} // namespace abt
