#include <abt/toolchain/NDKBridge.h>
#include <abt/engine/ProcessPool.h>
#include <abt/common/Logger.h>

#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>   // realpath
#include <dirent.h>
#include <sstream>
#include <algorithm>
#include <vector>

namespace abt {

static bool fileExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

// Read the ELF e_machine field to check if a binary matches the current CPU.
// This is reliable — runtime exec tests fail unpredictably because the kernel
// may silently fall back to interpreting an x86_64 ELF as a shell script,
// producing a syntax error instead of the expected "Exec format error".
//
// ELF e_machine values relevant to Android:
//   0x0028 = ARM32 (armeabi-v7a)
//   0x00B7 = AArch64 / ARM64
//   0x0003 = x86
//   0x003E = x86_64
static bool binaryRunnable(const std::string& path) {
    if (!fileExists(path)) return false;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    unsigned char hdr[20] = {};
    bool ok = fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr);
    fclose(f);
    if (!ok) return false;

    // Not an ELF (e.g. a shell script) — assume it works on any host
    if (hdr[0] != 0x7f || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F')
        return true;

    uint16_t e_machine = (uint16_t)(hdr[18] | (hdr[19] << 8));

#if defined(__aarch64__)
    return e_machine == 0x00B7;
#elif defined(__arm__)
    return e_machine == 0x0028;
#elif defined(__x86_64__)
    return e_machine == 0x003E;
#elif defined(__i386__)
    return e_machine == 0x0003;
#else
    return true; // unknown host — assume runnable
#endif
}

// Search PATH for a binary by name; return full path or empty string.
static std::string findInPath(const std::string& name) {
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) return "";
    std::istringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        std::string full = dir + "/" + name;
        if (fileExists(full)) return full;
    }
    return "";
}

// Find the highest available build-tools version
static std::string findBuildTools(const std::string& sdkRoot) {
    std::string btDir = sdkRoot + "/build-tools";
    DIR* d = opendir(btDir.c_str());
    if (!d) return "";
    std::vector<std::string> versions;
    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] != '.')
            versions.push_back(ent->d_name);
    }
    closedir(d);
    if (versions.empty()) return "";
    std::sort(versions.rbegin(), versions.rend()); // descending → newest first
    return btDir + "/" + versions.front();
}

SDKBridge::SDKBridge(const BuildConfig& config) : cfg_(config) {
    buildToolsDir_ = findBuildTools(config.sdkRoot);
}

Result SDKBridge::probe() {
    std::string sdkRoot = cfg_.sdkRoot;
    if (sdkRoot.empty()) {
        // 1. Try environment variables
        const char* env = getenv("ANDROID_SDK_ROOT");
        if (!env) env = getenv("ANDROID_HOME");

        if (env) {
            sdkRoot = env;
        } else {
            // Use $HOME so this works in Termux, standard Linux, any environment
            const char* home = getenv("HOME");
            const std::string defaultSdk =
                std::string(home ? home : ".") + "/assets/android-sdk";
            if (fileExists(defaultSdk)) {
                ABT_INFO("SDK: using default path: %s", defaultSdk.c_str());
                sdkRoot = defaultSdk;
            } else {
                return Result::fail(AbtError::SDK_NOT_FOUND,
                    "SDK not found. Set ANDROID_SDK_ROOT or place SDK at: " + defaultSdk);
            }
        }
        buildToolsDir_ = findBuildTools(sdkRoot);
    }
    sdkRoot_ = sdkRoot;  // persist resolved path for use in other methods

    if (buildToolsDir_.empty())
        return Result::fail(AbtError::SDK_NOT_FOUND,
            "No build-tools found in " + sdkRoot + "/build-tools");

    ABT_INFO("SDK build-tools: %s", buildToolsDir_.c_str());

    std::string a2 = aapt2Path();
    if (!fileExists(a2))
        return Result::fail(AbtError::SDK_NOT_FOUND,
            "aapt2 not found. Install it via 'pkg install aapt2' in Termux or place it in PATH.");
    ABT_INFO("aapt2: %s", a2.c_str());

    return Result::success();
}

std::string SDKBridge::aapt2Path() const {
    // ── Host-arch selection ─────────────────────────────────────────────────
    //
    // --64 (ARM64 host, e.g. Termux on Android):
    //   Termux ships a native arm64 aapt2 binary accessible via PATH / usr/bin.
    //   The SDK build-tools aapt2 is an x86_64 ELF — it can't run on ARM64
    //   without an emulator layer. So we always prefer the system aapt2 here.
    //
    // --86 (x86_64 host, e.g. Linux desktop, WSL):
    //   The SDK build-tools aapt2 is a native x86_64 binary — use it directly.
    //   Fall back to system aapt2 only if the SDK binary isn't executable.

    const HostArch host = cfg_.hostArch;

    // Auto-detect when not set explicitly by CLI flag
    HostArch effective = host;
    if (effective == HostArch::AUTO) {
#if defined(__aarch64__)
        effective = HostArch::ARM64;
#else
        effective = HostArch::X86_64;
#endif
    }

    if (effective == HostArch::ARM64) {
        // ARM64 host: prefer system aapt2 (native arm64 build from Termux)
        std::string sys = findInPath("aapt2");
        if (!sys.empty()) {
            ABT_INFO("aapt2 [arm64]: using system binary at %s", sys.c_str());
            return sys;
        }
        // Last resort: /usr/bin/aapt2
        if (fileExists("/usr/bin/aapt2")) return "/usr/bin/aapt2";
        // Nothing found — return SDK path so error messages are meaningful
        ABT_WARN("aapt2: no arm64 system binary found; SDK binary may fail on ARM64");
        return buildToolsDir_ + "/aapt2";
    }

    // X86_64 host: use SDK build-tools aapt2
    std::string sdkBin = buildToolsDir_ + "/aapt2";
    if (binaryRunnable(sdkBin)) {
        ABT_INFO("aapt2 [x86_64]: using SDK binary at %s", sdkBin.c_str());
        return sdkBin;
    }
    // SDK binary not runnable — fall back to system aapt2
    std::string sys = findInPath("aapt2");
    if (!sys.empty()) {
        ABT_WARN("aapt2: SDK binary not runnable; falling back to system aapt2 at %s", sys.c_str());
        return sys;
    }
    return sdkBin; // return SDK path so error messages are meaningful
}
std::string SDKBridge::d8Path()        const { return buildToolsDir_ + "/d8"; }
std::string SDKBridge::zipalignPath()  const { return buildToolsDir_ + "/zipalign"; }
std::string SDKBridge::apksignerPath() const { return buildToolsDir_ + "/apksigner"; }
std::string SDKBridge::buildToolsDir() const { return buildToolsDir_; }

Result SDKBridge::compileResources(
    const std::string& resDir,
    const std::string& manifestPath,
    const std::string& outApk) const
{
    // aapt2 compile --dir <resDir> -o <flat_dir>
    // aapt2 link --proto-format -o <outApk> -I <android.jar>
    //       --manifest <manifest> <flat_files...>

    // Use sdkRoot_ which was fully resolved (env vars + Termux default) in probe().
    // cfg_.sdkRoot alone is often empty when using auto-detection.
    std::string androidJar = sdkRoot_ + "/platforms/android-"
        + std::to_string(cfg_.targetSdkVersion) + "/android.jar";

    std::string flatDir = cfg_.outputDir + "/aapt_flat";
    spawnProcess({"mkdir", "-p", flatDir});

    // Step 1: compile
    auto r1 = spawnProcess({
        aapt2Path(), "compile",
        "--dir", resDir,
        "-o", flatDir
    });
    if (!r1.ok()) {
        return Result::fail(AbtError::COMPILE_FAILED,
            "aapt2 compile failed:\n" + r1.stderrStr);
    }

    // Collect .flat files
    // NOTE: do NOT pass --proto-format here. That flag outputs protobuf-encoded
    // resources for App Bundles (.aab). A regular APK needs binary XML format
    // (the default). Using --proto-format causes apksigner to fail with
    // "No XML chunk in file" because it expects binary XML in AndroidManifest.xml.
    std::vector<std::string> argv = {
        aapt2Path(), "link",
        "-o", outApk,
        "-I", androidJar,
        "--manifest", manifestPath,
        "--min-sdk-version",    std::to_string(cfg_.minSdkVersion),
        "--target-sdk-version", std::to_string(cfg_.targetSdkVersion),
        "--version-name",       cfg_.versionName,
        "--version-code",       std::to_string(cfg_.versionCode),
    };

    DIR* d = opendir(flatDir.c_str());
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d))) {
            std::string n = ent->d_name;
            if (n.size() > 5 && n.substr(n.size()-5) == ".flat")
                argv.push_back(flatDir + "/" + n);
        }
        closedir(d);
    }

    auto r2 = spawnProcess(argv);
    if (!r2.ok())
        return Result::fail(AbtError::COMPILE_FAILED,
            "aapt2 link failed:\n" + r2.stderrStr);

    ABT_INFO("Resources compiled → %s", outApk.c_str());
    return Result::success();
}

Result SDKBridge::packageApk(
    const std::string& resApk,
    const std::string& nativeLibDir,
    const std::string& tinyDexPath,
    const std::string& outApk) const
{
    // Resolve all caller-provided paths to absolute paths RIGHT HERE before
    // any spawnProcess call that uses a custom workingDir. When spawnProcess
    // calls chdir(), every relative path becomes relative to the NEW directory
    // instead of the project root — so "./build/MyApp.apk" would silently
    // become "./build/build/MyApp.apk" and zip fails with empty stderr because
    // it can't open the output file. realpath() anchors paths to the CWD once,
    // before any chdir happens.
    auto absPath = [](const std::string& p) -> std::string {
        char buf[PATH_MAX];
        // realpath requires the file to exist; for output files use the dir
        std::string dir = p.substr(0, p.rfind('/'));
        std::string base = p.substr(p.rfind('/') + 1);
        if (realpath(dir.c_str(), buf)) return std::string(buf) + "/" + base;
        if (realpath(p.c_str(), buf)) return buf;
        return p; // fallback: return as-is
    };

    const std::string absResApk      = absPath(resApk);
    const std::string absOutApk      = absPath(outApk);
    const std::string absTinyDex     = absPath(tinyDexPath);
    const std::string absNativeDir   = absPath(nativeLibDir);

    // Copy resources.ap_ to output APK path
    spawnProcess({"cp", absResApk, absOutApk});

    // Add classes.dex (zip -j = junk paths, store file flat at root of ZIP)
    auto r1 = spawnProcess({"zip", "-j", absOutApk, absTinyDex});
    if (!r1.ok())
        return Result::fail(AbtError::PACKAGE_FAILED,
            "Failed to add dex: " + r1.stderrStr);

    // Add native libs maintaining lib/<abi>/libmain.so inside the APK.
    // Run zip FROM absNativeDir (the build/ dir) so the stored path is
    // "lib/arm64-v8a/libmain.so" — exactly what Android's package manager
    // expects. absOutApk is absolute so chdir() doesn't affect it.
    auto r2 = spawnProcess(
        {"zip", "-r", absOutApk, "lib"},
        absNativeDir             // chdir here; absOutApk is safe (absolute)
    );
    if (!r2.ok())
        return Result::fail(AbtError::PACKAGE_FAILED,
            "Failed to add native libs: " + r2.stderrStr);

    // zipalign -v -p 4 (use abs paths — no workingDir change here but be safe)
    std::string alignedApk = absOutApk + ".aligned";
    auto r3 = spawnProcess({
        zipalignPath(), "-v", "-p", "4", absOutApk, alignedApk
    });
    if (!r3.ok())
        return Result::fail(AbtError::PACKAGE_FAILED,
            "zipalign failed: " + r3.stderrStr);

    spawnProcess({"mv", alignedApk, absOutApk});

    ABT_INFO("APK packaged → %s", absOutApk.c_str());
    return Result::success();
}

Result SDKBridge::signApk(
    const std::string& apkPath,
    const std::string& keystorePath) const
{
    auto r = spawnProcess({
        apksignerPath(), "sign",
        "--ks",          keystorePath,
        "--ks-pass",     "pass:android",
        "--key-pass",    "pass:android",
        "--out",         apkPath,
        apkPath
    });
    if (!r.ok())
        return Result::fail(AbtError::SIGN_FAILED,
            "apksigner failed:\n" + r.stderrStr);

    ABT_INFO("APK signed: %s", apkPath.c_str());
    return Result::success();
}

} // namespace abt
