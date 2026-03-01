#include "Timer.h"
#include "Logger.h"
#include <ctime>
#include <thread>
#include <chrono>

namespace sn {

static i64 getMonoNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64)ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
}

Timer::Timer() { reset(); }

void Timer::reset() {
    epochNs_ = tickNs_ = frameNs_ = getMonoNs();
}

double Timer::elapsedMs() const {
    return (getMonoNs() - epochNs_) * 1e-6;
}

double Timer::deltaMs() {
    i64 now   = getMonoNs();
    double ms = (now - tickNs_) * 1e-6;
    tickNs_   = now;
    return ms;
}

i64 Timer::nowNs() { return getMonoNs(); }

double Timer::endFrame(int targetFps) {
    i64 now      = getMonoNs();
    double elapsed = (now - frameNs_) * 1e-6;
    if (targetFps > 0) {
        double targetMs = 1000.0 / targetFps;
        double sleepMs  = targetMs - elapsed;
        if (sleepMs > 0.5) {
            std::this_thread::sleep_for(
                std::chrono::microseconds((i64)(sleepMs * 1000)));
        }
    }
    i64 after   = getMonoNs();
    double frame = (after - frameNs_) * 1e-6;
    frameNs_    = after;
    return frame;
}

// ── ScopedTimer ───────────────────────────────────────────────────────────────
ScopedTimer::ScopedTimer(const char* label)
    : label_(label), startNs_(getMonoNs()) {}

ScopedTimer::~ScopedTimer() {
    double ms = (getMonoNs() - startNs_) * 1e-6;
    SNLOGD("[ScopedTimer] %s: %.3f ms", label_, ms);
}

} // namespace sn
