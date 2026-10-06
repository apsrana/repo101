#pragma once
// Named cam positions. The defaults are evenly spaced guesses: watch the
// Furby while jogging the motor (`fwd`/`rev`/`stop`, then `setpose NAME`)
// to find the real counts for your unit, then `save`.

#include <stdint.h>
#include <string.h>

namespace furby {

enum class Pose : uint8_t {
  kHome,         // rest: eyes open, mouth closed, ears neutral
  kEyesClosed,
  kMouthOpen,
  kEarsUp,
  kEarsBack,
  kLeanForward,
  kSleep,        // eyes closed, slumped
  kCount
};

constexpr uint8_t kPoseCount = static_cast<uint8_t>(Pose::kCount);

inline const char* poseName(Pose p) {
  static const char* const kNames[kPoseCount] = {
      "home", "eyes_closed", "mouth_open", "ears_up",
      "ears_back", "lean_forward", "sleep"};
  uint8_t i = static_cast<uint8_t>(p);
  return i < kPoseCount ? kNames[i] : "?";
}

inline bool poseFromName(const char* name, Pose& out) {
  for (uint8_t i = 0; i < kPoseCount; ++i) {
    if (strcmp(name, poseName(static_cast<Pose>(i))) == 0) {
      out = static_cast<Pose>(i);
      return true;
    }
  }
  return false;
}

class PoseTable {
 public:
  // Default positions as fractions of one cam revolution (per mille).
  void setDefaults(int32_t countsPerRev) {
    static const int16_t kPerMille[kPoseCount] = {0, 120, 250, 380, 500, 650, 850};
    for (uint8_t i = 0; i < kPoseCount; ++i)
      counts_[i] = countsPerRev * kPerMille[i] / 1000;
  }
  int32_t get(Pose p) const { return counts_[static_cast<uint8_t>(p)]; }
  void set(Pose p, int32_t c) { counts_[static_cast<uint8_t>(p)] = c; }
  int32_t* raw() { return counts_; }

 private:
  int32_t counts_[kPoseCount] = {};
};

}  // namespace furby
