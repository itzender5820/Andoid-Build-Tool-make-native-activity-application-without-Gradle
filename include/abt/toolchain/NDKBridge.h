#pragma once

#include <abt/common/Types.h>
#include <string>
#include <vector>

namespace abt {

// ── Resolves NDK paths and generates compiler/linker invocations ─────────────
class NDKBridge {
public:
    explicit NDKBridge(const BuildConfig& config);

    // Validate NDK installation — check compilers exist
    Result probe();

    // Full path to clang++ for the given arch
    std::string clangPath(TargetArch arch) const;   // clang++ for C++
    std::string clangCPath(TargetArch arch) const;  // clang   for C
    // Full path to ld.lld
    std::string lldPath() const;
    // Full path to llvm-strip
    std::string stripPath() const;

    // Generate compile flags for a .cpp → .o step
    std::vector<std::string> compileFlags(TargetArch arch) const;

    // Generate link flags for .o files → libmain.so
    std::vector<std::string> linkFlags(TargetArch arch) const;
    std::vector<std::string> linkLibFlags() const;  // -l flags, must come AFTER objects

    // Sysroot for the given arch
    std::string sysrootPath(TargetArch arch) const;

    // ABI string (e.g. "arm64-v8a")
    static std::string abiString(TargetArch arch);
    // Clang target triple (e.g. "aarch64-linux-android24")
    std::string targetTriple(TargetArch arch) const;

    // Parse DT_NEEDED entries from an ELF shared library
    static std::vector<std::string> elfNeededLibs(const std::string& soPath);

    // Given a built .so, return full paths to NDK runtime libs that must be
    // bundled in the APK (e.g. libc++_shared.so). Excludes system libs that
    // Android ships on-device.
    std::vector<std::string> runtimeLibsToBundle(
        const std::string& soPath, TargetArch arch) const;

    // Directory inside NDK that holds runtime .so files for an arch
    std::string runtimeLibDir(TargetArch arch) const;

private:
    const BuildConfig& cfg_;
    std::string ndkRoot_;
    std::string toolchainBin_;  // prebuilt/linux-x86_64/bin
};

// ── Wraps Android SDK tools: aapt2, d8, zipalign, apksigner ─────────────────
class SDKBridge {
public:
    explicit SDKBridge(const BuildConfig& config);

    Result probe();

    std::string aapt2Path()      const;
    std::string d8Path()         const;
    std::string zipalignPath()   const;
    std::string apksignerPath()  const;
    std::string buildToolsDir()  const;

    // Compile res/ → resources.ap_ using aapt2
    Result compileResources(
        const std::string& resDir,
        const std::string& manifestPath,
        const std::string& outApk
    ) const;

    // Create a minimal APK by merging resources.ap_ + native .so + tiny_dex
    Result packageApk(
        const std::string& resApk,
        const std::string& nativeLibDir,   // contains arm64-v8a/libmain.so etc.
        const std::string& tinyDexPath,
        const std::string& outApk
    ) const;

    // Sign with debug key
    Result signApk(
        const std::string& apkPath,
        const std::string& keystorePath
    ) const;

private:
    const BuildConfig& cfg_;
    std::string sdkRoot_;        // resolved in probe() — may differ from cfg_.sdkRoot
    std::string buildToolsDir_;
};

} // namespace abt
