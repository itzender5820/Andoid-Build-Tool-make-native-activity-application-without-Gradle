#pragma once
#include "EventQueue.h"
#include "Logger.h"
#include <android/input.h>

namespace sn {

// ── InputHandler ──────────────────────────────────────────────────────────────
// Translates raw AInputEvent* from android_native_app_glue into sn::Events
// and enqueues them. Handles multi-touch tracking and tap detection.
class InputHandler {
public:
    static constexpr int MAX_POINTERS  = 10;
    static constexpr float TAP_SLOP_PX = 8.f;
    static constexpr i64  TAP_MAX_NS   = 200'000'000LL; // 200 ms

    explicit InputHandler(EventQueue& queue, const Logger& log);

    // Feed an AInputEvent from the glue callback.
    // Returns true if the event was consumed.
    bool processEvent(AInputEvent* event);

    // Call every frame to generate TOUCH_MOVE events for held pointers.
    void tick();

    // Stats
    u32 touchDownCount()  const { return stats_.touchDown;  }
    u32 touchUpCount()    const { return stats_.touchUp;    }
    u32 tapCount()        const { return stats_.taps;       }
    u32 keyDownCount()    const { return stats_.keyDown;    }

private:
    struct PointerState {
        bool  active   = false;
        i32   id       = -1;
        Vec2f downPos;
        Vec2f lastPos;
        i64   downTimeNs = 0;
    };

    struct Stats {
        u32 touchDown = 0, touchUp = 0, taps = 0, keyDown = 0;
    };

    bool handleMotion(AInputEvent* ev);
    bool handleKey   (AInputEvent* ev);
    bool isTap(const PointerState& ps, Vec2f upPos, i64 upTimeNs) const;

    EventQueue&   queue_;
    const Logger& log_;
    PointerState  pointers_[MAX_POINTERS] {};
    Stats         stats_;
};

} // namespace sn
