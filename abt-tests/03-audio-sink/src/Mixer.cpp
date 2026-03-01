#include "Mixer.h"
#include <algorithm>
#include <cstring>

namespace as {

Mixer::Mixer(RingBuffer& ring) : ring_(ring) {}

int Mixer::noteOn(int midiNote, f32 amp, WaveShape shape, f32 pan) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices_[i].active) {
            Voice& v = voices_[i];
            v.osc.setFrequency(midiToHz(midiNote));
            v.osc.setAmplitude(amp);
            v.osc.setWaveShape(shape);
            v.osc.setPan(pan);
            v.osc.reset();
            v.osc.setEnabled(true);
            v.gain   = 1.f;
            v.active = true;
            ASLOGI("Voice %d on: note=%d freq=%.1fHz", i, midiNote, midiToHz(midiNote));
            return i;
        }
    }
    ASLOGW("All %d voices busy", MAX_VOICES);
    return -1;
}

void Mixer::noteOff(int idx) {
    if (idx < 0 || idx >= MAX_VOICES) return;
    std::lock_guard<std::mutex> lk(mtx_);
    voices_[idx].active = false;
    voices_[idx].osc.setEnabled(false);
}

void Mixer::noteOffAll() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& v : voices_) { v.active = false; v.osc.setEnabled(false); }
}

int Mixer::activeVoiceCount() const {
    std::lock_guard<std::mutex> lk(mtx_);
    int n = 0;
    for (auto& v : voices_) if (v.active) ++n;
    return n;
}

int Mixer::render(int frames) {
    // Clamp to scratch buffer
    frames = std::min(frames, (int)scratch_.size());
    // Zero the scratch buffer
    std::fill(scratch_.begin(), scratch_.begin() + frames, Frame{});

    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& v : voices_) {
            if (!v.active) continue;
            for (int i = 0; i < frames; ++i)
                scratch_[i] += v.osc.tick() * v.gain;
        }
    }

    // Apply master gain and clamp
    for (int i = 0; i < frames; ++i) {
        scratch_[i] = scratch_[i] * masterGain_;
        scratch_[i].clamp();
    }

    return ring_.write(scratch_.data(), frames);
}

} // namespace as
