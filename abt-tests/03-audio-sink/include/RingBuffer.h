#pragma once
#include "AudioTypes.h"
#include <vector>
#include <atomic>

namespace as {

// ── RingBuffer ────────────────────────────────────────────────────────────────
// Lock-free single-producer / single-consumer ring buffer for stereo frames.
// Capacity must be a power of two.
class RingBuffer {
public:
    static constexpr int CAPACITY = 1 << 15;  // 32768 frames

    RingBuffer();

    // Write up to `count` frames from src; returns frames actually written.
    int write(const Frame* src, int count);
    // Read up to `count` frames into dst; returns frames actually read.
    int read (Frame* dst, int count);
    // Available frames to read.
    int available() const;
    // Free capacity to write.
    int freeSpace() const;

    void clear();
    bool isEmpty()  const { return available() == 0; }
    bool isFull()   const { return freeSpace() == 0; }

private:
    std::vector<Frame>  buf_;
    std::atomic<int>    wPos_ {0};
    std::atomic<int>    rPos_ {0};
};

} // namespace as
