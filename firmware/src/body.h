#pragma once
// Motor, encoder and home switch: everything that moves the cam.

#include <Arduino.h>

#include "motion.h"
#include "poses.h"

class Body {
 public:
  enum class Mode : uint8_t { kIdle, kManual, kHoming, kCalibrating, kMoving };

  void begin(int32_t countsPerRev, bool encoderInvert);
  void update();  // call every loop

  // Manual drive (console): dir +1/-1, 0 = brake. Cancels any move.
  void drive(int8_t dir, uint8_t pwm);
  void stop();

  void startHoming();
  void startCalibration();  // homes, then measures counts per revolution
  bool moveTo(int32_t count);
  bool moveTo(furby::Pose p) { return moveTo(poses_.get(p)); }

  bool busy() const { return mode_ != Mode::kIdle; }
  bool homed() const { return homed_; }
  Mode mode() const { return mode_; }
  int32_t count() const { return count_; }
  int32_t position() const { return furby::wrap(count_, cpr_); }
  int32_t countsPerRev() const { return cpr_; }
  int dutyPercent() const { return out_ * 100 / 255; }  // signed, after soft-start
  bool encoderInvert() const { return invert_; }
  bool homeActive() const { return homeActive_; }
  furby::MotionController::Status lastMoveStatus() const { return motion_.status(); }
  bool calibrationChanged() { bool c = calChanged_; calChanged_ = false; return c; }

  furby::PoseTable& poses() { return poses_; }

 private:
  void apply(int8_t dir, uint8_t pwm);
  void stepOutput(uint32_t now);
  void readEncoder();
  void onHomeEnter(int8_t dir);
  void setCountsPerRev(int32_t cpr);

  furby::MotionController motion_;
  furby::PoseTable poses_;
  Mode mode_ = Mode::kIdle;
  int8_t dir_ = 0;
  int16_t target_ = 0;  // commanded signed duty
  int16_t out_ = 0;     // signed duty actually applied (soft-started)
  uint32_t lastStepAt_ = 0;
  int32_t cpr_ = 1200;
  bool invert_ = false;
  bool homed_ = false;
  bool homeActive_ = false;
  bool calChanged_ = false;
  int16_t lastRaw_ = 0;
  int32_t count_ = 0;
  uint32_t modeStart_ = 0;
  uint32_t homeChangedAt_ = 0;
  bool homeRawLast_ = false;
  // Homing: whether the switch has been seen released, so the next
  // activation is a clean entering edge.
  bool sawRelease_ = false;
};
