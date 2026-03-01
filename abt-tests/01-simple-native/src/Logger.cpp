#include "Logger.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>

namespace sn {

Logger::Logger(const char* tag) {
    snprintf(tag_, sizeof(tag_), "%s", tag);
}

void Logger::info(const char* fmt, ...) const {
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, tag_, fmt, ap);
    va_end(ap);
}
void Logger::warn(const char* fmt, ...) const {
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_WARN, tag_, fmt, ap);
    va_end(ap);
}
void Logger::error(const char* fmt, ...) const {
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, tag_, fmt, ap);
    va_end(ap);
}
void Logger::debug(const char* fmt, ...) const {
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_DEBUG, tag_, fmt, ap);
    va_end(ap);
}

const char* Logger::stateName(AppState s) {
    switch (s) {
        case AppState::CREATED:   return "CREATED";
        case AppState::STARTED:   return "STARTED";
        case AppState::RESUMED:   return "RESUMED";
        case AppState::PAUSED:    return "PAUSED";
        case AppState::STOPPED:   return "STOPPED";
        case AppState::DESTROYED: return "DESTROYED";
    }
    return "UNKNOWN";
}

void Logger::logState(AppState from, AppState to) const {
    info("State: %s → %s", stateName(from), stateName(to));
}

} // namespace sn
