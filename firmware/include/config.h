#pragma once
// Pin map and tunables. See docs/WIRING.md for the matching wiring diagram.
// Every Furby-side wire must be identified with a meter before connecting:
// run the `probe` console command to confirm each input as you wire it.

#include <Arduino.h>  // sdkconfig (chip target) and LED_BUILTIN

#if CONFIG_IDF_TARGET_ESP32S3
// ---- ESP32-S3 (env:esp32s3) --------------------------------------------
// Uses only pins that are free on every S3 module, including octal-PSRAM
// ones (N8R8/N16R8). Avoided: 0, 3, 45, 46 (strapping), 19/20 (USB),
// 26-37 (flash/PSRAM), 43/44 (UART0), 38/48 (RGB LED on DevKitC-1).
// Analog inputs must be on ADC1 (GPIO 1-10).
// Free for later (e.g. round LCD eyes): 13, 14, 18, 21, 42, 47.
constexpr int PIN_MOTOR_IN1   = 15;
constexpr int PIN_MOTOR_IN2   = 16;
constexpr int PIN_MOTOR_SLEEP = 17;
constexpr int PIN_ENC_A       = 4;
constexpr int PIN_ENC_B       = 5;
constexpr int PIN_HOME        = 6;
constexpr int PIN_SW_HEAD     = 7;
constexpr int PIN_SW_BACK     = 8;
constexpr int PIN_SW_TUMMY    = 9;
constexpr int PIN_SW_TONGUE   = 10;
constexpr int PIN_SW_TAIL     = 11;
constexpr int PIN_SW_TILT     = 12;
constexpr int PIN_MIC         = 1;   // ADC1_CH0
constexpr int PIN_LIGHT       = 2;   // ADC1_CH1, -1 to disable
constexpr int PIN_I2S_BCLK    = 39;
constexpr int PIN_I2S_LRC     = 40;
constexpr int PIN_I2S_DIN     = 41;
#else
// ---- Original ESP32 / WROOM-32 (env:esp32dev) --------------------------
// GPIO 16/17 are free on WROOM-32 modules; on WROVER (PSRAM) boards move
// the tummy/tongue switches elsewhere.
constexpr int PIN_MOTOR_IN1   = 25;
constexpr int PIN_MOTOR_IN2   = 26;
constexpr int PIN_MOTOR_SLEEP = 27;
constexpr int PIN_ENC_A       = 32;
constexpr int PIN_ENC_B       = 33;
constexpr int PIN_HOME        = 14;
constexpr int PIN_SW_HEAD     = 13;
constexpr int PIN_SW_BACK     = 4;
constexpr int PIN_SW_TUMMY    = 16;
constexpr int PIN_SW_TONGUE   = 17;
constexpr int PIN_SW_TAIL     = 23;
constexpr int PIN_SW_TILT     = 18;
constexpr int PIN_MIC         = 36;  // VP
constexpr int PIN_LIGHT       = 39;  // VN, -1 to disable
constexpr int PIN_I2S_BCLK    = 19;
constexpr int PIN_I2S_LRC     = 21;
constexpr int PIN_I2S_DIN     = 22;
#endif

// Built-in LED, lit while the Furby is awake. On S3 DevKitC-1 boards this
// is the RGB LED (the core drives it as white). -1 to disable.
#ifdef LED_BUILTIN
constexpr int PIN_STATUS_LED = LED_BUILTIN;
#else
constexpr int PIN_STATUS_LED = -1;
#endif

// ---- Motor (DRV8833, channel A) ----------------------------------------
// IN1/IN2 are PWM; nSLEEP HIGH = enabled.
constexpr uint32_t MOTOR_PWM_FREQ = 20000;  // above hearing range
constexpr uint8_t  MOTOR_PWM_BITS = 8;
// Sized for a 5 V (USB) motor supply. On 4xAA (6 V) lower MAX to ~230.
constexpr uint8_t  MOTOR_PWM_MAX  = 255;
constexpr uint8_t  MOTOR_PWM_MIN  = 110;    // below this the gearbox stalls
constexpr uint8_t  MOTOR_PWM_JOG  = 160;
// Soft-start: time to ramp from stopped to full power. Longer = gentler
// on a USB supply (fewer brownout resets), slower to get moving.
constexpr uint32_t MOTOR_RAMP_MS  = 80;

// ---- Gearbox position sensing ------------------------------------------
// Two-channel optical quadrature encoder board on the motor hub (wires:
// red Vcc -> 3V3, blue GND, black channel A, white channel B; its 74HC14
// drives the outputs, so no pull-ups are needed) plus the cam "home"
// contact switch.
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
// The tilt switch is a ball switch: active when upside down.
constexpr uint32_t SWITCH_DEBOUNCE_MS = 25;

// ---- Analog inputs -----------------------------------------------------
// Mic: MAX9814 OUT (biased at ~1.25 V). Light: forehead sensor divider.
constexpr int MIC_LOUD_THRESHOLD = 900;   // peak-to-peak ADC counts in 30 ms
constexpr int LIGHT_DARK_THRESHOLD = 300; // ADC counts, below = dark

// ---- Audio out (MAX98357A I2S amp -> original 8 ohm speaker) -----------
constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;

// ---- Misc --------------------------------------------------------------
constexpr uint32_t SLEEP_AFTER_MS = 5UL * 60UL * 1000UL;
