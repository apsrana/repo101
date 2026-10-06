#pragma once
// Speaker output through a MAX98357A I2S amplifier. Plays
// /sounds/<name>.wav from LittleFS (16-bit or 8-bit PCM, mono or stereo,
// any rate - resampled by nearest neighbour) or falls back to synthesized
// babble. Playback runs in its own FreeRTOS task.

#include <Arduino.h>

#include "sounds.h"

namespace audio {

void begin();
void play(furby::Sound s);
bool playFile(const char* path);  // any WAV path on LittleFS
void stop();
bool busy();
void setVolume(uint8_t percent);  // 0-100
uint8_t volume();

}  // namespace audio
