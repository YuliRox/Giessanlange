#ifndef ULTRASCHALL_SENSOR_H
#define ULTRASCHALL_SENSOR_H

#include <stdint.h>

#include "A02yyuwFrameParser.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <HardwareSerial.h>

class UltraschallSensor
{
public:
    UltraschallSensor(
        HardwareSerial &serial,
        uint8_t uartRxPin,
        int8_t uartTxPin,
        uint8_t powerEnablePin,
        uint32_t baudRate = 9600U,
        uint32_t powerSettleMs = 120U);

    void begin();

    // PMOS is wired active-low: LOW powers the sensor, HIGH cuts power.
    void powerOn();
    void powerOff();
    bool isPowered() const;

    // Call repeatedly from loop() to parse incoming UART frames.
    void update();

    bool hasValidDistance() const;
    uint16_t getDistanceCm() const;

    // Convenience helper for one-shot read after power-on.
    bool readDistanceCm(uint16_t &distanceCm, uint32_t timeoutMs = 300U);

private:
    HardwareSerial &serial;
    const uint8_t uartRxPin;
    const int8_t uartTxPin;
    const uint8_t powerEnablePin;
    const uint32_t baudRate;
    const uint32_t powerSettleMs;

    bool powered = false;
    uint32_t poweredAtMs = 0U;

    A02yyuwFrameParser parser;

    bool powerSettled() const;
};

#endif

#endif
