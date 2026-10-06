#pragma once
// Closed-loop cam positioning. Every Furby movement is a position of one
// cam, so "pose X" means "drive the cam to encoder count X past home".
// Pure logic: the caller feeds encoder counts and time, and applies the
// returned motor command.

#include <stdint.h>

namespace furby {

// Non-negative modulo.
inline int32_t wrap(int32_t v, int32_t m) {
  int32_t r = v % m;
  return r < 0 ? r + m : r;
}

// Change between two reads of the ESP32's 16-bit PCNT counter, configured
// with limits [-32768, 32767]: on reaching a limit it resets to 0, so a
// jump of more than half the range means it wrapped. Valid while reads
// are frequent enough that fewer than 16384 counts happen in between.
inline int32_t pcntDelta(int16_t prev, int16_t cur) {
  int32_t d = static_cast<int32_t>(cur) - prev;
  if (d < -16384) d += 32767;
  else if (d > 16384) d -= 32768;
  return d;
}

// If `count` lies within `window` of a whole number of turns, returns the
// correction that snaps it onto that turn, otherwise 0. Used to cancel
// drift each time the cam passes the home switch.
inline int32_t homeCorrection(int32_t count, int32_t countsPerRev, int32_t window) {
  int32_t pos = wrap(count, countsPerRev);
  if (pos <= window) return -pos;
  if (countsPerRev - pos <= window) return countsPerRev - pos;
  return 0;
}

// Soft-start for the motor output. `out` and `target` are signed duties
// (+forward / -reverse). Speeding up is limited to `maxStep` per call so
// the motor's start-up current doesn't brown out a weak (USB) supply;
// slowing down and stopping are immediate, and a direction change drops
// to 0 first and then ramps up the other way.
inline int16_t slewDuty(int16_t out, int16_t target, int16_t maxStep) {
  if ((out > 0 && target < 0) || (out < 0 && target > 0)) out = 0;
  int16_t aOut = out < 0 ? -out : out;
  int16_t aTarget = target < 0 ? -target : target;
  if (aTarget <= aOut) return target;
  int16_t next = aOut + maxStep < aTarget ? aOut + maxStep : aTarget;
  return target < 0 ? -next : next;
}

struct MotorCmd {
  int8_t dir = 0;   // +1 forward, -1 reverse, 0 brake
  uint8_t pwm = 0;
};

struct MotionConfig {
  int32_t countsPerRev = 1200;
  int32_t tolerance = 6;
  uint8_t pwmMin = 110;
  uint8_t pwmMax = 230;
  bool allowReverse = true;
  uint32_t stallTimeoutMs = 400;
  uint32_t moveTimeoutMs = 6000;
};

class MotionController {
 public:
  enum class Status : uint8_t { kIdle, kMoving, kDone, kStalled, kTimeout };

  void configure(const MotionConfig& cfg) { cfg_ = cfg; }
  const MotionConfig& config() const { return cfg_; }

  void moveTo(int32_t target, uint32_t now) {
    target_ = wrap(target, cfg_.countsPerRev);
    status_ = Status::kMoving;
    dir_ = 0;
    startedAt_ = lastProgressAt_ = now;
    lastCount_ = INT32_MIN;
  }

  void stop() {
    if (status_ == Status::kMoving) status_ = Status::kIdle;
    dir_ = 0;
  }

  // Signed shortest error from position to target (positive = forward).
  int32_t error(int32_t count) const {
    const int32_t rev = cfg_.countsPerRev;
    int32_t fwd = wrap(target_ - wrap(count, rev), rev);
    if (cfg_.allowReverse && fwd > rev / 2) return fwd - rev;
    return fwd;
  }

  MotorCmd update(int32_t count, uint32_t now) {
    MotorCmd cmd;
    if (status_ != Status::kMoving) return cmd;

    if (count != lastCount_) {
      lastCount_ = count;
      lastProgressAt_ = now;
    }
    if (now - startedAt_ > cfg_.moveTimeoutMs) return finish(Status::kTimeout);
    if (dir_ != 0 && now - lastProgressAt_ > cfg_.stallTimeoutMs)
      return finish(Status::kStalled);

    const int32_t rev = cfg_.countsPerRev;
    int32_t err = error(count);
    int32_t mag = err < 0 ? -err : err;

    // Forward-only: a small overshoot shows up as an almost full turn of
    // forward error. Accept it rather than going all the way round again.
    if (!cfg_.allowReverse) {
      int32_t band = rev / 32 > cfg_.tolerance * 4 ? rev / 32 : cfg_.tolerance * 4;
      if (rev - mag <= band) mag = 0;
    }
    if (mag <= cfg_.tolerance) return finish(Status::kDone);

    // Ramp down over the last 1/8 turn.
    int32_t slowZone = rev / 8 > 1 ? rev / 8 : 1;
    int32_t span = cfg_.pwmMax - cfg_.pwmMin;
    int32_t pwm = mag >= slowZone ? cfg_.pwmMax : cfg_.pwmMin + span * mag / slowZone;

    dir_ = err > 0 ? 1 : -1;
    cmd.dir = dir_;
    cmd.pwm = static_cast<uint8_t>(pwm);
    return cmd;
  }

  Status status() const { return status_; }
  bool busy() const { return status_ == Status::kMoving; }
  int32_t target() const { return target_; }

 private:
  MotorCmd finish(Status s) {
    status_ = s;
    dir_ = 0;
    return MotorCmd{};
  }

  MotionConfig cfg_;
  Status status_ = Status::kIdle;
  int32_t target_ = 0;
  int8_t dir_ = 0;
  uint32_t startedAt_ = 0;
  uint32_t lastProgressAt_ = 0;
  int32_t lastCount_ = 0;
};

}  // namespace furby
