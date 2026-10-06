# Wiring: ESP32 brain for the 2012 Furby

The original board stays intact. Every Furby part plugs into it through
connectors, so you can unplug it, bag it, and restore the toy later.

> **Identify before you connect.** The wire roles below come from photos
> and published teardowns, not from a schematic. Check every wire with a
> multimeter using the steps in [Identifying the Furby's wires](#identifying-the-furbys-wires)
> before you connect it to the ESP32.

## Parts

| Part | Why | Notes |
|---|---|---|
| ESP32 dev board (WROOM-32, e.g. DevKitC or a mini "D1 R32"-style board) | New brain | The pin map assumes a WROOM-32. A smaller board fits inside the shell more easily. |
| DRV8833 motor driver module | Drives the single cam motor | Runs from the 6 V battery directly. Handles 1.5 A per channel. |
| MAX98357A I2S amplifier breakout | Drives the original speaker | Takes 3.3–5 V. Leave the GAIN pin unconnected for 9 dB. |
| MAX9814 microphone amp module | Hearing (claps, voices) | Has its own mic. You can swap in the Furby's capsule. |
| 3.3 V buck-boost regulator (e.g. Pololu S7V8F3) | Powers the ESP32 from 4×AA | Keeps working as the batteries drop from 6.4 V to about 3.6 V. |
| 10 kΩ resistors (×2–4), 220 Ω (×1) | Pull-ups and LED current | See the encoder section. |
| 470 µF / 10 V capacitor | Across the motor supply | Stops motor surges from resetting the ESP32. |
| 2-pin jumper or switch | Between the regulator and ESP32 3V3 | Open it while on USB (see Power). |

## Block diagram

```
 4xAA (6 V) ─┬───────────────► DRV8833 VM ──► AOUT1/AOUT2 ──► Furby motor (red 2-pin plug)
             │      +470 µF
             └──► 3.3 V buck-boost ──[jumper]──► ESP32 3V3 ──┬─► MAX98357A VIN
                                                             ├─► MAX9814 VDD
                                                             ├─► encoder LED / pull-ups
                                                             └─► light sensor divider
 All grounds joined: battery −, DRV8833 GND, regulator GND, ESP32 GND, modules, Furby common.
```

## ESP32 pin map

Every pin is set in [`firmware/include/config.h`](../firmware/include/config.h).
Change it there if your layout differs.

| ESP32 GPIO | Connects to | Furby side |
|---|---|---|
| 25 | DRV8833 AIN1 | |
| 26 | DRV8833 AIN2 | |
| 27 | DRV8833 nSLEEP (or tie it to 3V3) | |
| — | DRV8833 AOUT1 / AOUT2 | Motor, red 2-pin plug on the motor board |
| 32 | Encoder channel A | Gearbox optical encoder, signal 1 |
| 33 | Encoder channel B | Gearbox optical encoder, signal 2 |
| 14 | Home switch (other leg to GND) | Cam "home" contact switch |
| 13 | Head switch (other leg to GND) | Ribbon cable `SW-*` line |
| 4 | Back switch | Ribbon cable |
| 16 | Tummy switch | Ribbon cable |
| 17 | Tongue switch | Ribbon cable |
| 23 | Tail switch | `tail` / `SW-TACT` lead |
| 18 | Tilt (ball) switch | Ribbon cable or sensor board |
| 36 (VP) | MAX9814 OUT | Mic (module mic, or the Furby capsule moved onto the module) |
| 39 (VN) | Light sensor divider midpoint | Forehead light sensor |
| 19 | MAX98357A BCLK | |
| 21 | MAX98357A LRC | |
| 22 | MAX98357A DIN | |
| — | MAX98357A + / − | Speaker (2 wires, about 8 Ω) |
| 2 | On-board LED | Lit while the Furby is awake |

All switch inputs use the ESP32's internal pull-ups and read as **active when
shorted to GND**. If a Furby switch is wired to a supply line instead of a
shared ground, re-wire it as switch-to-GND.

Avoid GPIO 0, 5, 12 and 15 for anything new. They are boot-strapping pins.

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

### Optical encoder (two signals)
Teardowns describe a 2-channel optical quadrature encoder: an IR LED shining
through a slotted wheel onto two phototransistors. Find it by following the
wires from the slotted wheel near the motor. The 4-wire harness (red, black,
white, yellow) is the likely candidate. It may also sit on the small upright
motor board, in which case you tap its signals there.

1. **LED:** diode-test across pairs. An IR LED reads about 1.0–1.3 V one way
   and open the other. Feed it from 3V3 through **220 Ω**, unless the
   board already has a series resistor on that line.
2. **Signals:** the other wires go to the phototransistor collectors, with
   emitters to GND. Pull each up to 3V3 with 10 kΩ, or rely on the internal
   pull-up. Turn the gears slowly by hand: each signal should toggle between
   about 0 V and 3.3 V, offset from the other by a quarter step.
3. Connect them to GPIO 32/33. With `probe` on, turning the gears should
   change `encoder=`. Direction doesn't matter yet.

### Home switch
- A two-wire contact that closes (or opens) **once per full cam turn**.
  Check for continuity changes while turning the gears by hand.
- Wire it between GPIO 14 and GND. If it reads ACTIVE except near home,
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
  swings from kΩ to MΩ. Wire it as a divider: 3V3 to sensor to GPIO 39, and
  GPIO 39 through 10 kΩ to GND. Then tune `LIGHT_DARK_THRESHOLD` using the
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
