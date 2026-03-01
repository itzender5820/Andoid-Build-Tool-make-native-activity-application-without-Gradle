#include "EventQueue.h"
#include <time.h>

namespace sn {

static i64 nowNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64)ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
}

EventQueue::EventQueue(const Logger& log) : log_(log) {}

void EventQueue::push(Event e) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (queue_.size() >= MAX_QUEUED) {
        log_.warn("EventQueue full, dropping event type=%d", (int)e.type);
        return;
    }
    e.timestampNs = nowNs();
    queue_.push_back(std::move(e));
}

void EventQueue::drain(const std::function<void(const Event&)>& handler) {
    std::deque<Event> local;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        local.swap(queue_);
    }
    for (auto& e : local) handler(e);
}

std::vector<Event> EventQueue::drainToVec() {
    std::vector<Event> out;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        out.reserve(queue_.size());
        for (auto& e : queue_) out.push_back(e);
        queue_.clear();
    }
    return out;
}

void EventQueue::inject(EventType type, Vec2f pos) {
    Event e;
    e.type = type;
    if (type == EventType::TOUCH_DOWN || type == EventType::TOUCH_MOVE ||
        type == EventType::TOUCH_UP) {
        e.touch.pos = pos;
    }
    push(e);
}

void EventQueue::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    queue_.clear();
}

Event EventQueue::makeTouch(EventType type, Vec2f pos) {
    Event e;
    e.type     = type;
    e.touch.pos = pos;
    return e;
}

} // namespace sn
