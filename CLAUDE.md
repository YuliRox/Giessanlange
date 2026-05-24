# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Solar-powered plant watering controller ("Gießanlage") on an `ESP32-C6-DevKitC-1`, switching two `14V/24W` DC pumps via low-side `IRLZ34N` N-MOSFETs. Built with PlatformIO + Arduino framework.

## Build / Run / Test

PlatformIO is the build system. Defined environments in `platformio.ini`:

- `esp32-c6-devkitc-1` — default, real hardware target. Uses the pioarduino fork of platform-espressif32.
- `native` — host build for unit tests. Compiles the `lib/` components (e.g. `Giessanlage`, `DebouncedButton`) against Unity; `main.cpp` is in `src/` and is not part of this env.

Common commands:

- `pio run` — build default env (ESP32-C6).
- `pio run -t upload` — flash the device.
- `pio device monitor` — serial monitor at `115200` baud.
- `pio test -e native` — run host unit tests (suites under `test/test_*/`, Unity).
- `pio test -e native -f test_logic` — filter by suite directory name (e.g. `test_logic`, `test_button`).

## Architecture

Two layers, deliberately separated so the core logic is testable on the host:

1. **`lib/Giessanlage/src/Giessanlage.{h,cpp}`** — pure, platform-independent state machine. Owns `wateringTimer`, `pumpTimer`, and a `State` enum (`Idle`, `PumpingManual`, `PumpingAuto`). All time is passed in via `tick(delta_ms)`; the class never calls `millis()` itself. This is what `env:native` tests exercise.
2. **`src/main.cpp`** — Arduino glue: reads buttons / potentiometer / interval switch, debounces them, calls `anlage.tick(...)`, and writes `digitalWrite(PUMP_x_GPIO, anlage.isPumping() ? PUMP_ON : PUMP_OFF)`. **Note (per repo owner): `main.cpp` is stale from the old Nano revision** — its pin map matches the README/old design but predates the schematic-driven GPIO map in `docs/GPIO_MAPPING.md` (e.g. it has no I2C, ultrasonic enable, or TOF support yet). Treat `docs/GPIO_MAPPING.md` + `schematics/giessanlage/giessanlage.kicad_sch` as the source of truth for hardware; expect `main.cpp` to need rework to match.

## Hardware reference (where to look, not what to memorize)

- `schematics/giessanlage/giessanlage.kicad_sch` — authoritative wiring.
- `docs/GPIO_MAPPING.md` — current ESP32-C6 pin assignments and pins to avoid (boot straps, USB).
- `docs/ESP32_MOSFET_NEXT_STEPS.md` — BOM and migration plan for the MOSFET pump stage + ultrasonic high-side switch.
- `docs/HARDWARE_BOM.md`, `docs/SENSOR_WIRING_NOTES.md`, `docs/datasheets/` — component-level detail.
- `README.md` — describes the *old* GPIO map (GPIO21/22/23 buttons) that `main.cpp` still uses; superseded by `GPIO_MAPPING.md` for new work.

Key invariants when touching firmware:

- Pump outputs are **active-high** N-MOSFET gate drive (`PUMP_ON = HIGH`).
- Buttons/switches use `INPUT_PULLUP`; closed = `LOW`.
- Keep `Giessanlage` free of Arduino headers so the `native` test env keeps building.
