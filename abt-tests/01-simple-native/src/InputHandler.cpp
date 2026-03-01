#include "InputHandler.h"
#include "Timer.h"
#include <cmath>

namespace sn {

InputHandler::InputHandler(EventQueue& queue, const Logger& log)
    : queue_(queue), log_(log) {}

bool InputHandler::processEvent(AInputEvent* ev) {
    switch (AInputEvent_getType(ev)) {
        case AINPUT_EVENT_TYPE_MOTION: return handleMotion(ev);
        case AINPUT_EVENT_TYPE_KEY:    return handleKey(ev);
        default: return false;
    }
}

bool InputHandler::handleMotion(AInputEvent* ev) {
    int action       = AMotionEvent_getAction(ev);
    int actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    int ptrIdx       = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                        >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    size_t count     = AMotionEvent_getPointerCount(ev);
    i64 eventTimeNs  = AMotionEvent_getEventTime(ev);

    auto findSlot = [&](i32 id) -> int {
        for (int i = 0; i < MAX_POINTERS; ++i)
            if (pointers_[i].active && pointers_[i].id == id) return i;
        return -1;
    };
    auto freeSlot = [&]() -> int {
        for (int i = 0; i < MAX_POINTERS; ++i)
            if (!pointers_[i].active) return i;
        return -1;
    };

    auto makeEvent = [&](EventType type, size_t pi) {
        Event e;
        e.type              = type;
        e.touch.pointerId   = AMotionEvent_getPointerId(ev, pi);
        e.touch.pos.x       = AMotionEvent_getX(ev, pi);
        e.touch.pos.y       = AMotionEvent_getY(ev, pi);
        e.touch.pressure    = AMotionEvent_getPressure(ev, pi);
        e.timestampNs       = eventTimeNs;
        return e;
    };

    if (actionMasked == AMOTION_EVENT_ACTION_DOWN ||
        actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        int slot = freeSlot();
        if (slot >= 0) {
            i32 id = AMotionEvent_getPointerId(ev, ptrIdx);
            PointerState& ps = pointers_[slot];
            ps.active      = true;
            ps.id          = id;
            ps.downPos     = {AMotionEvent_getX(ev, ptrIdx),
                               AMotionEvent_getY(ev, ptrIdx)};
            ps.lastPos     = ps.downPos;
            ps.downTimeNs  = eventTimeNs;
            queue_.push(makeEvent(EventType::TOUCH_DOWN, ptrIdx));
            stats_.touchDown++;
        }
    }
    else if (actionMasked == AMOTION_EVENT_ACTION_UP ||
             actionMasked == AMOTION_EVENT_ACTION_POINTER_UP) {
        i32  id   = AMotionEvent_getPointerId(ev, ptrIdx);
        int  slot = findSlot(id);
        if (slot >= 0) {
            Vec2f upPos = {AMotionEvent_getX(ev, ptrIdx),
                            AMotionEvent_getY(ev, ptrIdx)};
            if (isTap(pointers_[slot], upPos, eventTimeNs)) {
                Event tap;
                tap.type      = EventType::TOUCH_UP;
                tap.touch.pos = upPos;
                queue_.push(tap);
                stats_.taps++;
            }
            queue_.push(makeEvent(EventType::TOUCH_UP, ptrIdx));
            pointers_[slot] = {};
            stats_.touchUp++;
        }
    }
    else if (actionMasked == AMOTION_EVENT_ACTION_MOVE) {
        for (size_t pi = 0; pi < count; ++pi) {
            i32 id   = AMotionEvent_getPointerId(ev, pi);
            int slot = findSlot(id);
            if (slot >= 0) {
                Vec2f newPos = {AMotionEvent_getX(ev, pi), AMotionEvent_getY(ev, pi)};
                pointers_[slot].lastPos = newPos;
                queue_.push(makeEvent(EventType::TOUCH_MOVE, pi));
            }
        }
    }
    else if (actionMasked == AMOTION_EVENT_ACTION_CANCEL) {
        for (auto& ps : pointers_) ps = {};
    }
    return true;
}

bool InputHandler::handleKey(AInputEvent* ev) {
    int action  = AKeyEvent_getAction(ev);
    int keyCode = AKeyEvent_getKeyCode(ev);
    Event e;
    e.type          = (action == AKEY_EVENT_ACTION_DOWN)
                        ? EventType::KEY_DOWN : EventType::KEY_UP;
    e.key.keyCode   = keyCode;
    e.key.scanCode  = AKeyEvent_getScanCode(ev);
    e.key.metaState = AKeyEvent_getMetaState(ev);
    queue_.push(e);
    if (action == AKEY_EVENT_ACTION_DOWN) stats_.keyDown++;
    if (keyCode == AKEYCODE_BACK) {
        Event back;
        back.type = EventType::BACK_PRESSED;
        queue_.push(back);
    }
    return true;
}

bool InputHandler::isTap(const PointerState& ps, Vec2f upPos, i64 upTimeNs) const {
    float dx = upPos.x - ps.downPos.x;
    float dy = upPos.y - ps.downPos.y;
    float dist = sqrtf(dx*dx + dy*dy);
    i64   dur  = upTimeNs - ps.downTimeNs;
    return dist < TAP_SLOP_PX && dur < TAP_MAX_NS;
}

void InputHandler::tick() {
    // Nothing needed per-frame for this implementation.
    // A more complex handler would generate HOVER or long-press events here.
}

} // namespace sn
