#pragma once
// Synthesized "Furbish": strings of chirpy syllables with pitch glides.
// Used whenever no WAV file exists for a sound, so the Furby can talk
// without any audio assets.

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "behavior.h"  // Rng
#include "sounds.h"

namespace furby {

class Babbler {
 public:
  void start(Sound style, uint32_t sampleRate, uint32_t seed) {
    rate_ = sampleRate;
    rng_ = Rng(seed);
    style_ = style;
    phase_ = 0;
    switch (style) {
      case Sound::kGiggle: syllablesLeft_ = rng_.range(7, 11); break;
      case Sound::kYawn:
      case Sound::kWhoa:
      case Sound::kPurr:   syllablesLeft_ = 1; break;
      case Sound::kHello:
      case Sound::kYum:    syllablesLeft_ = 3; break;
      case Sound::kHappy:  syllablesLeft_ = rng_.range(3, 5); break;
      case Sound::kChatter: syllablesLeft_ = rng_.range(4, 8); break;
      default:             syllablesLeft_ = 0; break;
    }
    total_ = syllablesLeft_;
    sylPos_ = sylLen_ = gapLeft_ = 0;
    nextSyllable();
  }

  bool done() const { return syllablesLeft_ == 0 && sylPos_ >= sylLen_ && gapLeft_ == 0; }

  // Renders up to n mono samples; returns how many were written.
  size_t render(int16_t* out, size_t n) {
    size_t i = 0;
    while (i < n && !done()) {
      if (sylPos_ < sylLen_) {
        float t = static_cast<float>(sylPos_) / sylLen_;
        float f = f0_ + (f1_ - f0_) * t;
        if (style_ == Sound::kPurr) f += 18.0f * sinf(2.0f * kPi * 25.0f * sylPos_ / rate_);
        phase_ += 2.0f * kPi * f / rate_;
        if (phase_ > 2.0f * kPi) phase_ -= 2.0f * kPi;
        // Attack 10%, sustain, release 30%.
        float env = t < 0.1f ? t / 0.1f : (t > 0.7f ? (1.0f - t) / 0.3f : 1.0f);
        float v = sinf(phase_) + 0.35f * sinf(2 * phase_) + 0.15f * sinf(3 * phase_);
        out[i++] = static_cast<int16_t>(v * env * 14000.0f);
        ++sylPos_;
      } else if (gapLeft_ > 0) {
        out[i++] = 0;
        --gapLeft_;
      } else {
        nextSyllable();
      }
    }
    return i;
  }

 private:
  static constexpr float kPi = 3.14159265f;

  uint32_t ms(uint32_t m) const { return rate_ * m / 1000; }

  void nextSyllable() {
    if (syllablesLeft_ == 0) return;
    uint32_t idx = total_ - syllablesLeft_;
    --syllablesLeft_;
    sylPos_ = 0;
    float base = static_cast<float>(rng_.range(520, 820));
    switch (style_) {
      case Sound::kGiggle:
        f0_ = base * 1.5f; f1_ = f0_ * 1.15f;
        sylLen_ = ms(rng_.range(55, 75)); gapLeft_ = ms(rng_.range(25, 40));
        break;
      case Sound::kYawn:
        f0_ = 650; f1_ = 260; sylLen_ = ms(1200); gapLeft_ = 0;
        break;
      case Sound::kWhoa:
        f0_ = 500; f1_ = 1300; sylLen_ = ms(700); gapLeft_ = 0;
        break;
      case Sound::kPurr:
        f0_ = 170; f1_ = 150; sylLen_ = ms(1100); gapLeft_ = 0;
        break;
      case Sound::kHappy:
        f0_ = base * (1.0f + 0.12f * idx); f1_ = f0_ * 1.3f;
        sylLen_ = ms(rng_.range(110, 160)); gapLeft_ = ms(rng_.range(30, 60));
        break;
      case Sound::kHello: {
        // "Dah-ay-loh": up, up, down.
        static const float kShape[3][2] = {{600, 760}, {800, 950}, {760, 520}};
        f0_ = kShape[idx % 3][0]; f1_ = kShape[idx % 3][1];
        sylLen_ = ms(idx == 2 ? 260 : 150); gapLeft_ = ms(40);
        break;
      }
      case Sound::kYum:
        f0_ = 420; f1_ = 380; sylLen_ = ms(idx == 2 ? 350 : 140); gapLeft_ = ms(50);
        break;
      default:
        f0_ = base; f1_ = base * (rng_.range(0, 1) ? 1.25f : 0.8f);
        sylLen_ = ms(rng_.range(90, 180)); gapLeft_ = ms(rng_.range(30, 70));
        break;
    }
    if (sylLen_ == 0) sylLen_ = 1;
  }

  Rng rng_;
  Sound style_ = Sound::kNone;
  uint32_t rate_ = 16000;
  uint32_t syllablesLeft_ = 0, total_ = 0;
  uint32_t sylPos_ = 0, sylLen_ = 0, gapLeft_ = 0;
  float f0_ = 0, f1_ = 0, phase_ = 0;
};

}  // namespace furby
