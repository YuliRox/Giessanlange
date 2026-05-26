Gießanlage mit Solarpanel und Laderegler

## Electronics and Wiring Notes

This repository does not currently contain a schematic, PCB design, or photo-based wiring documentation.
The notes below are reconstructed from the firmware and should be treated as the current best-known wiring map.

## Controller

- Target board: ESP32-C6-DevKitC-1
- Source: `platformio.ini`

## Pin Mapping

- `GPIO2` (ADC): potentiometer for pump runtime per watering cycle
- `GPIO21`: manual start button
- `GPIO22`: cancel/stop button
- `GPIO23`: 12h/24h interval switch
- `GPIO18`: pump 1 MOSFET gate control
- `GPIO19`: pump 2 MOSFET gate control

## About Old Arduino Labels (e.g. `D1`)

Old labels like `D1`, `D2`, `A2` refer to Arduino Nano pin naming and are no longer the source of truth.
For the ESP32-C6 firmware in this repo, use only the explicit `GPIO` numbers listed above.
`D1` is currently not referenced by the firmware.

## Expected Wiring Behavior

- `GPIO21`, `GPIO22`, and `GPIO23` are configured as `INPUT_PULLUP`
- That means each button or switch input is expected to connect the pin to `GND` when closed
- Pump outputs are active-high for MOSFET gate drive
- That means `GPIO18/19 = HIGH` turns the respective pump on, `LOW` turns it off

## Functional Behavior

- The potentiometer on `GPIO2` sets how long the pump runs each time it is activated
- The runtime is mapped from about `5s` to `60s`
- It does not change the 12h/24h watering interval
- The button on `GPIO21` starts a manual pump cycle
- The button on `GPIO22` stops the current pump cycle
- The switch on `GPIO23` selects the watering interval
- `GPIO23 = HIGH` selects `12h`
- `GPIO23 = LOW` selects `24h`

## Likely External Connections

- Potentiometer: one outer pin to `3V3`, the other outer pin to `GND`, and the wiper to `GPIO2`
- Manual button: one side to `GPIO21`, the other side to `GND`
- Cancel button: one side to `GPIO22`, the other side to `GND`
- 12h/24h switch: one side to `GPIO23`, the other side to `GND`
- Pump channel 1: `GPIO18` to MOSFET gate driver path
- Pump channel 2: `GPIO19` to MOSFET gate driver path
- Common ground between ESP32-C6 and pump power stage is required

## ASCII Wiring Sketch

```text
                           +---------------------------+
                           |     ESP32-C6-DevKitC-1    |
                           |                           |
             Pot wiper ----| GPIO2 (ADC)              |
      Manual button   -----| GPIO21                   |
      Cancel button   -----| GPIO22                   |
     Interval switch  -----| GPIO23                   |
       Pump 1 control -----| GPIO18                   |
       Pump 2 control -----| GPIO19                   |
                           |                           |
                 3V3  -----| 3V3                      |
                 GND  -----| GND                      |
                           +---------------------------+

Potentiometer
  outer pin 1 -> 3V3
  outer pin 2 -> GND
  wiper       -> GPIO2

Manual button
  GPIO21 ---[ button ]--- GND

Cancel button
  GPIO22 ---[ button ]--- GND

12h/24h switch
  GPIO23 ---[ switch ]--- GND

Pump stage
  GPIO18 -> Pump 1 gate drive path
  GPIO19 -> Pump 2 gate drive path
  ESP GND -> pump power GND (common reference)

Pump power path
  Switched externally via MOSFET power stage.
```

## What Is Still Missing

- Exact pump voltage
- Exact MOSFET part numbers and resistor values as physically assembled
- Power path between solar panel, charge controller, battery, ESP32-C6, and pump
- Final power path between solar charge controller USB output and ESP32-C6 input
- Whether the OLED helper library is planned hardware or leftover code

## Code References

- `platformio.ini`
- `src/main.cpp`
- `lib/Oled/src/Oled.h`

## Development Container

A dev container (PlatformIO, Node, GitHub CLI, Claude Code, and a Mosquitto MQTT broker)
is provided for building, testing, and flashing. See `.devcontainer/README.md` for setup,
USB passthrough for the ESP32, and MQTT details.

## Related Notes

- `ESP32_MOSFET_NEXT_STEPS.md`: migration and assembly notes for an `ESP32-C6-DevKitC-1` with two MOSFET-switched pumps
