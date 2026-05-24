# GPIO Mapping

This document summarizes the current `ESP32-C6-DevKitC-1` GPIO usage for the rework.

Source of truth:

- `schematics/giessanlage/giessanlage.kicad_sch`

## Current Functional Mapping

These assignments reflect the current design decisions:

- `GPIO6` -> shared `I2C SDA`
- `GPIO7` -> shared `I2C SCL`
- `GPIO16` -> ultrasonic sensor power enable, active-low PMOS gate control
- `GPIO17` -> ultrasonic sensor UART receive
- `GPIO18` -> pump 1 MOSFET gate
- `GPIO19` -> pump 2 MOSFET gate
- `GPIO20` -> `TOF_XSHUT`
- `GPIO2` -> potentiometer ADC input, if retained

The design no longer uses `GPIO4` for the ultrasonic power switch.

## Practical Guidance

Pins that are currently good choices in this design:

- `GPIO6`
- `GPIO7`
- `GPIO16`
- `GPIO17`
- `GPIO18`
- `GPIO19`
- `GPIO20`
- `GPIO2` for ADC use

Pins to avoid unless there is a specific reason:

- `GPIO4`
- `GPIO5`
- `GPIO8`
- `GPIO9`
- `GPIO15`
- `GPIO12`
- `GPIO13`

## Notes

- `GPIO16` and `GPIO17` are currently used in the schematic and should be treated as reserved
- if USB is important for flashing/debugging, avoid repurposing `GPIO12` and `GPIO13`
- if boot reliability is important, avoid repurposing `GPIO4`, `GPIO5`, `GPIO8`, `GPIO9`, or `GPIO15`
