#include "Compressor.h"
#include <cmath>
#include <algorithm>

namespace as {

Compressor::Compressor() { recompute(); }

void Compressor::setThresholdDb(f32 dB) { threshDb_ = dB; recompute(); }
void Compressor::setRatio      (f32 r)  { ratio_ = r < 1.f ? 1.f : r; }
void Compressor::setAttackMs   (f32 ms) { attackMs_  = ms; recompute(); }
void Compressor::setReleaseMs  (f32 ms) { releaseMs_ = ms; recompute(); }
void Compressor::setKneeDb     (f32 dB) { kneeDb_ = dB < 0.f ? 0.f : dB; }

void Compressor::setMakeupDb(f32 dB) {
    makeupDb_ = dB;
    makeup_   = powf(10.f, dB / 20.f);
}

void Compressor::recompute() {
    threshLin_  = powf(10.f, threshDb_ / 20.f);
    // Time constants
    attackCoef_  = (attackMs_  > 0.f) ? expf(-1.f / (SAMPLE_RATE * attackMs_  * 0.001f)) : 0.f;
    releaseCoef_ = (releaseMs_ > 0.f) ? expf(-1.f / (SAMPLE_RATE * releaseMs_ * 0.001f)) : 0.f;
}

f32 Compressor::computeGain(f32 inputLin) const {
    if (inputLin <= 0.f) return 1.f;
    f32 inputDb = 20.f * log10f(inputLin);
    f32 overDb  = inputDb - threshDb_;
    f32 halfKnee = kneeDb_ * 0.5f;

    f32 gainDb;
    if (overDb < -halfKnee) {
        gainDb = 0.f; // no compression
    } else if (overDb > halfKnee) {
        gainDb = overDb * (1.f - 1.f / ratio_);
    } else {
        // Soft knee interpolation
        f32 t = (overDb + halfKnee) / kneeDb_;
        gainDb = t * t * halfKnee * (1.f - 1.f / ratio_);
    }
    return powf(10.f, -gainDb / 20.f);
}

Frame Compressor::process(Frame in) {
    if (!enabled_) return in;
    f32 peak = std::max(std::abs(in.L), std::abs(in.R));
    // Envelope follower
    if (peak > envelope_)
        envelope_ = attackCoef_  * envelope_ + (1.f - attackCoef_)  * peak;
    else
        envelope_ = releaseCoef_ * envelope_ + (1.f - releaseCoef_) * peak;

    f32 gain = computeGain(envelope_) * makeup_;
    gainReductionDb_ = (gain < 1.f) ? 20.f * log10f(gain) : 0.f;
    return in * gain;
}

void Compressor::process(Frame* buf, int count) {
    for (int i = 0; i < count; ++i) buf[i] = process(buf[i]);
}

} // namespace as
