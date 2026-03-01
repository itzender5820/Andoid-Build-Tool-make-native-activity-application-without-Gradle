#include "StateManager.h"
#include <sstream>
#include <iomanip>

namespace sn {

// ── FrameStats ────────────────────────────────────────────────────────────────
void FrameStats::record(double frameMs) {
    frameCount++;
    totalMs    += frameMs;
    if (frameMs < minFrameMs) minFrameMs = frameMs;
    if (frameMs > maxFrameMs) maxFrameMs = frameMs;
    avgFps = frameCount / (totalMs * 1e-3);
}

void FrameStats::reset() { *this = {}; }

std::string FrameStats::summary() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1)
       << "frames=" << frameCount
       << " avg_fps=" << avgFps
       << " min=" << minFrameMs << "ms"
       << " max=" << maxFrameMs << "ms";
    return ss.str();
}

// ── StateManager ──────────────────────────────────────────────────────────────
StateManager::StateManager(LifecycleManager& lifecycle,
                            EventQueue& queue,
                            const Logger& log,
                            int targetFps)
    : lifecycle_(lifecycle), queue_(queue), log_(log), targetFps_(targetFps) {}

void StateManager::start() {
    running_ = true;
    timer_.reset();
    log_.info("StateManager started (targetFps=%d)", targetFps_);
}

void StateManager::requestStop() {
    stopRequested_.store(true);
    log_.info("StateManager: stop requested");
}

void StateManager::tick() {
    if (!running_) return;

    // Drain all pending events
    queue_.drain([this](const Event& e){ handleEvent(e); });

    // Frame timing
    double frameMs = timer_.endFrame(targetFps_);
    frameStats_.record(frameMs);

    // Periodic stats log every 300 frames
    if (frameStats_.frameCount % 300 == 0)
        log_.info("Perf: %s", frameStats_.summary().c_str());
}

void StateManager::handleEvent(const Event& e) {
    switch (e.type) {
        case EventType::TOUCH_DOWN:   onTouchDown(e);  break;
        case EventType::TOUCH_UP:     onTouchUp(e);    break;
        case EventType::BACK_PRESSED: onBackPressed();  break;
        case EventType::FOCUS_GAINED:
            log_.debug("Event: focus gained"); break;
        case EventType::FOCUS_LOST:
            log_.debug("Event: focus lost");   break;
        case EventType::WINDOW_RESIZE:
            log_.info("Event: window resize %dx%d",
                       e.window.width, e.window.height); break;
        default: break;
    }
}

void StateManager::onTouchDown(const Event& e) {
    log_.debug("Touch down (%.0f, %.0f) ptr=%d",
               e.touch.pos.x, e.touch.pos.y, e.touch.pointerId);
}

void StateManager::onTouchUp(const Event& e) {
    lastTapPos_ = e.touch.pos;
    tapCount_++;
    log_.info("Tap #%u at (%.0f, %.0f)", tapCount_, e.touch.pos.x, e.touch.pos.y);
}

void StateManager::onBackPressed() {
    log_.info("Back pressed → requesting stop");
    requestStop();
}

} // namespace sn
