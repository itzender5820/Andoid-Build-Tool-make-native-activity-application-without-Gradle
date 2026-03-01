#pragma once
#include "Types.h"
#include <android/log.h>

// ── Log macros ────────────────────────────────────────────────────────────────
#define SN_TAG  "sn"
#define SNLOGI(...) __android_log_print(ANDROID_LOG_INFO,  SN_TAG, __VA_ARGS__)
#define SNLOGW(...) __android_log_print(ANDROID_LOG_WARN,  SN_TAG, __VA_ARGS__)
#define SNLOGE(...) __android_log_print(ANDROID_LOG_ERROR, SN_TAG, __VA_ARGS__)
#define SNLOGD(...) __android_log_print(ANDROID_LOG_DEBUG, SN_TAG, __VA_ARGS__)

namespace sn {

// ── Logger ────────────────────────────────────────────────────────────────────
// Wraps android logging with optional rate-limiting and tag namespacing.
class Logger {
public:
    explicit Logger(const char* tag);

    void info (const char* fmt, ...) const __attribute__((format(printf,2,3)));
    void warn (const char* fmt, ...) const __attribute__((format(printf,2,3)));
    void error(const char* fmt, ...) const __attribute__((format(printf,2,3)));
    void debug(const char* fmt, ...) const __attribute__((format(printf,2,3)));

    // Emit a one-line state-change summary
    void logState(AppState from, AppState to) const;

    static const char* stateName(AppState s);

private:
    char tag_[64];
};

} // namespace sn
