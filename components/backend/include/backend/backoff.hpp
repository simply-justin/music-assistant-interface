#pragma once
#include <algorithm>
#include <cstdint>

namespace backend {

// Reconnect delays: 1, 2, 4, 8, 16, 30, 30 … s; reset() after a successful connect.
class Backoff {
public:
    static constexpr uint32_t kFirstMs = 1000, kMaxMs = 30000;
    uint32_t next() {
        uint32_t d = delay_;
        delay_ = std::min(delay_ * 2, kMaxMs);
        return d;
    }
    void reset() { delay_ = kFirstMs; }

private:
    uint32_t delay_ = kFirstMs;
};

}  // namespace backend
