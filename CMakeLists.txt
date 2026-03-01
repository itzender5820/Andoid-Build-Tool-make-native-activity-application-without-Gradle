cmake_minimum_required(VERSION 3.22)
project(abt VERSION 1.0.0 LANGUAGES CXX)

# ── Compiler settings ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ── Options ────────────────────────────────────────────────────────────────────
option(ABT_USE_CURL     "Enable libcurl for dependency fetching" ON)
option(ABT_USE_XXHASH   "Use vendored xxHash library"           ON)

# ── Include paths ──────────────────────────────────────────────────────────────
set(ABT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Source files ───────────────────────────────────────────────────────────────
set(ABT_SOURCES
    # Engine – Core
    src/engine/core/BuildGraph.cpp
    src/engine/core/TaskScheduler.cpp

    # Engine – Process
    src/engine/process/ProcessPool.cpp
    src/engine/process/SignalHandler.cpp

    # Engine – Filesystem
    src/engine/fs/xxHasher.cpp
    src/engine/fs/Watcher_inotify.cpp

    # Engine – Config
    src/engine/config/ConfigParser.cpp

    # Toolchain
    src/toolchain/ndk/NDKBridge.cpp
    src/toolchain/sdk/SDKBridge.cpp

    # Dependency Manager
    src/depman/resolver/Resolver.cpp

    # Entry point
    src/main.cpp
)

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(abt ${ABT_SOURCES})

target_include_directories(abt PRIVATE ${ABT_INCLUDE_DIRS})

# ── Threading ─────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(abt PRIVATE Threads::Threads)

# ── libcurl (optional) ────────────────────────────────────────────────────────
if(ABT_USE_CURL)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(abt PRIVATE CURL::libcurl)
        message(STATUS "libcurl: ${CURL_VERSION_STRING}")
    else()
        # Try pkg-config (Termux)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(CURL libcurl)
            if(CURL_FOUND)
                target_include_directories(abt PRIVATE ${CURL_INCLUDE_DIRS})
                target_link_libraries(abt PRIVATE ${CURL_LIBRARIES})
                message(STATUS "libcurl (pkg-config): found")
            else()
                message(WARNING "libcurl not found — dep fetching disabled. "
                                "Install with: pkg install libcurl")
                target_compile_definitions(abt PRIVATE ABT_NO_CURL)
            endif()
        else()
            target_compile_definitions(abt PRIVATE ABT_NO_CURL)
        endif()
    endif()
else()
    target_compile_definitions(abt PRIVATE ABT_NO_CURL)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(abt PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(abt PRIVATE -O3 -flto)
    target_link_options(abt PRIVATE -flto)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS abt DESTINATION bin)
install(DIRECTORY sdk/        DESTINATION share/abt/sdk)
install(DIRECTORY runtime/    DESTINATION share/abt/runtime)

# ── Output binary to bin/ ─────────────────────────────────────────────────────
set_target_properties(abt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

message(STATUS "──────────────────────────────────────")
message(STATUS "  abt Build Configuration")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler   : ${CMAKE_CXX_COMPILER}")
message(STATUS "  libcurl    : ${ABT_USE_CURL}")
message(STATUS "──────────────────────────────────────")
