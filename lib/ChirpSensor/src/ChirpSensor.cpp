#include "ChirpSensor.h"

#ifdef ARDUINO

ChirpSensor::ChirpSensor(uint8_t i2cAddress)
    : i2cAddress(i2cAddress),
      sensor(i2cAddress)
{
}

bool ChirpSensor::begin(TwoWire &wire)
{
    wire.begin();

    sensor.begin(true);
    core.setInitialized(true);
    return true;
}

bool ChirpSensor::isBusy()
{
    if (!core.isInitialized())
    {
        return true;
    }

    return sensor.isBusy();
}

bool ChirpSensor::readMoisture(uint16_t &capacitance)
{
    const int raw = sensor.getCapacitance();
    return core.convertMoisture(raw, capacitance);
}

bool ChirpSensor::readTemperatureC(float &temperatureC)
{
    const int rawDeciC = sensor.getTemperature();
    return core.convertTemperatureDeciC(rawDeciC, temperatureC);
}

bool ChirpSensor::sleep()
{
    if (!core.isInitialized())
    {
        return false;
    }

    sensor.sleep();
    return true;
}

bool ChirpSensor::reset()
{
    if (!core.isInitialized())
    {
        return false;
    }

    sensor.resetSensor();
    delay(500);
    return true;
}

uint8_t ChirpSensor::getAddress() const
{
    return i2cAddress;
}

#endif
