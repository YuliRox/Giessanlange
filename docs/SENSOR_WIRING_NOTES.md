# Sensor Wiring Notes

This document captures the current sensor wiring plan for the `ESP32-C6-DevKitC-1`.

## ESP32 Pin Map

- `GPIO6` -> shared `I2C SDA`
- `GPIO7` -> shared `I2C SCL`
- `GPIO16` -> ultrasonic sensor power enable, active-low
- `GPIO17` -> ultrasonic sensor UART receive
- `GPIO18` -> `Pump 1` MOSFET gate
- `GPIO19` -> `Pump 2` MOSFET gate
- `GPIO20` -> `TOF_XSHUT`
- `GPIO2` -> potentiometer ADC input, if kept

## Ultrasonic Sensor

Sensor marking: `A0221AM 38M1056`

This appears to be an `A02YYUW`-type waterproof ultrasonic sensor with UART output.

Wire it like this:

- `VCC` -> switched `3V3` through the ultrasonic PMOS high-side switch
- `GND` -> `ESP32 GND`
- `TX` -> `ESP32 GPIO17`
- `RX` -> `3V3` for filtered output, or leave floating

Notes:

- Do not connect the sensor `TX` directly to the ESP32 if the sensor is powered from `5V`
- Preferred setup is to power the sensor from `3V3`
- Default UART speed is `9600 baud`
- `GPIO16` controls the ultrasonic `VCC` switch:
- `LOW` -> sensor powered
- `HIGH` -> sensor off

## TOF200C-VL53L0X

This sensor is treated as an `I2C` distance sensor.

Wire it like this:

- `VCC` -> `ESP32 3V3`
- `GND` -> `ESP32 GND`
- `SDA` -> `ESP32 GPIO6`
- `SCL` -> `ESP32 GPIO7`
- `SHUT/XSHUT` -> `ESP32 GPIO20`

Expected default I2C address:

- `0x29`

## Chirp Soil Moisture Sensor

Sensor version:

- `2.7.5`

Observed pins:

- `MISO`
- `SCL`
- `RESET`
- `VCC`
- `SDA/MOSI`
- `GND`

This sensor uses `I2C`.

Wire it like this:

- `VCC` -> `ESP32 3V3`
- `GND` -> `ESP32 GND`
- `SCL` -> `ESP32 GPIO7`
- `SDA/MOSI` -> `ESP32 GPIO6`
- `RESET` -> leave unconnected for now
- `MISO` -> leave unconnected

Notes:

- The default I2C address is `0x20`
- `MISO` is not needed for normal I2C operation
- `RESET` is optional because the sensor can be reset over I2C

## Shared I2C Bus

The `VL53L0X` and `Chirp` share the same I2C bus:

- `GPIO6` -> `SDA`
- `GPIO7` -> `SCL`

Attached devices:

- `VL53L0X` at `0x29`
- `Chirp` at `0x20`

## Sensor Connectors

The current connector plan is:

- ultrasonic sensor -> `GX16-4`
- `TOF200C-VL53L0X` -> `GX16-6`
- `Chirp` -> `GX16-6`

## I2C Pull-Ups

- `4.7k` from `SDA` to `3V3`
- `4.7k` from `SCL` to `3V3`

## 3V3 Power Budget

Current estimates for the sensors:

- ultrasonic sensor (`A02YYUW`-type): about `8 mA` average
- `VL53L0X`: about `19 mA` average while ranging, with brief peaks up to about `40 mA`
- `Chirp`: about `2.8 mA` at `3.3V` when polled continuously

Estimated totals:

- typical combined load: about `30 mA`
- short peak combined load: about `50 to 55 mA`

Conclusion:

- the `ESP32-C6-DevKitC-1` `3V3` rail should be able to power these three sensors together
- this assumes the DevKit is powered normally from `5V`
- if instability appears, the more likely issue is supply noise or transient load, not steady-state sensor current

Watch for:

- ESP32 resets when Wi-Fi starts
- I2C devices occasionally disappearing
- noisy or unstable readings during pump switching

## KiCad Representation

Recommended schematic representation:

- ultrasonic sensor: custom 4-pin symbol or `Connector_Generic:Conn_01x04`
- `VL53L0X`: custom I2C sensor symbol or suitable breakout/module symbol
- `Chirp 2.7.5`: custom 6-pin symbol or `Connector_Generic:Conn_01x06`

For the `Chirp` symbol, label the pins:

- `MISO`
- `VCC`
- `SCL`
- `SDA/MOSI`
- `RESET`
- `GND`
