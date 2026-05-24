#include "UltraschallSensor.h"

#ifdef ARDUINO

UltraschallSensor::UltraschallSensor(
    HardwareSerial &serial,
    uint8_t uartRxPin,
    int8_t uartTxPin,
    uint8_t powerEnablePin,
    uint32_t baudRate,
    uint32_t powerSettleMs)
    : serial(serial),
      uartRxPin(uartRxPin),
      uartTxPin(uartTxPin),
      powerEnablePin(powerEnablePin),
      baudRate(baudRate),
      powerSettleMs(powerSettleMs)
{
}

void UltraschallSensor::begin()
{
    pinMode(powerEnablePin, OUTPUT);
    powerOff();

    serial.begin(baudRate, SERIAL_8N1, static_cast<int8_t>(uartRxPin), uartTxPin);
}

void UltraschallSensor::powerOn()
{
    digitalWrite(powerEnablePin, LOW);
    powered = true;
    poweredAtMs = millis();
    parser.clear();

    while (serial.available() > 0)
    {
        serial.read();
    }
}

void UltraschallSensor::powerOff()
{
    digitalWrite(powerEnablePin, HIGH);
    powered = false;
    parser.clear();

    while (serial.available() > 0)
    {
        serial.read();
    }
}

bool UltraschallSensor::isPowered() const
{
    return powered;
}

void UltraschallSensor::update()
{
    if (!powered || !powerSettled())
    {
        return;
    }

    while (serial.available() > 0)
    {
        const uint8_t value = static_cast<uint8_t>(serial.read());
        parser.consume(value);
    }
}

bool UltraschallSensor::hasValidDistance() const
{
    return parser.hasValidDistance();
}

uint16_t UltraschallSensor::getDistanceCm() const
{
    return parser.getDistanceCm();
}

bool UltraschallSensor::readDistanceCm(uint16_t &distanceCm, uint32_t timeoutMs)
{
    if (!powered)
    {
        return false;
    }

    const uint32_t startMs = millis();

    while ((millis() - startMs) < timeoutMs)
    {
        update();
        if (hasValidDistance())
        {
            distanceCm = getDistanceCm();
            return true;
        }

        delay(2);
    }

    return false;
}

bool UltraschallSensor::powerSettled() const
{
    return (millis() - poweredAtMs) >= powerSettleMs;
}

#endif
