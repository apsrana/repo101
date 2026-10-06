#pragma once
// Body switches, microphone and light sensor -> furby::Sensor events.

#include <Arduino.h>

#include "behavior.h"

namespace inputs {

using EventFn = void (*)(furby::Sensor);

void begin();
// suppressMic: ignore the mic (while speaking or moving, to avoid
// reacting to our own noise).
void update(uint32_t now, bool suppressMic, EventFn onEvent);

// Probe mode: print every raw input change, plus analog levels each second.
void setProbe(bool on);
bool probe();

int micLevel();    // last peak-to-peak window
int lightLevel();  // smoothed ADC reading, -1 if disabled

}  // namespace inputs
