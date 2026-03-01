/*
 * abt — Android Build Tool
 * Native C++ replacement for Gradle targeting NativeActivity APKs.
 *
 * Usage:
 *   abt build   [--release] [--arch arm64-v8a,x86_64] [--jobs N]
 *   abt clean
 *   abt watch   (incremental rebuild on file change)
 *   abt info    (print detected environment)
 *   abt install (build + adb install)
 */

#include <abt/common/Types.h>
#include <abt/common/Logger.h>
#include <abt/engine/BuildGraph.h>
#include <abt/engine/TaskScheduler.h>
#include <abt/engine/ProcessPool.h>
#include <abt/engine/xxHasher.h>
#include <abt/engine/IWatcher.h>
#include <abt/engine/ConfigParser.h>
#include <abt/toolchain/NDKBridge.h>
#include <abt/toolchain/DepManager.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// Forward declarations from SignalHandler.cpp
namespace abt {
    void installSignalHandlers(std::function<void()> cleanupCb);
    bool wasInterrupted();
}

namespace abt {

// ── Helpers ───────────────────────────────────────────────────────────────────
static void ensureDir(const std::string& d) { system(("mkdir -p " + d).c_str()); }

static std::string objFilePath(const BuildConfig& cfg,
                                TargetArch arch,
                                const std::string& src)
{
    std::string abi = NDKBridge::abiString(arch);
    // Strip leading "./" so "./src/foo.cpp" → "src_foo.cpp" not "._src_foo.cpp"
    std::string flat = src;
    if (flat.size() >= 2 && flat[0] == '.' && flat[1] == '/')
        flat = flat.substr(2);
    for (char& c : flat) if (c == '/') c = '_';
    return cfg.outputDir + "/obj/" + abi + "/" + flat + ".o";
}

// ── Build Pipeline ────────────────────────────────────────────────────────────
class BuildOrchestrator {
public:
    explicit BuildOrchestrator(BuildConfig cfg, int jobs = 0)
        : cfg_(std::move(cfg))
        , ndk_(cfg_)
        , sdk_(cfg_)
        , hashCache_(cfg_.outputDir + "/.abt/cache")
        , resolver_(cfg_.outputDir + "/.abt/deps",
                    cfg_.outputDir + "/.abt/extracted")
        , jobs_(jobs > 0 ? jobs : (int)std::thread::hardware_concurrency())
    {}

    Result run() {
        ensureDir(cfg_.outputDir);
        ensureDir(cfg_.outputDir + "/.abt/cache");

        ABT_CHECK(hashCache_.load());

        // ── Phase 1: Environment Probe ──
        ABT_INFO("=== Phase 1: Environment Probe ===");
        ABT_CHECK(ndk_.probe());
        ABT_CHECK(sdk_.probe());

        // ── Phase 2: Dependency Resolution ──
        ABT_INFO("=== Phase 2: Dependencies ===");
        for (const auto& dep : cfg_.dependencies) {
            for (TargetArch arch : cfg_.architectures) {
                ResolvedDep resolved;
                auto r = resolver_.resolve(dep, arch, resolved);
                if (!r.ok()) ABT_WARN("Dep warning: %s", r.msg.c_str());
                else {
                    cfg_.includeDirs.insert(cfg_.includeDirs.end(),
                        resolved.includeDirs.begin(), resolved.includeDirs.end());
                }
            }
        }

        // ── Phase 3: Build Graph Construction ──
        ABT_INFO("=== Phase 3: Build Graph ===");
        BuildGraph graph;
        buildGraph(graph);
        ABT_CHECK(graph.topologicalSort());

        ABT_INFO("Tasks in graph: %zu", graph.taskCount());

        // ── Phase 4: Parallel Execution ──
        ABT_INFO("=== Phase 4: Execute (jobs=%d) ===", jobs_);
        TaskScheduler scheduler(graph, jobs_);
        scheduler.setProgressCallback([](const std::string& label, Task::State state) {
            // Progress is handled in the scheduler via ABT_INFO
            (void)label; (void)state;
        });
        ABT_CHECK(scheduler.run());

        auto& stats = scheduler.stats();
        ABT_INFO("=== Build Complete ===");
        ABT_INFO("  Total:   %d", stats.total);
        ABT_INFO("  Ran:     %d", stats.ran);
        ABT_INFO("  Skipped: %d", stats.skipped);
        ABT_INFO("  Time:    %.1f ms", stats.wallTimeMs);

        ABT_CHECK(hashCache_.save());

        return Result::success();
    }

private:
    BuildConfig  cfg_;
    NDKBridge    ndk_;
    SDKBridge    sdk_;
    HashCache    hashCache_;
    Resolver     resolver_;
    int          jobs_;

    void buildGraph(BuildGraph& graph) {
        // One compile task per source per arch, then link, then package, then sign.

        // Final APK task (depends on everything)
        TaskId signId = graph.addTask(TaskType::SIGN_APK, "sign APK");
        TaskId packId = graph.addTask(TaskType::PACKAGE_APK, "package APK");
        graph.addEdge(signId, packId);

        // Resource compilation (one task, arch-independent)
        TaskId resId = graph.addTask(TaskType::COMPILE_RES, "aapt2 compile resources");
        graph.addEdge(packId, resId);

        std::string resDir      = cfg_.projectRoot + "/res";
        std::string manifestPath= cfg_.projectRoot + "/AndroidManifest.xml";
        std::string resApk      = cfg_.outputDir + "/resources.ap_";

        auto* resTask = graph.getTask(resId);
        resTask->inputs  = {resDir, manifestPath};
        resTask->outputs = {resApk};
        resTask->action  = [this, resDir, manifestPath, resApk]() -> Result {
            // Check if resources are up to date
            if (hashCache_.isUpToDate(manifestPath)) {
                ABT_INFO("  ↷  resources (up to date)");
                return Result::success();
            }
            ensureDir(cfg_.outputDir);
            // Generate manifest from template if it doesn't exist
            generateManifestIfNeeded(manifestPath);
            return sdk_.compileResources(resDir, manifestPath, resApk);
        };

        // Per-arch: compile + link
        for (TargetArch arch : cfg_.architectures) {
            std::string abi = NDKBridge::abiString(arch);
            ensureDir(cfg_.outputDir + "/obj/" + abi);
            ensureDir(cfg_.outputDir + "/lib/" + abi);

            std::string soOut = cfg_.outputDir + "/lib/" + abi + "/libmain.so";

            TaskId linkId = graph.addTask(TaskType::LINK_SO,
                "link " + abi + " → libmain.so");
            graph.addEdge(packId, linkId);

            auto* linkTask = graph.getTask(linkId);
            linkTask->outputs = {soOut};

            std::vector<std::string> objFiles;

            for (const auto& src : cfg_.cppSources) {
                std::string obj = objFilePath(cfg_, arch, src);
                objFiles.push_back(obj);

                TaskId compId = graph.addTask(TaskType::COMPILE_CPP,
                    "compile [" + abi + "] " + src.substr(src.rfind('/') + 1));
                graph.addEdge(linkId, compId);

                linkTask->inputs.push_back(obj);

                auto* compTask = graph.getTask(compId);
                compTask->inputs  = {src};
                compTask->outputs = {obj};

                compTask->action = [this, src, obj, arch]() -> Result {
                    // Cache key is source path + ABI so arm64 and x86_64
                    // each track their own staleness independently.
                    const std::string cacheKey = src + "@" + NDKBridge::abiString(arch);
                    if (hashCache_.isUpToDate(src, cacheKey)) {
                        ABT_INFO("  ↷  %s (up to date)", src.c_str());
                        return Result::success();
                    }
                    return compileSource(src, obj, arch);
                };
            }

            linkTask->action = [this, objFiles, soOut, arch]() -> Result {
                return linkObjects(objFiles, soOut, arch);
            };
        }

        // Package action
        auto* packTask = graph.getTask(packId);
        std::string tinyDex = findAbtRoot() + "/runtime/tiny_dex/classes.dex";
        std::string libDir  = cfg_.outputDir;
        std::string outApk  = cfg_.outputDir + "/" + cfg_.projectName + ".apk";
        packTask->action = [this, resApk, libDir, tinyDex, outApk]() -> Result {
            return sdk_.packageApk(resApk, libDir, tinyDex, outApk);
        };

        // Sign action
        std::string keystore = cfg_.outputDir + "/debug.keystore";
        auto* signTask = graph.getTask(signId);
        signTask->action = [this, outApk, keystore]() -> Result {
            ensureDebugKeystore(keystore);
            return sdk_.signApk(outApk, keystore);
        };
    }

    Result compileSource(const std::string& src, const std::string& obj,
                          TargetArch arch) const
    {
        // Use clang (not clang++) for plain C files. clang++ rejects valid C
        // idioms like implicit void* casts from malloc/calloc — which breaks
        // NDK C sources like android_native_app_glue.c.
        bool isCFile = src.size() >= 2 &&
                       src[src.size()-1] == 'c' &&
                       src[src.size()-2] == '.';
        std::string compiler = isCFile ? ndk_.clangCPath(arch) : ndk_.clangPath(arch);

        std::vector<std::string> argv = {compiler};
        auto flags = ndk_.compileFlags(arch);
        // Drop C++-only flags when compiling C sources
        for (const auto& flag : flags) {
            if (isCFile && (flag == "-std=c++17" || flag == "-std=c++14")) continue;
            argv.push_back(flag);
        }
        argv.insert(argv.end(), {"-c", src, "-o", obj});

        auto r = spawnProcess(argv);
        if (!r.ok()) {
            if (!r.stderrStr.empty()) fputs(r.stderrStr.c_str(), stderr);
            return Result::fail(AbtError::COMPILE_FAILED,
                (isCFile ? "clang failed for " : "clang++ failed for ") + src);
        }
        return Result::success();
    }

    Result linkObjects(const std::vector<std::string>& objs,
                        const std::string& soOut, TargetArch arch) const
    {
        // CRITICAL LINKER ORDER:
        //   clang++ [link-flags] [object-files] [output] [-l libraries]
        //
        // The GNU/LLVM linker resolves symbols left-to-right. Libraries (-l flags)
        // must come AFTER the .o files that reference them.
        std::vector<std::string> argv = {ndk_.clangPath(arch)};
        auto flags = ndk_.linkFlags(arch);
        argv.insert(argv.end(), flags.begin(), flags.end());
        for (const auto& o : objs) argv.push_back(o);
        argv.insert(argv.end(), {"-o", soOut});
        // Libraries AFTER objects
        auto libs = ndk_.linkLibFlags();
        argv.insert(argv.end(), libs.begin(), libs.end());

        auto r = spawnProcess(argv);
        if (!r.ok()) {
            if (!r.stderrStr.empty()) fputs(r.stderrStr.c_str(), stderr);
            return Result::fail(AbtError::LINK_FAILED, "ld.lld failed");
        }

        // ── Bundle NDK runtime libs ─────────────────────────────────────────
        // Android does NOT ship libc++_shared.so (or other NDK runtime libs)
        // on-device. Gradle silently copies them — we replicate that here.
        // Scan DT_NEEDED entries in the freshly-linked .so and copy any NDK
        // runtime libs (non-system libs) into the same lib/<abi>/ directory.
        // They'll get zipped into the APK alongside libmain.so automatically.
        ABT_INFO("Scanning %s for NDK runtime dependencies...",
                 soOut.substr(soOut.rfind('/')+1).c_str());

        auto runtimeLibs = ndk_.runtimeLibsToBundle(soOut, arch);
        std::string libDir = soOut.substr(0, soOut.rfind('/'));

        for (const auto& libSrc : runtimeLibs) {
            std::string libName = libSrc.substr(libSrc.rfind('/')+1);
            std::string libDst  = libDir + "/" + libName;
            auto cr = spawnProcess({"cp", libSrc, libDst});
            if (!cr.ok()) {
                ABT_WARN("Failed to copy runtime lib %s: %s",
                         libName.c_str(), cr.stderrStr.c_str());
            } else {
                ABT_INFO("  bundled: %s", libName.c_str());
            }
        }

        return Result::success();
    }

    void generateManifestIfNeeded(const std::string& manifestPath) const {
        struct stat st;
        if (stat(manifestPath.c_str(), &st) == 0) return; // already exists

        ABT_INFO("Generating AndroidManifest.xml from template");
        std::string manifest = R"(<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package=")" + cfg_.packageName + R"("
    android:versionCode=")" + std::to_string(cfg_.versionCode) + R"("
    android:versionName=")" + cfg_.versionName + R"(">

    <uses-sdk
        android:minSdkVersion=")" + std::to_string(cfg_.minSdkVersion) + R"("
        android:targetSdkVersion=")" + std::to_string(cfg_.targetSdkVersion) + R"(" />

    <uses-feature android:glEsVersion="0x00020000" android:required="true" />

    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.READ_MEDIA_AUDIO" />

    <application
        android:label=")" + cfg_.projectName + R"("
        android:icon="@mipmap/ic_launcher"
        android:hasCode="false">
        <activity
            android:name="android.app.NativeActivity"
            android:exported="true"
            android:screenOrientation="portrait"
            android:configChanges="orientation|keyboardHidden|screenSize">
            <meta-data android:name="android.app.lib_name" android:value="main" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
)";
        std::ofstream f(manifestPath);
        f << manifest;
    }

    void ensureDebugKeystore(const std::string& path) const {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) return;

        ABT_INFO("Generating debug keystore...");
        spawnProcess({
            "keytool", "-genkeypair",
            "-keystore", path,
            "-alias",    "androiddebugkey",
            "-keypass",  "android",
            "-storepass","android",
            "-dname",    "CN=Android Debug,O=Android,C=US",
            "-keyalg",   "RSA",
            "-keysize",  "2048",
            "-validity", "10000",
        });
    }

    std::string findAbtRoot() const {
        // Try to find the abt binary and return its directory
        char buf[4096];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            std::string p(buf);
            size_t slash = p.rfind('/');
            if (slash != std::string::npos) {
                // bin/ -> parent
                std::string binDir = p.substr(0, slash);
                slash = binDir.rfind('/');
                if (slash != std::string::npos)
                    return binDir.substr(0, slash);
            }
        }
        return ".";
    }
};

// ── Watch mode ────────────────────────────────────────────────────────────────
Result runWatch(BuildConfig cfg, int jobs) {
    ABT_INFO("Watch mode — listening for changes (Ctrl+C to stop)");
    auto watcher = createWatcher();
    watcher->addWatch(cfg.projectRoot + "/src");
    if (cfg.projectRoot != ".") watcher->addWatch(cfg.projectRoot);

    std::atomic<bool> needsRebuild{true}; // build immediately on start
    std::atomic<bool> building{false};

    watcher->start([&](const FileEvent& ev) {
        // Filter to source files only
        const std::string& p = ev.path;
        bool isSrc = p.find(".cpp") != std::string::npos ||
                     p.find(".h")   != std::string::npos ||
                     p.find(".c")   != std::string::npos;
        if (isSrc && !building.load()) {
            ABT_INFO("Changed: %s", p.c_str());
            needsRebuild.store(true);
        }
    });

    while (!wasInterrupted()) {
        if (needsRebuild.load() && !building.load()) {
            needsRebuild.store(false);
            building.store(true);

            BuildOrchestrator orch(cfg, jobs);
            auto r = orch.run();
            if (!r.ok()) ABT_ERR("Build failed: %s", r.msg.c_str());

            building.store(false);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    watcher->stop();
    return Result::success();
}

} // namespace abt

// ── CLI ───────────────────────────────────────────────────────────────────────
static void printUsage(const char* prog) {
    fprintf(stderr,
        "\033[1mabt\033[0m — Android Build Tool v1.0\n\n"
        "Usage: %s <command> [options]\n\n"
        "Commands:\n"
        "  build     Build the project (default)\n"
        "  clean     Remove build output\n"
        "  watch     Incremental rebuild on file change\n"
        "  info      Print detected environment\n"
        "  install   Build + adb install\n\n"
        "Options:\n"
        "  --release          Release build (default: debug)\n"
        "  --arch <abis>      Comma-separated: arm64-v8a,x86_64 (default: arm64-v8a)\n"
        "  --jobs, -j <N>     Parallel jobs (default: nproc)\n"
        "  --project, -p <dir> Project directory (default: .)\n"
        "  --verbose, -v      Enable verbose logging\n\n"
        "Host arch flags (controls which aapt2 binary is used):\n"
        "  --64               ARM64 host (Termux on Android) — use system aapt2 from PATH\n"
        "  --86               x86_64 host (Linux desktop/WSL) — use SDK build-tools aapt2\n"
        "  (default: auto-detected from host CPU at runtime)\n\n"
        "Default paths when env vars are not set:\n"
        "  NDK : $HOME/asserts/android_NDK   (override: ANDROID_NDK_HOME)\n"
        "  SDK : $HOME/asserts/android-sdk   (override: ANDROID_SDK_ROOT)\n\n"
        "Examples:\n"
        "  abt build --64                          # ARM64 device / Termux\n"
        "  abt build --86 --release                # x86_64 Linux desktop\n"
        "  abt build --release --arch arm64-v8a,x86_64 -j 8\n",
        prog);
}

int main(int argc, char** argv) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    std::string command     = argv[1];
    std::string projectDir  = ".";
    bool        release     = false;
    int         jobs        = 0;
    bool        verbose     = false;
    std::vector<std::string> arches;
    abt::HostArch hostArch  = abt::HostArch::AUTO;

    // Parse flags
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--release")                    release = true;
        else if (a == "--verbose" || a == "-v")  verbose = true;
        else if (a == "--64")                    hostArch = abt::HostArch::ARM64;
        else if (a == "--86")                    hostArch = abt::HostArch::X86_64;
        else if ((a == "--project" || a == "-p") && i + 1 < argc)
            projectDir = argv[++i];
        else if ((a == "--jobs" || a == "-j") && i + 1 < argc)
            jobs = std::atoi(argv[++i]);
        else if (a == "--arch" && i + 1 < argc) {
            std::istringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) arches.push_back(tok);
        }
        else if (a == "--help" || a == "-h") { printUsage(argv[0]); return 0; }
    }

    if (verbose) abt::Logger::level = abt::LogLevel::TRACE;

    if (command == "clean") {
        std::string buildDir = projectDir + "/build";
        ABT_INFO("Cleaning %s", buildDir.c_str());
        return system(("rm -rf " + buildDir).c_str());
    }

    // Parse config
    abt::ConfigParser parser(projectDir);
    abt::BuildConfig cfg;
    auto parseResult = parser.parse(cfg);
    if (!parseResult.ok()) {
        // Minimal defaults if no AbtProject.cmake
        cfg.projectRoot  = projectDir;
        cfg.projectName  = "MyApp";
        cfg.packageName  = "com.example.myapp";
        cfg.outputDir    = projectDir + "/build";
        ABT_WARN("No AbtProject.cmake found — using defaults");
    }

    // Apply CLI overrides
    if (release)    cfg.buildType = abt::BuildType::RELEASE;
    cfg.hostArch = hostArch;  // --64 / --86 / AUTO
    if (!arches.empty()) {
        cfg.architectures.clear();
        for (const auto& a : arches) {
            if      (a == "arm64-v8a")   cfg.architectures.push_back(abt::TargetArch::ARM64);
            else if (a == "x86_64")      cfg.architectures.push_back(abt::TargetArch::X86_64);
            else if (a == "armeabi-v7a") cfg.architectures.push_back(abt::TargetArch::ARM32);
            else if (a == "x86")         cfg.architectures.push_back(abt::TargetArch::X86);
        }
    }

    // Install signal handlers
    abt::installSignalHandlers([&]() {
        ABT_WARN("Interrupted — saving hash cache...");
    });

    if (command == "info") {
        abt::NDKBridge ndk(cfg);
        abt::SDKBridge sdk(cfg);
        ABT_INFO("Project:      %s", cfg.projectName.c_str());
        ABT_INFO("Package:      %s", cfg.packageName.c_str());
        ABT_INFO("Build type:   %s", cfg.buildType == abt::BuildType::RELEASE ? "release" : "debug");
        ABT_INFO("Min SDK:      %d", cfg.minSdkVersion);
        ABT_INFO("Sources:      %zu", cfg.cppSources.size());
        ndk.probe();
        sdk.probe();
        return 0;
    }

    if (command == "watch") {
        auto r = abt::runWatch(cfg, jobs);
        return r.ok() ? 0 : 1;
    }

    if (command == "build" || command == "install") {
        abt::BuildOrchestrator orch(cfg, jobs);
        auto r = orch.run();
        if (!r.ok()) {
            ABT_ERR("Build failed: %s", r.msg.c_str());
            return 1;
        }
        if (command == "install") {
            std::string apk = cfg.outputDir + "/" + cfg.projectName + ".apk";
            ABT_INFO("Installing: %s", apk.c_str());
            auto ir = abt::spawnProcess({"adb", "install", "-r", apk});
            if (!ir.ok()) {
                ABT_ERR("adb install failed:\n%s", ir.stderrStr.c_str());
                return 1;
            }
            ABT_INFO("%s", ir.stdoutStr.c_str());
        }
        return 0;
    }

    ABT_ERR("Unknown command: %s", command.c_str());
    printUsage(argv[0]);
    return 1;
}
