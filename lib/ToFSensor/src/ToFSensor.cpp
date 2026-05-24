#include "ToFSensor.h"

#ifdef ARDUINO

ToFSensor::ToFSensor(uint8_t xshutPin, uint8_t i2cAddress, uint16_t bootDelayMs)
    : xshutPin(xshutPin),
      i2cAddress(i2cAddress),
      bootDelayMs(bootDelayMs)
{
}

bool ToFSensor::begin(TwoWire &wire)
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

void ToFSensor::powerOff()
{
    digitalWrite(xshutPin, LOW);
    powered = false;
    initialized = false;
}

void ToFSensor::powerOn()
{
    digitalWrite(xshutPin, HIGH);
    powered = true;
    delay(bootDelayMs);
}

bool ToFSensor::isPowered() const
{
    return powered;
}

bool ToFSensor::readDistanceMm(uint16_t &distanceMm)
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

void ToFSensor::setTimeout(uint16_t timeoutMs)
{
    sensor.setTimeout(timeoutMs);
}

bool ToFSensor::didTimeout() const
{
    return sensor.timeoutOccurred();
}

bool ToFSensor::setMeasurementTimingBudgetUs(uint32_t budgetUs)
{
    if (!initialized)
    {
        return false;
    }

    sensor.setMeasurementTimingBudget(budgetUs);
    return true;
}

bool ToFSensor::setSignalRateLimitMcps(float limitMcps)
{
    if (!initialized)
    {
        return false;
    }

    return sensor.setSignalRateLimit(limitMcps);
}

#endif
