#ifndef CHIRP_SENSOR_CORE_H
#define CHIRP_SENSOR_CORE_H

#include <stdint.h>

class ChirpSensorCore
{
public:
    void setInitialized(bool value)
    {
        initialized = value;
    }

    bool isInitialized() const
    {
        return initialized;
    }

    bool convertMoisture(int rawCapacitance, uint16_t &capacitance) const
    {
        if (!initialized || rawCapacitance < 0)
        {
            return false;
        }

        capacitance = static_cast<uint16_t>(rawCapacitance);
        return true;
    }

    bool convertTemperatureDeciC(int rawDeciC, float &temperatureC) const
    {
        if (!initialized)
        {
            return false;
        }

        temperatureC = static_cast<float>(rawDeciC) / 10.0F;
        return true;
    }

private:
    bool initialized = false;
};

#endif
