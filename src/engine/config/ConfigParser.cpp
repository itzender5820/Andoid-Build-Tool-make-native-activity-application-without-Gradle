#include <cstdlib>   // getenv
#include <abt/engine/ConfigParser.h>
#include <abt/common/Logger.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace abt {

ConfigParser::ConfigParser(const std::string& projectRoot)
    : projectRoot_(projectRoot) {
    // Seed built-in variables
    vars_["CMAKE_CURRENT_SOURCE_DIR"] = projectRoot_;
    vars_["CMAKE_SOURCE_DIR"]         = projectRoot_;

    // Inject ANDROID_NDK so AbtProject.cmake can reference NDK sources.
    // Mirror the same resolution order as NDKBridge::probe():
    //   1. ANDROID_NDK_HOME env var
    //   2. ANDROID_NDK env var
    //   3. NDK_ROOT env var
    //   4. Termux default path
    auto resolveNdk = []() -> std::string {
        for (const char* var : {"ANDROID_NDK_HOME", "ANDROID_NDK", "NDK_ROOT"}) {
            const char* v = getenv(var);
            if (v && v[0]) return v;
        }
        // Use $HOME so this works in Termux, Linux, any environment
        const char* home = getenv("HOME");
        return std::string(home ? home : ".") + "/asserts/android_NDK";
    };
    vars_["ANDROID_NDK"] = resolveNdk();
}

// ── Tokenizer ─────────────────────────────────────────────────────────────────
// Parses: DIRECTIVE(arg1 arg2 ...) — handles quoted strings and comments
static bool parseLine(const std::string& line,
                      std::string& name,
                      std::vector<std::string>& args)
{
    size_t parenOpen = line.find('(');
    if (parenOpen == std::string::npos) return false;
    size_t parenClose = line.rfind(')');
    if (parenClose == std::string::npos) return false;

    name = line.substr(0, parenOpen);
    // Strip leading/trailing whitespace from name
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);
    if (name.empty()) return false;

    std::string inner = line.substr(parenOpen + 1, parenClose - parenOpen - 1);
    args.clear();

    // Split on whitespace, respecting double-quoted strings
    std::istringstream ss(inner);
    std::string tok;
    bool inQuote = false;
    std::string cur;

    for (char c : inner) {
        if (c == '"') {
            inQuote = !inQuote;
        } else if ((c == ' ' || c == '\t' || c == '\n') && !inQuote) {
            if (!cur.empty()) { args.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) args.push_back(cur);

    return true;
}

std::string ConfigParser::expandVars(const std::string& s) const {
    std::string result = s;
    static const std::regex varRe(R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})");
    std::smatch m;
    std::string out;
    auto it = result.cbegin();
    while (std::regex_search(it, result.cend(), m, varRe)) {
        out.append(it, m.prefix().second);
        auto vit = vars_.find(m[1].str());
        out += (vit != vars_.end()) ? vit->second : m[0].str();
        it = m.suffix().first;
    }
    out.append(it, result.cend());
    return out;
}

Result ConfigParser::parse(BuildConfig& config) {
    std::string cmakePath = projectRoot_ + "/AbtProject.cmake";
    std::ifstream f(cmakePath);
    if (!f.is_open())
        return Result::fail(AbtError::PARSE_FAILED,
            "Cannot open: " + cmakePath);

    config.projectRoot = projectRoot_;
    if (config.outputDir.empty())
        config.outputDir = projectRoot_ + "/build";

    // Multi-line accumulator: some calls span multiple lines
    std::string accumLine;
    int parenDepth = 0;
    std::string rawLine;

    while (std::getline(f, rawLine)) {
        // Strip comments
        size_t cpos = rawLine.find('#');
        if (cpos != std::string::npos) rawLine.resize(cpos);

        for (char c : rawLine) {
            if (c == '(') parenDepth++;
            if (c == ')') parenDepth--;
        }
        accumLine += ' ' + rawLine;

        if (parenDepth <= 0 && !accumLine.empty()) {
            std::string name;
            std::vector<std::string> args;
            if (parseLine(accumLine, name, args)) {
                // Expand vars in args
                for (auto& a : args) a = expandVars(a);
                auto r = handleDirective(name, args, config);
                if (!r.ok()) {
                    ABT_WARN("ConfigParser: %s", r.msg.c_str());
                }
            }
            accumLine.clear();
            parenDepth = 0;
        }
    }
    return Result::success();
}

Result ConfigParser::handleDirective(const std::string& name,
                                      const std::vector<std::string>& args,
                                      BuildConfig& config)
{
    // set(VAR value)
    if (name == "set" && args.size() >= 2) {
        vars_[args[0]] = args[1];
        return Result::success();
    }

    // abt_project(MyApp)
    if (name == "abt_project" && !args.empty()) {
        config.projectName = args[0];
        ABT_INFO("Project: %s", config.projectName.c_str());
        return Result::success();
    }

    // abt_set_package(com.example.app)
    if (name == "abt_set_package" && !args.empty()) {
        config.packageName = args[0];
        return Result::success();
    }

    // abt_set_min_sdk(24)
    if (name == "abt_set_min_sdk" && !args.empty()) {
        config.minSdkVersion = std::stoi(args[0]);
        return Result::success();
    }

    // abt_set_target_sdk(34)
    if (name == "abt_set_target_sdk" && !args.empty()) {
        config.targetSdkVersion = std::stoi(args[0]);
        return Result::success();
    }

    // abt_add_sources(src/main.cpp src/renderer.cpp ...)
    if (name == "abt_add_sources") {
        for (const auto& s : args) {
            // Make relative paths absolute
            std::string path = (s[0] == '/') ? s : config.projectRoot + "/" + s;
            config.cppSources.push_back(path);
        }
        ABT_INFO("Sources: %zu files", config.cppSources.size());
        return Result::success();
    }

    // abt_add_includes(include/ third_party/some/include)
    if (name == "abt_add_includes") {
        for (const auto& d : args) {
            std::string path = (d[0] == '/') ? d : config.projectRoot + "/" + d;
            config.includeDirs.push_back(path);
        }
        return Result::success();
    }

    // abt_link_libraries(log android EGL GLESv3)
    if (name == "abt_link_libraries") {
        for (const auto& lib : args) {
            config.linkLibraries.push_back(lib);
        }
        return Result::success();
    }

    // abt_dependency(com.example:nativelib:1.0.0)
    if (name == "abt_dependency" && !args.empty()) {
        config.dependencies.push_back(args[0]);
        return Result::success();
    }

    // abt_architectures(arm64-v8a x86_64)
    if (name == "abt_architectures") {
        config.architectures.clear();
        for (const auto& a : args) {
            if      (a == "arm64-v8a")  config.architectures.push_back(TargetArch::ARM64);
            else if (a == "x86_64")     config.architectures.push_back(TargetArch::X86_64);
            else if (a == "armeabi-v7a")config.architectures.push_back(TargetArch::ARM32);
            else if (a == "x86")        config.architectures.push_back(TargetArch::X86);
            else ABT_WARN("Unknown arch: %s", a.c_str());
        }
        return Result::success();
    }

    // abt_build_type(debug | release)
    if (name == "abt_build_type" && !args.empty()) {
        config.buildType = (args[0] == "release")
            ? BuildType::RELEASE : BuildType::DEBUG;
        return Result::success();
    }

    // abt_version(name code)
    if (name == "abt_version" && args.size() >= 2) {
        config.versionName = args[0];
        config.versionCode = std::stoi(args[1]);
        return Result::success();
    }

    // abt_output_dir(path)
    if (name == "abt_output_dir" && !args.empty()) {
        config.outputDir = args[0];
        return Result::success();
    }

    // Unknown directive — not an error, just ignore
    ABT_TRACE("ConfigParser: ignoring directive '%s'", name.c_str());
    return Result::success();
}

} // namespace abt
