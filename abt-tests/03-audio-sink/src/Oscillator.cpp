#include "Oscillator.h"
#include <cmath>

namespace as {

Oscillator::Oscillator() { updatePhaseInc(); }

void Oscillator::setFrequency(f32 hz) {
    freq_ = hz;
    updatePhaseInc();
}

void Oscillator::setAmplitude(f32 a) { amp_ = a; }
void Oscillator::setPan(f32 p)       { pan_ = p < -1.f ? -1.f : p > 1.f ? 1.f : p; }
void Oscillator::setWaveShape(WaveShape s) { shape_ = s; }

void Oscillator::setDetuneCents(f32 cents) {
    detuneSemi_ = cents / 100.f;
    updatePhaseInc();
}

void Oscillator::updatePhaseInc() {
    f32 detunedFreq = freq_ * powf(2.f, detuneSemi_ / 12.f);
    phaseInc_ = detunedFreq * INV_SAMPLE_RATE;
}

f32 Oscillator::nextNoise() const {
    noiseSeed_ ^= noiseSeed_ << 13;
    noiseSeed_ ^= noiseSeed_ >> 17;
    noiseSeed_ ^= noiseSeed_ << 5;
    return (f32)(int)noiseSeed_ / 2147483648.f;
}

f32 Oscillator::generateSample() const {
    switch (shape_) {
        case WaveShape::SINE:
            return sinf(phase_ * TWO_PI);
        case WaveShape::SQUARE:
            return phase_ < 0.5f ? 1.f : -1.f;
        case WaveShape::SAWTOOTH:
            return 2.f * phase_ - 1.f;
        case WaveShape::TRIANGLE:
            return phase_ < 0.5f
                ? 4.f * phase_ - 1.f
                : 3.f - 4.f * phase_;
        case WaveShape::NOISE:
            return nextNoise();
    }
    return 0.f;
}

Frame Oscillator::tick() {
    if (!enabled_) return {};
    f32 s = generateSample() * amp_;
    // Constant-power pan
    f32 panL = cosf((pan_ + 1.f) * 0.25f * TWO_PI);
    f32 panR = sinf((pan_ + 1.f) * 0.25f * TWO_PI);
    // Advance phase
    phase_ += phaseInc_;
    if (phase_ >= 1.f) phase_ -= 1.f;
    return {s * panL, s * panR};
}

void Oscillator::render(Frame* buf, int count) {
    for (int i = 0; i < count; ++i)
        buf[i] += tick();
}

} // namespace as
