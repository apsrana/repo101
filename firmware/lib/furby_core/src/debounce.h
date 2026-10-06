#pragma once
#include <stdint.h>

namespace furby {

// Debounces one active-high logical input. Reports press/release edges once
// the raw level has been stable for `debounceMs`.
class Debouncer {
 public:
  enum Edge : uint8_t { kNone, kPressed, kReleased };

  explicit Debouncer(uint32_t debounceMs = 25) : debounceMs_(debounceMs) {}

  Edge update(bool raw, uint32_t now) {
    if (raw != lastRaw_) {
      lastRaw_ = raw;
      changedAt_ = now;
      return kNone;
    }
    if (raw != stable_ && now - changedAt_ >= debounceMs_) {
      stable_ = raw;
      return raw ? kPressed : kReleased;
    }
    return kNone;
  }
  bool pressed() const { return stable_; }

 private:
  uint32_t debounceMs_;
  bool lastRaw_ = false;
  bool stable_ = false;
  uint32_t changedAt_ = 0;
};

}  // namespace furby
