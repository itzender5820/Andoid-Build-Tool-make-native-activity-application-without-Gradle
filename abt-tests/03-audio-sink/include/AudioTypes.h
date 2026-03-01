#pragma once
#include <cstdint>
#include <cmath>
#include <android/log.h>

#define AS_TAG  "as"
#define ASLOGI(...) __android_log_print(ANDROID_LOG_INFO,  AS_TAG, __VA_ARGS__)
#define ASLOGE(...) __android_log_print(ANDROID_LOG_ERROR, AS_TAG, __VA_ARGS__)
#define ASLOGD(...) __android_log_print(ANDROID_LOG_DEBUG, AS_TAG, __VA_ARGS__)
#define ASLOGW(...) __android_log_print(ANDROID_LOG_WARN,  AS_TAG, __VA_ARGS__)

namespace as {

using i32 = int32_t;
using i64 = int64_t;
using u32 = uint32_t;
using f32 = float;
using f64 = double;

// ── Audio format constants ────────────────────────────────────────────────────
static constexpr int   SAMPLE_RATE   = 48000;
static constexpr int   CHANNELS      = 2;       // stereo interleaved
static constexpr int   FRAME_SIZE    = CHANNELS; // floats per frame
static constexpr float TWO_PI        = 6.28318530718f;
static constexpr float INV_SAMPLE_RATE = 1.f / SAMPLE_RATE;

// ── Waveform types ────────────────────────────────────────────────────────────
enum class WaveShape : u32 {
    SINE,
    SQUARE,
    SAWTOOTH,
    TRIANGLE,
    NOISE,
};

// ── Note helpers ──────────────────────────────────────────────────────────────
// MIDI note → frequency (A4 = 440 Hz = note 69)
inline f32 midiToHz(int note) {
    return 440.f * powf(2.f, (note - 69) / 12.f);
}

// ── Stereo frame ──────────────────────────────────────────────────────────────
struct Frame {
    f32 L = 0.f, R = 0.f;
    Frame operator+(Frame b)  const { return {L+b.L, R+b.R}; }
    Frame operator*(f32 gain) const { return {L*gain, R*gain}; }
    Frame& operator+=(Frame b) { L+=b.L; R+=b.R; return *this; }
    void clamp() {
        auto cl = [](f32 v){ return v > 1.f ? 1.f : v < -1.f ? -1.f : v; };
        L = cl(L); R = cl(R);
    }
};

// ── BiquadCoeff / State ───────────────────────────────────────────────────────
struct BiquadCoeff {
    f32 b0=1,b1=0,b2=0,a1=0,a2=0;
};
struct BiquadState {
    f32 x1=0,x2=0,y1=0,y2=0;
    void reset() { x1=x2=y1=y2=0.f; }
};

inline f32 processBiquad(f32 x, const BiquadCoeff& c, BiquadState& s) {
    f32 y = c.b0*x + c.b1*s.x1 + c.b2*s.x2 - c.a1*s.y1 - c.a2*s.y2;
    s.x2=s.x1; s.x1=x;
    s.y2=s.y1; s.y1=y;
    return y;
}

} // namespace as
