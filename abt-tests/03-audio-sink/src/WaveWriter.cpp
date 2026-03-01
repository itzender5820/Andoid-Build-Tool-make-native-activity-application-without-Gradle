#include "WaveWriter.h"
#include <cstring>
#include <cmath>
#include <vector>

namespace as {

WaveWriter::~WaveWriter() { if (isOpen()) close(); }

bool WaveWriter::open(const std::string& path) {
    path_ = path;
    file_.open(path, std::ios::binary);
    if (!file_) { ASLOGE("WaveWriter: cannot open %s", path.c_str()); return false; }
    // Reserve space for header
    WavHeader hdr;
    file_.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    sampleCount_ = 0;
    ASLOGI("WaveWriter: opened %s", path.c_str());
    return true;
}

int WaveWriter::write(const Frame* frames, int count) {
    if (!file_) return -1;
    for (int i = 0; i < count; ++i) {
        int16_t l = toInt16(frames[i].L);
        int16_t r = toInt16(frames[i].R);
        file_.write(reinterpret_cast<const char*>(&l), 2);
        file_.write(reinterpret_cast<const char*>(&r), 2);
    }
    sampleCount_ += count;
    return count;
}

bool WaveWriter::close() {
    if (!file_) return false;

    WavHeader hdr;
    u32 dataBytes   = sampleCount_ * CHANNELS * 2;
    hdr.dataSize    = dataBytes;
    hdr.fileSize    = sizeof(WavHeader) - 8 + dataBytes;

    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    file_.close();
    ASLOGI("WaveWriter: closed %s  (%u frames = %.2f sec)",
           path_.c_str(), sampleCount_, sampleCount_ / (f32)SAMPLE_RATE);
    return true;
}

bool WaveWriter::writeTone(const std::string& path, f32 hz, f32 durSec, f32 amp) {
    WaveWriter w;
    if (!w.open(path)) return false;
    int totalFrames = (int)(durSec * SAMPLE_RATE);
    f32 phase = 0.f, phaseInc = hz * INV_SAMPLE_RATE;
    std::vector<Frame> buf(4096);
    int remaining = totalFrames;
    while (remaining > 0) {
        int n = remaining < 4096 ? remaining : 4096;
        for (int i = 0; i < n; ++i) {
            f32 s = sinf(phase * TWO_PI) * amp;
            buf[i] = {s, s};
            phase += phaseInc;
            if (phase >= 1.f) phase -= 1.f;
        }
        w.write(buf.data(), n);
        remaining -= n;
    }
    return w.close();
}

} // namespace as
