#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace sn {

// ── Fundamental types ─────────────────────────────────────────────────────────
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

// ── 2D point / size ───────────────────────────────────────────────────────────
struct Vec2i { i32 x = 0, y = 0; };
struct Vec2f { f32 x = 0.f, y = 0.f; };
struct Rect  { i32 x = 0, y = 0, w = 0, h = 0; };

// ── Application states ────────────────────────────────────────────────────────
enum class AppState : u8 {
    CREATED,
    STARTED,
    RESUMED,
    PAUSED,
    STOPPED,
    DESTROYED,
};

// ── Event types ───────────────────────────────────────────────────────────────
enum class EventType : u8 {
    NONE,
    TOUCH_DOWN,
    TOUCH_UP,
    TOUCH_MOVE,
    KEY_DOWN,
    KEY_UP,
    WINDOW_RESIZE,
    FOCUS_GAINED,
    FOCUS_LOST,
    BACK_PRESSED,
};

struct TouchEvent {
    i32 pointerId = 0;
    Vec2f pos;
    f32   pressure = 1.f;
};

struct KeyEvent {
    i32 keyCode  = 0;
    i32 scanCode = 0;
    u32 metaState = 0;
};

struct WindowEvent {
    i32 width  = 0;
    i32 height = 0;
};

struct Event {
    EventType type = EventType::NONE;
    union {
        TouchEvent  touch;
        KeyEvent    key;
        WindowEvent window;
    };
    i64 timestampNs = 0;

    Event() : type(EventType::NONE), touch{} {}
};

// ── Result ────────────────────────────────────────────────────────────────────
struct Result {
    bool        ok  = true;
    std::string msg;

    static Result success()                    { return {true,  ""}; }
    static Result fail(std::string m)          { return {false, std::move(m)}; }
    explicit operator bool() const             { return ok; }
};

} // namespace sn
