#pragma once
#include "AudioTypes.h"

namespace as {

// ── LowPassFilter ─────────────────────────────────────────────────────────────
// Stereo biquad low-pass filter with resonance control.
// Uses the bilinear-transform cookbook formulas.
class LowPassFilter {
public:
    LowPassFilter();

    // Design the filter. cutoffHz: 20..22000, resonance Q: 0.5..20.
    void setCutoff   (f32 cutoffHz, f32 Q = 0.707f);
    void setResonance(f32 Q);
    void setCutoffHz (f32 hz);
    void setEnabled  (bool on) { enabled_ = on; }
    void reset();

    // Process one stereo frame.
    Frame process(Frame in);
    // Process a buffer in-place.
    void  process(Frame* buf, int count);

    f32 cutoffHz()  const { return cutoffHz_; }
    f32 resonance() const { return Q_; }

private:
    void recompute();

    f32        cutoffHz_ = 8000.f;
    f32        Q_        = 0.707f;
    BiquadCoeff coeff_;
    BiquadState stateL_;
    BiquadState stateR_;
    bool        enabled_ = true;
};

} // namespace as
