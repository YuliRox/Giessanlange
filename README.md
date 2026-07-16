# Gießanlage

Solar-powered ESP32-C6 plant watering controller. Two channels, MOSFET pump driver, three buttons. Talks to a local MQTT broker for status / config (in progress).

## Hardware

- **Controller**: `ESP32-C6-DevKitC-1`
- **Pumps**: 2× DC pump, ~14 V / 24 W, switched low-side via `IRLZ34N` N-MOSFETs
- **Sensors**: Chirp soil moisture (I2C), VL53L0X TOF (I2C), A02YYUW ultrasonic (UART)
- **Display**: 2.9" tri-colour e-paper, SPI (Reichelt #253924)

For details:

- **Authoritative wiring** — `schematics/giessanlage/giessanlage.kicad_sch`
- **GPIO map** — `docs/GPIO_MAPPING.md`
- **Bill of materials** — `docs/HARDWARE_BOM.md`
- **Sensor wiring notes** — `docs/SENSOR_WIRING_NOTES.md`
- **MOSFET stage details** — `docs/ESP32_MOSFET_NEXT_STEPS.md`
- **LAN OTA updates** — `docs/OTA_UPDATES.md`

The schematic is the source of truth. Docs follow it; firmware constants in `src/main.cpp` follow the docs.

## Project layout

```
src/main.cpp                 Arduino glue. The only file allowed to touch
                             Arduino headers, GPIO pin numbers, and millis().

lib/Giessanlage/             Pure state machine. Platform-independent.
                             Multi-channel, per-channel pumpTime, per-channel
                             API (Channel::One / Channel::Two).
lib/DebouncedButton/         Reusable button debouncer with std::function
                             PinReader injection. Native-testable.
lib/ChirpSensor/             I2C soil moisture wrapper.
lib/ToFSensor/               VL53L0X water-level wrapper.
lib/UltraschallSensor/       A02YYUW frame parser + UART glue.

test/test_logic/             Giessanlage state-machine tests (Unity).
test/test_button/            DebouncedButton tests.
test/test_chirp/             ChirpSensor tests.
test/test_ultraschall/       Ultrasonic frame parser tests.

docs/                        Hardware references (see list above) +
                             HAL_TESTABILITY.md (the pattern every lib/
                             component follows).

schematics/giessanlage/      KiCad project + schematic (authoritative).
```

The HAL pattern in `docs/HAL_TESTABILITY.md` is the project convention: classes that ultimately touch hardware take a `std::function` reader/writer at construction. `main.cpp` provides the real one (`digitalRead`/`digitalWrite` lambdas); tests provide a stub. This keeps every `lib/` component compilable and unit-testable in the `native` env.

## Build, flash, test

PlatformIO. Environments in `platformio.ini`:

- `esp32-c6-devkitc-1` — real hardware target, serial flash.
- `esp32-c6-devkitc-1-ota` — same firmware, flashed over WiFi (see `docs/OTA_UPDATES.md`).
- `native` — host build for unit tests.

```bash
pio run                    # build for ESP32-C6 (default env)
pio run -t upload          # flash the device over serial
pio device monitor         # serial monitor @ 115200 baud

pio run -e esp32-c6-devkitc-1-ota -t upload   # flash over LAN (docs/OTA_UPDATES.md)

pio test -e native                  # run all host unit tests
pio test -e native -f test_logic    # filter by suite directory name
```

WSL note: the `native` env needs `gcc`/`g++` on PATH. On Windows hosts, run the test commands from WSL Ubuntu (see `CLAUDE.local.md` if it exists locally for the exact venv invocation).

## Firmware API at a glance

`Giessanlage` is the state machine. Two channels, named `Channel::One` and `Channel::Two`. Per-channel `pumpTime`, shared `wateringInterval`. All time is injected via `tick(delta_ms)` — the class never calls `millis()`.

```cpp
using Channel = Giessanlage::Channel;

Giessanlage anlage(
    /*wateringTime=*/ Giessanlage::INTERVAL_24H,
    /*pumpTimeCh1=*/  Giessanlage::INTERVAL_30S,
    /*pumpTimeCh2=*/  Giessanlage::INTERVAL_30S);

// each tick of the main loop:
anlage.tick(elapsedMs);
digitalWrite(PUMP_1_GPIO, anlage.isPumping(Channel::One) ? HIGH : LOW);
digitalWrite(PUMP_2_GPIO, anlage.isPumping(Channel::Two) ? HIGH : LOW);

// user actions:
anlage.triggerPump(Channel::One);   // manual start
anlage.stopPump(Channel::One);      // manual stop
anlage.stopAllPumps();              // cancel button

// multi-channel aggregates:
anlage.isAnyPumping();              // any channel running?
anlage.triggerAllPumps();           // start every idle channel
```

Pump outputs are **active-high** N-MOSFET gate drive (`HIGH` = pump on). Buttons are `INPUT_PULLUP`, so closed = `LOW`.

## Status

## Development Container

A dev container (PlatformIO, Node, GitHub CLI, Claude Code, and a Mosquitto MQTT broker)
is provided for building, testing, and flashing. See `.devcontainer/README.md` for setup,
USB passthrough for the ESP32, and MQTT details.

## Related Notes
Working today: pump state machine, debounced buttons, MOSFET pump driver stage (schematic; firmware drives the gates correctly). All `native` test suites green.

In progress / planned: see the GitHub issue tracker. MVP milestone covers the baseplate assembly, WiFi + MQTT integration, on-device e-paper UI, and a few safety/diagnostics items.
