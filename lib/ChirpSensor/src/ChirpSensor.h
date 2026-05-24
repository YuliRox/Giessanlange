#ifndef CHIRP_SENSOR_H
#define CHIRP_SENSOR_H

#include <stdint.h>

#include "ChirpSensorCore.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <I2CSoilMoistureSensor.h>

class ChirpSensor
{
public:
    explicit ChirpSensor(uint8_t i2cAddress = 0x20);

    bool begin(TwoWire &wire = Wire);

    bool isBusy();
    bool readMoisture(uint16_t &capacitance);
    bool readTemperatureC(float &temperatureC);

    bool sleep();
    bool reset();

    uint8_t getAddress() const;

private:
    uint8_t i2cAddress;
    I2CSoilMoistureSensor sensor;
    ChirpSensorCore core;
};

#endif

#endif
