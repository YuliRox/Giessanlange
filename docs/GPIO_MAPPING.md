# GPIO Mapping

This document summarizes the current `ESP32-C6-DevKitC-1` GPIO usage for the rework.

Source of truth:

- `schematics/giessanlage/giessanlage.kicad_sch` — for everything actually wired today.
- This file — for additional locked-in allocations not yet on the schematic (e-paper display, battery ADC).

## Current Functional Mapping

Wired in the schematic today:

- `GPIO0`  -> `TOF XSHUT`
- `GPIO1`  -> cancel button, `INPUT_PULLUP`
- `GPIO6`  -> shared `I2C SDA` (Chirp + TOF)
- `GPIO7`  -> shared `I2C SCL` (Chirp + TOF)
- `GPIO10` -> pump 2 button, `INPUT_PULLUP`
- `GPIO11` -> pump 1 button, `INPUT_PULLUP`
- `GPIO16` -> ultrasonic sensor power enable, PMOS gate via 100R series
- `GPIO17` -> ultrasonic sensor UART receive
- `GPIO22` -> pump 2 MOSFET gate (`Q2`)
- `GPIO23` -> pump 1 MOSFET gate (`Q1`)

Wired in the schematic for the e-paper display (added 2026-05-25):

- `GPIO18` -> e-paper display `DIN` (`SPI MOSI`)
- `GPIO19` -> e-paper display `CLK` (`SPI SCK`)
- `GPIO20` -> e-paper display `CS`, active-low
- `GPIO21` -> e-paper display `RST`, active-low
- `GPIO3`  -> e-paper display `DC` (data/command select) — consumes `ADC1_CH3`
- `GPIO2`  -> e-paper display `BUSY`, input — consumes `ADC1_CH2`

Reserved for a future build (not yet wired):

- `GPIO4`  -> battery voltage divider, `ADC1_CH4` (see #35)

Reserved as spare ADC:

- `GPIO5`  -> last remaining ADC-capable pin, `ADC1_CH5`

## ADC Capability (ESP32-C6)

The ESP32-C6 has **only ADC1** (no ADC2) with 7 channels:

| Channel | GPIO | Status |
|---|---|---|
| ADC1_CH0 | GPIO0  | consumed (TOF XSHUT, digital) |
| ADC1_CH1 | GPIO1  | consumed (cancel button, digital) |
| ADC1_CH2 | GPIO2  | consumed (e-paper BUSY, digital) |
| ADC1_CH3 | GPIO3  | consumed (e-paper DC, digital) |
| ADC1_CH4 | GPIO4  | **battery voltage divider** |
| ADC1_CH5 | GPIO5  | spare |
| ADC1_CH6 | GPIO6  | consumed (I2C SDA) |

## Pins to Avoid

- `GPIO8`, `GPIO9` — boot-mode strapping pins. Wrong level at reset causes the chip to fail to boot. **Do not repurpose.**
- `GPIO12`, `GPIO13` — USB-Serial-JTAG (D-/D+). Repurposing breaks flashing and debugging over USB.
- `GPIO15` — strapping pin for ROM-message UART verbosity. Cosmetic only, but no reason to use.
- `GPIO14` — **does not exist on the DevKitC-1 headers.**

## Notes on Strapping Pins

The DevKitC-1 user guide labels `GPIO4`, `GPIO5`, `GPIO8`, `GPIO9`, `GPIO15` as strapping pins. The implications for *this* project:

- `GPIO4` / `GPIO5` — strap the JTAG source. Gated by the `JTAG_SEL_ENABLE` eFuse, which this project will never burn. The strap sample is read into a discard register at reset and has no functional effect. Pad-JTAG over GPIO4/5/6/7 is already permanently unavailable on this board because GPIO6/7 are used for I2C. **Safe to use as ADC.**
- `GPIO8` / `GPIO9` — chip boot mode. Hard requirement to leave unconnected (or pulled to the safe boot levels).
- `GPIO15` — ROM message verbosity at boot. Cosmetic.
