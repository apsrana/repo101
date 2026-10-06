#pragma once
// The Furby's "personality": turns sensor events and the passage of time
// into which animation to play next. Pure logic, unit tested on the host.

#include <stdint.h>

#include "animation.h"

namespace furby {

enum class Sensor : uint8_t {
  kHead,
  kBack,
  kTummy,
  kTongue,
  kTail,
  kTilt,   // turned upside down
  kLoud,   // loud sound / clap
  kDark,   // light level dropped (covered eyes, lights off)
  kCount
};

inline const char* sensorName(Sensor s) {
  static const char* const kNames[] = {"head", "back", "tummy", "tongue",
                                       "tail", "tilt", "loud",  "dark"};
  uint8_t i = static_cast<uint8_t>(s);
  return i < static_cast<uint8_t>(Sensor::kCount) ? kNames[i] : "?";
}

// Small deterministic PRNG so behaviour is testable.
class Rng {
 public:
  explicit Rng(uint32_t seed = 1) : s_(seed ? seed : 1) {}
  uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  // Uniform in [lo, hi].
  uint32_t range(uint32_t lo, uint32_t hi) { return lo + next() % (hi - lo + 1); }

 private:
  uint32_t s_;
};

class Brain {
 public:
  enum class State : uint8_t { kAsleep, kAwake };

  struct Config {
    uint32_t sleepAfterMs = 5UL * 60UL * 1000UL;
    uint32_t idleMinMs = 4000;
    uint32_t idleMaxMs = 12000;
    uint32_t tickleWindowMs = 4000;
    uint8_t tickleCount = 3;
    uint32_t loudCooldownMs = 3000;
  };

  void begin(const Config& cfg, uint32_t now, uint32_t seed) {
    cfg_ = cfg;
    rng_ = Rng(seed);
    state_ = State::kAsleep;
    pending_ = &anim::wake;   // power-on wake-up routine
    lastInteraction_ = now;
    lastLoud_ = now - cfg.loudCooldownMs;
    scheduleIdle(now);
  }

  void onSensor(Sensor s, uint32_t now) {
    if (s == Sensor::kLoud) {
      if (now - lastLoud_ < cfg_.loudCooldownMs) return;
      lastLoud_ = now;
    }
    if (state_ == State::kAsleep) {
      // Darkness never wakes it; anything else does.
      if (s == Sensor::kDark) return;
      state_ = State::kAwake;
      queue(&anim::wake, kPrioWake);
      lastInteraction_ = now;
      return;
    }
    lastInteraction_ = now;
    switch (s) {
      case Sensor::kHead:
      case Sensor::kBack:   queue(&anim::purr, kPrioReact); break;
      case Sensor::kTummy:  onTickle(now); break;
      case Sensor::kTongue: queue(&anim::eat, kPrioReact); break;
      case Sensor::kTail:   queue(&anim::startle, kPrioReact); break;
      case Sensor::kTilt:   queue(&anim::whoa, kPrioUrgent); break;
      case Sensor::kLoud:   queue(&anim::listen, kPrioReact); break;
      case Sensor::kDark:   goToSleep(); break;
      case Sensor::kCount:  break;
    }
  }

  // Force sleep / wake (console commands).
  void sleep() { goToSleep(); }
  void wake(uint32_t now) {
    if (state_ == State::kAwake) return;
    state_ = State::kAwake;
    lastInteraction_ = now;
    queue(&anim::wake, kPrioWake);
  }

  // Call every loop. When `busy` is false and something should play,
  // returns the animation to start; otherwise nullptr.
  const Animation* poll(uint32_t now, bool busy) {
    if (state_ == State::kAwake && pending_ == nullptr && !busy &&
        now - lastInteraction_ >= cfg_.sleepAfterMs) {
      goToSleep();
    }
    if (busy) return nullptr;
    if (pending_) {
      const Animation* a = pending_;
      pending_ = nullptr;
      pendingPrio_ = 0;
      if (a == &anim::wake) state_ = State::kAwake;
      scheduleIdle(now);
      return a;
    }
    if (state_ == State::kAwake && static_cast<int32_t>(now - nextIdle_) >= 0) {
      scheduleIdle(now);
      uint32_t roll = rng_.range(0, 99);
      if (roll < 60) return &anim::blink;
      if (roll < 85) return &anim::look_around;
      return &anim::talk;
    }
    return nullptr;
  }

  State state() const { return state_; }
  bool hasPending() const { return pending_ != nullptr; }

 private:
  enum : uint8_t { kPrioIdle = 1, kPrioReact = 2, kPrioUrgent = 3, kPrioSleep = 4, kPrioWake = 5 };

  // One pending slot: a higher (or equal, i.e. newer) priority replaces it.
  void queue(const Animation* a, uint8_t prio) {
    if (pending_ && prio < pendingPrio_) return;
    pending_ = a;
    pendingPrio_ = prio;
  }

  void onTickle(uint32_t now) {
    if (now - tickleStart_ > cfg_.tickleWindowMs) {
      tickleStart_ = now;
      tickles_ = 0;
    }
    if (++tickles_ >= cfg_.tickleCount) {
      tickles_ = 0;
      queue(&anim::big_giggle, kPrioReact);
    } else {
      queue(&anim::giggle, kPrioReact);
    }
  }

  void goToSleep() {
    if (state_ == State::kAsleep) return;
    state_ = State::kAsleep;
    pending_ = nullptr;
    pendingPrio_ = 0;
    queue(&anim::go_sleep, kPrioSleep);
  }

  void scheduleIdle(uint32_t now) { nextIdle_ = now + rng_.range(cfg_.idleMinMs, cfg_.idleMaxMs); }

  Config cfg_;
  Rng rng_;
  State state_ = State::kAsleep;
  const Animation* pending_ = nullptr;
  uint8_t pendingPrio_ = 0;
  uint32_t lastInteraction_ = 0;
  uint32_t nextIdle_ = 0;
  uint32_t lastLoud_ = 0;
  uint32_t tickleStart_ = 0;
  uint8_t tickles_ = 0;
};

}  // namespace furby
