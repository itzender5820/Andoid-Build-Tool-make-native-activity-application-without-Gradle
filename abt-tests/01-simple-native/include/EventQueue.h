#pragma once
#include "Types.h"
#include "Logger.h"
#include <deque>
#include <mutex>
#include <functional>

namespace sn {

// ── EventQueue ────────────────────────────────────────────────────────────────
// Thread-safe MPSC queue for app-level events (input, lifecycle, window).
// Producers (ALooper callbacks) push; the main render loop drains.
class EventQueue {
public:
    static constexpr size_t MAX_QUEUED = 256;

    explicit EventQueue(const Logger& log);

    // Thread-safe push — drops if queue full (with a warning).
    void push(Event e);

    // Drain all pending events, calling handler for each.
    // Must be called from the main thread only.
    void drain(const std::function<void(const Event&)>& handler);

    // Drain, returning events instead of calling a handler.
    std::vector<Event> drainToVec();

    // Peek at queue depth (approx., non-locking for metrics).
    size_t size() const { return queue_.size(); }

    // Inject a synthetic event (test helper).
    void inject(EventType type, Vec2f pos = {});

    // Clear all pending events.
    void clear();

private:
    const Logger&    log_;
    std::deque<Event> queue_;
    mutable std::mutex mtx_;

    static Event makeTouch(EventType type, Vec2f pos);
};

} // namespace sn
