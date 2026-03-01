#pragma once
#include "AudioTypes.h"

namespace as {

// ── Oscillator ────────────────────────────────────────────────────────────────
// Band-limited oscillator supporting multiple waveforms, detune, and panning.
class Oscillator {
public:
    Oscillator();

    void setFrequency(f32 hz);
    void setAmplitude(f32 amp);       // 0..1
    void setPan      (f32 pan);       // -1=L, 0=C, 1=R
    void setWaveShape(WaveShape s);
    void setDetuneCents(f32 cents);   // +/- semitones * 100
    void setEnabled  (bool on)        { enabled_ = on; }

    // Render the next frame (advance phase).
    Frame tick();

    // Render `count` frames into buf (additive — does NOT zero buf first).
    void render(Frame* buf, int count);

    // Reset phase to 0.
    void reset() { phase_ = 0.f; }

    f32 frequency() const { return freq_; }
    bool isEnabled()const { return enabled_; }

private:
    f32       freq_        = 440.f;
    f32       amp_         = 0.5f;
    f32       pan_         = 0.f;
    f32       phase_       = 0.f;
    f32       phaseInc_    = 0.f;
    WaveShape shape_       = WaveShape::SINE;
    f32       detuneSemi_  = 0.f;
    bool      enabled_     = false;

    void updatePhaseInc();
    f32  generateSample() const;

    // Simple noise state
    mutable u32 noiseSeed_ = 0x12345678u;
    f32 nextNoise() const;
};

} // namespace as
