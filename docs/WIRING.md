# Wiring: ESP32 brain for the 2012 Furby

The original board stays intact. Every Furby part plugs into it through
connectors, so you can unplug it, bag it, and restore the toy later.

> **Identify before you connect.** Apart from the encoder board, whose
> wiring is documented (see its section), the wire roles below come from
> photos and published teardowns, not from a schematic. Check every wire with a
> multimeter using the steps in [Identifying the Furby's wires](#identifying-the-furbys-wires)
> before you connect it to the ESP32.

## Parts

| Part | Why | Notes |
|---|---|---|
| ESP32-S3 dev board (e.g. ESP32-S3-DevKitC-1), **or** an original ESP32 WROOM-32 board | New brain | Both are supported, each with its own pin map. The S3 is the default build. Needs 8 MB+ flash for the default S3 build. A smaller board fits inside the shell more easily. |
| DRV8833 motor driver module | Drives the single cam motor | Runs from the 6 V battery directly. Handles 1.5 A per channel. |
| MAX98357A I2S amplifier breakout | Drives the original speaker | Takes 3.3–5 V. Leave the GAIN pin unconnected for 9 dB. |
| MAX9814 microphone amp module | Hearing (claps, voices) | Has its own mic. You can swap in the Furby's capsule. |
| 3.3 V buck-boost regulator (e.g. Pololu S7V8F3) | Powers the ESP32 from 4×AA | Keeps working as the batteries drop from 6.4 V to about 3.6 V. |
| 10 kΩ resistor (×1–2) | Light sensor divider | The encoder board needs no extra resistors (see the encoder section). |
| 470 µF / 10 V capacitor | Across the motor supply | Stops motor surges from resetting the ESP32. |
| 2-pin jumper or switch | Between the regulator and ESP32 3V3 | Open it while on USB (see Power). |

## Block diagram

```
 4xAA (6 V) ─┬───────────────► DRV8833 VM ──► AOUT1/AOUT2 ──► Furby motor (red 2-pin plug)
             │      +470 µF
             └──► 3.3 V buck-boost ──[jumper]──► ESP32 3V3 ──┬─► MAX98357A VIN
                                                             ├─► MAX9814 VDD
                                                             ├─► encoder board (red wire)
                                                             └─► light sensor divider
 All grounds joined: battery −, DRV8833 GND, regulator GND, ESP32 GND, modules, Furby common.
```

## Pin map

Every pin is set in [`firmware/include/config.h`](../firmware/include/config.h),
which picks the right column automatically for the chip you build for.
Change it there if your layout differs.

| ESP32-S3 GPIO | Original ESP32 GPIO | Connects to | Furby side |
|---|---|---|---|
| 15 | 25 | DRV8833 AIN1 | |
| 16 | 26 | DRV8833 AIN2 | |
| 17 | 27 | DRV8833 nSLEEP (or tie it to 3V3) | |
| — | — | DRV8833 AOUT1 / AOUT2 | Motor, red 2-pin plug on the motor board |
| 4 | 32 | Encoder channel A | Gearbox optical encoder, signal 1 |
| 5 | 33 | Encoder channel B | Gearbox optical encoder, signal 2 |
| 6 | 14 | Home switch (other leg to GND) | Cam "home" contact switch |
| 7 | 13 | Head switch (other leg to GND) | Ribbon cable `SW-*` line |
| 8 | 4 | Back switch | Ribbon cable |
| 9 | 16 | Tummy switch | Ribbon cable |
| 10 | 17 | Tongue switch | Ribbon cable |
| 11 | 23 | Tail switch | `tail` / `SW-TACT` lead |
| 12 | 18 | Tilt (ball) switch | Ribbon cable or sensor board |
| 1 | 36 (VP) | MAX9814 OUT | Mic (module mic, or the Furby capsule moved onto the module) |
| 2 | 39 (VN) | Light sensor divider midpoint | Forehead light sensor |
| 39 | 19 | MAX98357A BCLK | |
| 40 | 21 | MAX98357A LRC | |
| 41 | 22 | MAX98357A DIN | |
| — | — | MAX98357A + / − | Speaker (2 wires, about 8 Ω) |
| built-in RGB LED | 2 (built-in LED) | Status | Lit while the Furby is awake |

### ESP32-S3 notes
- The S3 map uses only pins that are free on every S3 module, including
  ones with octal PSRAM (N8R8, N16R8).
- **Don't use these S3 pins:** 0, 3, 45, 46 (boot strapping), 19/20
  (USB), 26–37 (flash/PSRAM), 43/44 (serial), 38/48 (RGB LED).
- The mic and light sensor must be on GPIO 1–10 (ADC1).
- Free for later (e.g. round LCD eyes): 13, 14, 18, 21, 42, 47.
- The console runs on the **native USB port** (labelled "USB" on
  DevKitC-1). Flash and monitor through that port.
- Flash with `pio run -t upload`. The S3 is the default build.

All switch inputs use the ESP32's internal pull-ups and read as **active when
shorted to GND**. If a Furby switch is wired to a supply line instead of a
shared ground, re-wire it as switch-to-GND.

On an original ESP32, avoid GPIO 0, 5, 12 and 15 for anything new. They
are boot-strapping pins.

## Power

- The motor driver's VM pin runs straight from the batteries. Only 3.3 V
  logic ever reaches the ESP32 pins. **Never connect a 6 V line to a GPIO.**
- **On USB:** open the regulator jumper. The ESP32 then runs from USB. With
  batteries fitted, the motor still works, because VM comes from the
  batteries. Never join USB power and the regulator output.
- **Untethered:** close the jumper and unplug USB.
- Put the 470 µF capacitor right at the DRV8833 VM/GND pins.

## Identifying the Furby's wires

Unplug each harness from the original board and work through them one at a
time. Turn on `probe` in the serial console to watch each input live.

### Motor (red 2-pin plug, motor board)
- Resistance across it should be a few ohms.
- Touch it briefly to 3 V (two AAs). The whole mechanism turns.
  Reversing the wires reverses it.
- Connect it to DRV8833 AOUT1/AOUT2. Which way round doesn't matter:
  calibration sorts out the direction.

### Optical encoder board (on the motor hub)
This is the small round green board mounted beside the motor, with a black
housing that looks like a tiny camera. A slotted wheel spins through that
housing, between an IR LED and two photodiodes. The board also carries a
74HC14/74HCT14 Schmitt-trigger chip that cleans up the signals, plus its
own LED and pull-up resistors.

Its 4-wire harness, as documented by
[RoBotz SF](http://robotzsf.blogspot.com/2013/03/furby-2012-motor-quadrature-encoder.html)
(blog post and schematic by C. Brown, March 2013):

| Encoder wire | Function | Connect to |
|---|---|---|
| **Red** | Vcc | **3V3** (see the warning below) |
| **Blue** | Ground | **GND** |
| **Black** | Channel 1 | Encoder A (S3: GPIO 4, ESP32: GPIO 32) |
| **White** | Channel 2 | Encoder B (S3: GPIO 5, ESP32: GPIO 33) |

- **No extra resistors.** The board has its own LED resistor and pull-ups,
  and the Schmitt-trigger chip actively drives both outputs high and low.
- **Power it from 3.3 V, not 5 V.** The outputs swing all the way to
  whatever voltage feeds the red wire, so 5 V there would put 5 V on ESP32
  pins. The original Furby logic runs at 3.3 V (the boards are marked
  `VCC33`), so 3.3 V is very likely what it's designed for. The blog used
  5 V only because its Arduino is a 5 V board.
- **Read the chip marking** (the 14-pin chip on the back). The blog's text
  says CD54**HC**14 and its schematic says CD74**HCT**14. An **HC** chip is
  rated down to 2 V, so 3.3 V is fine. An **HCT** chip is officially a 5 V
  part. It usually still works at 3.3 V, but if the signals don't toggle
  cleanly under `probe`, power it from 5 V and put a divider on each signal
  (10 kΩ in series, 20 kΩ to GND) to bring it down to 3.3 V.
- Never power the red wire straight from the batteries. Fresh AAs give
  6.4 V, above the chip's limit.
- The chip inverts the signals. That's harmless: `cal` works out the
  counting direction automatically.

**Check:** with the ESP32 running and `probe` on, turn the gears slowly by
hand. `encoder=` should count up one way and down the other.

### Home switch
- The **blue plastic switch** on top of the motor hub, next to the motor
  (labelled in the RoBotz SF photo). It's a two-wire contact that changes
  **once per full cam turn**. Check for continuity changes while turning
  the gears by hand.
- Its trigger point is set by a screw on the rear with red glue on it.
  Leave it alone: the firmware treats wherever it triggers as "home".
- Wire it between the home pin (S3: GPIO 6, ESP32: GPIO 14) and GND. If it reads ACTIVE except near home,
  set `HOME_ACTIVE_LOW = false`.

### Touch, tongue and tilt switches (grey ribbon, `SW-1`…`SW-11`)
- Find the **common** wire: it shows continuity to every other wire when the
  matching switch is pressed. Connect the common to GND.
- Press each body spot with `probe` on, and wire whichever line responds to
  the matching GPIO from the table.
- The tilt switch is a ball switch. It changes when you turn the Furby over.

### Speaker, mic, light sensor
- **Speaker:** two wires, about 8 Ω. Connect to the MAX98357A output.
- **Mic:** the shielded grey cable going to the yellow capsule. The shield
  is ground. The simplest option is to use the MAX9814 module's own mic and
  mount it behind the Furby's mic hole.
- **Light sensor** (forehead): measure resistance while shading it. An LDR
  swings from kΩ to MΩ. Wire it as a divider: 3V3 to sensor to the light
  pin (S3: GPIO 2, ESP32: GPIO 39), and the same pin through 10 kΩ to GND. Then tune `LIGHT_DARK_THRESHOLD` using the
  `probe` readout. Set `PIN_LIGHT = -1` to skip it.

### The LCD eyes
The eye displays are bonded directly to the original main chip and can't be
reused. The firmware leaves the eye area alone. Two round GC9A01 displays are
a common replacement (a future add-on).

## Bring-up order

1. Flash the firmware with nothing attached. Open the console (`pio device
   monitor`) and type `help`.
2. Connect the amp and speaker, then `say hello`.
3. Connect the switches, then `probe`, and press each one.
4. Connect the encoder and home switch. With `probe` on, turn the gears by
   hand and watch `encoder=` and `home_switch=`.
5. Connect the motor and batteries. Run `fwd`, then `stop`, then `rev`, then
   `stop`. Then `home`, then `cal`, then `save`.
6. Tune poses. For each pose (`poses` lists them): jog with `fwd 120` /
   `stop` until the Furby looks right, then `setpose <name>`. Check it with
   `pose <name>`. Finish with `save`.
7. `auto on`. Your Furby now runs on the ESP32.
