#include "LifecycleManager.h"

namespace sn {

LifecycleManager::LifecycleManager(const Config& cfg, EventQueue& queue,
                                    const Logger& log)
    : cfg_(cfg), queue_(queue), log_(log) {}

void LifecycleManager::setCallbacks(Callbacks cb) { cb_ = std::move(cb); }

void LifecycleManager::transition(AppState to) {
    AppState from = state_;
    log_.logState(from, to);
    state_ = to;
    if (cb_.onStateChanged) cb_.onStateChanged(from, to);
}

void LifecycleManager::emitEvent(EventType type) {
    Event e;
    e.type = type;
    queue_.push(e);
}

void LifecycleManager::onStart()   { transition(AppState::STARTED);  }
void LifecycleManager::onResume()  { transition(AppState::RESUMED);   }
void LifecycleManager::onPause()   { transition(AppState::PAUSED);    }
void LifecycleManager::onStop()    { transition(AppState::STOPPED);   }
void LifecycleManager::onDestroy() { transition(AppState::DESTROYED); }

void LifecycleManager::onWindowInit(ANativeWindow* win) {
    window_ = win;
    width_  = ANativeWindow_getWidth(win);
    height_ = ANativeWindow_getHeight(win);
    log_.info("Window ready: %d x %d", width_, height_);
    if (cb_.onWindowCreated) cb_.onWindowCreated(win);

    Event e;
    e.type         = EventType::WINDOW_RESIZE;
    e.window.width = width_;
    e.window.height = height_;
    queue_.push(e);
}

void LifecycleManager::onWindowTerm() {
    log_.info("Window lost");
    window_ = nullptr;
    if (cb_.onWindowDestroyed) cb_.onWindowDestroyed();
}

void LifecycleManager::onWindowResized(int w, int h) {
    width_  = w;
    height_ = h;
    log_.info("Window resized: %d x %d", w, h);
    if (cb_.onWindowResized) cb_.onWindowResized(w, h);
    Event e;
    e.type          = EventType::WINDOW_RESIZE;
    e.window.width  = w;
    e.window.height = h;
    queue_.push(e);
}

void LifecycleManager::onFocusGained() {
    hasFocus_ = true;
    log_.info("Focus gained");
    if (cb_.onFocusChanged) cb_.onFocusChanged(true);
    emitEvent(EventType::FOCUS_GAINED);
}

void LifecycleManager::onFocusLost() {
    hasFocus_ = false;
    log_.info("Focus lost");
    if (cb_.onFocusChanged) cb_.onFocusChanged(false);
    emitEvent(EventType::FOCUS_LOST);
}

} // namespace sn
