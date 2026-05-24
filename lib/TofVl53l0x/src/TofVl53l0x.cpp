#include "TofVl53l0x.h"

#ifdef ARDUINO

TofVl53l0x::TofVl53l0x(uint8_t xshutPin, uint8_t i2cAddress, uint16_t bootDelayMs)
    : xshutPin(xshutPin),
      i2cAddress(i2cAddress),
      bootDelayMs(bootDelayMs)
{
}

bool TofVl53l0x::begin(TwoWire &wire)
{
    pinMode(xshutPin, OUTPUT);

    powerOff();
    delay(2);
    powerOn();

    sensor.setBus(&wire);

    if (!sensor.init())
    {
        initialized = false;
        return false;
    }

    sensor.setAddress(i2cAddress);
    sensor.startContinuous();

    initialized = true;
    return true;
}

void TofVl53l0x::powerOff()
{
    digitalWrite(xshutPin, LOW);
    powered = false;
    initialized = false;
}

void TofVl53l0x::powerOn()
{
    digitalWrite(xshutPin, HIGH);
    powered = true;
    delay(bootDelayMs);
}

bool TofVl53l0x::isPowered() const
{
    return powered;
}

bool TofVl53l0x::readDistanceMm(uint16_t &distanceMm)
{
    if (!powered || !initialized)
    {
        return false;
    }

    distanceMm = sensor.readRangeContinuousMillimeters();
    if (sensor.timeoutOccurred())
    {
        return false;
    }

    return true;
}

void TofVl53l0x::setTimeout(uint16_t timeoutMs)
{
    sensor.setTimeout(timeoutMs);
}

bool TofVl53l0x::didTimeout() const
{
    return sensor.timeoutOccurred();
}

bool TofVl53l0x::setMeasurementTimingBudgetUs(uint32_t budgetUs)
{
    if (!initialized)
    {
        return false;
    }

    sensor.setMeasurementTimingBudget(budgetUs);
    return true;
}

bool TofVl53l0x::setSignalRateLimitMcps(float limitMcps)
{
    if (!initialized)
    {
        return false;
    }

    return sensor.setSignalRateLimit(limitMcps);
}

#endif
