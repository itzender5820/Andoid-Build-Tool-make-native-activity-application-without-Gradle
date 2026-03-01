#pragma once
#include "LowPassFilter.h"
#include "Compressor.h"
#include <vector>

namespace as {

// ── SimpleDelay ───────────────────────────────────────────────────────────────
struct SimpleDelay {
    int                writeIdx = 0;
    int                delaySamples = 0;
    std::vector<Frame> buf;
    f32                feedback = 0.3f;
    f32                mix      = 0.25f;

    void init(int maxDelaySamples);
    void setDelayMs(f32 ms);
    Frame process(Frame in);
};

// ── Effects ───────────────────────────────────────────────────────────────────
// Signal chain: LPF → Delay → Compressor
// Tests that LowPassFilter and Compressor objects can be composed together.
class Effects {
public:
    Effects();

    // Configure the chain
    void setLpfCutoff   (f32 hz)     { lpf_.setCutoffHz(hz); }
    void setLpfResonance(f32 Q)      { lpf_.setResonance(Q); }
    void setDelayMs     (f32 ms)     { delay_.setDelayMs(ms); }
    void setDelayFeedback(f32 fb)    { delay_.feedback = fb; }
    void setDelayMix    (f32 mix)    { delay_.mix = mix; }
    void setCompThresh  (f32 dB)     { comp_.setThresholdDb(dB); }
    void setCompRatio   (f32 r)      { comp_.setRatio(r); }
    void setCompAttack  (f32 ms)     { comp_.setAttackMs(ms); }
    void setCompRelease (f32 ms)     { comp_.setReleaseMs(ms); }
    void setCompMakeup  (f32 dB)     { comp_.setMakeupDb(dB); }

    void setLpfEnabled  (bool on)    { lpf_.setEnabled(on); }
    void setDelayEnabled(bool on)    { delayEnabled_ = on; }
    void setCompEnabled (bool on)    { comp_.setEnabled(on); }

    Frame process(Frame in);
    void  process(Frame* buf, int count);

    f32 gainReductionDb() const { return comp_.gainReductionDb(); }

private:
    LowPassFilter lpf_;
    SimpleDelay   delay_;
    Compressor    comp_;
    bool          delayEnabled_ = true;
};

} // namespace as
