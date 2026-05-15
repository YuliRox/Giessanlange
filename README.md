Gießanlage mit Solarpanel und Laderegler

## Electronics and Wiring Notes

This repository does not currently contain a schematic, PCB design, or photo-based wiring documentation.
The notes below are reconstructed from the firmware and should be treated as the current best-known wiring map.

## Controller

- Target board: Arduino Nano / ATmega328
- Source: `platformio.ini`

## Pin Mapping

- `A2`: potentiometer for pump runtime per watering cycle
- `D3`: manual start button
- `D2`: cancel/stop button
- `D4`: pump relay control
- `D5`: 12h/24h interval switch

## Expected Wiring Behavior

- `D2`, `D3`, and `D5` are configured as `INPUT_PULLUP`
- That means each button or switch input is expected to connect the pin to `GND` when closed
- The pump relay is active-low
- That means `D4 = LOW` turns the pump on and `D4 = HIGH` turns it off

## Functional Behavior

- The potentiometer on `A2` sets how long the pump runs each time it is activated
- The runtime is mapped from about `5s` to `60s`
- It does not change the 12h/24h watering interval
- The button on `D3` starts a manual pump cycle
- The button on `D2` stops the current pump cycle
- The switch on `D5` selects the watering interval
- `D5 = HIGH` selects `12h`
- `D5 = LOW` selects `24h`

## Likely External Connections

- Potentiometer: one outer pin to `5V`, the other outer pin to `GND`, and the wiper to `A2`
- Manual button: one side to `D3`, the other side to `GND`
- Cancel button: one side to `D2`, the other side to `GND`
- 12h/24h switch: one side to `D5`, the other side to `GND`
- Relay module: control input to `D4`, module `GND` to Nano `GND`, and module `VCC` to the supply expected by the relay hardware used

## ASCII Wiring Sketch

```text
                           +----------------------+
                           |     Arduino Nano     |
                           |                      |
             Pot wiper ----| A2                   |
      Manual button   -----| D3                   |
      Cancel button   -----| D2                   |
     Interval switch  -----| D5                   |
       Relay control  -----| D4                   |
                           |                      |
                  5V  -----| 5V                   |
                 GND  -----| GND                  |
                           +----------------------+

Potentiometer
  outer pin 1 -> 5V
  outer pin 2 -> GND
  wiper       -> A2

Manual button
  D3 ---[ button ]--- GND

Cancel button
  D2 ---[ button ]--- GND

12h/24h switch
  D5 ---[ switch ]--- GND

Relay module
  D4  -> IN
  GND -> GND
  VCC -> relay supply VCC

Pump power path
  Not documented in the repo.
  Likely switched externally by the relay module.
```

## What Is Still Missing

- Exact pump voltage
- Exact relay module type
- Power path between solar panel, charge controller, battery, Nano, and pump
- Whether a dedicated transistor, MOSFET, or relay board is used
- Whether the OLED helper library is planned hardware or leftover code

## Code References

- `platformio.ini`
- `src/main.cpp`
- `lib/Oled/src/Oled.h`

## Related Notes

- `ESP32_MOSFET_NEXT_STEPS.md`: migration and assembly notes for an `ESP32-C6-DevKitC-1` with two MOSFET-switched pumps
