# AbtProject.cmake
# Include this in your project root to configure an abt build.
#
# Example:
#   abt_project(MyGame)
#   abt_set_package(com.example.mygame)
#   abt_set_min_sdk(24)
#   abt_set_target_sdk(34)
#   abt_architectures(arm64-v8a x86_64)
#   abt_build_type(debug)
#   abt_version(1.0 1)
#   abt_add_sources(
#       src/main.cpp
#       src/renderer.cpp
#       src/audio.cpp
#   )
#   abt_add_includes(
#       include/
#       third_party/glm
#   )
#   abt_link_libraries(EGL GLESv3 OpenSLES android log)
#   abt_dependency(com.google.oboe:oboe:1.7.0)

# ─────────────────────────────────────────────────────────────────────────────
# NOTE: This file is parsed by abt's ConfigParser, NOT by CMake itself.
# Syntax supported:
#   directive(arg1 arg2 ...)
#   set(VAR value)
#   # comments
#   ${VAR} expansion
# ─────────────────────────────────────────────────────────────────────────────
