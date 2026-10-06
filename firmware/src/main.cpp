// Furby 2012 ESP32 brain transplant.
//
// Loop: read sensors -> Brain picks an animation -> Sequencer steps through
// its poses and sounds -> Body drives the cam, audio plays the sounds.

#include <Arduino.h>

#include "app.h"
#include "audio.h"
#include "config.h"
#include "inputs.h"

Body body;
furby::Brain brain;
furby::Sequencer sequencer;
bool autonomous = true;

namespace {
// Keep ignoring the mic briefly after the motor or speaker stops.
constexpr uint32_t kMicQuietTailMs = 300;
uint32_t lastNoiseAt_ = 0;

// In probe mode, report encoder and home switch changes (twice a second).
void probeBody(uint32_t now) {
  static uint32_t lastAt = 0;
  static int32_t lastCount = INT32_MIN;
  static bool lastHome = false;
  if (!inputs::probe() || now - lastAt < 500) return;
  lastAt = now;
  if (body.count() == lastCount && body.homeActive() == lastHome) return;
  lastCount = body.count();
  lastHome = body.homeActive();
  Serial.printf("[probe] encoder=%ld home_switch=%s\n", (long)lastCount,
                lastHome ? "ACTIVE" : "open");
}

void onSensor(furby::Sensor s) {
  Serial.printf("[sense] %s\n", furby::sensorName(s));
  if (autonomous) brain.onSensor(s, millis());
}
}  // namespace

void playAnimation(const furby::Animation* a) {
  Serial.printf("[anim] %s\n", a->name);
  sequencer.start(a);
}

void stopAll() {
  sequencer.cancel();
  body.stop();
  audio::stop();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_STATUS_LED, OUTPUT);

  audio::begin();
  int32_t cpr;
  bool invert;
  loadSettings(cpr, invert);
  body.begin(cpr, invert);
  loadPoses();
  inputs::begin();

  furby::Brain::Config cfg;
  cfg.sleepAfterMs = SLEEP_AFTER_MS;
  brain.begin(cfg, millis(), esp_random());

  consoleBegin();
  body.startHoming();
}

void loop() {
  const uint32_t now = millis();

  body.update();
  if (body.calibrationChanged()) Serial.println(F("type `save` to keep the calibration"));

  const bool noisy = body.busy() || audio::busy();
  if (noisy) lastNoiseAt_ = now;
  inputs::update(now, noisy || now - lastNoiseAt_ < kMicQuietTailMs, onSensor);
  probeBody(now);

  if (autonomous && body.homed()) {
    const furby::Animation* a = brain.poll(now, sequencer.busy() || body.busy());
    if (a) playAnimation(a);
  }

  furby::Sequencer::Request req = sequencer.update(now, !body.busy(), !audio::busy());
  if (req.move) body.moveTo(req.pose);
  if (req.play) audio::play(req.sound);

  digitalWrite(PIN_STATUS_LED, brain.state() == furby::Brain::State::kAwake);
  consoleUpdate();
  delay(1);
}
