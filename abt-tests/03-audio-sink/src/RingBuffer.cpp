#include "RingBuffer.h"
#include <algorithm>

namespace as {

RingBuffer::RingBuffer() : buf_(CAPACITY) {}

int RingBuffer::available() const {
    return wPos_.load(std::memory_order_acquire)
         - rPos_.load(std::memory_order_acquire);
}

int RingBuffer::freeSpace() const {
    return CAPACITY - available();
}

int RingBuffer::write(const Frame* src, int count) {
    int avail = freeSpace();
    int n     = std::min(count, avail);
    int wp    = wPos_.load(std::memory_order_relaxed) & (CAPACITY - 1);
    int part1 = std::min(n, CAPACITY - wp);
    int part2 = n - part1;
    std::copy(src,        src + part1,  buf_.data() + wp);
    if (part2) std::copy(src + part1, src + n, buf_.data());
    wPos_.fetch_add(n, std::memory_order_release);
    return n;
}

int RingBuffer::read(Frame* dst, int count) {
    int avail = available();
    int n     = std::min(count, avail);
    int rp    = rPos_.load(std::memory_order_relaxed) & (CAPACITY - 1);
    int part1 = std::min(n, CAPACITY - rp);
    int part2 = n - part1;
    std::copy(buf_.data() + rp, buf_.data() + rp + part1, dst);
    if (part2) std::copy(buf_.data(), buf_.data() + part2, dst + part1);
    rPos_.fetch_add(n, std::memory_order_release);
    return n;
}

void RingBuffer::clear() {
    rPos_.store(wPos_.load(std::memory_order_acquire), std::memory_order_release);
}

} // namespace as
