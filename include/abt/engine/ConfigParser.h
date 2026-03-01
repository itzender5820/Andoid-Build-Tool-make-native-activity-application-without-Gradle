#pragma once

#include <abt/common/Types.h>
#include <string>

namespace abt {

// ── Parse AbtProject.cmake into a BuildConfig ────────────────────────────────
// We do NOT invoke CMake — we parse the file directly using a minimal
// CMake-syntax reader that understands:
//   set(VAR value ...)
//   abt_project(name ...)
//   abt_add_sources(...)
//   abt_add_includes(...)
//   abt_set_package(...)
//   abt_link_libraries(...)
//   abt_dependency(...)
class ConfigParser {
public:
    explicit ConfigParser(const std::string& projectRoot);

    // Parse AbtProject.cmake and fill config
    Result parse(BuildConfig& config);

private:
    std::string projectRoot_;

    // Handle a single parsed CMake-style directive
    Result handleDirective(
        const std::string& name,
        const std::vector<std::string>& args,
        BuildConfig& config
    );

    // Expand ${VAR} references from a simple variable table
    std::string expandVars(const std::string& s) const;

    std::unordered_map<std::string, std::string> vars_;
};

} // namespace abt
