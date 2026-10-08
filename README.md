# Furby 2012: ESP32 brain transplant

This replaces the 2012 Hasbro Furby's original controller (Jetta board
`RUCT-101A` / `39834+003+01`) with an ESP32-S3 (default) or an original
ESP32. The new brain keeps the body,
the single cam motor, the touch/tilt switches, the speaker and the mic.
It gives the Furby a personality you can reprogram, plus a serial console
for tuning.

- **[docs/WIRING.md](docs/WIRING.md):** parts list, pin map, power, how to
  identify each Furby wire, and the bring-up order.
- **[firmware/](firmware/):** PlatformIO project (Arduino framework).
  Build environments: `esp32s3` (default) and `esp32dev` (original ESP32).

Separate project in this repository:
**[lenovo-smart-display-10/](lenovo-smart-display-10/)** has the unlock
tooling and the postmarketOS port for the Lenovo Smart Display 10 (`blueberry`).

## How the Furby moves

All of the Furby's motion (eyelids, ears, mouth, leaning) comes from **one
motor turning one cam**. Each expression is a cam position. The gearbox
reports position through a 2-channel optical encoder plus a "home" switch
that triggers once per turn. The firmware:

1. **Homes** at power-on: it drives until the home switch triggers and zeroes the encoder.
2. **Calibrates** with `cal`: it measures encoder counts per full turn and the encoder's direction.
3. Moves to **named poses**, stored as encoder counts, with closed-loop control
   (ramp-down, shortest direction, stall and timeout detection, and drift
   correction each time it passes home).

## Firmware layout

```
firmware/
  include/config.h            pin map and tunables
  lib/furby_core/src/         hardware-independent logic (unit tested)
    motion.h                  cam position controller, encoder maths
    poses.h                   named cam positions
    animation.h               animation scripts + sequencer
    behavior.h                personality: sensors + time -> animation
    babble.h                  synthesized "Furbish" speech
    wav.h, debounce.h, sounds.h
  src/
    main.cpp                  main loop
    body.cpp                  DRV8833 motor, PCNT hardware encoder, home switch
    audio.cpp                 I2S (MAX98357A) playback, WAV or babble
    inputs.cpp                switches, mic loudness, light sensor
    settings.cpp              calibration + poses in flash (NVS)
    console.cpp               serial command console
  data/sounds/                optional WAVs, override the babble
  test/test_core/             host-side unit tests
```

## Behaviour

| Event | Reaction |
|---|---|
| Power on | Homes the cam, then wakes up (yawn, "hello") |
| Head / back pet | Purr with eyes closed |
| Tummy | Giggle. Three tickles within 4 s gives a big giggle |
| Tongue | Eat: chomp, chomp, "yum" |
| Tail | Startle |
| Turned upside down | "Whoa!" |
| Loud sound / clap | Ears up, listen, happy reply |
| Dark for 2 s, or 5 min with no attention | Yawn and fall asleep |
| Any touch while asleep | Wake up |
| Idle | Random blinks, looking around, chatter every 4–12 s |

All sounds are synthesized, so no audio files are needed. Drop WAV files into
`firmware/data/sounds/` to replace any of them.

## Build, flash, test

```
pip install platformio
cd firmware
pio run -t upload          # build and flash (ESP32-S3)
pio run -e esp32dev -t upload   # ...or for an original ESP32
pio run -t uploadfs        # optional: upload data/ (WAV files)
pio device monitor         # console at 115200, type `help`
pio test -e native         # unit tests on your computer
```

## Console quick reference

```
probe                 live switch / encoder / home / mic / light readout
fwd [pwm] | rev | stop  jog the motor
home | cal | save     zero, measure a full turn, store
poses | pose NAME | setpose NAME [count] | resetposes
anims | anim NAME     e.g. anim giggle
say SOUND | play PATH | vol 0-100
auto on|off | sleep | wake | status
```

## Status

- The hardware-independent logic passes its unit tests on a computer.
- The ESP32 sources pass a syntax/type check, for both the S3 and the
  original ESP32 pin maps, against stub headers that copy the Arduino-ESP32
  2.0.17 / ESP-IDF 4.4 API signatures. A full `pio run`
  cross-build hasn't been done yet (the toolchain couldn't be downloaded in
  the environment this was written in).
- **Not yet run on real hardware.** Wire roles and pose positions must be
  confirmed on your unit with `probe` and the bring-up steps in
  [docs/WIRING.md](docs/WIRING.md).
