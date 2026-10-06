#pragma once
// Animations are short scripts of poses and sounds, played by Sequencer.

#include <stdint.h>
#include <string.h>

#include "poses.h"
#include "sounds.h"

namespace furby {

struct Step {
  Pose pose;
  uint16_t holdMs;  // wait after reaching the pose (and starting the sound)
  Sound sound;      // started when the pose is reached
  bool waitSound;   // hold also lasts until the sound finishes
};

struct Animation {
  const char* name;
  const Step* steps;
  uint8_t count;
};

#define FURBY_ANIM(var, ...)                          \
  static const Step var##_steps[] = {__VA_ARGS__};    \
  static const Animation var = {#var, var##_steps,    \
                                sizeof(var##_steps) / sizeof(Step)}

namespace anim {
using P = Pose;
using S = Sound;
FURBY_ANIM(wake,
           {P::kSleep, 200, S::kNone, false},
           {P::kEyesClosed, 300, S::kYawn, true},
           {P::kHome, 300, S::kNone, false},
           {P::kEarsUp, 200, S::kHello, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(blink,
           {P::kEyesClosed, 80, S::kNone, false},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(look_around,
           {P::kEarsUp, 400, S::kNone, false},
           {P::kEarsBack, 400, S::kNone, false},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(talk,
           {P::kMouthOpen, 0, S::kChatter, false},
           {P::kHome, 150, S::kNone, false},
           {P::kMouthOpen, 150, S::kNone, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(purr,
           {P::kEyesClosed, 0, S::kPurr, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(giggle,
           {P::kLeanForward, 0, S::kGiggle, false},
           {P::kMouthOpen, 200, S::kNone, false},
           {P::kLeanForward, 200, S::kNone, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(big_giggle,
           {P::kLeanForward, 0, S::kGiggle, true},
           {P::kMouthOpen, 0, S::kGiggle, true},
           {P::kLeanForward, 0, S::kHappy, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(eat,
           {P::kMouthOpen, 250, S::kNone, false},
           {P::kHome, 150, S::kNone, false},
           {P::kMouthOpen, 250, S::kNone, false},
           {P::kHome, 0, S::kYum, true});
FURBY_ANIM(startle,
           {P::kEarsUp, 0, S::kWhoa, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(whoa,
           {P::kLeanForward, 0, S::kWhoa, true},
           {P::kEarsBack, 300, S::kNone, false},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(listen,
           {P::kEarsUp, 600, S::kNone, false},
           {P::kMouthOpen, 0, S::kHappy, true},
           {P::kHome, 0, S::kNone, false});
FURBY_ANIM(go_sleep,
           {P::kEyesClosed, 0, S::kYawn, true},
           {P::kSleep, 0, S::kNone, false});

static const Animation* const kAll[] = {&wake,   &blink,      &look_around, &talk,
                                        &purr,   &giggle,     &big_giggle,  &eat,
                                        &startle, &whoa,      &listen,      &go_sleep};
constexpr uint8_t kAllCount = sizeof(kAll) / sizeof(kAll[0]);

inline const Animation* find(const char* name) {
  for (uint8_t i = 0; i < kAllCount; ++i)
    if (strcmp(kAll[i]->name, name) == 0) return kAll[i];
  return nullptr;
}
}  // namespace anim

// Steps through an Animation. The caller owns the motor and audio: it
// performs the requests returned by update() and reports back whether the
// last move and sound have finished.
class Sequencer {
 public:
  struct Request {
    bool move = false;
    Pose pose = Pose::kHome;
    bool play = false;
    Sound sound = Sound::kNone;
  };

  void start(const Animation* a) {
    anim_ = a;
    index_ = 0;
    phase_ = a && a->count ? kNeedMove : kIdle;
  }
  void cancel() { anim_ = nullptr; phase_ = kIdle; }
  bool busy() const { return phase_ != kIdle; }
  const Animation* current() const { return anim_; }

  Request update(uint32_t now, bool moveDone, bool soundDone) {
    Request r;
    if (phase_ == kIdle) return r;
    const Step& s = anim_->steps[index_];
    switch (phase_) {
      case kNeedMove:
        r.move = true;
        r.pose = s.pose;
        phase_ = kMoving;
        break;
      case kMoving:
        if (!moveDone) break;
        if (s.sound != Sound::kNone) {
          r.play = true;
          r.sound = s.sound;
        }
        holdUntil_ = now + s.holdMs;
        phase_ = kHolding;
        break;
      case kHolding:
        // soundDone reflects the sound just requested only from the next
        // update on, which is when this phase runs.
        if (static_cast<int32_t>(now - holdUntil_) < 0) break;
        if (s.waitSound && !soundDone) break;
        if (++index_ >= anim_->count) {
          cancel();
        } else {
          phase_ = kNeedMove;
          return update(now, moveDone, soundDone);
        }
        break;
      case kIdle:
        break;
    }
    return r;
  }

 private:
  enum Phase : uint8_t { kIdle, kNeedMove, kMoving, kHolding };
  const Animation* anim_ = nullptr;
  uint8_t index_ = 0;
  Phase phase_ = kIdle;
  uint32_t holdUntil_ = 0;
};

}  // namespace furby
