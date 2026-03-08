#include <abt/toolchain/NDKBridge.h>
#include <abt/common/Logger.h>
#include <abt/engine/ProcessPool.h>

#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <elf.h>
#include <set>

namespace abt {

static bool fileExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

NDKBridge::NDKBridge(const BuildConfig& config) : cfg_(config) {
    ndkRoot_ = config.ndkRoot;
    // On Linux/Termux, the toolchain is always linux-x86_64
    toolchainBin_ = ndkRoot_ + "/toolchains/llvm/prebuilt/linux-x86_64/bin";
}

Result NDKBridge::probe() {
    if (ndkRoot_.empty()) {
        // 1. Try environment variables
        const char* env = getenv("ANDROID_NDK_HOME");
        if (!env) env = getenv("ANDROID_NDK");
        if (!env) env = getenv("NDK_ROOT");

        if (env) {
            ndkRoot_ = env;
        } else {
            // Use $HOME so this works in Termux, standard Linux, any environment
            const char* home = getenv("HOME");
            const std::string defaultNdk =
                std::string(home ? home : ".") + "/assets/android-ndk";
            if (fileExists(defaultNdk)) {
                ABT_INFO("NDK: using default path: %s", defaultNdk.c_str());
                ndkRoot_ = defaultNdk;
            } else {
                return Result::fail(AbtError::NDK_NOT_FOUND,
                    "NDK not found. Set ANDROID_NDK_HOME or place NDK at: " + defaultNdk);
            }
        }
        toolchainBin_ = ndkRoot_ + "/toolchains/llvm/prebuilt/linux-x86_64/bin";
    }

    ABT_INFO("NDK root: %s", ndkRoot_.c_str());

    // Validate clang for arm64 (minimum sanity check)
    std::string clang = clangPath(TargetArch::ARM64);
    if (!fileExists(clang))
        return Result::fail(AbtError::NDK_NOT_FOUND,
            "clang++ not found at: " + clang);

    ABT_INFO("NDK probe OK — clang: %s", clang.c_str());
    return Result::success();
}

std::string NDKBridge::abiString(TargetArch arch) {
    switch (arch) {
        case TargetArch::ARM64:  return "arm64-v8a";
        case TargetArch::X86_64: return "x86_64";
        case TargetArch::ARM32:  return "armeabi-v7a";
        case TargetArch::X86:    return "x86";
    }
    return "arm64-v8a";
}

std::string NDKBridge::targetTriple(TargetArch arch) const {
    std::string api = std::to_string(cfg_.minSdkVersion);
    switch (arch) {
        case TargetArch::ARM64:  return "aarch64-linux-android"  + api;
        case TargetArch::X86_64: return "x86_64-linux-android"   + api;
        case TargetArch::ARM32:  return "armv7a-linux-androideabi" + api;
        case TargetArch::X86:    return "i686-linux-android"      + api;
    }
    return "aarch64-linux-android24";
}

std::string NDKBridge::clangPath(TargetArch arch) const {
    return toolchainBin_ + "/" + targetTriple(arch) + "-clang++";
}

// C compiler (clang, not clang++) — used for .c source files
std::string NDKBridge::clangCPath(TargetArch arch) const {
    return toolchainBin_ + "/" + targetTriple(arch) + "-clang";
}

std::string NDKBridge::lldPath() const {
    return toolchainBin_ + "/ld.lld";
}

std::string NDKBridge::stripPath() const {
    return toolchainBin_ + "/llvm-strip";
}

std::string NDKBridge::sysrootPath(TargetArch arch) const {
    return ndkRoot_ + "/toolchains/llvm/prebuilt/linux-x86_64/sysroot";
}

std::vector<std::string> NDKBridge::compileFlags(TargetArch arch) const {
    std::vector<std::string> flags = {
        "-target", targetTriple(arch),
        "--sysroot", sysrootPath(arch),
        "-fPIC",
        "-ffunction-sections",
        "-fdata-sections",
        "-fstack-protector-strong",
        "-std=c++17",
        "-DANDROID",
        // NOTE: do NOT define __ANDROID_API__ here — NDK clang already
        // defines it via __ANDROID_MIN_SDK_VERSION__ in the built-in headers.
        // Adding it again causes -Wmacro-redefined warnings and potential
        // value conflicts.
    };

    if (cfg_.buildType == BuildType::RELEASE) {
        flags.insert(flags.end(), {
            "-O3", "-flto", "-DNDEBUG"
        });
    } else {
        flags.insert(flags.end(), {
            "-O0", "-g", "-DDEBUG"
            // NOTE: -fsanitize=address removed — ASan requires libclang_rt.asan
            // to be bundled in the APK and loaded via wrap.sh, which abt does not
            // yet support. Enabling it here causes linker errors for all __asan_*
            // symbols since the runtime is never linked.
        });
    }

    // ARM64-specific tuning
    if (arch == TargetArch::ARM64) {
        flags.insert(flags.end(), {
            "-march=armv8-a+simd", "-mtune=cortex-a55"
        });
    } else if (arch == TargetArch::ARM32) {
        flags.insert(flags.end(), {
            "-march=armv7-a", "-mfpu=neon", "-mfloat-abi=softfp"
        });
    }

    // User include dirs
    for (const auto& inc : cfg_.includeDirs)
        flags.push_back("-I" + inc);

    return flags;
}

std::vector<std::string> NDKBridge::linkFlags(TargetArch arch) const {
    std::vector<std::string> flags = {
        "-target", targetTriple(arch),
        "--sysroot", sysrootPath(arch),
        "-shared",

        // SONAME must match the filename Android will dlopen().
        // NativeActivity looks for the name in android.app.lib_name meta-data
        // (which we set to "main"), so Android calls dlopen("libmain.so").
        // Without -soname the linker embeds the full build path as the library
        // identity — the dynamic linker then can't match it and refuses to load,
        // causing an instant silent crash back to the home screen.
        "-Wl,-soname,libmain.so",

        // Keep ANativeActivity_onCreate even though --gc-sections would normally
        // remove it (nothing in user code calls it by name — Android's runtime does).
        // This is the real entry point the system dlopen's into.
        "-Wl,--gc-sections",
        "-Wl,--undefined=ANativeActivity_onCreate",

        "-Wl,--build-id=sha1",
        "-Wl,--no-undefined",
        "-Wl,-z,noexecstack",
        "-Wl,-z,relro",
        "-Wl,-z,now",
    };

    if (cfg_.buildType == BuildType::RELEASE) {
        flags.push_back("-flto");
        // Don't strip-all in release either — stripping ANativeActivity_onCreate
        // will make the .so unloadable. Use strip-debug instead.
        flags.push_back("-Wl,--strip-debug");
    }

    // NOTE: -l flags are NOT added here. Libraries must come AFTER object files
    // in the linker command line — the linker resolves symbols left-to-right,
    // so a -lfoo before the .o files means undefined refs in those .o files
    // have nothing to resolve against. linkObjects() appends libs after objects.

    return flags;
}

// Separate method: returns only the -l flags, to be appended AFTER object files
std::vector<std::string> NDKBridge::linkLibFlags() const {
    std::vector<std::string> libs;
    for (const auto& lib : cfg_.linkLibraries)
        libs.push_back("-l" + lib);
    // android and log are always needed
    libs.push_back("-landroid");
    libs.push_back("-llog");
    return libs;
}

// ── Runtime lib bundling ───────────────────────────────────────────────────────

// Android system libraries — present on every device, never need bundling
static const std::set<std::string> kAndroidSystemLibs = {
    "libandroid.so", "libc.so", "libm.so", "libdl.so", "libz.so",
    "liblog.so", "libEGL.so", "libGLESv1_CM.so", "libGLESv2.so",
    "libGLESv3.so", "libvulkan.so", "libOpenSLES.so", "libOpenMAXAL.so",
    "libmediandk.so", "libcamera2ndk.so", "libnativewindow.so",
    "libneuralnetworks.so", "libsync.so", "libaaudio.so",
    "libamidi.so", "libandroid_net.so", "libbinder_ndk.so",
    "libjnigraphics.so", "libnativehelper.so",
    // linker internals
    "libdl_android.so", "ld-android.so",
};

// Parse the DT_NEEDED entries from an ELF64 shared library
std::vector<std::string> NDKBridge::elfNeededLibs(const std::string& soPath) {
    std::vector<std::string> result;
    FILE* f = fopen(soPath.c_str(), "rb");
    if (!f) return result;

    // Read ELF header
    Elf64_Ehdr ehdr;
    if (fread(&ehdr, sizeof(ehdr), 1, f) != 1) { fclose(f); return result; }
    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E') { fclose(f); return result; }

    // Find the dynamic segment (PT_DYNAMIC) and the string table (PT_LOAD + sh_type=SHT_STRTAB)
    // Easier: walk section headers to find SHT_DYNAMIC and its associated SHT_STRTAB

    if (ehdr.e_shoff == 0 || ehdr.e_shentsize == 0) { fclose(f); return result; }

    // Load all section headers
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    fseek(f, (long)ehdr.e_shoff, SEEK_SET);
    fread(shdrs.data(), sizeof(Elf64_Shdr), ehdr.e_shnum, f);

    // Find SHT_DYNAMIC and its linked SHT_STRTAB
    Elf64_Shdr* dynSec   = nullptr;
    Elf64_Shdr* strtabSec = nullptr;
    for (auto& sh : shdrs) {
        if (sh.sh_type == SHT_DYNAMIC) {
            dynSec    = &sh;
            if (sh.sh_link < ehdr.e_shnum)
                strtabSec = &shdrs[sh.sh_link];
            break;
        }
    }
    if (!dynSec || !strtabSec) { fclose(f); return result; }

    // Read the string table
    std::vector<char> strtab(strtabSec->sh_size);
    fseek(f, (long)strtabSec->sh_offset, SEEK_SET);
    fread(strtab.data(), 1, strtabSec->sh_size, f);

    // Walk dynamic entries looking for DT_NEEDED
    size_t nEntries = dynSec->sh_size / sizeof(Elf64_Dyn);
    std::vector<Elf64_Dyn> dyns(nEntries);
    fseek(f, (long)dynSec->sh_offset, SEEK_SET);
    fread(dyns.data(), sizeof(Elf64_Dyn), nEntries, f);

    for (const auto& d : dyns) {
        if (d.d_tag == DT_NEEDED) {
            size_t off = (size_t)d.d_un.d_val;
            if (off < strtab.size())
                result.push_back(strtab.data() + off);
        }
    }

    fclose(f);
    return result;
}

std::string NDKBridge::runtimeLibDir(TargetArch arch) const {
    // NDK ships runtime libs in two possible locations — prefer the sysroot one
    std::string abi = abiString(arch);

    // Location 1: toolchain sysroot (always present)
    std::string sysrootLibs = ndkRoot_ +
        "/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/" +
        [&]() -> std::string {
            switch (arch) {
                case TargetArch::ARM64:  return "aarch64-linux-android";
                case TargetArch::X86_64: return "x86_64-linux-android";
                case TargetArch::ARM32:  return "arm-linux-androideabi";
                case TargetArch::X86:    return "i686-linux-android";
            }
            return "aarch64-linux-android";
        }();

    return sysrootLibs;
}

std::vector<std::string> NDKBridge::runtimeLibsToBundle(
    const std::string& soPath, TargetArch arch) const
{
    std::vector<std::string> toBundle;
    std::vector<std::string> needed = elfNeededLibs(soPath);
    std::string libDir = runtimeLibDir(arch);

    ABT_INFO("Checking %zu DT_NEEDED entries for bundling...", needed.size());

    for (const auto& lib : needed) {
        // Skip Android system libs — they're on the device already
        if (kAndroidSystemLibs.count(lib)) {
            ABT_INFO("  [system] %s", lib.c_str());
            continue;
        }

        // Look for it in the NDK runtime lib directory
        std::string candidate = libDir + "/" + lib;
        struct stat st;
        if (stat(candidate.c_str(), &st) == 0) {
            ABT_INFO("  [bundle] %s  →  %s", lib.c_str(), candidate.c_str());
            toBundle.push_back(candidate);
        } else {
            ABT_WARN("  [missing] %s not found in NDK (looked in %s)",
                     lib.c_str(), libDir.c_str());
        }
    }

    return toBundle;
}

} // namespace abt
