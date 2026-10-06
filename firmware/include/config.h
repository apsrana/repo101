#pragma once
// Pin map and tunables. See docs/WIRING.md for the matching wiring diagram.
// Every Furby-side wire must be identified with a meter before connecting:
// run the `probe` console command to confirm each input as you wire it.

#include <stdint.h>

// ---- Motor driver (DRV8833, channel A) ---------------------------------
constexpr int PIN_MOTOR_IN1   = 25;   // PWM
constexpr int PIN_MOTOR_IN2   = 26;   // PWM
constexpr int PIN_MOTOR_SLEEP = 27;   // DRV8833 nSLEEP, HIGH = enabled

constexpr uint32_t MOTOR_PWM_FREQ = 20000;  // above hearing range
constexpr uint8_t  MOTOR_PWM_BITS = 8;
constexpr uint8_t  MOTOR_PWM_MAX  = 230;    // cap duty: ~5.4 V average from 6 V
constexpr uint8_t  MOTOR_PWM_MIN  = 110;    // below this the gearbox stalls
constexpr uint8_t  MOTOR_PWM_JOG  = 160;

// ---- Gearbox position sensing ------------------------------------------
// Two-channel optical quadrature encoder on the motor board plus a cam
// "home" contact switch. Phototransistor outputs need a pull-up; the
// internal ~45k is usually enough, add 10k to 3V3 if edges look slow.
constexpr int  PIN_ENC_A       = 32;
constexpr int  PIN_ENC_B       = 33;
constexpr int  PIN_HOME        = 14;
constexpr bool HOME_ACTIVE_LOW = true;

// Overwritten by the `cal` console command (stored in flash).
constexpr int32_t DEFAULT_COUNTS_PER_REV = 1200;
constexpr int32_t POSITION_TOLERANCE     = 6;    // counts
constexpr uint32_t STALL_TIMEOUT_MS      = 400;  // no encoder edge while driving
constexpr uint32_t MOVE_TIMEOUT_MS       = 6000;
// Whether the cam may be driven backwards. The 2012 gearbox is
// bidirectional, but set false if reverse slips a clutch on your unit.
constexpr bool ALLOW_REVERSE = true;

// ---- Body switches (active low, internal pull-ups) ---------------------
// GPIO 16/17 are free on WROOM-32 modules; on WROVER (PSRAM) boards move
// the tummy/tongue switches elsewhere.
constexpr int PIN_SW_HEAD   = 13;
constexpr int PIN_SW_BACK   = 4;
constexpr int PIN_SW_TUMMY  = 16;
constexpr int PIN_SW_TONGUE = 17;
constexpr int PIN_SW_TAIL   = 23;
constexpr int PIN_SW_TILT   = 18;   // ball tilt switch: active when upside down
constexpr uint32_t SWITCH_DEBOUNCE_MS = 25;

// ---- Analog inputs -----------------------------------------------------
constexpr int PIN_MIC   = 36;  // MAX9814 OUT (biased at ~1.25 V)
constexpr int PIN_LIGHT = 39;  // forehead light sensor divider, -1 to disable
constexpr int MIC_LOUD_THRESHOLD = 900;  // peak-to-peak ADC counts in 30 ms
constexpr int LIGHT_DARK_THRESHOLD = 300; // ADC counts, below = dark

// ---- Audio out (MAX98357A I2S amp -> original 8 ohm speaker) -----------
constexpr int PIN_I2S_BCLK = 19;
constexpr int PIN_I2S_LRC  = 21;
constexpr int PIN_I2S_DIN  = 22;
constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;

// ---- Misc --------------------------------------------------------------
constexpr int PIN_STATUS_LED = 2;
constexpr uint32_t SLEEP_AFTER_MS = 5UL * 60UL * 1000UL;
