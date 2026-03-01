#pragma once

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <string>

namespace abt {

enum class LogLevel { TRACE, INFO, WARN, ERR, NONE };

class Logger {
public:
    static LogLevel level;

    static const char* levelStr(LogLevel l) {
        switch (l) {
            case LogLevel::TRACE: return "\033[90mTRACE\033[0m";
            case LogLevel::INFO:  return "\033[32m INFO\033[0m";
            case LogLevel::WARN:  return "\033[33m WARN\033[0m";
            case LogLevel::ERR:   return "\033[31m  ERR\033[0m";
            default:              return "     ";
        }
    }

    // Plain string log — no format parsing, no warning
    static void logStr(LogLevel l, const char* msg) {
        if (l < level) return;
        time_t t = time(nullptr);
        char tbuf[9];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
        fprintf(stderr, "[%s][%s] %s\n", tbuf, levelStr(l), msg);
    }

    // va_list variant — used internally
    static void logFmtV(LogLevel l, const char* fmt, va_list ap);

    // printf-style — compiler checks format string via __attribute__
    static void logFmt(LogLevel l, const char* fmt, ...)
        __attribute__((format(printf, 2, 3)));
};

inline void Logger::logFmtV(LogLevel l, const char* fmt, va_list ap) {
    if (l < level) return;
    time_t t = time(nullptr);
    char tbuf[9];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s][%s] ", tbuf, levelStr(l));
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

inline void Logger::logFmt(LogLevel l, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logFmtV(l, fmt, ap);
    va_end(ap);
}

} // namespace abt

// Route no-arg calls to logStr (avoids -Wformat-security on bare literals).
// Route calls with args to logFmt (printf-style, compiler-checked).
#define _ABT_LOG_IMPL(lvl, fmt, ...) \
    do { abt::Logger::logFmt(lvl, fmt, ##__VA_ARGS__); } while(0)

#define ABT_TRACE(fmt, ...) _ABT_LOG_IMPL(abt::LogLevel::TRACE, fmt, ##__VA_ARGS__)
#define ABT_INFO(fmt, ...)  _ABT_LOG_IMPL(abt::LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define ABT_WARN(fmt, ...)  _ABT_LOG_IMPL(abt::LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define ABT_ERR(fmt, ...)   _ABT_LOG_IMPL(abt::LogLevel::ERR,   fmt, ##__VA_ARGS__)

#define ABT_CHECK(result, ...) \
    do { \
        auto _r = (result); \
        if (!_r.ok()) { \
            ABT_ERR("Check failed [%s]: %s", #result, _r.msg.c_str()); \
            return _r; \
        } \
    } while(0)
