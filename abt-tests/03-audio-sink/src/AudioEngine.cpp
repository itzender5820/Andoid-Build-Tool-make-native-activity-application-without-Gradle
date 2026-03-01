#include "AudioEngine.h"
#include <cstring>
#include <cmath>

namespace as {

AudioEngine::AudioEngine() {}
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init(const std::string& dataPath) {
    dataPath_ = dataPath;

    mixer_   = std::make_unique<Mixer>(ring_);
    effects_ = std::make_unique<Effects>();

    // Write a quick self-test tone to verify WaveWriter + file I/O works
    WaveWriter::writeTone(dataPath + "/selftest.wav", 440.f, 0.5f);

    // Build AAudio stream
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        ASLOGE("createStreamBuilder failed");
        return false;
    }

    AAudioStreamBuilder_setFormat           (builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount     (builder, CHANNELS);
    AAudioStreamBuilder_setSampleRate       (builder, SAMPLE_RATE);
    AAudioStreamBuilder_setPerformanceMode  (builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode      (builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setDataCallback     (builder, audioCallback, this);
    AAudioStreamBuilder_setErrorCallback    (builder, errorCallback, this);

    aaudio_result_t res = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);

    if (res != AAUDIO_OK) {
        ASLOGE("openStream failed: %s", AAudio_convertResultToText(res));
        return false;
    }

    sampleRate_ = AAudioStream_getSampleRate(stream_);
    ASLOGI("AAudio stream: sr=%d  burstFrames=%d",
           sampleRate_, AAudioStream_getFramesPerBurst(stream_));

    if (AAudioStream_requestStart(stream_) != AAUDIO_OK) {
        ASLOGE("requestStart failed");
        return false;
    }

    running_.store(true);
    ASLOGI("AudioEngine ready");
    return true;
}

void AudioEngine::shutdown() {
    running_.store(false);
    stopCapture();
    if (stream_) {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
    mixer_.reset();
    effects_.reset();
}

int AudioEngine::noteOn(int note, f32 amp, WaveShape shape, f32 pan) {
    if (!mixer_) return -1;
    return mixer_->noteOn(note, amp, shape, pan);
}
void AudioEngine::noteOff(int idx)  { if(mixer_) mixer_->noteOff(idx); }
void AudioEngine::noteOffAll()      { if(mixer_) mixer_->noteOffAll(); }

bool AudioEngine::startCapture(const std::string& path) {
    std::lock_guard<std::mutex> lk(captureMtx_);
    if (capturing_.load()) return false;
    waveWriter_ = std::make_unique<WaveWriter>();
    if (!waveWriter_->open(path)) { waveWriter_.reset(); return false; }
    capturing_.store(true);
    return true;
}

void AudioEngine::stopCapture() {
    std::lock_guard<std::mutex> lk(captureMtx_);
    if (!capturing_.load()) return;
    capturing_.store(false);
    if (waveWriter_) { waveWriter_->close(); waveWriter_.reset(); }
}

aaudio_data_callback_result_t AudioEngine::audioCallback(
    AAudioStream*, void* userData, void* audioData, int32_t numFrames)
{
    auto* self = static_cast<AudioEngine*>(userData);
    auto* out  = static_cast<float*>(audioData);

    // 1) Render fresh samples from Mixer into ring
    self->mixer_->render(numFrames);

    // 2) Read from ring
    int n = self->ring_.read(self->scratch_, numFrames);
    if (n < numFrames)
        memset(self->scratch_ + n, 0, (numFrames - n) * sizeof(Frame));

    // 3) Apply effects chain
    self->effects_->process(self->scratch_, numFrames);

    // 4) Write to AAudio output (interleaved float: L, R, L, R ...)
    for (int i = 0; i < numFrames; ++i) {
        out[i*2 + 0] = self->scratch_[i].L;
        out[i*2 + 1] = self->scratch_[i].R;
    }

    // 5) Capture to WAV if active (non-blocking check)
    if (self->capturing_.load()) {
        std::unique_lock<std::mutex> lk(self->captureMtx_, std::try_to_lock);
        if (lk.owns_lock() && self->waveWriter_)
            self->waveWriter_->write(self->scratch_, numFrames);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AudioEngine::errorCallback(AAudioStream*, void* userData, aaudio_result_t error) {
    ASLOGE("AAudio error: %s", AAudio_convertResultToText(error));
    auto* self = static_cast<AudioEngine*>(userData);
    self->running_.store(false);
}

} // namespace as
