#pragma once
#include <cstdint>
#include <string>

namespace core {

// Ignores the same command repeated within a short window (a double tap on a wall panel).
class RepeatGuard {
public:
    static constexpr uint32_t kWindowMs = 1500;

    bool allow(const std::string& key, uint32_t now_ms) {
        bool repeat = has_last_ && key == last_ && now_ms - last_ms_ < kWindowMs;
        if (!repeat) {
            last_ = key;
            last_ms_ = now_ms;
            has_last_ = true;
        }
        return !repeat;
    }

private:
    std::string last_;
    uint32_t last_ms_ = 0;
    bool has_last_ = false;
};

}  // namespace core
