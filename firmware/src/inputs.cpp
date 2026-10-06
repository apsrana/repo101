#include "inputs.h"

#include "config.h"
#include "debounce.h"

namespace inputs {
namespace {

struct SwitchInput {
  int pin;
  furby::Sensor sensor;
  furby::Debouncer deb;
  bool lastRaw;
};

SwitchInput switches_[] = {
    {PIN_SW_HEAD, furby::Sensor::kHead, furby::Debouncer(SWITCH_DEBOUNCE_MS), false},
    {PIN_SW_BACK, furby::Sensor::kBack, furby::Debouncer(SWITCH_DEBOUNCE_MS), false},
    {PIN_SW_TUMMY, furby::Sensor::kTummy, furby::Debouncer(SWITCH_DEBOUNCE_MS), false},
    {PIN_SW_TONGUE, furby::Sensor::kTongue, furby::Debouncer(SWITCH_DEBOUNCE_MS), false},
    {PIN_SW_TAIL, furby::Sensor::kTail, furby::Debouncer(SWITCH_DEBOUNCE_MS), false},
    // The ball tilt switch rattles, so it gets a long debounce.
    {PIN_SW_TILT, furby::Sensor::kTilt, furby::Debouncer(300), false},
};

constexpr uint32_t kMicWindowMs = 30;
constexpr uint32_t kDarkHoldMs = 2000;
constexpr int kLightHysteresis = 80;

bool probe_ = false;
uint32_t probeReportAt_ = 0;

int micMin_ = 4095, micMax_ = 0, micLevel_ = 0;
uint32_t micWindowStart_ = 0;

int light_ = -1;
bool dark_ = false;
uint32_t darkSince_ = 0;
bool darkReported_ = false;

void updateMic(uint32_t now, bool suppress, EventFn onEvent) {
  int v = analogRead(PIN_MIC);
  if (v < micMin_) micMin_ = v;
  if (v > micMax_) micMax_ = v;
  if (now - micWindowStart_ < kMicWindowMs) return;
  micLevel_ = micMax_ - micMin_;
  micMin_ = 4095;
  micMax_ = 0;
  micWindowStart_ = now;
  if (!suppress && micLevel_ > MIC_LOUD_THRESHOLD) onEvent(furby::Sensor::kLoud);
}

void updateLight(uint32_t now, EventFn onEvent) {
  if (PIN_LIGHT < 0) return;
  int v = analogRead(PIN_LIGHT);
  light_ = light_ < 0 ? v : (light_ * 15 + v) / 16;
  if (!dark_ && light_ < LIGHT_DARK_THRESHOLD) {
    dark_ = true;
    darkSince_ = now;
    darkReported_ = false;
  } else if (dark_ && light_ > LIGHT_DARK_THRESHOLD + kLightHysteresis) {
    dark_ = false;
  }
  if (dark_ && !darkReported_ && now - darkSince_ >= kDarkHoldMs) {
    darkReported_ = true;
    onEvent(furby::Sensor::kDark);
  }
}

}  // namespace

void begin() {
  for (auto& s : switches_) pinMode(s.pin, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MIC, ADC_11db);
  if (PIN_LIGHT >= 0) analogSetPinAttenuation(PIN_LIGHT, ADC_11db);
}

void update(uint32_t now, bool suppressMic, EventFn onEvent) {
  for (auto& s : switches_) {
    bool raw = digitalRead(s.pin) == LOW;  // active low
    if (probe_ && raw != s.lastRaw)
      Serial.printf("[probe] %-6s (GPIO%d) %s\n", furby::sensorName(s.sensor), s.pin,
                    raw ? "ACTIVE" : "released");
    s.lastRaw = raw;
    if (s.deb.update(raw, now) == furby::Debouncer::kPressed) onEvent(s.sensor);
  }
  updateMic(now, suppressMic, onEvent);
  updateLight(now, onEvent);

  if (probe_ && now - probeReportAt_ >= 1000) {
    probeReportAt_ = now;
    Serial.printf("[probe] mic p2p=%d (loud>%d)  light=%d (dark<%d)\n", micLevel_,
                  MIC_LOUD_THRESHOLD, light_, LIGHT_DARK_THRESHOLD);
  }
}

void setProbe(bool on) { probe_ = on; }
bool probe() { return probe_; }
int micLevel() { return micLevel_; }
int lightLevel() { return light_; }

}  // namespace inputs
