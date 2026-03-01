#pragma once
#include "AudioTypes.h"

namespace as {

// ── Compressor ────────────────────────────────────────────────────────────────
// Stereo feed-forward RMS dynamics compressor with soft-knee.
class Compressor {
public:
    Compressor();

    // All times in milliseconds.
    void setThresholdDb(f32 dB);   // default: -12 dB
    void setRatio      (f32 r);    // 1:1 to ∞:1, default: 4
    void setAttackMs   (f32 ms);   // default: 10 ms
    void setReleaseMs  (f32 ms);   // default: 100 ms
    void setMakeupDb   (f32 dB);   // default: 0 dB
    void setKneeDb     (f32 dB);   // 0 = hard knee, default: 6 dB
    void setEnabled    (bool on)   { enabled_ = on; }

    Frame process(Frame in);
    void  process(Frame* buf, int count);

    f32 gainReductionDb() const { return gainReductionDb_; }

private:
    f32 threshLin_     = 0.f;
    f32 ratio_         = 4.f;
    f32 attackCoef_    = 0.f;
    f32 releaseCoef_   = 0.f;
    f32 makeup_        = 1.f;
    f32 kneeDb_        = 6.f;
    bool enabled_      = true;

    f32 envelope_      = 0.f;
    f32 gainReductionDb_ = 0.f;

    void recompute();
    f32  computeGain(f32 inputLin) const;

    f32 threshDb_    = -12.f;
    f32 attackMs_    = 10.f;
    f32 releaseMs_   = 100.f;
    f32 makeupDb_    = 0.f;
};

} // namespace as
