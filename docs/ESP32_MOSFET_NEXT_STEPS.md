# ESP32-C6 + MOSFET Pump Control: Next Steps

This document describes how to replace the Arduino Nano + relay setup with an `ESP32-C6-DevKitC-1` and two independent MOSFET pump channels.

Assumptions:

- `2` DC pumps
- each pump is rated `14 V / 24 W`
- both pumps may be switched independently in the future
- switching is done with `N-channel` logic-level MOSFETs
- low-side switching is acceptable
- the existing solar charge controller and lead-acid battery setup remain in place
- no fuse is included in this design

## 1. Parts

Per build:

- `1x` `ESP32-C6-DevKitC-1`
- `2x` `IRLZ34N`
- `2x` `1N4004`
- `2x` `100 ohm` gate resistor
- `2x` `10k ohm` gate pulldown resistor
- `2x` `4.7k ohm` I2C pull-up resistor
- `1x` `IRLML6402` for ultrasonic sensor high-side power switching
- `1x` `100 ohm` resistor for the ultrasonic PMOS gate
- `1x` `10k ohm` pull-up resistor from ultrasonic PMOS gate to `3V3`
- `1x` perfboard or stripboard
- `1x` header set for the ESP32, if the board is not already socketed
- `5x` 2-pin screw terminal blocks
- `1x` 3-pin screw terminal block, or equivalent power distribution terminal
- `1x` `GX16-4` connector set for the ultrasonic sensor
- `2x` `GX16-6` connector sets for `Chirp` and `TOF`

Schematic-selected parts:

- `Q1`, `Q2`: `IRLZ34N`
- `D1`, `D2`: `1N4004`
- `R1`, `R3`: `100 ohm`
- `R2`, `R4`: `10k ohm`
- `R5`, `R6`: `4.7k ohm`
- `Q3`: `IRLML6402`
- `R7`: `100 ohm`
- `R8`: `10k ohm`

## 2. Recommended ESP32 Pins

Use GPIOs that are easy to access and avoid the obvious footguns:

- `GPIO18` for `Pump 1`
- `GPIO19` for `Pump 2`
- `GPIO16` for ultrasonic sensor power enable, active-low
- `GPIO17` for ultrasonic sensor UART receive
- `GPIO20` for `TOF_XSHUT`
- `GPIO6` for shared `I2C SDA`
- `GPIO7` for shared `I2C SCL`
- `GPIO2` for the potentiometer input, if you later reconnect an analog pot

Reasons:

- `GPIO18` and `GPIO19` are exposed on the DevKitC-1 header
- they are not listed as strapping pins
- they are not the default `USB-JTAG` pins

Avoid for this purpose unless you have a good reason:

- `GPIO4`, `GPIO5`, `GPIO8`, `GPIO9`, `GPIO15`: strapping pins
- `GPIO12`, `GPIO13`: USB-JTAG pins

Sources:

- ESP32-C6-DevKitC-1 header block
- ESP32-C6 GPIO restrictions documentation

## 3. Electrical Topology

Use one MOSFET channel per pump.

High-level power path:

- battery / charge controller `+` goes to both pump positive terminals
- each pump negative terminal goes to its own MOSFET `drain`
- each MOSFET `source` goes to common `GND`
- ESP32 `GND` must connect to the same common `GND`

High-level control path:

- `GPIO18` drives MOSFET channel 1 gate
- `GPIO19` drives MOSFET channel 2 gate
- `GPIO16` controls the ultrasonic sensor high-side PMOS
- `GPIO20` controls `TOF_XSHUT`
- `GPIO6` and `GPIO7` provide the shared I2C bus for `Chirp` and `TOF`

## 4. Ultrasonic Sensor Power Switching

The ultrasonic sensor is not tied directly to `3V3` in the current design.
Its `VCC` is switched by a high-side `P-channel` MOSFET so the ESP32 can depower it between measurements.

Wiring:

- `IRLML6402 Source` -> `3V3`
- `IRLML6402 Drain` -> ultrasonic sensor `VCC`
- `IRLML6402 Gate` -> `GPIO16` through `100 ohm`
- `IRLML6402 Gate` -> `3V3` through `10k ohm`

Behavior:

- `GPIO16 = LOW` -> ultrasonic sensor powered
- `GPIO16 = HIGH` -> ultrasonic sensor off

## 5. Per-Channel Wiring

For each pump channel:

- ESP32 GPIO -> `100 ohm` resistor -> MOSFET `gate`
- MOSFET `gate` -> `10k ohm` resistor -> `GND`
- MOSFET `source` -> `GND`
- MOSFET `drain` -> pump negative terminal
- pump positive terminal -> battery / charge controller positive rail
- flyback diode across the pump terminals

Flyback diode orientation:

- diode `cathode` to pump `+`
- diode `anode` to pump `-`

That means the diode is reverse-biased during normal operation and only conducts when the pump is switched off and the motor kicks back.

## 6. Text Schematic

```text
Pump supply +  -------------------+--------------------> Pump 1 +
                                  |
                                  +--------------------> Pump 2 +

Pump 1 - -------------------------> MOSFET 1 Drain
MOSFET 1 Source ------------------> GND
ESP32 GPIO18 --[100R]-------------> MOSFET 1 Gate
                      |
                    [10k]
                      |
                     GND

Pump 2 - -------------------------> MOSFET 2 Drain
MOSFET 2 Source ------------------> GND
ESP32 GPIO19 --[100R]-------------> MOSFET 2 Gate
                      |
                    [10k]
                      |
                     GND

ESP32 GND ------------------------> same GND as pump supply

Flyback diode on Pump 1:
  cathode -> Pump 1 +
  anode   -> Pump 1 -

Flyback diode on Pump 2:
  cathode -> Pump 2 +
  anode   -> Pump 2 -
```

## 7. Assembly Order

1. Mount the screw terminals on the perfboard.
2. Mount the two MOSFETs with enough spacing to wire them cleanly.
3. Add the two `100 ohm` gate resistors near the MOSFET gates.
4. Add the two `10k ohm` pulldown resistors from each gate to ground.
5. Create a common ground rail on the board.
6. Create a pump positive distribution rail on the board.
7. Wire each MOSFET source to the ground rail.
8. Wire each MOSFET drain to its own pump output terminal.
9. Bring the shared pump positive rail to the pump output terminals.
10. Install each flyback diode across its pump terminal pair, with the stripe toward pump positive.
11. Add the ESP32 header or socket connection.
12. Wire ESP32 `GND` to the board ground rail.
13. Wire `GPIO18` to pump channel 1 gate resistor input.
14. Wire `GPIO19` to pump channel 2 gate resistor input.
15. Add the `IRLML6402` high-side switch for the ultrasonic sensor.
16. Wire `GPIO16` to the ultrasonic PMOS gate resistor input.
17. Wire `GPIO20` to the `TOF_XSHUT` pin.
18. Wire the charge-controller or battery positive to the board pump positive input terminal.
19. Wire the charge-controller or battery ground to the board ground input terminal.

## 8. First Power-Up Sequence

Do not connect the pumps immediately.

1. Assemble the board completely except for the pumps.
2. Check continuity manually:
   - no short between pump `+` and `GND`
   - no short between MOSFET `gate` and `drain`
   - ESP32 `GND` is common with power `GND`
3. Power the ESP32 separately over USB-C first.
4. Flash a minimal test program that drives `GPIO18` and `GPIO19`.
5. Confirm the GPIO outputs toggle as expected.
6. Disconnect USB power.
7. Connect the pump supply.
8. Power the ESP32 again.
9. Verify that both pumps stay off at boot.
10. Connect one pump and test one channel.
11. Connect the second pump and test the second channel.

## 9. Minimal Firmware Plan

Initial migration goal:

- replace `D4 relay on/off` with `GPIO18` and `GPIO19`
- switch both outputs together first
- keep the option to separate them later

Practical first firmware behavior:

- when watering should run:
  - set `GPIO18 = HIGH`
  - set `GPIO19 = HIGH`
- when watering should stop:
  - set `GPIO18 = LOW`
  - set `GPIO19 = LOW`

Later, if you want independent control:

- pump 1 can stay on its current watering schedule
- pump 2 can get its own schedule, manual override, or zone assignment
- the ultrasonic sensor can be powered only for short sampling windows
- the `TOF` sensor can be put into hardware standby through `XSHUT`

## 10. Potentiometer Migration

If you keep the runtime potentiometer:

- outer pin 1 -> `3V3`
- outer pin 2 -> `GND`
- wiper -> `GPIO2` as ADC input

Important:

- the ESP32 ADC is `3.3 V` based
- do not feed `5 V` into the ESP32 analog input

## 11. Notes for Layout

- keep pump current wiring physically separate from the ESP32 signal wiring
- keep the flyback diodes close to the pump terminals or pump output terminals
- keep the gate resistors close to the MOSFET gates
- keep the shared ground path low-resistance and direct
- keep the ultrasonic PMOS close to the ESP32/sensor power routing
- keep the sensor cable connectors mechanically supported at the enclosure
- do not run pump current through breadboard spring contacts for permanent use

## 12. Migration Summary

Old design:

- Arduino Nano
- one relay output
- both pumps effectively treated as one switched load

New design:

- ESP32-C6-DevKitC-1
- two MOSFET outputs
- one PMOS high-side switch for the ultrasonic sensor
- each pump has its own switching channel
- firmware can run the pumps together now and separately later
- `Chirp` and `TOF` share I2C
- the ultrasonic sensor is power-gated for lower standby consumption
