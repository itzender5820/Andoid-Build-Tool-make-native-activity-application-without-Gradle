#pragma once
#include "RingBuffer.h"
#include "Oscillator.h"
#include <array>
#include <mutex>

namespace as {

static constexpr int MAX_VOICES = 8;

// ── Voice ─────────────────────────────────────────────────────────────────────
// A single oscillator voice with individual gain and enable state.
struct Voice {
    Oscillator osc;
    f32        gain = 1.f;
    bool       active = false;
};

// ── Mixer ─────────────────────────────────────────────────────────────────────
// Mixes MAX_VOICES oscillator voices into a stereo RingBuffer.
// The audio callback reads from the ring; the main thread writes voices here.
class Mixer {
public:
    explicit Mixer(RingBuffer& ring);

    // Voice management (call from main thread).
    // Returns voice index or -1 if all voices busy.
    int  noteOn (int midiNote, f32 amplitude = 0.5f,
                 WaveShape shape = WaveShape::SINE, f32 pan = 0.f);
    void noteOff(int voiceIdx);
    void noteOffAll();
    void setMasterGain(f32 g) { masterGain_ = g; }

    // Called from the audio callback to fill the ring with `frames` more frames.
    // Returns frames actually rendered.
    int render(int frames);

    // Statistics
    int  activeVoiceCount() const;
    f32  masterGain()       const { return masterGain_; }

private:
    RingBuffer& ring_;
    std::array<Voice, MAX_VOICES> voices_ {};
    f32   masterGain_ = 0.7f;
    mutable std::mutex mtx_;

    // Scratch buffer to avoid heap allocation in audio thread.
    std::array<Frame, 4096> scratch_ {};
};

} // namespace as
