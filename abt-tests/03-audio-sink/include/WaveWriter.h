#pragma once
#include "AudioTypes.h"
#include <string>
#include <fstream>
#include <vector>

namespace as {

// ── WaveWriter ────────────────────────────────────────────────────────────────
// Writes stereo 16-bit PCM WAV files from Frame buffers.
// Tests file I/O integration in the audio pipeline.
class WaveWriter {
public:
    WaveWriter() = default;
    ~WaveWriter();

    // Open a .wav file for writing. Returns false on failure.
    bool open(const std::string& path);

    // Write frames to file (converts float → int16).
    // Returns number of frames written or -1 on error.
    int write(const Frame* frames, int count);

    // Finalize and close the file (writes WAV header).
    bool close();

    bool isOpen()  const { return file_.is_open(); }
    u32  samples() const { return sampleCount_; }

    // Quick utility: write a tone to disk (for self-test).
    static bool writeTone(const std::string& path, f32 hz, f32 durationSec, f32 amp = 0.5f);

private:
    struct WavHeader {
        char     riff[4]      = {'R','I','F','F'};
        u32      fileSize     = 0;
        char     wave[4]      = {'W','A','V','E'};
        char     fmt[4]       = {'f','m','t',' '};
        u32      fmtLen       = 16;
        uint16_t audioFmt     = 1;  // PCM
        uint16_t channels     = CHANNELS;
        u32      sampleRate   = SAMPLE_RATE;
        u32      byteRate     = SAMPLE_RATE * CHANNELS * 2;
        uint16_t blockAlign   = CHANNELS * 2;
        uint16_t bitsPerSample= 16;
        char     data[4]      = {'d','a','t','a'};
        u32      dataSize     = 0;
    };

    std::ofstream file_;
    u32           sampleCount_ = 0;
    std::string   path_;

    static int16_t toInt16(f32 f) {
        int v = (int)(f * 32767.f);
        return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
    }
};

} // namespace as
