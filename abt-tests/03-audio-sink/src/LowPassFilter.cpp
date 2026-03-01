#include "LowPassFilter.h"
#include <cmath>

namespace as {

LowPassFilter::LowPassFilter() { recompute(); }

void LowPassFilter::setCutoff(f32 hz, f32 Q) {
    cutoffHz_ = hz < 20.f ? 20.f : hz > 20000.f ? 20000.f : hz;
    Q_        = Q  < 0.5f ? 0.5f : Q  > 20.f    ? 20.f    : Q;
    recompute();
}
void LowPassFilter::setCutoffHz (f32 hz) { setCutoff(hz, Q_);        }
void LowPassFilter::setResonance(f32 Q)  { setCutoff(cutoffHz_, Q);  }

void LowPassFilter::reset() { stateL_.reset(); stateR_.reset(); }

void LowPassFilter::recompute() {
    // Bilinear transform cookbook (RBJ Audio EQ Cookbook)
    f32 w0    = TWO_PI * cutoffHz_ * INV_SAMPLE_RATE;
    f32 cosW  = cosf(w0);
    f32 sinW  = sinf(w0);
    f32 alpha = sinW / (2.f * Q_);
    f32 a0    = 1.f + alpha;
    coeff_.b0 = (1.f - cosW) * 0.5f / a0;
    coeff_.b1 =  (1.f - cosW)       / a0;
    coeff_.b2 = coeff_.b0;
    coeff_.a1 = -2.f * cosW         / a0;
    coeff_.a2 = (1.f - alpha)       / a0;
}

Frame LowPassFilter::process(Frame in) {
    if (!enabled_) return in;
    return {
        processBiquad(in.L, coeff_, stateL_),
        processBiquad(in.R, coeff_, stateR_),
    };
}

void LowPassFilter::process(Frame* buf, int count) {
    if (!enabled_) return;
    for (int i = 0; i < count; ++i) buf[i] = process(buf[i]);
}

} // namespace as
