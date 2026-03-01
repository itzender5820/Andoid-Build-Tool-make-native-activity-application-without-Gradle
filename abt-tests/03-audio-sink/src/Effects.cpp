#include "Effects.h"
#include <cmath>

namespace as {

// ── SimpleDelay ───────────────────────────────────────────────────────────────
void SimpleDelay::init(int maxSamples) {
    buf.assign(maxSamples, {});
    delaySamples = maxSamples / 2;
}

void SimpleDelay::setDelayMs(f32 ms) {
    delaySamples = (int)(ms * 0.001f * SAMPLE_RATE);
    if (delaySamples >= (int)buf.size())
        delaySamples = (int)buf.size() - 1;
    if (delaySamples < 1) delaySamples = 1;
}

Frame SimpleDelay::process(Frame in) {
    int readIdx = writeIdx - delaySamples;
    if (readIdx < 0) readIdx += (int)buf.size();
    Frame delayed = buf[readIdx];
    // Write with feedback
    buf[writeIdx] = in + delayed * feedback;
    writeIdx = (writeIdx + 1) % (int)buf.size();
    // Dry/wet mix
    return in * (1.f - mix) + delayed * mix;
}

// ── Effects ───────────────────────────────────────────────────────────────────
Effects::Effects() {
    delay_.init(SAMPLE_RATE); // up to 1 second delay
    delay_.setDelayMs(250.f);
    delay_.feedback = 0.35f;
    delay_.mix      = 0.2f;

    lpf_.setCutoff(8000.f, 0.9f);

    comp_.setThresholdDb(-18.f);
    comp_.setRatio(3.f);
    comp_.setAttackMs(15.f);
    comp_.setReleaseMs(150.f);
    comp_.setMakeupDb(4.f);
}

Frame Effects::process(Frame in) {
    Frame s = lpf_.process(in);
    if (delayEnabled_) s = delay_.process(s);
    s = comp_.process(s);
    return s;
}

void Effects::process(Frame* buf, int count) {
    for (int i = 0; i < count; ++i) buf[i] = process(buf[i]);
}

} // namespace as
