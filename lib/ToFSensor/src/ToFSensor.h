#ifndef TOF_SENSOR_H
#define TOF_SENSOR_H

#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

class ToFSensor
{
public:
    explicit ToFSensor(uint8_t xshutPin, uint8_t i2cAddress = 0x29, uint16_t bootDelayMs = 5U);

    bool begin(TwoWire &wire = Wire);

    void powerOff();
    void powerOn();
    bool isPowered() const;

    bool readDistanceMm(uint16_t &distanceMm);

    void setTimeout(uint16_t timeoutMs);
    bool didTimeout() const;

    bool setMeasurementTimingBudgetUs(uint32_t budgetUs);
    bool setSignalRateLimitMcps(float limitMcps);

private:
    uint8_t xshutPin;
    uint8_t i2cAddress;
    uint16_t bootDelayMs;

    bool powered = false;
    bool initialized = false;
    bool lastReadTimedOut = false;

    VL53L0X sensor;
};

#endif

#endif
