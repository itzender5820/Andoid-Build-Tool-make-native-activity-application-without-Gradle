# ABT : Lightning-Fast Native Android Builds (No Gradle, No JRE, No BS)

# Build native C++ Android apps with lightning fast speed 

# ABT — Android Build Tool

> A fast, native C++ replacement for Gradle, purpose-built for compiling **NativeActivity APKs** directly from Termux or any Linux environment.

**License:** Apache 2.0 — Copyright © 2026 itz-ender

---

## Table of Contents

- [What is ABT?](#what-is-abt)
- [Prerequisites](#prerequisites)
  - [Required Asset Layout](#required-asset-layout)
  - [Termux (ARM64) — Special Note on aapt2](#termux-arm64--special-note-on-aapt2)
- [Building ABT Itself](#building-abt-itself)
- [Commands](#commands)
- [Host Architecture Flags](#host-architecture-flags)
- [All CLI Options](#all-cli-options)
- [Project Configuration — AbtProject.cmake](#project-configuration--abtprojectcmake)
  - [All Directives](#all-directives)
  - [Variable Expansion](#variable-expansion)
- [How a Build Works — The 4-Phase Pipeline](#how-a-build-works--the-4-phase-pipeline)
  - [Phase 1: Environment Probe](#phase-1-environment-probe)
  - [Phase 2: Dependency Resolution](#phase-2-dependency-resolution)
  - [Phase 3: Build Graph Construction](#phase-3-build-graph-construction)
  - [Phase 4: Parallel Execution](#phase-4-parallel-execution)
- [Incremental Builds — The Hash Cache](#incremental-builds--the-hash-cache)
- [APK Packaging Pipeline](#apk-packaging-pipeline)
- [Watch Mode](#watch-mode)
- [Project Structure](#project-structure)
- [Example Projects (abt-tests)](#example-projects-abt-tests)
  - [01-simple-native](#01-simple-native)
  - [02-gl-triangle](#02-gl-triangle)
  - [03-audio-sink](#03-audio-sink)
- [Environment Variables](#environment-variables)
- [Troubleshooting](#troubleshooting)

---

## What is ABT?

ABT replaces Gradle for projects that only need a **native shared library** (`libmain.so`) packaged into an APK. It skips the entire JVM toolchain — no Java, no Kotlin, no Gradle daemon, no `.gradle` cache eating gigabytes of storage.

Under the hood ABT drives the **Android NDK's clang/clang++ toolchain** directly, packs resources with **aapt2**, bundles a prebuilt minimal `classes.dex` for the NativeActivity bootstrap, and signs the result with **apksigner** (or `jarsigner` as fallback). The entire tool is a single statically-linked C++ binary — fast to start, zero runtime dependencies.

ABT was designed from the ground up to run comfortably inside **Termux on an Android phone**, where Gradle is impractical.

---

## Prerequisites

### Required Asset Layout

ABT expects the Android NDK and SDK to live inside an `assets` folder in your `$HOME` directory. This is the default lookup path when no environment variable is set.

```
$HOME/
└── assets/
    ├── android_NDK/          ← Android NDK (e.g. r26c or later)
    │   ├── toolchains/
    │   │   └── llvm/
    │   │       └── prebuilt/
    │   │           └── linux-x86_64/
    │   │               └── bin/   ← clang, clang++, ld.lld, llvm-strip …
    │   └── sources/
    │       └── android/
    │           └── native_app_glue/
    └── android-sdk/          ← Android SDK
        └── build-tools/
            └── <version>/
                └── aapt2, apksigner, zipalign …
```

You can override either path with environment variables (see [Environment Variables](#environment-variables)).

### Termux (ARM64) — Special Note on aapt2

`aapt2` from the SDK `build-tools` folder is an **x86_64 ELF binary** and cannot run natively on an ARM device. When you build with `--64` (ARM64 host mode), ABT switches to the system `aapt2` from your Termux `PATH` instead. You must install it manually:

```bash
pkg install aapt2
```

Without this, resource compilation will fail on ARM64 hosts even if the NDK is correctly installed.

---

## Building ABT Itself

ABT ships a `build.sh` script that compiles the tool from source using CMake and Termux's clang. Run it once from the ABT source directory:

```bash
cd ABT
bash build.sh
```

The script will:
1. Check for and install missing Termux packages (`cmake`, `clang`, `make`, `python`)
2. Generate the bundled `runtime/tiny_dex/classes.dex` using a Python helper
3. Configure and build ABT with CMake in Release mode
4. Place the final binary at `ABT/bin/abt`

After building, add `bin/` to your PATH:

```bash
export PATH="$HOME/ABT/bin:$PATH"
# Add this line to ~/.bashrc or ~/.zshrc to make it permanent
```

Verify everything is working:

```bash
abt info
```

---

## Commands

| Command | Description |
|---|---|
| `abt build` | Build the project in the current directory |
| `abt clean` | Delete the `build/` output directory |
| `abt watch` | Continuously rebuild when source files change |
| `abt info` | Print detected NDK/SDK paths and project info |
| `abt install` | Build then push the APK to a connected device via `adb install` |

---

## Host Architecture Flags

These flags tell ABT which host machine it is running on. This controls which `aapt2` binary gets used during resource compilation.

```bash
abt build --64   # You are on an ARM64 device (Termux — primary use case)
abt build --86   # You are on an x86_64 machine (Linux desktop, WSL, emulator host)
```

If you omit the flag, ABT will attempt to auto-detect the host CPU at runtime. It is strongly recommended to always pass the flag explicitly to avoid ambiguity.

**When to use which flag:**

- Running ABT **inside Termux on any Android phone/tablet** → use `--64`
- Running ABT on a **standard Linux PC, WSL, or CI server** → use `--86`

---

## All CLI Options

```
abt <command> [options]

Options:
  --release              Release build with optimisations (default: debug)
  --arch <abis>          Comma-separated target ABIs: arm64-v8a,x86_64,armeabi-v7a,x86
                         (default: arm64-v8a)
  --jobs, -j <N>         Number of parallel compile jobs (default: all CPU cores)
  --project, -p <dir>    Path to project directory (default: current directory)
  --verbose, -v          Enable verbose/trace logging
  --64                   ARM64 host — use system aapt2 from PATH (Termux)
  --86                   x86_64 host — use SDK build-tools aapt2
  --help, -h             Show help and exit
```

### Examples

```bash
# Build for ARM64 device from Termux
abt build --64

# Build release APK for both ARM64 and x86_64
abt build --86 --release --arch arm64-v8a,x86_64

# Build a project in another directory with 8 parallel jobs
abt build --64 -p ~/projects/MyGame -j 8

# Build and immediately install to connected device
abt install --64

# Watch for changes and rebuild automatically
abt watch --64

# Check environment (verify NDK/SDK are found correctly)
abt info
```

---

## Project Configuration — AbtProject.cmake

Every ABT project needs an `AbtProject.cmake` file in its root directory. Despite the `.cmake` extension, this file is parsed by ABT's own lightweight config parser — **not** by CMake itself. The syntax is a strict subset of CMake function-call syntax.

A minimal example:

```cmake
abt_project(MyApp)
abt_set_package(com.example.myapp)
abt_set_min_sdk(24)
abt_set_target_sdk(34)
abt_version(1.0 1)

abt_architectures(arm64-v8a x86_64)

abt_add_sources(
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    src/main.cpp
    src/Renderer.cpp
)

abt_add_includes(
    include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

abt_link_libraries(android log EGL GLESv3 m)
```

### All Directives

| Directive | Arguments | Description |
|---|---|---|
| `abt_project` | `<name>` | Sets the project/app name. Also used as the APK filename. |
| `abt_set_package` | `<package>` | Android package name (e.g. `com.example.myapp`) |
| `abt_set_min_sdk` | `<level>` | `minSdkVersion` (integer) |
| `abt_set_target_sdk` | `<level>` | `targetSdkVersion` (integer) |
| `abt_version` | `<name> <code>` | Version name string and integer version code |
| `abt_architectures` | `<abi> …` | Target ABI(s): `arm64-v8a`, `x86_64`, `armeabi-v7a`, `x86` |
| `abt_build_type` | `debug` or `release` | Default build type (overridden by `--release` CLI flag) |
| `abt_add_sources` | `<file> …` | Source files to compile (`.cpp` and `.c` supported) |
| `abt_add_includes` | `<dir> …` | Include search directories passed to clang |
| `abt_link_libraries` | `<lib> …` | Libraries to link against (passed as `-l<lib>` flags) |
| `abt_dependency` | `<group:name:version>` | External dependency (resolved via DepManager) |
| `abt_output_dir` | `<path>` | Override the default `build/` output directory |
| `set` | `<VAR> <value>` | Define a local variable usable via `${VAR}` |

### Variable Expansion

ABT expands `${VAR}` references inside directive arguments. Two variables are always pre-seeded:

| Variable | Value |
|---|---|
| `${ANDROID_NDK}` | Resolved NDK root path (same lookup order as the tool itself) |
| `${CMAKE_CURRENT_SOURCE_DIR}` | Absolute path to the project root directory |
| `${CMAKE_SOURCE_DIR}` | Same as above |

You can also define your own with `set(MY_VAR value)` and reference them as `${MY_VAR}`.

---

## How a Build Works — The 4-Phase Pipeline

When you run `abt build`, the `BuildOrchestrator` executes four sequential phases.

### Phase 1: Environment Probe

ABT validates that the NDK and SDK are accessible and finds the correct compiler binaries. It looks for the NDK in this priority order:

1. `ANDROID_NDK_HOME` environment variable
2. `ANDROID_NDK` environment variable
3. `NDK_ROOT` environment variable
4. `$HOME/assets/android_NDK` (the default expected location)

The minimum sanity check is confirming that the `aarch64-linux-android<minSdk>-clang++` binary exists inside the NDK toolchain. If nothing is found, the build aborts with a clear error pointing to the missing path.

### Phase 2: Dependency Resolution

If your `AbtProject.cmake` uses `abt_dependency()`, ABT's `DepManager` (`Resolver`) attempts to locate or download the specified native library for each target ABI. Resolved include directories are automatically merged into the build config. This phase continues even if a dependency warning occurs — it is non-fatal by design.

### Phase 3: Build Graph Construction

ABT constructs a directed acyclic graph (DAG) of tasks before executing anything. The graph always has this shape:

```
[compile source A (arm64)] ─┐
[compile source B (arm64)] ─┤
[compile source C (arm64)] ─┤─→ [link arm64 → libmain.so] ─┐
                             │                               │
[compile source A (x86_64)] ┤                               ├→ [package APK] → [sign APK]
[compile source B (x86_64)] ┤                               │
[compile source C (x86_64)] ┘─→ [link x86_64 → libmain.so] ┘
                                                             │
                                      [compile resources] ──┘
```

Each node is a `Task` with typed actions (`COMPILE_CPP`, `COMPILE_C`, `LINK_SO`, `COMPILE_RES`, `PACKAGE_APK`, `SIGN_APK`). Edges encode dependencies. A topological sort determines the legal execution order. Cycle detection runs automatically — a circular dependency is a hard build error.

**Compiler selection per file:** ABT detects `.c` vs `.cpp` files automatically. Plain C files (including `android_native_app_glue.c`) are compiled with `clang` to avoid C++-only flags like `-std=c++17` that would cause errors.

**Linker flag ordering:** Object files are always placed before `-l` library flags on the linker command line, matching GNU/LLVM's left-to-right symbol resolution requirement.

**NDK runtime bundling:** After linking, ABT reads the `DT_NEEDED` ELF entries from the freshly built `.so`. Any NDK runtime libraries (such as `libc++_shared.so`) that are not guaranteed to be present on-device are automatically copied into `build/lib/<abi>/` and get bundled into the APK alongside `libmain.so`.

### Phase 4: Parallel Execution

The `TaskScheduler` runs tasks across a thread pool. The number of threads defaults to the hardware concurrency of the host (`nproc`), or the value you pass with `-j`. Tasks are dispatched as soon as all their dependencies complete. At the end of a successful build, ABT prints a summary:

```
=== Build Complete ===
  Total:   23
  Ran:     5
  Skipped: 18
  Time:    1843.2 ms
```

---

## Incremental Builds — The Hash Cache

ABT tracks source file changes using **xxHash3** (a very fast non-cryptographic hash). On every compile task, the hash of the source file is compared against the stored value in `build/.abt/cache/hashes.bin`. If the hash matches, the task is skipped with a `↷ (up to date)` message. Cache entries are keyed by `sourcepath@abi` so that arm64 and x86_64 objects each track their own staleness independently.

The hash cache is saved at the end of every successful build and loaded at the start of the next. If the build is interrupted (Ctrl+C), ABT catches the signal and saves the cache before exiting so progress is not lost.

Running `abt clean` deletes the entire `build/` directory including the cache, forcing a full rebuild on the next run.

---

## APK Packaging Pipeline

After compilation and linking, ABT assembles the APK without any JVM tooling:

1. **Resource compilation** — `aapt2 compile` flattens your `res/` directory into `.flat` files, then `aapt2 link` combines them with `AndroidManifest.xml` to produce `build/resources.ap_`.
2. **Manifest auto-generation** — If no `AndroidManifest.xml` exists in your project root, ABT generates one from a built-in template using the values from your `AbtProject.cmake`. The template configures `android.app.NativeActivity`, sets `lib_name` to `main` (matching `libmain.so`), and requests common permissions.
3. **APK assembly** — The native libraries (`lib/<abi>/libmain.so`, `libc++_shared.so`, etc.), the compiled resources, and the bundled `runtime/tiny_dex/classes.dex` (a minimal Dalvik bootstrap) are zipped together into `build/<ProjectName>.apk`.
4. **Debug signing** — ABT checks for `build/debug.keystore`. If it does not exist, it generates one automatically using `keytool` with the standard Android debug certificate parameters (`CN=Android Debug,O=Android,C=US`). The APK is then signed with `apksigner`.

---

## Watch Mode

```bash
abt watch --64
```

Watch mode uses **inotify** to monitor your project's `src/` directory for changes to `.cpp`, `.c`, and `.h` files. When a change is detected, ABT triggers a full incremental build (skipping up-to-date files via the hash cache). The loop runs until you press `Ctrl+C`.

Watch mode is intentionally simple: one build runs at a time. If files change during a build, the rebuild is queued and starts immediately after the current build finishes.

---

## Project Structure

This is the layout of the ABT source tree itself:

```
ABT/
├── build.sh                  ← Script to compile ABT from source (run this first)
├── CMakeLists.txt            ← CMake build definition for abt itself
├── LICENSE                   ← Apache 2.0
├── NOTICE
│
├── src/                      ← ABT C++ source code
│   ├── main.cpp              ← CLI entry point, BuildOrchestrator, Watch mode
│   ├── engine/
│   │   ├── config/
│   │   │   └── ConfigParser.cpp    ← AbtProject.cmake parser
│   │   ├── core/
│   │   │   ├── BuildGraph.cpp      ← DAG construction and topological sort
│   │   │   └── TaskScheduler.cpp   ← Parallel task executor
│   │   ├── fs/
│   │   │   ├── Watcher_inotify.cpp ← inotify-based file watcher (watch mode)
│   │   │   └── xxHasher.cpp        ← xxHash3 for incremental build cache
│   │   └── process/
│   │       ├── ProcessPool.cpp     ← Subprocess spawning (clang, aapt2, etc.)
│   │       └── SignalHandler.cpp   ← SIGINT/SIGTERM handling
│   ├── toolchain/
│   │   ├── ndk/
│   │   │   └── NDKBridge.cpp       ← NDK path resolution, compiler flags, ELF scanner
│   │   └── sdk/
│   │       └── SDKBridge.cpp       ← SDK/aapt2/apksigner integration
│   └── depman/
│       └── resolver/
│           └── Resolver.cpp        ← External dependency resolution
│
├── include/abt/              ← Public headers
│   ├── common/
│   │   ├── Logger.h          ← ABT_INFO / ABT_WARN / ABT_ERR / ABT_TRACE macros
│   │   └── Types.h           ← BuildConfig, TargetArch, BuildType, Result, Task…
│   ├── engine/
│   │   ├── BuildGraph.h
│   │   ├── ConfigParser.h
│   │   ├── IWatcher.h
│   │   ├── ProcessPool.h
│   │   ├── TaskScheduler.h
│   │   └── xxHasher.h
│   └── toolchain/
│       ├── NDKBridge.h
│       └── DepManager.h
│
├── sdk/
│   ├── AbtProject.cmake      ← Template / reference for AbtProject.cmake files
│   └── ToolchainAndroid.cmake← CMake toolchain for cross-compiling (optional)
│
├── runtime/
│   └── tiny_dex/
│       ├── classes.dex            ← Prebuilt minimal NativeActivity Dalvik bootstrap
│       └── generate_tiny_dex.py   ← Script that regenerates classes.dex
│
└── abt-tests/                ← Example projects (stress tests)
    ├── 01-simple-native/
    ├── 02-gl-triangle/
    └── 03-audio-sink/
```

---

## Example Projects (abt-tests)

The `abt-tests/` folder contains three projects that were used to stress-test ABT across different use cases. All three were successfully compiled to APKs by ABT. Study them to understand how a real project is structured.

### 01-simple-native

**Path:** `abt-tests/01-simple-native/`

A baseline NativeActivity app that tests the core build pipeline: parallel compilation, hash cache correctness, and deep dependency chains. It exercises the Android Looper and input handling APIs.

Dependency chain:
```
Types → Logger → Config → EventQueue → InputHandler
                        ↘ LifecycleManager → StateManager → main
Timer (standalone)
```

Libraries linked: `android`, `log`, `m`

```bash
cd abt-tests/01-simple-native
abt build --64    # ARM64 device / Termux
abt build --86    # x86_64 emulator
```

### 02-gl-triangle

**Path:** `abt-tests/02-gl-triangle/`

A fully working OpenGL ES 3.0 renderer. Tests EGL surface setup, shader compilation, vertex buffers, textures, a camera system, and cross-file linking of a deep graphics stack.

Dependency chain:
```
GLTypes → MathUtils → ShaderCompiler → ShaderProgram
                    ↘ VertexBuffer → Mesh
                    ↘ Texture         ↘ Renderer → EGLHelper → main
                    ↘ Camera         ↗
```

Libraries linked: `android`, `log`, `EGL`, `GLESv3`, `m`

```bash
cd abt-tests/02-gl-triangle
abt build --64
```

### 03-audio-sink

**Path:** `abt-tests/03-audio-sink/`

An AAudio-based audio engine with synthesis, mixing, effects, and WAV writing. Tests multi-file linking with no graphics at all, confirming ABT handles non-GL native apps correctly.

Dependency chain:
```
AudioTypes → RingBuffer  →  Mixer   → AudioEngine → main
           → Oscillator  ↗
           → LowPassFilter → Effects ↗
           → Compressor   ↗
           → WaveWriter  → AudioEngine
```

Libraries linked: `aaudio`, `android`, `log`, `m`

```bash
cd abt-tests/03-audio-sink
abt build --64
```

Each test project follows the same layout your own projects should use:

```
my-project/
├── AbtProject.cmake        ← Required: ABT project configuration
├── AndroidManifest.xml     ← Optional: ABT generates one if missing
├── src/                    ← C/C++ source files
├── include/                ← Header files
└── res/                    ← Android resources
    ├── mipmap-hdpi/
    ├── mipmap-mdpi/
    ├── mipmap-xhdpi/
    ├── mipmap-xxhdpi/
    ├── mipmap-xxxhdpi/
    └── values/
        └── strings.xml
```

---

## Environment Variables

| Variable | Description |
|---|---|
| `ANDROID_NDK_HOME` | Override the NDK root path |
| `ANDROID_NDK` | Alternative NDK root path (checked second) |
| `NDK_ROOT` | Alternative NDK root path (checked third) |
| `ANDROID_SDK_ROOT` | Override the SDK root path |

If none of these are set, ABT falls back to `$HOME/assets/android_NDK` and `$HOME/assets/android-sdk`.

---

## Troubleshooting

**`NDK not found` error**
Verify the NDK exists at `$HOME/assets/android_NDK` or set `ANDROID_NDK_HOME` to the correct path. Run `abt info` to see exactly which path ABT resolved.

**`aapt2` fails on Termux / ARM64**
The SDK's `aapt2` binary is x86_64 only. Install the Termux-native version: `pkg install aapt2`. Always use `--64` when running on an ARM device.

**`keytool: command not found`**
ABT needs `keytool` to auto-generate the debug keystore. On Termux: `pkg install openjdk-21` or install any JDK package that provides `keytool`.

**`adb install` fails after `abt install`**
Make sure USB debugging is enabled on your target device and `adb` is installed (`pkg install android-tools` on Termux).

**Compile errors from `android_native_app_glue.c`**
ABT automatically compiles `.c` files with `clang` (not `clang++`) to avoid C++-incompatible idioms. If you see issues, make sure you are referencing the glue source via `${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c` in `abt_add_sources()`.

**Build is not picking up my changes**
Run `abt clean` then `abt build` to force a full rebuild and clear the hash cache.
