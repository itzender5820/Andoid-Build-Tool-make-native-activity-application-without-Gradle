#pragma once
#include "Mixer.h"
#include "Effects.h"
#include "WaveWriter.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <string>
#include <mutex>
#include <functional>

namespace as {

// ── AudioEngine ───────────────────────────────────────────────────────────────
// Top of the audio dep chain: owns AAudio stream, RingBuffer, Mixer, Effects.
// Callback path: AAudio → ring → Effects → output (all lock-free).
// Control path : noteOn/Off → Mixer (mutex-guarded).
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init(const std::string& dataPath);
    void shutdown();

    // ── Playback control ──────────────────────────────────────────────────────
    int  noteOn (int midiNote,
                 f32 amp    = 0.5f,
                 WaveShape  = WaveShape::SINE,
                 f32 pan    = 0.f);
    void noteOff(int voiceIdx);
    void noteOffAll();

    void setMasterGain(f32 g) { if(mixer_) mixer_->setMasterGain(g); }

    // ── Effects control ───────────────────────────────────────────────────────
    Effects* effects() { return effects_.get(); }

    // ── Wave capture ──────────────────────────────────────────────────────────
    // Start/stop writing output to a WAV file.
    bool startCapture(const std::string& path);
    void stopCapture();
    bool isCapturing() const { return capturing_.load(); }

    // ── Status ────────────────────────────────────────────────────────────────
    bool    isRunning()     const { return running_.load(); }
    int     activeVoices()  const { return mixer_ ? mixer_->activeVoiceCount() : 0; }
    i32     sampleRate()    const { return sampleRate_; }
    f32     outputLoadPct() const { return loadPct_.load(); }

private:
    static aaudio_data_callback_result_t audioCallback(
        AAudioStream*, void* userData, void* audioData, int32_t numFrames);
    static void errorCallback(AAudioStream*, void* userData, aaudio_result_t error);

    AAudioStream*  stream_      = nullptr;
    RingBuffer     ring_;
    std::unique_ptr<Mixer>   mixer_;
    std::unique_ptr<Effects> effects_;
    std::unique_ptr<WaveWriter> waveWriter_;

    i32  sampleRate_  = SAMPLE_RATE;
    std::atomic<bool> running_   {false};
    std::atomic<bool> capturing_ {false};
    std::atomic<f32>  loadPct_   {0.f};
    std::string dataPath_;

    // Scratch for effects processing in callback (avoid alloc)
    static constexpr int SCRATCH_SIZE = 4096;
    Frame scratch_[SCRATCH_SIZE];

    std::mutex captureMtx_;
};

} // namespace as
