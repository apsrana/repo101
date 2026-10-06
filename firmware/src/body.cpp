#include "body.h"

#include <driver/pcnt.h>

#include "config.h"

namespace {
constexpr uint8_t kPwmChIn1 = 0;
constexpr uint8_t kPwmChIn2 = 1;
constexpr pcnt_unit_t kPcntUnit = PCNT_UNIT_0;
constexpr uint32_t kHomeDebounceMs = 3;
constexpr uint32_t kHomingTimeoutMs = 10000;
constexpr uint32_t kCalibrationTimeoutMs = 20000;
// A second home edge closer than this to the first is switch bounce,
// not a full turn.
constexpr int32_t kMinRevCounts = 100;

void setupPcnt() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);

  // Full 4x quadrature: each channel counts both edges of one signal, with
  // the other signal deciding the direction.
  pcnt_config_t c = {};
  c.unit = kPcntUnit;
  c.counter_h_lim = 32767;
  c.counter_l_lim = -32768;

  c.channel = PCNT_CHANNEL_0;
  c.pulse_gpio_num = PIN_ENC_A;
  c.ctrl_gpio_num = PIN_ENC_B;
  c.pos_mode = PCNT_COUNT_DEC;
  c.neg_mode = PCNT_COUNT_INC;
  c.lctrl_mode = PCNT_MODE_REVERSE;
  c.hctrl_mode = PCNT_MODE_KEEP;
  pcnt_unit_config(&c);

  c.channel = PCNT_CHANNEL_1;
  c.pulse_gpio_num = PIN_ENC_B;
  c.ctrl_gpio_num = PIN_ENC_A;
  c.pos_mode = PCNT_COUNT_INC;
  c.neg_mode = PCNT_COUNT_DEC;
  pcnt_unit_config(&c);

  pcnt_set_filter_value(kPcntUnit, 250);  // ignore glitches < ~3 us
  pcnt_filter_enable(kPcntUnit);
  pcnt_counter_pause(kPcntUnit);
  pcnt_counter_clear(kPcntUnit);
  pcnt_counter_resume(kPcntUnit);
}

bool readHomeRaw() {
  bool level = digitalRead(PIN_HOME) == HIGH;
  return HOME_ACTIVE_LOW ? !level : level;
}
}  // namespace

void Body::begin(int32_t countsPerRev, bool encoderInvert) {
  pinMode(PIN_MOTOR_SLEEP, OUTPUT);
  digitalWrite(PIN_MOTOR_SLEEP, HIGH);
  ledcSetup(kPwmChIn1, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
  ledcSetup(kPwmChIn2, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
  ledcAttachPin(PIN_MOTOR_IN1, kPwmChIn1);
  ledcAttachPin(PIN_MOTOR_IN2, kPwmChIn2);
  apply(0, 0);

  pinMode(PIN_HOME, HOME_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  homeActive_ = homeRawLast_ = readHomeRaw();

  setupPcnt();
  invert_ = encoderInvert;
  setCountsPerRev(countsPerRev);
  poses_.setDefaults(cpr_);
}

void Body::setCountsPerRev(int32_t cpr) {
  cpr_ = cpr > kMinRevCounts ? cpr : DEFAULT_COUNTS_PER_REV;
  furby::MotionConfig mc;
  mc.countsPerRev = cpr_;
  mc.tolerance = POSITION_TOLERANCE;
  mc.pwmMin = MOTOR_PWM_MIN;
  mc.pwmMax = MOTOR_PWM_MAX;
  mc.allowReverse = ALLOW_REVERSE;
  mc.stallTimeoutMs = STALL_TIMEOUT_MS;
  mc.moveTimeoutMs = MOVE_TIMEOUT_MS;
  motion_.configure(mc);
}

void Body::apply(int8_t dir, uint8_t pwm) {
  dir_ = dir;
  target_ = dir * static_cast<int16_t>(pwm);
  stepOutput(millis());
}

void Body::stepOutput(uint32_t now) {
  uint32_t elapsed = now - lastStepAt_;
  lastStepAt_ = now;
  int32_t step = 255L * elapsed / (MOTOR_RAMP_MS ? MOTOR_RAMP_MS : 1);
  out_ = furby::slewDuty(out_, target_, step > 255 ? 255 : step);
  if (dir_ == 0) {
    // Both inputs high = DRV8833 brake (motor terminals shorted).
    ledcWrite(kPwmChIn1, 255);
    ledcWrite(kPwmChIn2, 255);
  } else if (out_ >= 0) {
    ledcWrite(kPwmChIn1, out_);
    ledcWrite(kPwmChIn2, 0);
  } else {
    ledcWrite(kPwmChIn1, 0);
    ledcWrite(kPwmChIn2, -out_);
  }
}

void Body::readEncoder() {
  int16_t raw = 0;
  pcnt_get_counter_value(kPcntUnit, &raw);
  int32_t d = furby::pcntDelta(lastRaw_, raw);
  lastRaw_ = raw;
  count_ += invert_ ? -d : d;
}

void Body::drive(int8_t dir, uint8_t pwm) {
  motion_.stop();
  mode_ = dir ? Mode::kManual : Mode::kIdle;
  apply(dir, pwm);
}

void Body::stop() {
  motion_.stop();
  mode_ = Mode::kIdle;
  apply(0, 0);
}

void Body::startHoming() {
  motion_.stop();
  mode_ = Mode::kHoming;
  modeStart_ = millis();
  sawRelease_ = !homeActive_;
  apply(1, MOTOR_PWM_JOG);
}

void Body::startCalibration() {
  startHoming();
  mode_ = Mode::kCalibrating;
  homed_ = false;
}

bool Body::moveTo(int32_t count) {
  if (!homed_ || mode_ == Mode::kHoming || mode_ == Mode::kCalibrating) return false;
  mode_ = Mode::kMoving;
  motion_.moveTo(count, millis());
  return true;
}

void Body::onHomeEnter(int8_t dir) {
  switch (mode_) {
    case Mode::kHoming:
      if (!sawRelease_) return;
      count_ = 0;
      homed_ = true;
      Serial.println(F("[body] homed"));
      stop();
      return;

    case Mode::kCalibrating:
      if (!sawRelease_) return;
      if (!homed_) {
        // First pass: zero here and keep going for one full turn.
        count_ = 0;
        homed_ = true;
        return;
      }
      if (abs(count_) < kMinRevCounts) return;
      {
        int32_t measured = abs(count_);
        if (count_ < 0) invert_ = !invert_;
        bool bigChange = abs(measured - cpr_) * 10 > cpr_;
        setCountsPerRev(measured);
        if (bigChange) poses_.setDefaults(cpr_);
        count_ = 0;
        calChanged_ = true;
        Serial.printf("[body] calibrated: %ld counts/rev%s%s\n", (long)cpr_,
                      invert_ ? ", encoder inverted" : "",
                      bigChange ? ", poses reset to defaults" : "");
      }
      stop();
      return;

    default:
      // Cancel accumulated drift on each forward pass of the home switch.
      if (homed_ && dir > 0) {
        int32_t corr = furby::homeCorrection(count_, cpr_, cpr_ / 8);
        count_ += corr;
        if (abs(corr) > POSITION_TOLERANCE)
          Serial.printf("[body] drift corrected by %ld\n", (long)corr);
      }
      return;
  }
}

void Body::update() {
  const uint32_t now = millis();
  readEncoder();
  stepOutput(now);

  bool raw = readHomeRaw();
  if (raw != homeRawLast_) {
    homeRawLast_ = raw;
    homeChangedAt_ = now;
  } else if (raw != homeActive_ && now - homeChangedAt_ >= kHomeDebounceMs) {
    homeActive_ = raw;
    if (homeActive_) onHomeEnter(dir_);
    else sawRelease_ = true;
  }

  switch (mode_) {
    case Mode::kHoming:
    case Mode::kCalibrating:
      if (now - modeStart_ > (mode_ == Mode::kHoming ? kHomingTimeoutMs : kCalibrationTimeoutMs)) {
        Serial.println(F("[body] home switch never triggered - check PIN_HOME wiring "
                         "(use `probe`). Treating current position as home."));
        stop();
        count_ = 0;
        homed_ = true;
      }
      break;
    case Mode::kMoving: {
      furby::MotorCmd cmd = motion_.update(count_, now);
      if (motion_.busy()) {
        apply(cmd.dir, cmd.pwm);
      } else {
        apply(0, 0);
        mode_ = Mode::kIdle;
        auto s = motion_.status();
        if (s == furby::MotionController::Status::kStalled)
          Serial.println(F("[body] stalled: no encoder movement (jammed, or encoder unwired?)"));
        else if (s == furby::MotionController::Status::kTimeout)
          Serial.println(F("[body] move timed out"));
      }
      break;
    }
    default:
      break;
  }
}
