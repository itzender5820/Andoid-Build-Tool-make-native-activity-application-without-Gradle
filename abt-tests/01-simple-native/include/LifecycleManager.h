#pragma once
#include "Logger.h"
#include "Config.h"
#include "EventQueue.h"
#include <functional>
#include <vector>
#include <android/native_window.h>

namespace sn {

// ── LifecycleManager ──────────────────────────────────────────────────────────
// Tracks Android activity lifecycle transitions, window acquisition/loss,
// and focus changes. Fires registered callbacks on each transition.
class LifecycleManager {
public:
    struct Callbacks {
        std::function<void(ANativeWindow*)> onWindowCreated;
        std::function<void()>               onWindowDestroyed;
        std::function<void(int,int)>         onWindowResized; // w,h
        std::function<void(AppState,AppState)> onStateChanged;
        std::function<void(bool)>            onFocusChanged;
    };

    explicit LifecycleManager(const Config& cfg, EventQueue& queue,
                               const Logger& log);

    void setCallbacks(Callbacks cb);

    // Called from android_native_app_glue APP_CMD_* handlers:
    void onStart();
    void onResume();
    void onPause();
    void onStop();
    void onDestroy();
    void onWindowInit(ANativeWindow* win);
    void onWindowTerm();
    void onWindowResized(int w, int h);
    void onFocusGained();
    void onFocusLost();

    AppState currentState() const { return state_; }
    bool     hasWindow()    const { return window_ != nullptr; }
    bool     hasFocus()     const { return hasFocus_; }
    ANativeWindow* window() const { return window_; }
    int width()  const { return width_;  }
    int height() const { return height_; }

private:
    void transition(AppState to);
    void emitEvent(EventType type);

    const Config& cfg_;
    EventQueue&   queue_;
    const Logger& log_;
    Callbacks     cb_;
    AppState      state_  = AppState::CREATED;
    ANativeWindow* window_ = nullptr;
    bool           hasFocus_ = false;
    int            width_  = 0;
    int            height_ = 0;
};

} // namespace sn
