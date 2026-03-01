#pragma once
#include "LifecycleManager.h"
#include "Timer.h"
#include "Logger.h"
#include <atomic>
#include <string>

namespace sn {

// ── FrameStats ────────────────────────────────────────────────────────────────
struct FrameStats {
    u32    frameCount   = 0;
    double totalMs      = 0.0;
    double minFrameMs   = 1e9;
    double maxFrameMs   = 0.0;
    double avgFps       = 0.0;

    void record(double frameMs);
    void reset();
    std::string summary() const;
};

// ── StateManager ──────────────────────────────────────────────────────────────
// Top-level coordinator: drives the main loop, routes events from the
// EventQueue to the appropriate subsystems, and tracks frame timing.
class StateManager {
public:
    explicit StateManager(LifecycleManager& lifecycle,
                          EventQueue& queue,
                          const Logger& log,
                          int targetFps = 60);

    // Called from android_main's event loop.
    void tick();

    // Call once after all subsystems are ready.
    void start();

    // Request a clean shutdown.
    void requestStop();

    bool shouldStop()   const { return stopRequested_.load(); }
    bool isRunning()    const { return running_; }
    const FrameStats& frameStats() const { return frameStats_; }

private:
    void handleEvent(const Event& e);
    void onTouchDown(const Event& e);
    void onTouchUp  (const Event& e);
    void onBackPressed();

    LifecycleManager& lifecycle_;
    EventQueue&       queue_;
    const Logger&     log_;
    Timer             timer_;
    FrameStats        frameStats_;
    int               targetFps_;
    bool              running_    = false;
    std::atomic<bool> stopRequested_ { false };

    // Simple gesture: track last tap position for logging
    Vec2f lastTapPos_ {};
    u32   tapCount_   = 0;
};

} // namespace sn
