#pragma once
#include "Types.h"
#include <time.h>

namespace sn {

// ── Timer ─────────────────────────────────────────────────────────────────────
// Monotonic clock helpers for frame timing and profiling.
class Timer {
public:
    Timer();

    // Reset the epoch; deltaMs() returns 0 after this.
    void     reset();

    // Time since last reset() or construction, in milliseconds.
    double   elapsedMs() const;

    // Time since last tick() call, in milliseconds; also updates the cursor.
    double   deltaMs();

    // Current monotonic time in nanoseconds (static util).
    static i64 nowNs();

    // Frame-rate governor: sleep enough to maintain targetFps.
    // Returns actual elapsed ms for this frame.
    double   endFrame(int targetFps);

private:
    i64 epochNs_  = 0;
    i64 tickNs_   = 0;
    i64 frameNs_  = 0;
};

// ── ScopedTimer ───────────────────────────────────────────────────────────────
// RAII helper that logs elapsed time on destruction.
class ScopedTimer {
public:
    explicit ScopedTimer(const char* label);
    ~ScopedTimer();
private:
    const char* label_;
    i64         startNs_;
};

} // namespace sn
