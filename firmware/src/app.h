#pragma once
// Shared application state, owned by main.cpp.

#include "animation.h"
#include "behavior.h"
#include "body.h"

extern Body body;
extern furby::Brain brain;
extern furby::Sequencer sequencer;
extern bool autonomous;  // false = brain paused, console drives everything

void playAnimation(const furby::Animation* a);
// Why the chip last reset, e.g. "brownout (supply voltage dipped)".
const char* resetReason();
void stopAll();

// settings.cpp: calibration, poses and volume persisted in NVS.
void loadSettings(int32_t& countsPerRev, bool& encoderInvert);
void loadPoses();
void saveSettings();

// console.cpp
void consoleBegin();
void consoleUpdate();
