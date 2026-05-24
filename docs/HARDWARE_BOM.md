# Hardware BOM

This document is the consolidated hardware bill of materials for the current rework of the watering device.

Current design assumptions:

- controller is `ESP32-C6-DevKitC-1`
- `2` DC pumps, each about `14V / 24W`
- one low-side `N-MOSFET` switch channel per pump
- pump channels are independently controllable
- `Chirp`, `VL53L0X`, and ultrasonic sensor are powered from `ESP32 3V3`
- gate pulldown resistors are `10k ohm`
- the KiCad schematic is the source of truth for concrete part/value choices

## Core Control Electronics

- `1x` `ESP32-C6-DevKitC-1`
- `1x` perfboard or stripboard
- `1x` header/socket set for the ESP32 board, if needed

## Pump Driver Stage

- `2x` `IRLZ34N`
- `2x` `1N4004`
- `2x` `100 ohm` gate resistor
- `2x` `10k ohm` gate pulldown resistor

## Sensor Power Switching

- `1x` `IRLML6402` for ultrasonic sensor high-side power switching
- `1x` `100 ohm` gate resistor
- `1x` `10k ohm` gate pull-up resistor to `3V3`

KiCad references:

- `Q1`, `Q2`: `IRLZ34N`
- `D1`, `D2`: `1N4004`
- `R1`, `R3`: `100 ohm`
- `R2`, `R4`: `10k ohm`
- `Q3`: `IRLML6402`
- `R7`: `100 ohm`
- `R8`: `10k ohm`

## Sensors

- `1x` ultrasonic sensor marked `A0221AM 38M1056`
- `1x` `TOF200C-VL53L0X`
- `1x` `Chirp` soil moisture sensor, version `2.7.5`

## I2C Support Parts

- `2x` `4.7k ohm` resistor for `SDA` and `SCL` pull-ups to `3V3`

KiCad references:

- `R5`, `R6`: `4.7k ohm`

## Connectors and Terminals

- `5x` 2-pin screw terminal blocks
- `1x` 3-pin screw terminal block, or equivalent power distribution terminal
- `1x` `GX16` 4-pin connector set for the ultrasonic sensor
- `2x` `GX16` 6-pin connector sets for `Chirp` and `TOF`

Typical use:

- pump 1 output
- pump 2 output
- power input
- spare distribution / convenience
- sensor or auxiliary breakout as needed

Sensor connector note:

- `GX16-4` is planned for the ultrasonic sensor
- `GX16-6` is planned for the `Chirp` and `TOF` sensors

## Existing System Parts Retained

These are part of the system but not newly introduced by the rework:

- `2x` lead-acid battery, `12V 7.2Ah`
- `1x` solar charge controller with USB-A power outputs
- `2x` DC pump, about `14V / 24W`
- `1x` runtime potentiometer, if retained
- `1x` `12h/24h` interval switch, if retained
- manual and cancel buttons, if retained

## Current Pin Plan

- `GPIO6` -> shared `I2C SDA`
- `GPIO7` -> shared `I2C SCL`
- `GPIO16` -> ultrasonic sensor power enable, active-low
- `GPIO17` -> ultrasonic sensor UART receive
- `GPIO18` -> Pump 1 MOSFET gate
- `GPIO19` -> Pump 2 MOSFET gate
- `GPIO20` -> `TOF_XSHUT`
- `GPIO2` -> potentiometer ADC input, if retained

## Notes

- The `VL53L0X` and `Chirp` share the same I2C bus
- `Chirp` default I2C address: `0x20`
- `VL53L0X` expected default I2C address: `0x29`
- The ultrasonic sensor is treated as a `3.3V` UART-output sensor
- The ultrasonic sensor `VCC` is switched by `Q3` rather than tied directly to `3V3`
- The `ESP32-C6-DevKitC-1` `3V3` rail should be able to power the three sensors together under normal conditions

## Related Documents

- `ESP32_MOSFET_NEXT_STEPS.md`
- `SENSOR_WIRING_NOTES.md`
- `GPIO_MAPPING.md`
- `README.md`
