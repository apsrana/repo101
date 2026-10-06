// Host-side tests for the hardware-independent logic: pio test -e native

#include <unity.h>

#include <stdlib.h>

#include <algorithm>
#include <vector>

#include "animation.h"
#include "babble.h"
#include "behavior.h"
#include "debounce.h"
#include "motion.h"
#include "poses.h"
#include "wav.h"

using namespace furby;

void setUp() {}
void tearDown() {}

// ---- motion helpers ----------------------------------------------------

void test_wrap() {
  TEST_ASSERT_EQUAL(0, wrap(0, 100));
  TEST_ASSERT_EQUAL(99, wrap(-1, 100));
  TEST_ASSERT_EQUAL(1, wrap(201, 100));
}

void test_pcnt_delta() {
  TEST_ASSERT_EQUAL(5, pcntDelta(10, 15));
  TEST_ASSERT_EQUAL(-5, pcntDelta(15, 10));
  // Counter hit +32767 and reset to 0, then counted 3 more.
  TEST_ASSERT_EQUAL(1 + 3, pcntDelta(32766, 3));
  // Counter hit -32768 and reset to 0, then counted -2 more.
  TEST_ASSERT_EQUAL(-1 - 2, pcntDelta(-32767, -2));
}

void test_home_correction() {
  TEST_ASSERT_EQUAL(-5, homeCorrection(1205, 1200, 150));
  TEST_ASSERT_EQUAL(4, homeCorrection(2396, 1200, 150));
  TEST_ASSERT_EQUAL(0, homeCorrection(600, 1200, 150));
  TEST_ASSERT_EQUAL(3, homeCorrection(-1203, 1200, 150));
}

MotionConfig cfg(bool reverse) {
  MotionConfig c;
  c.countsPerRev = 1000;
  c.tolerance = 5;
  c.pwmMin = 100;
  c.pwmMax = 200;
  c.allowReverse = reverse;
  c.stallTimeoutMs = 400;
  c.moveTimeoutMs = 6000;
  return c;
}

void test_motion_shortest_path() {
  MotionController m;
  m.configure(cfg(true));
  m.moveTo(900, 0);
  MotorCmd c = m.update(100, 1);  // 200 back is shorter than 800 forward
  TEST_ASSERT_EQUAL(-1, c.dir);
  TEST_ASSERT_EQUAL(200, c.pwm);

  m.configure(cfg(false));
  m.moveTo(900, 0);
  c = m.update(100, 1);
  TEST_ASSERT_EQUAL(1, c.dir);
}

void test_motion_ramp_and_done() {
  MotionController m;
  m.configure(cfg(true));
  m.moveTo(500, 0);
  MotorCmd c = m.update(500 - 62, 1);  // half of the 125-count slow zone
  TEST_ASSERT_EQUAL(1, c.dir);
  TEST_ASSERT_UINT8_WITHIN(2, 150, c.pwm);
  c = m.update(498, 2);
  TEST_ASSERT_EQUAL(0, c.dir);
  TEST_ASSERT(m.status() == MotionController::Status::kDone);
}

void test_motion_forward_only_overshoot_accepted() {
  MotionController m;
  m.configure(cfg(false));
  m.moveTo(500, 0);
  m.update(400, 1);
  m.update(510, 2);  // overshot by 10: 990 forward to go, accept instead
  TEST_ASSERT(m.status() == MotionController::Status::kDone);
}

void test_motion_stall_and_timeout() {
  MotionController m;
  m.configure(cfg(true));
  m.moveTo(500, 0);
  m.update(0, 10);
  m.update(0, 300);
  TEST_ASSERT(m.busy());
  m.update(0, 500);  // no encoder change for 490 ms
  TEST_ASSERT(m.status() == MotionController::Status::kStalled);

  m.moveTo(500, 1000);
  for (uint32_t t = 1000, pos = 0; t < 8000; t += 100, ++pos) m.update(pos, t);
  TEST_ASSERT(m.status() == MotionController::Status::kTimeout);
}

void test_motion_simulated_plant_converges() {
  MotionController m;
  m.configure(cfg(true));
  int32_t pos = 0;
  uint32_t t = 0;
  m.moveTo(730, t);
  while (m.busy() && t < 5000) {
    MotorCmd c = m.update(pos, t);
    pos += c.dir * (c.pwm / 20);  // crude motor: speed proportional to pwm
    t += 5;
  }
  TEST_ASSERT(m.status() == MotionController::Status::kDone);
  TEST_ASSERT_INT_WITHIN(15, 730, wrap(pos, 1000));
}

// ---- debounce ----------------------------------------------------------

void test_debounce() {
  Debouncer d(25);
  TEST_ASSERT_EQUAL(Debouncer::kNone, d.update(true, 0));
  TEST_ASSERT_EQUAL(Debouncer::kNone, d.update(false, 5));  // bounce
  TEST_ASSERT_EQUAL(Debouncer::kNone, d.update(true, 10));
  TEST_ASSERT_EQUAL(Debouncer::kNone, d.update(true, 30));
  TEST_ASSERT_EQUAL(Debouncer::kPressed, d.update(true, 35));
  TEST_ASSERT_EQUAL(Debouncer::kNone, d.update(true, 100));
  d.update(false, 200);
  TEST_ASSERT_EQUAL(Debouncer::kReleased, d.update(false, 230));
}

// ---- poses -------------------------------------------------------------

void test_poses() {
  PoseTable t;
  t.setDefaults(1000);
  TEST_ASSERT_EQUAL(0, t.get(Pose::kHome));
  TEST_ASSERT_EQUAL(250, t.get(Pose::kMouthOpen));
  Pose p;
  TEST_ASSERT_TRUE(poseFromName("ears_up", p));
  TEST_ASSERT(p == Pose::kEarsUp);
  TEST_ASSERT_FALSE(poseFromName("nope", p));
}

// ---- wav ---------------------------------------------------------------

std::vector<uint8_t> makeWav(uint16_t fmt, uint16_t ch, uint32_t rate, uint16_t bits,
                             bool extraChunk) {
  std::vector<uint8_t> v;
  auto s = [&](const char* x) { v.insert(v.end(), x, x + 4); };
  auto u32 = [&](uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back(x >> (8 * i)); };
  auto u16 = [&](uint16_t x) { v.push_back(x); v.push_back(x >> 8); };
  s("RIFF"); u32(0); s("WAVE");
  s("fmt "); u32(16); u16(fmt); u16(ch); u32(rate); u32(rate * ch * bits / 8);
  u16(ch * bits / 8); u16(bits);
  if (extraChunk) { s("LIST"); u32(3); v.push_back(1); v.push_back(2); v.push_back(3); v.push_back(0); }
  s("data"); u32(8);
  for (int i = 0; i < 8; ++i) v.push_back(i);
  return v;
}

void test_wav_parse() {
  WavInfo w;
  auto a = makeWav(1, 1, 16000, 16, false);
  TEST_ASSERT_TRUE(parseWav(a.data(), a.size(), w));
  TEST_ASSERT_EQUAL(1, w.channels);
  TEST_ASSERT_EQUAL(16000, w.sampleRate);
  TEST_ASSERT_EQUAL(44, w.dataOffset);
  TEST_ASSERT_EQUAL(8, w.dataSize);

  auto b = makeWav(1, 2, 22050, 8, true);  // odd-sized chunk gets padded
  TEST_ASSERT_TRUE(parseWav(b.data(), b.size(), w));
  TEST_ASSERT_EQUAL(44 + 12, w.dataOffset);

  auto c = makeWav(3, 1, 16000, 32, false);  // float: rejected
  TEST_ASSERT_FALSE(parseWav(c.data(), c.size(), w));
  TEST_ASSERT_FALSE(parseWav(a.data(), 10, w));
}

// ---- babble ------------------------------------------------------------

void test_babble_terminates_and_is_audible() {
  for (uint8_t s = 1; s < static_cast<uint8_t>(Sound::kCount); ++s) {
    Babbler b;
    b.start(static_cast<Sound>(s), 16000, 42);
    int16_t buf[256];
    size_t total = 0;
    int peak = 0;
    while (!b.done() && total < 16000 * 5) {
      size_t n = b.render(buf, 256);
      for (size_t i = 0; i < n; ++i) peak = std::max(peak, abs(buf[i]));
      total += n;
    }
    TEST_ASSERT_TRUE_MESSAGE(b.done(), soundName(static_cast<Sound>(s)));
    TEST_ASSERT_GREATER_THAN(1000, total);
    TEST_ASSERT_GREATER_THAN(5000, peak);
  }
  Babbler none;
  none.start(Sound::kNone, 16000, 1);
  TEST_ASSERT_TRUE(none.done());
}

// ---- sequencer ---------------------------------------------------------

void test_sequencer_runs_steps() {
  static const Step steps[] = {{Pose::kMouthOpen, 100, Sound::kHello, true},
                               {Pose::kHome, 0, Sound::kNone, false}};
  static const Animation a = {"t", steps, 2};
  Sequencer s;
  s.start(&a);
  auto r = s.update(0, true, true);
  TEST_ASSERT_TRUE(r.move);
  TEST_ASSERT(r.pose == Pose::kMouthOpen);
  r = s.update(10, false, true);  // still moving
  TEST_ASSERT_FALSE(r.move || r.play);
  r = s.update(20, true, true);  // arrived: start sound
  TEST_ASSERT_TRUE(r.play);
  TEST_ASSERT(r.sound == Sound::kHello);
  r = s.update(200, true, false);  // hold over but sound still playing
  TEST_ASSERT_FALSE(r.move);
  r = s.update(300, true, true);  // next step
  TEST_ASSERT_TRUE(r.move);
  TEST_ASSERT(r.pose == Pose::kHome);
  s.update(310, true, true);
  s.update(320, true, true);
  TEST_ASSERT_FALSE(s.busy());
}

void test_all_animations_finish() {
  for (uint8_t i = 0; i < anim::kAllCount; ++i) {
    Sequencer s;
    s.start(anim::kAll[i]);
    uint32_t t = 0;
    while (s.busy() && t < 60000) {
      s.update(t, true, true);
      t += 10;
    }
    TEST_ASSERT_FALSE_MESSAGE(s.busy(), anim::kAll[i]->name);
  }
  TEST_ASSERT_NOT_NULL(anim::find("giggle"));
  TEST_ASSERT_NULL(anim::find("nope"));
}

// ---- brain -------------------------------------------------------------

Brain makeBrain() {
  Brain b;
  Brain::Config c;
  c.sleepAfterMs = 60000;
  b.begin(c, 0, 123);
  return b;
}

void test_brain_wakes_on_boot() {
  Brain b = makeBrain();
  TEST_ASSERT_NULL(b.poll(0, true));  // waits while busy (homing)
  TEST_ASSERT_EQUAL_PTR(&anim::wake, b.poll(1, false));
  TEST_ASSERT(b.state() == Brain::State::kAwake);
}

void test_brain_reactions() {
  Brain b = makeBrain();
  b.poll(0, false);
  b.onSensor(Sensor::kTongue, 100);
  TEST_ASSERT_EQUAL_PTR(&anim::eat, b.poll(101, false));
  b.onSensor(Sensor::kHead, 200);
  b.onSensor(Sensor::kTilt, 210);  // urgent replaces pending
  b.onSensor(Sensor::kBack, 220);  // lower priority ignored
  TEST_ASSERT_EQUAL_PTR(&anim::whoa, b.poll(230, false));
}

void test_brain_tickle_escalates() {
  Brain b = makeBrain();
  b.poll(0, false);
  b.onSensor(Sensor::kTummy, 1000);
  TEST_ASSERT_EQUAL_PTR(&anim::giggle, b.poll(1001, false));
  b.onSensor(Sensor::kTummy, 1500);
  b.poll(1501, false);
  b.onSensor(Sensor::kTummy, 2000);
  TEST_ASSERT_EQUAL_PTR(&anim::big_giggle, b.poll(2001, false));
}

void test_brain_idles_then_sleeps_and_wakes() {
  Brain b = makeBrain();
  b.poll(0, false);
  bool sawIdle = false;
  const Animation* a = nullptr;
  for (uint32_t t = 10; t < 59000; t += 10) {
    a = b.poll(t, false);
    if (a == &anim::blink || a == &anim::look_around || a == &anim::talk) sawIdle = true;
  }
  TEST_ASSERT_TRUE(sawIdle);
  TEST_ASSERT_EQUAL_PTR(&anim::go_sleep, b.poll(60001, false));
  TEST_ASSERT(b.state() == Brain::State::kAsleep);
  for (uint32_t t = 60010; t < 120000; t += 100) TEST_ASSERT_NULL(b.poll(t, false));

  b.onSensor(Sensor::kDark, 120000);  // darkness doesn't wake
  TEST_ASSERT_NULL(b.poll(120001, false));
  b.onSensor(Sensor::kBack, 120100);
  TEST_ASSERT_EQUAL_PTR(&anim::wake, b.poll(120101, false));
}

void test_brain_dark_sleeps_and_loud_cooldown() {
  Brain b = makeBrain();
  b.poll(0, false);
  b.onSensor(Sensor::kLoud, 100);
  TEST_ASSERT_EQUAL_PTR(&anim::listen, b.poll(101, false));
  b.onSensor(Sensor::kLoud, 1000);  // within cooldown
  TEST_ASSERT_FALSE(b.hasPending());
  b.onSensor(Sensor::kDark, 2000);
  TEST_ASSERT_EQUAL_PTR(&anim::go_sleep, b.poll(2001, false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wrap);
  RUN_TEST(test_pcnt_delta);
  RUN_TEST(test_home_correction);
  RUN_TEST(test_motion_shortest_path);
  RUN_TEST(test_motion_ramp_and_done);
  RUN_TEST(test_motion_forward_only_overshoot_accepted);
  RUN_TEST(test_motion_stall_and_timeout);
  RUN_TEST(test_motion_simulated_plant_converges);
  RUN_TEST(test_debounce);
  RUN_TEST(test_poses);
  RUN_TEST(test_wav_parse);
  RUN_TEST(test_babble_terminates_and_is_audible);
  RUN_TEST(test_sequencer_runs_steps);
  RUN_TEST(test_all_animations_finish);
  RUN_TEST(test_brain_wakes_on_boot);
  RUN_TEST(test_brain_reactions);
  RUN_TEST(test_brain_tickle_escalates);
  RUN_TEST(test_brain_idles_then_sleeps_and_wakes);
  RUN_TEST(test_brain_dark_sleeps_and_loud_cooldown);
  return UNITY_END();
}
