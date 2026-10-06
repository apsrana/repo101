#pragma once
#include <stdint.h>

namespace furby {

// Each sound plays /sounds/<name>.wav from LittleFS when present, otherwise
// a synthesized "Furbish" babble in the matching style.
enum class Sound : uint8_t {
  kNone,
  kChatter,
  kHappy,
  kGiggle,
  kYawn,
  kWhoa,
  kPurr,
  kYum,
  kHello,
  kCount
};

inline const char* soundName(Sound s) {
  static const char* const kNames[] = {"none", "chatter", "happy", "giggle", "yawn",
                                       "whoa", "purr",    "yum",   "hello"};
  uint8_t i = static_cast<uint8_t>(s);
  return i < static_cast<uint8_t>(Sound::kCount) ? kNames[i] : "?";
}

}  // namespace furby
